# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=import-error,print-statement,relative-import

"""Plumbing for a Jinja-based code generator, including CodeGeneratorBase, a base class for all generators."""

import os
import posixpath
import re
import sys

from idl_types import set_ancestors, IdlType
from v8_globals import includes
from v8_interface import constant_filters
from v8_types import set_component_dirs
from v8_methods import method_filters
from v8_utilities import capitalize
from utilities import (idl_filename_to_component, is_valid_component_dependency,
                       format_remove_duplicates, format_blink_cpp_source_code,
                       to_snake_case, normalize_path)
import v8_utilities

# Path handling for libraries and templates
# Paths have to be normalized because Jinja uses the exact template path to
# determine the hash used in the cache filename, and we need a pre-caching step
# to be concurrency-safe. Use absolute path because __file__ is absolute if
# module is imported, and relative if executed directly.
# If paths differ between pre-caching and individual file compilation, the cache
# is regenerated, which causes a race condition and breaks concurrent build,
# since some compile processes will try to read the partially written cache.
MODULE_PATH, _ = os.path.split(os.path.realpath(__file__))
THIRD_PARTY_DIR = os.path.normpath(os.path.join(
    MODULE_PATH, os.pardir, os.pardir, os.pardir, os.pardir))
TEMPLATES_DIR = os.path.normpath(os.path.join(
    MODULE_PATH, os.pardir, 'templates'))

# jinja2 is in chromium's third_party directory.
# Insert at 1 so at front to override system libraries, and
# after path[0] == invoking script dir
sys.path.insert(1, THIRD_PARTY_DIR)
import jinja2


def generate_indented_conditional(code, conditional):
    # Indent if statement to level of original code
    indent = re.match(' *', code).group(0)
    return ('%sif (%s) {\n' % (indent, conditional) +
            '  %s\n' % '\n  '.join(code.splitlines()) +
            '%s}\n' % indent)


# [Exposed]
def exposed_if(code, exposed_test):
    if not exposed_test:
        return code
    return generate_indented_conditional(code, 'execution_context && (%s)' % exposed_test)


# [SecureContext]
def secure_context_if(code, secure_context_test):
    if secure_context_test is None:
        return code
    return generate_indented_conditional(code, secure_context_test)


# [RuntimeEnabled]
def origin_trial_enabled_if(code, origin_trial_feature_name, execution_context=None):
    if not origin_trial_feature_name:
        return code

    function = v8_utilities.origin_trial_function_call(
        origin_trial_feature_name, execution_context)
    return generate_indented_conditional(code, function)


# [RuntimeEnabled]
def runtime_enabled_if(code, name):
    if not name:
        return code

    function = v8_utilities.runtime_enabled_function(name)
    return generate_indented_conditional(code, function)


def initialize_jinja_env(cache_dir):
    jinja_env = jinja2.Environment(
        loader=jinja2.FileSystemLoader(TEMPLATES_DIR),
        # Bytecode cache is not concurrency-safe unless pre-cached:
        # if pre-cached this is read-only, but writing creates a race condition.
        bytecode_cache=jinja2.FileSystemBytecodeCache(cache_dir),
        keep_trailing_newline=True,  # newline-terminate generated files
        lstrip_blocks=True,  # so can indent control flow tags
        trim_blocks=True)
    jinja_env.filters.update({
        'blink_capitalize': capitalize,
        'exposed': exposed_if,
        'format_blink_cpp_source_code': format_blink_cpp_source_code,
        'format_remove_duplicates': format_remove_duplicates,
        'origin_trial_enabled': origin_trial_enabled_if,
        'runtime_enabled': runtime_enabled_if,
        'runtime_enabled_function': v8_utilities.runtime_enabled_function,
        'secure_context': secure_context_if})
    jinja_env.filters.update(constant_filters())
    jinja_env.filters.update(method_filters())
    return jinja_env


_BLINK_RELATIVE_PATH_PREFIXES = ('bindings/', 'core/', 'modules/', 'platform/')

