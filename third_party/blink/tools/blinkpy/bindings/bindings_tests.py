# Copyright (C) 2011 Google Inc.  All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

from contextlib import contextmanager
import difflib
import filecmp
import fnmatch
import os
import shutil
import tempfile

from blinkpy.common.system.executive import Executive

from blinkpy.common import path_finder
path_finder.add_bindings_scripts_dir_to_sys_path()
path_finder.add_build_scripts_dir_to_sys_path()

from code_generator_v8 import CodeGeneratorDictionaryImpl
from code_generator_v8 import CodeGeneratorV8
from code_generator_v8 import CodeGeneratorUnionType
from code_generator_v8 import CodeGeneratorCallbackFunction
from compute_interfaces_info_individual import InterfaceInfoCollector
from compute_interfaces_info_overall import (compute_interfaces_info_overall,
                                             interfaces_info)
from generate_origin_trial_features import generate_origin_trial_features
from idl_compiler import (generate_bindings,
                          generate_union_type_containers,
                          generate_dictionary_impl,
                          generate_callback_function_impl)
from json5_generator import Json5File
from utilities import ComponentInfoProviderCore
from utilities import ComponentInfoProviderModules
from utilities import get_file_contents
from utilities import get_first_interface_name_from_idl
from utilities import to_snake_case


PASS_MESSAGE = 'All tests PASS!'
FAIL_MESSAGE = """Some tests FAIL!
To update the reference files, execute:
    third_party/blink/tools/run_bindings_tests.py --reset-results

If the failures are not due to your changes, test results may be out of sync;
please rebaseline them in a separate CL, after checking that tests fail in ToT.
In CL, please set:
NOTRY=true
TBR=someone in third_party/blink/renderer/bindings/OWNERS or WATCHLISTS:bindings
"""

SOURCE_PATH = path_finder.get_source_dir()
DEPENDENCY_IDL_FILES = frozenset([
    'test_interface_mixin.idl',
    'test_interface_mixin_2.idl',
    'test_interface_mixin_3.idl',
    'test_interface_partial.idl',
    'test_interface_partial_2.idl',
    'test_interface_partial_3.idl',
    'test_interface_partial_4.idl',
    'test_interface_partial_secure_context.idl',
    'test_interface_2_partial.idl',
    'test_interface_2_partial_2.idl',
])

COMPONENT_DIRECTORY = frozenset(['core', 'modules'])
TEST_INPUT_DIRECTORY = os.path.join(SOURCE_PATH, 'bindings', 'tests', 'idls')
REFERENCE_DIRECTORY = os.path.join(SOURCE_PATH, 'bindings', 'tests', 'results')

# component -> ComponentInfoProvider.
# Note that this dict contains information about testing idl files, which live
# in Source/bindings/tests/idls/{core,modules}, not in Source/{core,modules}.
component_info_providers = {}


@contextmanager
def TemporaryDirectory():
    """Wrapper for tempfile.mkdtemp() so it's usable with 'with' statement.

    Simple backport of tempfile.TemporaryDirectory from Python 3.2.
    """
    name = tempfile.mkdtemp()
    try:
        yield name
    finally:
        shutil.rmtree(name)


