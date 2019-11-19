# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utility functions (file reading, simple IDL parsing by regexes) for IDL build.

Design doc: http://www.chromium.org/developers/design-documents/idl-build
"""

import os
import cPickle as pickle
import re
import shlex
import string
import subprocess
import sys

sys.path.append(os.path.join(os.path.dirname(__file__),
                             '..', '..', 'build', 'scripts'))
from blinkbuild.name_style_converter import NameStyleConverter


KNOWN_COMPONENTS = frozenset(['core', 'modules'])
KNOWN_COMPONENTS_WITH_TESTING = frozenset(['core', 'modules', 'testing'])


def idl_filename_to_basename(idl_filename):
    """Returns the basename without the extension."""
    return os.path.splitext(os.path.basename(idl_filename))[0]


def idl_filename_to_component_with_known_components(idl_filename, known_components):
    path = os.path.dirname(os.path.realpath(idl_filename))
    while path:
        dirname, basename = os.path.split(path)
        if not basename:
            break
        if basename.lower() in known_components:
            return basename.lower()
        path = dirname
    raise Exception('Unknown component type for %s' % idl_filename)


def idl_filename_to_component(idl_filename):
    return idl_filename_to_component_with_known_components(idl_filename, KNOWN_COMPONENTS)


def is_testing_target(idl_filename):
    component = idl_filename_to_component_with_known_components(idl_filename, KNOWN_COMPONENTS_WITH_TESTING)
    return component == 'testing'


# See whether "component" can depend on "dependency" or not:
# Suppose that we have interface X and Y:
# - if X is a partial interface and Y is the original interface,
#   use is_valid_component_dependency(X, Y).
# - if X implements Y, use is_valid_component_dependency(X, Y)
# Suppose that X is a cpp file and Y is a header file:
# - if X includes Y, use is_valid_component_dependency(X, Y)
def is_valid_component_dependency(component, dependency):
    assert component in KNOWN_COMPONENTS
    assert dependency in KNOWN_COMPONENTS
    if component == 'core' and dependency == 'modules':
        return False
    return True


class ComponentInfoProvider(object):
    """Base class of information provider which provides component-specific
    information.
    """
    def __init__(self):
        pass

    @property
    def interfaces_info(self):
        return {}

    @property
    def component_info(self):
        return {}

    @property
    def enumerations(self):
        return {}

    @property
    def typedefs(self):
        return {}

    @property
    def union_types(self):
        return set()

    @property
    def include_path_for_union_types(self, union_type):
        return None

    @property
    def callback_functions(self):
        return {}


class ComponentInfoProviderCore(ComponentInfoProvider):
    def __init__(self, interfaces_info, component_info):
        super(ComponentInfoProviderCore, self).__init__()
        self._interfaces_info = interfaces_info
        self._component_info = component_info

    @property
    def interfaces_info(self):
        return self._interfaces_info

    @property
    def component_info(self):
        return self._component_info

    @property
    def enumerations(self):
        return self._component_info['enumerations']

    @property
    def typedefs(self):
        return self._component_info['typedefs']

    @property
    def union_types(self):
        return self._component_info['union_types']

    def include_path_for_union_types(self, union_type):
        name = to_snake_case(shorten_union_name(union_type))
        return 'bindings/core/v8/%s.h' % name

    @property
    def callback_functions(self):
        return self._component_info['callback_functions']

    @property
    def specifier_for_export(self):
        return 'CORE_EXPORT '

    @property
    def include_path_for_export(self):
        return 'core/core_export.h'


class ComponentInfoProviderModules(ComponentInfoProvider):
    def __init__(self, interfaces_info, component_info_core,
                 component_info_modules):
        super(ComponentInfoProviderModules, self).__init__()
        self._interfaces_info = interfaces_info
        self._component_info_core = component_info_core
        self._component_info_modules = component_info_modules

    @property
    def interfaces_info(self):
        return self._interfaces_info

    @property
    def component_info(self):
        return self._component_info_modules

    @property
    def enumerations(self):
        enums = self._component_info_core['enumerations'].copy()
        enums.update(self._component_info_modules['enumerations'])
        return enums

    @property
    def typedefs(self):
        typedefs = self._component_info_core['typedefs'].copy()
        typedefs.update(self._component_info_modules['typedefs'])
        return typedefs

    @property
    def union_types(self):
        # Remove duplicate union types from component_info_modules to avoid
        # generating multiple container generation.
        return self._component_info_modules['union_types'] - self._component_info_core['union_types']

    def include_path_for_union_types(self, union_type):
        core_union_type_names = [core_union_type.name for core_union_type
                                 in self._component_info_core['union_types']]
        name = shorten_union_name(union_type)
        if union_type.name in core_union_type_names:
            return 'bindings/core/v8/%s.h' % to_snake_case(name)
        return 'bindings/modules/v8/%s.h' % to_snake_case(name)

    @property
    def callback_functions(self):
        return dict(self._component_info_core['callback_functions'].items() +
                    self._component_info_modules['callback_functions'].items())

    @property
    def specifier_for_export(self):
        return 'MODULES_EXPORT '

    @property
    def include_path_for_export(self):
        return 'modules/modules_export.h'


def load_interfaces_info_overall_pickle(info_dir):
    with open(os.path.join(info_dir, 'interfaces_info.pickle')) as interface_info_file:
        return pickle.load(interface_info_file)


def merge_dict_recursively(target, diff):
    """Merges two dicts into one.
    |target| will be updated with |diff|.  Part of |diff| may be re-used in
    |target|.
    """
    for key, value in diff.iteritems():
        if key not in target:
            target[key] = value
        elif type(value) == dict:
            merge_dict_recursively(target[key], value)
        elif type(value) == list:
            target[key].extend(value)
        elif type(value) == set:
            target[key].update(value)
        else:
            # Testing IDLs want to overwrite the values.  Production code
            # doesn't need any overwriting.
            target[key] = value


def create_component_info_provider_core(info_dir):
    interfaces_info = load_interfaces_info_overall_pickle(info_dir)
    with open(os.path.join(info_dir, 'core', 'component_info_core.pickle')) as component_info_file:
        component_info = pickle.load(component_info_file)
    return ComponentInfoProviderCore(interfaces_info, component_info)


def create_component_info_provider_modules(info_dir):
    interfaces_info = load_interfaces_info_overall_pickle(info_dir)
    with open(os.path.join(info_dir, 'core', 'component_info_core.pickle')) as component_info_file:
        component_info_core = pickle.load(component_info_file)
    with open(os.path.join(info_dir, 'modules', 'component_info_modules.pickle')) as component_info_file:
        component_info_modules = pickle.load(component_info_file)
    return ComponentInfoProviderModules(
        interfaces_info, component_info_core, component_info_modules)


def create_component_info_provider(info_dir, component):
    if component == 'core':
        return create_component_info_provider_core(info_dir)
    elif component == 'modules':
        return create_component_info_provider_modules(info_dir)
    else:
        return ComponentInfoProvider()


################################################################################
# Basic file reading/writing
################################################################################

def get_file_contents(filename):
    with open(filename) as f:
        return f.read()


def read_file_to_list(filename):
    """Returns a list of (stripped) lines for a given filename."""
    with open(filename) as f:
        return [line.rstrip('\n') for line in f]


def resolve_cygpath(cygdrive_names):
    if not cygdrive_names:
        return []
    cmd = ['cygpath', '-f', '-', '-wa']
    process = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    idl_file_names = []
    for file_name in cygdrive_names:
        process.stdin.write('%s\n' % file_name)
        process.stdin.flush()
        idl_file_names.append(process.stdout.readline().rstrip())
    process.stdin.close()
    process.wait()
    return idl_file_names


def read_idl_files_list_from_file(filename):
    """Similar to read_file_to_list, but also resolves cygpath."""
    with open(filename) as input_file:
        file_names = sorted(shlex.split(input_file))
        idl_file_names = [file_name for file_name in file_names
                          if not file_name.startswith('/cygdrive')]
        cygdrive_names = [file_name for file_name in file_names
                          if file_name.startswith('/cygdrive')]
        idl_file_names.extend(resolve_cygpath(cygdrive_names))
        return idl_file_names


def read_pickle_files(pickle_filenames):
    for pickle_filename in pickle_filenames:
        yield read_pickle_file(pickle_filename)


def read_pickle_file(pickle_filename):
    with open(pickle_filename) as pickle_file:
        return pickle.load(pickle_file)


def write_file(new_text, destination_filename):
    # If |new_text| is same with the file content, we skip updating.
    if os.path.isfile(destination_filename):
        with open(destination_filename) as destination_file:
            if destination_file.read() == new_text:
                return

    destination_dirname = os.path.dirname(destination_filename)
    if not os.path.exists(destination_dirname):
        os.makedirs(destination_dirname)
    # Write file in binary so that when run on Windows, line endings are not
    # converted
    with open(destination_filename, 'wb') as destination_file:
        destination_file.write(new_text)


def write_pickle_file(pickle_filename, data):
    # If |data| is same with the file content, we skip updating.
    if os.path.isfile(pickle_filename):
        with open(pickle_filename) as pickle_file:
            try:
                if pickle.load(pickle_file) == data:
                    return
            except Exception:
                # If trouble unpickling, overwrite
                pass
    with open(pickle_filename, 'w') as pickle_file:
        pickle.dump(data, pickle_file)


################################################################################
# IDL parsing
#
# TODO(bashi): We use regular expressions for parsing; this is incorrect
# (Web IDL is not a regular language) and broken. Remove these functions and
# always use the parser and ASTs.
# Leading and trailing context (e.g. following '{') used to avoid false matches.
################################################################################

def is_non_legacy_callback_interface_from_idl(file_contents):
    """Returns True if the specified IDL is a non-legacy callback interface."""
    match = re.search(r'callback\s+interface\s+\w+\s*{', file_contents)
    # Having constants means it's a legacy callback interface.
    # https://heycam.github.io/webidl/#legacy-callback-interface-object
    return bool(match) and not re.search(r'\s+const\b', file_contents)


def is_interface_mixin_from_idl(file_contents):
    """Returns True if the specified IDL is an interface mixin."""
    match = re.search(r'interface\s+mixin\s+\w+\s*{', file_contents)
    return bool(match)


def should_generate_impl_file_from_idl(file_contents):
    """True when a given IDL file contents could generate .h/.cpp files."""
    # FIXME: This would be error-prone and we should use AST rather than
    # improving the regexp pattern.
    match = re.search(r'(interface|dictionary)\s+\w+', file_contents)
    return bool(match)


def match_interface_extended_attributes_and_name_from_idl(file_contents):
    # Strip comments
    # re.compile needed b/c Python 2.6 doesn't support flags in re.sub
    single_line_comment_re = re.compile(r'//.*$', flags=re.MULTILINE)
    block_comment_re = re.compile(r'/\*.*?\*/', flags=re.MULTILINE | re.DOTALL)
    file_contents = re.sub(single_line_comment_re, '', file_contents)
    file_contents = re.sub(block_comment_re, '', file_contents)

    match = re.search(
        r'(?:\[([^{};]*)\]\s*)?'
        r'(interface|interface\s+mixin|callback\s+interface|partial\s+interface|dictionary)\s+'
        r'(\w+)\s*'
        r'(:\s*\w+\s*)?'
        r'{',
        file_contents, flags=re.DOTALL)
    return match


def get_interface_extended_attributes_from_idl(file_contents):
    match = match_interface_extended_attributes_and_name_from_idl(file_contents)
    if not match or not match.group(1):
        return {}

    extended_attributes_string = match.group(1).strip()
    parts = [extended_attribute.strip()
             for extended_attribute in re.split(',', extended_attributes_string)
             # Discard empty parts, which may exist due to trailing comma
             if extended_attribute.strip()]

    # Joins |parts| with commas as far as the parences are not balanced,
    # and then converts a (joined) term to a dict entry.
    # ex. ['ab=c', 'ab(cd', 'ef', 'gh)', 'f=(a', 'b)']
    #   => {'ab': 'c', 'ab(cd,ef,gh)': '', 'f': '(a,b)'}
    extended_attributes = {}
    concatenated = None
    for part in parts:
        concatenated = (concatenated + ', ' + part) if concatenated else part
        parences = concatenated.count('(') - concatenated.count(')')
        square_brackets = concatenated.count('[') - concatenated.count(']')
        if parences < 0 or square_brackets < 0:
            raise ValueError('You have more close braces than open braces.')
        if parences == 0 and square_brackets == 0:
            name, _, value = map(string.strip, concatenated.partition('='))
            extended_attributes[name] = value
            concatenated = None
    return extended_attributes


def get_interface_exposed_arguments(file_contents):
    match = match_interface_extended_attributes_and_name_from_idl(file_contents)
    if not match or not match.group(1):
        return None

    extended_attributes_string = match.group(1)
    match = re.search(r'[^=]\bExposed\(([^)]*)\)', file_contents)
    if not match:
        return None
    arguments = []
    for argument in map(string.strip, match.group(1).split(',')):
        exposed, runtime_enabled = argument.split()
        arguments.append({'exposed': exposed, 'runtime_enabled': runtime_enabled})

    return arguments


def get_first_interface_name_from_idl(file_contents):
    match = match_interface_extended_attributes_and_name_from_idl(file_contents)
    if match:
        return match.group(3)
    return None


# Workaround for crbug.com/611437 and crbug.com/711464
# TODO(bashi): Remove this hack once we resolve too-long generated file names.
# pylint: disable=line-too-long
def shorten_union_name(union_type):
    aliases = {
        # modules/canvas2d/CanvasRenderingContext2D.idl
        'CSSImageValueOrHTMLImageElementOrSVGImageElementOrHTMLVideoElementOrHTMLCanvasElementOrImageBitmapOrOffscreenCanvas': 'CanvasImageSource',
        # modules/canvas/htmlcanvas/html_canvas_element_module_support_webgl2_compute.idl
        # Due to html_canvas_element_module_support_webgl2_compute.idl and html_canvas_element_module.idl are exclusive in modules_idl_files.gni, they have same shorten name.
        'CanvasRenderingContext2DOrWebGLRenderingContextOrWebGL2RenderingContextOrWebGL2ComputeRenderingContextOrImageBitmapRenderingContextOrGPUCanvasContext': 'RenderingContext',
        # modules/canvas/htmlcanvas/html_canvas_element_module.idl
        'CanvasRenderingContext2DOrWebGLRenderingContextOrWebGL2RenderingContextOrImageBitmapRenderingContextOrGPUCanvasContext': 'RenderingContext',
        # core/frame/window_or_worker_global_scope.idl
        'HTMLImageElementOrSVGImageElementOrHTMLVideoElementOrHTMLCanvasElementOrBlobOrImageDataOrImageBitmapOrOffscreenCanvas': 'ImageBitmapSource',
        # bindings/tests/idls/core/TestTypedefs.idl
        'NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord': 'NestedUnionType',
        # modules/canvas/offscreencanvas/offscreen_canvas_module_support_webgl2_compute.idl.
        # Due to offscreen_canvas_module_support_webgl2_compute.idl and offscreen_canvas_module.idl are exclusive in modules_idl_files.gni, they have same shorten name.
        'OffscreenCanvasRenderingContext2DOrWebGLRenderingContextOrWebGL2RenderingContextOrWebGL2ComputeRenderingContextOrImageBitmapRenderingContext': 'OffscreenRenderingContext',
        # modules/canvas/offscreencanvas/offscreen_canvas_module.idl
        'OffscreenCanvasRenderingContext2DOrWebGLRenderingContextOrWebGL2RenderingContextOrImageBitmapRenderingContext': 'OffscreenRenderingContext',
    }

    idl_type = union_type
    if union_type.is_nullable:
        idl_type = union_type.inner_type
    name = idl_type.cpp_type or idl_type.name
    alias = aliases.get(name)
    if alias:
        return alias
    if len(name) >= 80:
        raise Exception('crbug.com/711464: The union name %s is too long. '
                        'Please add an alias to shorten_union_name()' % name)
    return name


def to_snake_case(name):
    return NameStyleConverter(name).to_snake_case()


def to_header_guard(path):
    return NameStyleConverter(path).to_header_guard()


def normalize_path(path):
    return path.replace("\\", "/")


def format_remove_duplicates(text, patterns):
    """Removes duplicated line-basis patterns.

    Based on simple pattern matching, removes duplicated lines in a block
    of lines.  Lines that match with a same pattern are considered as
    duplicates.

    Designed to be used as a filter function for Jinja2.

    Args:
        text: A str of multi-line text.
        patterns: A list of str where each str represents a simple
            pattern.  The patterns are not considered as regexp, and
            exact match is applied.

    Returns:
        A formatted str with duplicates removed.
    """
    pattern_founds = [False] * len(patterns)
    output = []
    for line in text.split('\n'):
        to_be_removed = False
        for i, pattern in enumerate(patterns):
            if pattern not in line:
                continue
            if pattern_founds[i]:
                to_be_removed = True
            else:
                pattern_founds[i] = True
        if to_be_removed:
            continue
        output.append(line)

    # Let |'\n'.join| emit the last newline.
    if output:
        output.append('')

    return '\n'.join(output)


def format_blink_cpp_source_code(text):
    """Formats C++ source code.

    Supported modifications are:
    - Reduces successive empty lines into a single empty line.
    - Removes empty lines just after an open brace or before closing brace.
      This rule does not apply to namespaces.

    Designed to be used as a filter function for Jinja2.

    Args:
        text: A str of C++ source code.

    Returns:
        A formatted str of the source code.
    """
    re_empty_line = re.compile(r'^\s*$')
    re_first_brace = re.compile(r'(?P<first>[{}])')
    re_last_brace = re.compile(r'.*(?P<last>[{}]).*?$')
    was_open_brace = True  # Trick to remove the empty lines at the beginning.
    was_empty_line = False
    output = []
    for line in text.split('\n'):
        # Skip empty lines.
        if line == '' or re_empty_line.match(line):
            was_empty_line = True
            continue

        # Emit a single empty line if needed.
        if was_empty_line:
            was_empty_line = False
            if '}' in line:
                match = re_first_brace.search(line)
            else:
                match = None

            if was_open_brace:
                # No empty line just after an open brace.
                pass
            elif match and match.group('first') == '}' and 'namespace' not in line:
                # No empty line just before a closing brace.
                pass
            else:
                # Preserve a single empty line.
                output.append('')

        # Emit the line itself.
        output.append(line)

        # Remember an open brace.
        if '{' in line:
            match = re_last_brace.search(line)
        else:
            match = None
        was_open_brace = (match and match.group('last') == '{' and 'namespace' not in line)

    # Let |'\n'.join| emit the last newline.
    if output:
        output.append('')

    return '\n'.join(output)