def normalize_and_sort_includes(include_paths):
    normalized_include_paths = set()
    for include_path in include_paths:
        match = re.search(r'/gen/(third_party/blink/.*)$', posixpath.abspath(include_path))
        if match:
            include_path = match.group(1)
        elif include_path.startswith(_BLINK_RELATIVE_PATH_PREFIXES):
            include_path = 'third_party/blink/renderer/' + include_path
        normalized_include_paths.add(include_path)
    return sorted(normalized_include_paths)


def render_template(template, context):
    filename = str(template.filename)
    filename = filename[filename.rfind('third_party'):]
    filename = normalize_path(filename)

    context['jinja_template_filename'] = filename
    return template.render(context)


class CodeGeneratorBase(object):
    """Base class for jinja-powered jinja template generation.
    """
    def __init__(self, generator_name, info_provider, cache_dir, output_dir):
        self.generator_name = generator_name
        self.info_provider = info_provider
        self.jinja_env = initialize_jinja_env(cache_dir)
        self.output_dir = output_dir
        self.set_global_type_info()

    def should_generate_code(self, definitions):
        return definitions.interfaces or definitions.dictionaries

    def set_global_type_info(self):
        interfaces_info = self.info_provider.interfaces_info
        set_ancestors(interfaces_info['ancestors'])
        IdlType.set_callback_interfaces(interfaces_info['callback_interfaces'])
        IdlType.set_dictionaries(interfaces_info['dictionaries'])
        IdlType.set_enums(self.info_provider.enumerations)
        IdlType.set_callback_functions(self.info_provider.callback_functions)
        IdlType.set_implemented_as_interfaces(interfaces_info['implemented_as_interfaces'])
        IdlType.set_garbage_collected_types(interfaces_info['garbage_collected_interfaces'])
        IdlType.set_garbage_collected_types(interfaces_info['dictionaries'])
        set_component_dirs(interfaces_info['component_dirs'])

    def render_templates(self, include_paths, header_template, cpp_template,
                         context, component=None):
        context['code_generator'] = self.generator_name

        # Add includes for any dependencies
        for include_path in include_paths:
            if component:
                dependency = idl_filename_to_component(include_path)
                assert is_valid_component_dependency(component, dependency)
            includes.add(include_path)

        cpp_includes = set(context.get('cpp_includes', []))
        context['cpp_includes'] = normalize_and_sort_includes(cpp_includes | includes)
        context['header_includes'] = normalize_and_sort_includes(context['header_includes'])

        header_text = render_template(header_template, context)
        cpp_text = render_template(cpp_template, context)
        return header_text, cpp_text

    def generate_code(self, definitions, definition_name):
        """Invokes code generation. The [definitions] argument is a list of definitions,
        and the [definition_name] is the name of the definition
        """
        # This should be implemented in subclasses.
        raise NotImplementedError()

    def normalize_this_header_path(self, header_path):
        header_path = normalize_path(header_path)
        match = re.search('(third_party/blink/.*)$', header_path)
        assert match, 'Unkown style of path to output: ' + header_path
        return match.group(1)


def main(argv):
    # If file itself executed, cache templates
    try:
        cache_dir = argv[1]
        dummy_filename = argv[2]
    except IndexError:
        print 'Usage: %s CACHE_DIR DUMMY_FILENAME' % argv[0]
        return 1

    # Cache templates
    jinja_env = initialize_jinja_env(cache_dir)
    template_filenames = [filename for filename in os.listdir(TEMPLATES_DIR)
                          # Skip .svn, directories, etc.
                          if filename.endswith(('.tmpl', '.txt'))]
    for template_filename in template_filenames:
        jinja_env.get_template(template_filename)

    # Create a dummy file as output for the build system,
    # since filenames of individual cache files are unpredictable and opaque
    # (they are hashes of the template path, which varies based on environment)
    with open(dummy_filename, 'w') as dummy_file:
        pass  # |open| creates or touches the file


if __name__ == '__main__':
    sys.exit(main(sys.argv))