def generate_interface_dependencies(runtime_enabled_features):
    def idl_paths_recursive(directory):
        # This is slow, especially on Windows, due to os.walk making
        # excess stat() calls. Faster versions may appear in Python 3.5 or
        # later:
        # https://github.com/benhoyt/scandir
        # http://bugs.python.org/issue11406
        idl_paths = []
        for dirpath, _, files in os.walk(directory):
            idl_paths.extend(os.path.join(dirpath, filename)
                             for filename in fnmatch.filter(files, '*.idl'))
        return idl_paths

    def collect_blink_idl_paths():
        """Returns IDL file paths which blink actually uses."""
        idl_paths = []
        for component in COMPONENT_DIRECTORY:
            directory = os.path.join(SOURCE_PATH, component)
            idl_paths.extend(idl_paths_recursive(directory))
        return idl_paths

    def collect_interfaces_info(idl_path_list):
        info_collector = InterfaceInfoCollector()
        for idl_path in idl_path_list:
            info_collector.collect_info(idl_path)
        info = info_collector.get_info_as_dict()
        # TestDictionary.{h,cpp} are placed under
        # Source/bindings/tests/idls/core. However, IdlCompiler generates
        # TestDictionary.{h,cpp} by using relative_dir.
        # So the files will be generated under
        # output_dir/core/bindings/tests/idls/core.
        # To avoid this issue, we need to clear relative_dir here.
        for value in info['interfaces_info'].itervalues():
            value['relative_dir'] = ''
        component_info = info_collector.get_component_info_as_dict(runtime_enabled_features)
        return info, component_info

    # We compute interfaces info for *all* IDL files, not just test IDL
    # files, as code generator output depends on inheritance (both ancestor
    # chain and inherited extended attributes), and some real interfaces
    # are special-cased, such as Node.
    #
    # For example, when testing the behavior of interfaces that inherit
    # from Node, we also need to know that these inherit from EventTarget,
    # since this is also special-cased and Node inherits from EventTarget,
    # but this inheritance information requires computing dependencies for
    # the real Node.idl file.
    non_test_idl_paths = collect_blink_idl_paths()
    # For bindings test IDL files, we collect interfaces info for each
    # component so that we can generate union type containers separately.
    test_idl_paths = {}
    for component in COMPONENT_DIRECTORY:
        test_idl_paths[component] = idl_paths_recursive(
            os.path.join(TEST_INPUT_DIRECTORY, component))
    # 2nd-stage computation: individual, then overall
    #
    # Properly should compute separately by component (currently test
    # includes are invalid), but that's brittle (would need to update this file
    # for each new component) and doesn't test the code generator any better
    # than using a single component.
    non_test_interfaces_info, non_test_component_info = collect_interfaces_info(non_test_idl_paths)
    test_interfaces_info = {}
    test_component_info = {}
    for component, paths in test_idl_paths.iteritems():
        test_interfaces_info[component], test_component_info[component] = collect_interfaces_info(paths)
    # In order to allow test IDL files to override the production IDL files if
    # they have the same interface name, process the test IDL files after the
    # non-test IDL files.
    info_individuals = [non_test_interfaces_info] + test_interfaces_info.values()
    compute_interfaces_info_overall(info_individuals)
    # Add typedefs which are specified in the actual IDL files to the testing
    # component info.
    test_component_info['core']['typedefs'].update(
        non_test_component_info['typedefs'])
    component_info_providers['core'] = ComponentInfoProviderCore(
        interfaces_info, test_component_info['core'])
    component_info_providers['modules'] = ComponentInfoProviderModules(
        interfaces_info, test_component_info['core'],
        test_component_info['modules'])


class IdlCompilerOptions(object):
    def __init__(self, output_directory, cache_directory, impl_output_directory,
                 target_component):
        self.output_directory = output_directory
        self.cache_directory = cache_directory
        self.impl_output_directory = impl_output_directory
        self.target_component = target_component


def bindings_tests(output_directory, verbose, suppress_diff):
    executive = Executive()

    def list_files(directory):
        if not os.path.isdir(directory):
            return []

        files = []
        for component in os.listdir(directory):
            if component not in COMPONENT_DIRECTORY:
                continue
            directory_with_component = os.path.join(directory, component)
            for filename in os.listdir(directory_with_component):
                files.append(os.path.join(directory_with_component, filename))
        return files

    def diff(filename1, filename2):
        with open(filename1) as file1:
            file1_lines = file1.readlines()
        with open(filename2) as file2:
            file2_lines = file2.readlines()

        # Use Python's difflib module so that diffing works across platforms
        return ''.join(difflib.context_diff(file1_lines, file2_lines))

    def is_cache_file(filename):
        return filename.endswith('.cache')

    def delete_cache_files():
        # FIXME: Instead of deleting cache files, don't generate them.
        cache_files = [path for path in list_files(output_directory)
                       if is_cache_file(os.path.basename(path))]
        for cache_file in cache_files:
            os.remove(cache_file)

    def identical_file(reference_filename, output_filename):
        reference_basename = os.path.basename(reference_filename)

        if not os.path.isfile(reference_filename):
            print 'Missing reference file!'
            print '(if adding new test, update reference files)'
            print reference_basename
            print
            return False

        if not filecmp.cmp(reference_filename, output_filename):
            # cmp is much faster than diff, and usual case is "no difference",
            # so only run diff if cmp detects a difference
            print 'FAIL: %s' % reference_basename
            if not suppress_diff:
                print diff(reference_filename, output_filename)
            return False

        if verbose:
            print 'PASS: %s' % reference_basename
        return True

    def identical_output_files(output_files):
        reference_files = [os.path.join(REFERENCE_DIRECTORY,
                                        os.path.relpath(path, output_directory))
                           for path in output_files]
        return all([identical_file(reference_filename, output_filename)
                    for (reference_filename, output_filename) in zip(reference_files, output_files)])

    def no_excess_files(output_files):
        generated_files = set([os.path.relpath(path, output_directory)
                               for path in output_files])
        excess_files = []
        for path in list_files(REFERENCE_DIRECTORY):
            relpath = os.path.relpath(path, REFERENCE_DIRECTORY)
            # Ignore backup files made by a VCS.
            if os.path.splitext(relpath)[1] == '.orig':
                continue
            if relpath not in generated_files:
                excess_files.append(relpath)
        if excess_files:
            print ('Excess reference files! '
                   '(probably cruft from renaming or deleting):\n' +
                   '\n'.join(excess_files))
            return False
        return True

    def make_runtime_features_dict():
        input_filename = os.path.join(TEST_INPUT_DIRECTORY, 'runtime_enabled_features.json5')
        json5_file = Json5File.load_from_files([input_filename])
        features_map = {}
        for feature in json5_file.name_dictionaries:
            features_map[str(feature['name'])] = {
                'in_origin_trial': feature['in_origin_trial']
            }
        return features_map

    try:
        generate_interface_dependencies(make_runtime_features_dict())
        for component in COMPONENT_DIRECTORY:
            output_dir = os.path.join(output_directory, component)
            if not os.path.exists(output_dir):
                os.makedirs(output_dir)

            options = IdlCompilerOptions(
                output_directory=output_dir,
                impl_output_directory=output_dir,
                cache_directory=None,
                target_component=component)

            if component == 'core':
                partial_interface_output_dir = os.path.join(output_directory,
                                                            'modules')
                if not os.path.exists(partial_interface_output_dir):
                    os.makedirs(partial_interface_output_dir)
                partial_interface_options = IdlCompilerOptions(
                    output_directory=partial_interface_output_dir,
                    impl_output_directory=None,
                    cache_directory=None,
                    target_component='modules')

            idl_filenames = []
            dictionary_impl_filenames = []
            partial_interface_filenames = []
            input_directory = os.path.join(TEST_INPUT_DIRECTORY, component)
            for filename in os.listdir(input_directory):
                if (filename.endswith('.idl') and
                        # Dependencies aren't built
                        # (they are used by the dependent)
                        filename not in DEPENDENCY_IDL_FILES):
                    idl_path = os.path.realpath(
                        os.path.join(input_directory, filename))
                    idl_filenames.append(idl_path)
                    idl_basename = os.path.basename(idl_path)
                    name_from_basename, _ = os.path.splitext(idl_basename)
                    definition_name = get_first_interface_name_from_idl(get_file_contents(idl_path))
                    is_partial_interface_idl = to_snake_case(definition_name) != name_from_basename
                    if not is_partial_interface_idl:
                        interface_info = interfaces_info[definition_name]
                        if interface_info['is_dictionary']:
                            dictionary_impl_filenames.append(idl_path)
                        if component == 'core' and interface_info[
                                'dependencies_other_component_full_paths']:
                            partial_interface_filenames.append(idl_path)

            info_provider = component_info_providers[component]
            partial_interface_info_provider = component_info_providers['modules']

            generate_union_type_containers(CodeGeneratorUnionType,
                                           info_provider, options)
            generate_callback_function_impl(CodeGeneratorCallbackFunction,
                                            info_provider, options)
            generate_bindings(
                CodeGeneratorV8,
                info_provider,
                options,
                idl_filenames)
            generate_bindings(
                CodeGeneratorV8,
                partial_interface_info_provider,
                partial_interface_options,
                partial_interface_filenames)
            generate_dictionary_impl(
                CodeGeneratorDictionaryImpl,
                info_provider,
                options,
                dictionary_impl_filenames)
            generate_origin_trial_features(
                info_provider,
                options,
                [filename for filename in idl_filenames
                 if filename not in dictionary_impl_filenames])

    finally:
        delete_cache_files()

    # Detect all changes
    output_files = list_files(output_directory)
    passed = identical_output_files(output_files)
    passed &= no_excess_files(output_files)

    if passed:
        if verbose:
            print
            print PASS_MESSAGE
        return 0
    print
    print FAIL_MESSAGE
    return 1


def run_bindings_tests(reset_results, verbose, suppress_diff):
    # Generate output into the reference directory if resetting results, or
    # a temp directory if not.
    if reset_results:
        print 'Resetting results'
        return bindings_tests(REFERENCE_DIRECTORY, verbose, suppress_diff)
    with TemporaryDirectory() as temp_dir:
        # TODO(peria): Remove this hack.
        # Some internal algorithms depend on the path of output directory.
        temp_source_path = os.path.join(temp_dir, 'third_party', 'blink', 'renderer')
        temp_output_path = os.path.join(temp_source_path, 'bindings', 'tests', 'results')
        return bindings_tests(temp_output_path, verbose, suppress_diff)
