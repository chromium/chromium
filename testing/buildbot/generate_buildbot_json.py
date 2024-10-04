#!/usr/bin/env vpython3
# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script to generate the majority of the JSON files in the src/testing/buildbot
directory. Maintaining these files by hand is too unwieldy.
"""

import argparse
import ast
import collections
import copy
import difflib
import functools
import glob
import itertools
import json
import os
import string
import sys

import buildbot_json_magic_substitutions as magic_substitutions

# pylint: disable=super-with-arguments,useless-super-delegation

THIS_DIR = os.path.dirname(os.path.abspath(__file__))

BROWSER_CONFIG_TO_TARGET_SUFFIX_MAP = {
    'android-chromium': '_android_chrome',
    'android-chromium-monochrome': '_android_monochrome',
    'android-webview': '_android_webview',
}


class BBGenErr(Exception):
  def __init__(self, message):
    super(BBGenErr, self).__init__(message)


class BaseGenerator(object):  # pylint: disable=useless-object-inheritance
  def __init__(self, bb_gen):
    self.bb_gen = bb_gen

  def generate(self, waterfall, tester_name, tester_config, input_tests):
    raise NotImplementedError()  # pragma: no cover


class GPUTelemetryTestGenerator(BaseGenerator):
  def __init__(self,
               bb_gen,
               is_android_webview=False,
               is_cast_streaming=False,
               is_skylab=False):
    super(GPUTelemetryTestGenerator, self).__init__(bb_gen)
    self._is_android_webview = is_android_webview
    self._is_cast_streaming = is_cast_streaming
    self._is_skylab = is_skylab

  def generate(self, waterfall, tester_name, tester_config, input_tests):
    isolated_scripts = []
    for test_name, test_config in sorted(input_tests.items()):
      # Variants allow more than one definition for a given test, and is defined
      # in array format from resolve_variants().
      if not isinstance(test_config, list):
        test_config = [test_config]

      for config in test_config:
        test = self.bb_gen.generate_gpu_telemetry_test(
            waterfall, tester_name, tester_config, test_name, config,
            self._is_android_webview, self._is_cast_streaming, self._is_skylab)
        if test:
          isolated_scripts.append(test)

    return isolated_scripts


class SkylabGPUTelemetryTestGenerator(GPUTelemetryTestGenerator):
  def __init__(self, bb_gen):
    super(SkylabGPUTelemetryTestGenerator, self).__init__(bb_gen,
                                                          is_skylab=True)

  def generate(self, *args, **kwargs):
    # This should be identical to a regular GPU Telemetry test, but with any
    # swarming arguments removed.
    isolated_scripts = super(SkylabGPUTelemetryTestGenerator,
                             self).generate(*args, **kwargs)
    for test in isolated_scripts:
      # chromium_GPU is the Autotest wrapper created for browser GPU tests
      # run in Skylab.
      test['autotest_name'] = 'chromium_Graphics'
      # As of 22Q4, Skylab tests are running on a CrOS flavored Autotest
      # framework and it does not support the sub-args like
      # extra-browser-args. So we have to pop it out and create a new
      # key for it. See crrev.com/c/3965359 for details.
      for idx, arg in enumerate(test.get('args', [])):
        if '--extra-browser-args' in arg:
          test['args'].pop(idx)
          test['extra_browser_args'] = arg.replace('--extra-browser-args=', '')
          break
    return isolated_scripts


class GTestGenerator(BaseGenerator):
  def generate(self, waterfall, tester_name, tester_config, input_tests):
    # The relative ordering of some of the tests is important to
    # minimize differences compared to the handwritten JSON files, since
    # Python's sorts are stable and there are some tests with the same
    # key (see gles2_conform_d3d9_test and similar variants). Avoid
    # losing the order by avoiding coalescing the dictionaries into one.
    gtests = []
    for test_name, test_config in sorted(input_tests.items()):
      # Variants allow more than one definition for a given test, and is defined
      # in array format from resolve_variants().
      if not isinstance(test_config, list):
        test_config = [test_config]

      for config in test_config:
        test = self.bb_gen.generate_gtest(
            waterfall, tester_name, tester_config, test_name, config)
        if test:
          # generate_gtest may veto the test generation on this tester.
          gtests.append(test)
    return gtests


class IsolatedScriptTestGenerator(BaseGenerator):
  def generate(self, waterfall, tester_name, tester_config, input_tests):
    isolated_scripts = []
    for test_name, test_config in sorted(input_tests.items()):
      # Variants allow more than one definition for a given test, and is defined
      # in array format from resolve_variants().
      if not isinstance(test_config, list):
        test_config = [test_config]

      for config in test_config:
        test = self.bb_gen.generate_isolated_script_test(
          waterfall, tester_name, tester_config, test_name, config)
        if test:
          isolated_scripts.append(test)
    return isolated_scripts


class ScriptGenerator(BaseGenerator):
  def generate(self, waterfall, tester_name, tester_config, input_tests):
    scripts = []
    for test_name, test_config in sorted(input_tests.items()):
      test = self.bb_gen.generate_script_test(
        waterfall, tester_name, tester_config, test_name, test_config)
      if test:
        scripts.append(test)
    return scripts


class SkylabGenerator(BaseGenerator):
  def generate(self, waterfall, tester_name, tester_config, input_tests):
    scripts = []
    for test_name, test_config in sorted(input_tests.items()):
      for config in test_config:
        test = self.bb_gen.generate_skylab_test(waterfall, tester_name,
                                                tester_config, test_name,
                                                config)
        if test:
          scripts.append(test)
    return scripts


def check_compound_references(other_test_suites=None,
                              sub_suite=None,
                              suite=None,
                              target_test_suites=None,
                              test_type=None,
                              **kwargs):
  """Ensure comound reference's don't target other compounds"""
  del kwargs
  if sub_suite in other_test_suites or sub_suite in target_test_suites:
    raise BBGenErr('%s may not refer to other composition type test '
                   'suites (error found while processing %s)' %
                   (test_type, suite))


def check_basic_references(basic_suites=None,
                           sub_suite=None,
                           suite=None,
                           **kwargs):
  """Ensure test has a basic suite reference"""
  del kwargs
  if sub_suite not in basic_suites:
    raise BBGenErr('Unable to find reference to %s while processing %s' %
                   (sub_suite, suite))


def check_conflicting_definitions(basic_suites=None,
                                  seen_tests=None,
                                  sub_suite=None,
                                  suite=None,
                                  test_type=None,
                                  target_test_suites=None,
                                  **kwargs):
  """Ensure that if a test is reachable via multiple basic suites,
  all of them have an identical definition of the tests.
  """
  del kwargs
  variants = None
  if test_type == 'matrix_compound_suites':
    variants = target_test_suites[suite][sub_suite].get('variants')
  variants = variants or [None]
  for test_name in basic_suites[sub_suite]:
    for variant in variants:
      key = (test_name, variant)
      if ((seen_sub_suite := seen_tests.get(key)) is not None
          and basic_suites[sub_suite][test_name] !=
          basic_suites[seen_sub_suite][test_name]):
        test_description = (test_name if variant is None else
                            f'{test_name} with variant {variant} applied')
        raise BBGenErr(
            'Conflicting test definitions for %s from %s '
            'and %s in %s (error found while processing %s)' %
            (test_description, seen_tests[key], sub_suite, test_type, suite))
      seen_tests[key] = sub_suite


def check_matrix_identifier(sub_suite=None,
                            suite=None,
                            suite_def=None,
                            all_variants=None,
                            **kwargs):
  """Ensure 'idenfitier' is defined for each variant"""
  del kwargs
  sub_suite_config = suite_def[sub_suite]
  for variant_name in sub_suite_config.get('variants', []):
    if variant_name not in all_variants:
      raise BBGenErr('Missing variant definition for %s in variants.pyl' %
                     variant_name)
    variant = all_variants[variant_name]

    if not 'identifier' in variant:
      raise BBGenErr('Missing required identifier field in matrix '
                     'compound suite %s, %s' % (suite, sub_suite))
    if variant['identifier'] == '':
      raise BBGenErr('Identifier field can not be "" in matrix '
                     'compound suite %s, %s' % (suite, sub_suite))
    if variant['identifier'].strip() != variant['identifier']:
      raise BBGenErr('Identifier field can not have leading and trailing '
                     'whitespace in matrix compound suite %s, %s' %
                     (suite, sub_suite))


class BBJSONGenerator(object):  # pylint: disable=useless-object-inheritance
  def __init__(self, args):
    self.args = args
    self.waterfalls = None
    self.test_suites = None
    self.exceptions = None
    self.mixins = None
    self.gn_isolate_map = None
    self.variants = None

  @staticmethod
  def parse_args(argv):

    # RawTextHelpFormatter allows for styling of help statement
    parser = argparse.ArgumentParser(
        formatter_class=argparse.RawTextHelpFormatter)

    group = parser.add_mutually_exclusive_group()
    group.add_argument(
        '-c',
        '--check',
        action='store_true',
        help=
        'Do consistency checks of configuration and generated files and then '
        'exit. Used during presubmit. '
        'Causes the tool to not generate any files.')
    group.add_argument(
        '--query',
        type=str,
        help=('Returns raw JSON information of buildbots and tests.\n'
              'Examples:\n  List all bots (all info):\n'
              '    --query bots\n\n'
              '  List all bots and only their associated tests:\n'
              '    --query bots/tests\n\n'
              '  List all information about "bot1" '
              '(make sure you have quotes):\n    --query bot/"bot1"\n\n'
              '  List tests running for "bot1" (make sure you have quotes):\n'
              '    --query bot/"bot1"/tests\n\n  List all tests:\n'
              '    --query tests\n\n'
              '  List all tests and the bots running them:\n'
              '    --query tests/bots\n\n'
              '  List all tests that satisfy multiple parameters\n'
              '  (separation of parameters by "&" symbol):\n'
              '    --query tests/"device_os:Android&device_type:hammerhead"\n\n'
              '  List all tests that run with a specific flag:\n'
              '    --query bots/"--test-launcher-print-test-studio=always"\n\n'
              '  List specific test (make sure you have quotes):\n'
              '    --query test/"test1"\n\n'
              '  List all bots running "test1" '
              '(make sure you have quotes):\n    --query test/"test1"/bots'))
    parser.add_argument(
        '--json',
        metavar='JSON_FILE_PATH',
        type=os.path.abspath,
        help='Outputs results into a json file. Only works with query function.'
    )
    parser.add_argument(
        '-n',
        '--new-files',
        action='store_true',
        help=
        'Write output files as .new.json. Useful during development so old and '
        'new files can be looked at side-by-side.')
    parser.add_argument('--dimension-sets-handling',
                        choices=['disable'],
                        default='disable',
                        help=('This flag no longer has any effect:'
                              ' dimension_sets fields are not allowed'))
    parser.add_argument('-v',
                        '--verbose',
                        action='store_true',
                        help='Increases verbosity. Affects consistency checks.')
    parser.add_argument('waterfall_filters',
                        metavar='waterfalls',
                        type=str,
                        nargs='*',
                        help='Optional list of waterfalls to generate.')
    parser.add_argument(
        '--pyl-files-dir',
        type=os.path.abspath,
        help=('Path to the directory containing the input .pyl files.'
              ' By default the directory containing this script will be used.'))
    parser.add_argument(
        '--output-dir',
        type=os.path.abspath,
        help=('Path to the directory to output generated .json files.'
              'By default, the pyl files directory will be used.'))
    parser.add_argument('--isolate-map-file',
                        metavar='PATH',
                        help='path to additional isolate map files.',
                        type=os.path.abspath,
                        default=[],
                        action='append',
                        dest='isolate_map_files')
    parser.add_argument(
        '--infra-config-dir',
        help='Path to the LUCI services configuration directory',
        type=os.path.abspath,
        default=os.path.join(os.path.dirname(__file__), '..', '..', 'infra',
                             'config'))

    args = parser.parse_args(argv)
    if args.json and not args.query:
      parser.error(
          'The --json flag can only be used with --query.')  # pragma: no cover

    args.pyl_files_dir = args.pyl_files_dir or THIS_DIR
    args.output_dir = args.output_dir or args.pyl_files_dir

    def pyl_dir_path(filename):
      return os.path.join(args.pyl_files_dir, filename)

    args.waterfalls_pyl_path = pyl_dir_path('waterfalls.pyl')
    args.test_suite_exceptions_pyl_path = pyl_dir_path(
        'test_suite_exceptions.pyl')
    args.autoshard_exceptions_json_path = os.path.join(
        args.infra_config_dir, 'targets', 'autoshard_exceptions.json')

    if args.pyl_files_dir == THIS_DIR:

      def infra_config_testing_path(filename):
        return os.path.join(args.infra_config_dir, 'generated', 'testing',
                            filename)

      args.gn_isolate_map_pyl_path = infra_config_testing_path(
          'gn_isolate_map.pyl')
      args.mixins_pyl_path = infra_config_testing_path('mixins.pyl')
      args.test_suites_pyl_path = infra_config_testing_path('test_suites.pyl')
      args.variants_pyl_path = infra_config_testing_path('variants.pyl')
    else:
      args.gn_isolate_map_pyl_path = pyl_dir_path('gn_isolate_map.pyl')
      args.mixins_pyl_path = pyl_dir_path('mixins.pyl')
      args.test_suites_pyl_path = pyl_dir_path('test_suites.pyl')
      args.variants_pyl_path = pyl_dir_path('variants.pyl')

    return args

  def print_line(self, line):
    # Exists so that tests can mock
    print(line)  # pragma: no cover

  def read_file(self, relative_path):
    with open(relative_path) as fp:
      return fp.read()

  def write_file(self, file_path, contents):
    with open(file_path, 'w', newline='') as fp:
      fp.write(contents)

  # pylint: disable=inconsistent-return-statements
  def load_pyl_file(self, pyl_file_path):
    try:
      return ast.literal_eval(self.read_file(pyl_file_path))
    except (SyntaxError, ValueError) as e: # pragma: no cover
      raise BBGenErr('Failed to parse pyl file "%s": %s' %
                     (pyl_file_path, e)) from e
    # pylint: enable=inconsistent-return-statements

  # TOOD(kbr): require that os_type be specified for all bots in waterfalls.pyl.
  # Currently it is only mandatory for bots which run GPU tests. Change these to
  # use [] instead of .get().
  def is_android(self, tester_config):
    return tester_config.get('os_type') == 'android'

  def is_chromeos(self, tester_config):
    return tester_config.get('os_type') == 'chromeos'

  def is_fuchsia(self, tester_config):
    return tester_config.get('os_type') == 'fuchsia'

  def is_lacros(self, tester_config):
    return tester_config.get('os_type') == 'lacros'

  def is_linux(self, tester_config):
    return tester_config.get('os_type') == 'linux'

  def is_mac(self, tester_config):
    return tester_config.get('os_type') == 'mac'

  def is_win(self, tester_config):
    return tester_config.get('os_type') == 'win'

  def is_win64(self, tester_config):
    return (tester_config.get('os_type') == 'win' and
        tester_config.get('browser_config') == 'release_x64')

  def get_exception_for_test(self, test_config):
    return self.exceptions.get(test_config['name'])

  def should_run_on_tester(self, waterfall, tester_name, test_config):
    # Currently, the only reason a test should not run on a given tester is that
    # it's in the exceptions. (Once the GPU waterfall generation script is
    # incorporated here, the rules will become more complex.)
    exception = self.get_exception_for_test(test_config)
    if not exception:
      return True
    remove_from = None
    remove_from = exception.get('remove_from')
    if remove_from:
      if tester_name in remove_from:
        return False
      # TODO(kbr): this code path was added for some tests (including
      # android_webview_unittests) on one machine (Nougat Phone
      # Tester) which exists with the same name on two waterfalls,
      # chromium.android and chromium.fyi; the tests are run on one
      # but not the other. Once the bots are all uniquely named (a
      # different ongoing project) this code should be removed.
      # TODO(kbr): add coverage.
      return (tester_name + ' ' + waterfall['name']
              not in remove_from) # pragma: no cover
    return True

  def get_test_modifications(self, test, tester_name):
    exception = self.get_exception_for_test(test)
    if not exception:
      return None
    return exception.get('modifications', {}).get(tester_name)

  def get_test_replacements(self, test, tester_name):
    exception = self.get_exception_for_test(test)
    if not exception:
      return None
    return exception.get('replacements', {}).get(tester_name)

  def merge_command_line_args(self, arr, prefix, splitter):
    prefix_len = len(prefix)
    idx = 0
    first_idx = -1
    accumulated_args = []
    while idx < len(arr):
      flag = arr[idx]
      delete_current_entry = False
      if flag.startswith(prefix):
        arg = flag[prefix_len:]
        accumulated_args.extend(arg.split(splitter))
        if first_idx < 0:
          first_idx = idx
        else:
          delete_current_entry = True
      if delete_current_entry:
        del arr[idx]
      else:
        idx += 1
    if first_idx >= 0:
      arr[first_idx] = prefix + splitter.join(accumulated_args)
    return arr

  def maybe_fixup_args_array(self, arr):
    # The incoming array of strings may be an array of command line
    # arguments. To make it easier to turn on certain features per-bot or
    # per-test-suite, look specifically for certain flags and merge them
    # appropriately.
    #   --enable-features=Feature1 --enable-features=Feature2
    # are merged to:
    #   --enable-features=Feature1,Feature2
    # and:
    #   --extra-browser-args=arg1 --extra-browser-args=arg2
    # are merged to:
    #   --extra-browser-args=arg1 arg2
    arr = self.merge_command_line_args(arr, '--enable-features=', ',')
    arr = self.merge_command_line_args(arr, '--extra-browser-args=', ' ')
    arr = self.merge_command_line_args(arr, '--test-launcher-filter-file=', ';')
    arr = self.merge_command_line_args(arr, '--extra-app-args=', ',')
    return arr

  def substitute_magic_args(self, test_config, tester_name, tester_config):
    """Substitutes any magic substitution args present in |test_config|.

    Substitutions are done in-place.

    See buildbot_json_magic_substitutions.py for more information on this
    feature.

    Args:
      test_config: A dict containing a configuration for a specific test on
          a specific builder.
      tester_name: A string containing the name of the tester that |test_config|
          came from.
      tester_config: A dict containing the configuration for the builder that
          |test_config| is for.
    """
    substituted_array = []
    original_args = test_config.get('args', [])
    for arg in original_args:
      if arg.startswith(magic_substitutions.MAGIC_SUBSTITUTION_PREFIX):
        function = arg.replace(
            magic_substitutions.MAGIC_SUBSTITUTION_PREFIX, '')
        if hasattr(magic_substitutions, function):
          substituted_array.extend(
              getattr(magic_substitutions, function)(test_config, tester_name,
                                                     tester_config))
        else:
          raise BBGenErr(
              'Magic substitution function %s does not exist' % function)
      else:
        substituted_array.append(arg)
    if substituted_array != original_args:
      test_config['args'] = self.maybe_fixup_args_array(substituted_array)

  @staticmethod
  def merge_swarming(swarming1, swarming2):
    swarming2 = dict(swarming2)
    if 'dimensions' in swarming2:
      swarming1.setdefault('dimensions', {}).update(swarming2.pop('dimensions'))
    if 'named_caches' in swarming2:
      named_caches = swarming1.setdefault('named_caches', [])
      named_caches.extend(swarming2.pop('named_caches'))
    swarming1.update(swarming2)

  def clean_swarming_dictionary(self, swarming_dict):
    # Clean out redundant entries from a test's "swarming" dictionary.
    # This is really only needed to retain 100% parity with the
    # handwritten JSON files, and can be removed once all the files are
    # autogenerated.
    if 'shards' in swarming_dict:
      if swarming_dict['shards'] == 1: # pragma: no cover
        del swarming_dict['shards'] # pragma: no cover
    if 'hard_timeout' in swarming_dict:
      if swarming_dict['hard_timeout'] == 0: # pragma: no cover
        del swarming_dict['hard_timeout'] # pragma: no cover
    del swarming_dict['can_use_on_swarming_builders']

  def resolve_os_conditional_values(self, test, builder):
    for key, fn in (
        ('android_swarming', self.is_android),
        ('chromeos_swarming', self.is_chromeos),
    ):
      swarming = test.pop(key, None)
      if swarming and fn(builder):
        self.merge_swarming(test['swarming'], swarming)

    for key, fn in (
        ('desktop_args', lambda cfg: not self.is_android(cfg)),
        ('lacros_args', self.is_lacros),
        ('linux_args', self.is_linux),
        ('android_args', self.is_android),
        ('chromeos_args', self.is_chromeos),
        ('mac_args', self.is_mac),
        ('win_args', self.is_win),
        ('win64_args', self.is_win64),
    ):
      args = test.pop(key, [])
      if fn(builder):
        test.setdefault('args', []).extend(args)

  def apply_common_transformations(self,
                                   waterfall,
                                   builder_name,
                                   builder,
                                   test,
                                   test_name,
                                   *,
                                   swarmable=True,
                                   supports_args=True):
    # Initialize the swarming dictionary
    swarmable = swarmable and builder.get('use_swarming', True)
    test.setdefault('swarming', {}).setdefault('can_use_on_swarming_builders',
                                               swarmable)

    # Test common mixins are mixins specified in the test declaration itself. To
    # match the order of expansion in starlark, they take effect before anything
    # specified in the legacy_test_config.
    test_common = test.pop('test_common', {})
    if test_common:
      test_common_mixins = test_common.pop('mixins', [])
      self.ensure_valid_mixin_list(test_common_mixins,
                                   f'test {test_name} test_common mixins')
      test_common = self.apply_mixins(test_common, test_common_mixins, [],
                                      builder)
      test = self.apply_mixin(test, test_common, builder)

    mixins_to_ignore = test.pop('remove_mixins', [])
    self.ensure_valid_mixin_list(mixins_to_ignore,
                                 f'test {test_name} remove_mixins')

    # Expand any conditional values
    self.resolve_os_conditional_values(test, builder)

    # Apply mixins from the test
    test_mixins = test.pop('mixins', [])
    self.ensure_valid_mixin_list(test_mixins, f'test {test_name} mixins')
    test = self.apply_mixins(test, test_mixins, mixins_to_ignore, builder)

    # Apply any variant details
    variant = test.pop('*variant*', None)
    if variant is not None:
      test = self.apply_mixin(variant, test)
      variant_mixins = test.pop('*variant_mixins*', [])
      self.ensure_valid_mixin_list(
          variant_mixins,
          (f'variant mixins for test {test_name}'
           f' with variant with identifier{test["variant_id"]}'))
      test = self.apply_mixins(test, variant_mixins, mixins_to_ignore, builder)

    # Add any swarming or args from the builder
    self.merge_swarming(test['swarming'], builder.get('swarming', {}))
    if supports_args:
      test.setdefault('args', []).extend(builder.get('args', []))

    # Apply mixins from the waterfall
    waterfall_mixins = waterfall.get('mixins', [])
    self.ensure_valid_mixin_list(waterfall_mixins,
                                 f"waterfall {waterfall['name']} mixins")
    test = self.apply_mixins(test, waterfall_mixins, mixins_to_ignore, builder)

    # Apply mixins from the builder
    builder_mixins = builder.get('mixins', [])
    self.ensure_valid_mixin_list(builder_mixins,
                                 f'builder {builder_name} mixins')
    test = self.apply_mixins(test, builder_mixins, mixins_to_ignore, builder)

    # See if there are any exceptions that need to be merged into this
    # test's specification.
    modifications = self.get_test_modifications(test, builder_name)
    if modifications:
      test = self.apply_mixin(modifications, test, builder)

    # Clean up the swarming entry or remove it if it's unnecessary
    if (swarming_dict := test.get('swarming')) is not None:
      if swarming_dict.get('can_use_on_swarming_builders'):
        self.clean_swarming_dictionary(swarming_dict)
      else:
        del test['swarming']

    # Ensure all Android Swarming tests run only on userdebug builds if another
    # build type was not specified.
    if 'swarming' in test and self.is_android(builder):
      dimensions = test.get('swarming', {}).get('dimensions', {})
      if (dimensions.get('os') == 'Android'
          and not dimensions.get('device_os_type')):
        dimensions['device_os_type'] = 'userdebug'

    # Apply any replacements specified for the test for the builder
    self.replace_test_args(test, test_name, builder_name)

    # Remove args if it is empty
    if 'args' in test:
      if not test['args']:
        del test['args']
      else:
        # Replace any magic arguments with their actual value
        self.substitute_magic_args(test, builder_name, builder)

        test['args'] = self.maybe_fixup_args_array(test['args'])

    return test

  def replace_test_args(self, test, test_name, tester_name):
    replacements = self.get_test_replacements(test, tester_name) or {}
    valid_replacement_keys = ['args', 'non_precommit_args', 'precommit_args']
    for key, replacement_dict in replacements.items():
      if key not in valid_replacement_keys:
        raise BBGenErr(
            'Given replacement key %s for %s on %s is not in the list of valid '
            'keys %s' % (key, test_name, tester_name, valid_replacement_keys))
      for replacement_key, replacement_val in replacement_dict.items():
        found_key = False
        for i, test_key in enumerate(test.get(key, [])):
          # Handle both the key/value being replaced being defined as two
          # separate items or as key=value.
          if test_key == replacement_key:
            found_key = True
            # Handle flags without values.
            if replacement_val is None:
              del test[key][i]
            else:
              test[key][i+1] = replacement_val
            break
          if test_key.startswith(replacement_key + '='):
            found_key = True
            if replacement_val is None:
              del test[key][i]
            else:
              test[key][i] = '%s=%s' % (replacement_key, replacement_val)
            break
        if not found_key:
          raise BBGenErr('Could not find %s in existing list of values for key '
                         '%s in %s on %s' % (replacement_key, key, test_name,
                             tester_name))

  def add_common_test_properties(self, test, tester_config):
    if self.is_chromeos(tester_config) and tester_config.get('use_swarming',
                                                               True):
      # The presence of the "device_type" dimension indicates that the tests
      # are targeting CrOS hardware and so need the special trigger script.
      if 'device_type' in test.get('swarming', {}).get('dimensions', {}):
        test['trigger_script'] = {
          'script': '//testing/trigger_scripts/chromeos_device_trigger.py',
        }

  def add_android_presentation_args(self, tester_config, result):
    bucket = tester_config.get('results_bucket', 'chromium-result-details')
    result.setdefault('args', []).append('--gs-results-bucket=%s' % bucket)

    if ('swarming' in result and 'merge' not in 'result'
        and not tester_config.get('skip_merge_script', False)):
      result['merge'] = {
          'args': [
              '--bucket',
              bucket,
              '--test-name',
              result['name'],
          ],
          'script': ('//build/android/pylib/results/presentation/'
                     'test_results_presentation.py'),
      }

  def generate_gtest(self, waterfall, tester_name, tester_config, test_name,
                     test_config):
    if not self.should_run_on_tester(waterfall, tester_name, test_config):
      return None
    result = copy.deepcopy(test_config)
    # Use test_name here instead of test['name'] because test['name'] will be
    # modified with the variant identifier in a matrix compound suite
    result.setdefault('test', test_name)

    result = self.apply_common_transformations(waterfall, tester_name,
                                               tester_config, result, test_name)
    if self.is_android(tester_config) and 'swarming' in result:
      if not result.get('use_isolated_scripts_api', False):
        # TODO(crbug.com/40725094) make Android presentation work with
        # isolated scripts in test_results_presentation.py merge script
        self.add_android_presentation_args(tester_config, result)
        result['args'] = result.get('args', []) + ['--recover-devices']
    self.add_common_test_properties(result, tester_config)

    if 'swarming' in result and not result.get('merge'):
      if test_config.get('use_isolated_scripts_api', False):
        merge_script = 'standard_isolated_script_merge'
      else:
        merge_script = 'standard_gtest_merge'

      result['merge'] = {
          'script': '//testing/merge_scripts/%s.py' % merge_script,
      }
    return result

  def generate_isolated_script_test(self, waterfall, tester_name, tester_config,
                                    test_name, test_config):
    if not self.should_run_on_tester(waterfall, tester_name, test_config):
      return None
    result = copy.deepcopy(test_config)
    # Use test_name here instead of test['name'] because test['name'] will be
    # modified with the variant identifier in a matrix compound suite
    result.setdefault('test', test_name)
    result = self.apply_common_transformations(waterfall, tester_name,
                                               tester_config, result, test_name)
    if self.is_android(tester_config) and 'swarming' in result:
      if tester_config.get('use_android_presentation', False):
        # TODO(crbug.com/40725094) make Android presentation work with
        # isolated scripts in test_results_presentation.py merge script
        self.add_android_presentation_args(tester_config, result)
    self.add_common_test_properties(result, tester_config)

    if 'swarming' in result and not result.get('merge'):
      # TODO(crbug.com/41456107): Consider adding the ability to not have
      # this default.
      result['merge'] = {
        'script': '//testing/merge_scripts/standard_isolated_script_merge.py',
      }
    return result

  _SCRIPT_FIELDS = ('name', 'script', 'args', 'precommit_args',
                    'non_precommit_args', 'resultdb')

  def generate_script_test(self, waterfall, tester_name, tester_config,
                           test_name, test_config):
    # TODO(crbug.com/40623237): Remove this check whenever a better
    # long-term solution is implemented.
    if (waterfall.get('forbid_script_tests', False) or
        waterfall['machines'][tester_name].get('forbid_script_tests', False)):
      raise BBGenErr('Attempted to generate a script test on tester ' +
                     tester_name + ', which explicitly forbids script tests')
    if not self.should_run_on_tester(waterfall, tester_name, test_config):
      return None
    result = copy.deepcopy(test_config)
    result = self.apply_common_transformations(waterfall,
                                               tester_name,
                                               tester_config,
                                               result,
                                               test_name,
                                               swarmable=False,
                                               supports_args=False)
    result = {k: result[k] for k in self._SCRIPT_FIELDS if k in result}
    return result

  def generate_skylab_test(self, waterfall, tester_name, tester_config,
                           test_name, test_config):
    if not self.should_run_on_tester(waterfall, tester_name, test_config):
      return None
    result = copy.deepcopy(test_config)
    result.setdefault('test', test_name)
    result['run_cft'] = True

    if 'cros_board' in result or 'cros_board' in tester_config:
      result['cros_board'] = tester_config.get('cros_board') or result.get(
          'cros_board')
    else:
      raise BBGenErr('skylab tests must specify cros_board.')
    if 'cros_model' in result or 'cros_model' in tester_config:
      result['cros_model'] = tester_config.get('cros_model') or result.get(
          'cros_model')
    if 'dut_pool' in result or 'cros_dut_pool' in tester_config:
      result['dut_pool'] = tester_config.get('cros_dut_pool') or result.get(
          'dut_pool')
    if 'cros_build_target' in result or 'cros_build_target' in tester_config:
      result['cros_build_target'] = tester_config.get(
          'cros_build_target') or result.get('cros_build_target')

    # Skylab tests enable the shard-level-retry by default.
    if ('shard_level_retries_on_ctp' in result
        or 'shard_level_retries_on_ctp' in tester_config):
      result['shard_level_retries_on_ctp'] = (
          tester_config.get('shard_level_retries_on_ctp')
          or result.get('shard_level_retries_on_ctp'))
    elif result.get('experiment_percentage') != 100:
      result['shard_level_retries_on_ctp'] = 1

    result = self.apply_common_transformations(waterfall,
                                               tester_name,
                                               tester_config,
                                               result,
                                               test_name,
                                               swarmable=False)
    return result

  def substitute_gpu_args(self, tester_config, test, args):
    substitutions = {
      # Any machine in waterfalls.pyl which desires to run GPU tests
      # must provide the os_type key.
      'os_type': tester_config['os_type'],
      'gpu_vendor_id': '0',
      'gpu_device_id': '0',
    }
    dimensions = test.get('swarming', {}).get('dimensions', {})
    if 'gpu' in dimensions:
      # First remove the driver version, then split into vendor and device.
      gpu = dimensions['gpu']
      if gpu != 'none':
        gpu = gpu.split('-')[0].split(':')
        substitutions['gpu_vendor_id'] = gpu[0]
        substitutions['gpu_device_id'] = gpu[1]
    return [string.Template(arg).safe_substitute(substitutions) for arg in args]

  # LINT.IfChange(gpu_telemetry_test)

  def generate_gpu_telemetry_test(self, waterfall, tester_name, tester_config,
                                  test_name, test_config, is_android_webview,
                                  is_cast_streaming, is_skylab):
    # These are all just specializations of isolated script tests with
    # a bunch of boilerplate command line arguments added.

    # The step name must end in 'test' or 'tests' in order for the
    # results to automatically show up on the flakiness dashboard.
    # (At least, this was true some time ago.) Continue to use this
    # naming convention for the time being to minimize changes.
    #
    # test name is the name of the test without the variant ID added
    if not (test_name.endswith('test') or test_name.endswith('tests')):
      raise BBGenErr(
          f'telemetry test names must end with test or tests, got {test_name}')
    result = self.generate_isolated_script_test(waterfall, tester_name,
                                                tester_config, test_name,
                                                test_config)
    if not result:
      return None
    result['test'] = test_config.get('test') or self.get_default_isolate_name(
        tester_config, is_android_webview)

    # Populate test_id_prefix.
    gn_entry = self.gn_isolate_map[result['test']]
    result['test_id_prefix'] = 'ninja:%s/' % gn_entry['label']

    args = result.get('args', [])
    # Use test_name here instead of test['name'] because test['name'] will be
    # modified with the variant identifier in a matrix compound suite
    test_to_run = result.pop('telemetry_test_name', test_name)

    # These tests upload and download results from cloud storage and therefore
    # aren't idempotent yet. https://crbug.com/549140.
    if 'swarming' in result:
      result['swarming']['idempotent'] = False

    browser = ''
    if is_cast_streaming:
      browser = 'cast-streaming-shell'
    elif is_android_webview:
      browser = 'android-webview-instrumentation'
    else:
      browser = tester_config['browser_config']

    extra_browser_args = []

    # Most platforms require --enable-logging=stderr to get useful browser logs.
    # However, this actively messes with logging on CrOS (because Chrome's
    # stderr goes nowhere on CrOS) AND --log-level=0 is required for some reason
    # in order to see JavaScript console messages. See
    # https://chromium.googlesource.com/chromium/src.git/+/HEAD/docs/chrome_os_logging.md
    if self.is_chromeos(tester_config):
      extra_browser_args.append('--log-level=0')
    elif not self.is_fuchsia(tester_config) or browser != 'fuchsia-chrome':
      # Stderr logging is not needed for Chrome browser on Fuchsia, as ordinary
      # logging via syslog is captured.
      extra_browser_args.append('--enable-logging=stderr')

    # --expose-gc allows the WebGL conformance tests to more reliably
    # reproduce GC-related bugs in the V8 bindings.
    extra_browser_args.append('--js-flags=--expose-gc')

    # Skylab supports sharding, so reuse swarming's shard config.
    if is_skylab and 'shards' not in result and test_config.get(
        'swarming', {}).get('shards'):
      result['shards'] = test_config['swarming']['shards']

    args = [
        test_to_run,
        '--show-stdout',
        '--browser=%s' % browser,
        # --passthrough displays more of the logging in Telemetry when
        # run via typ, in particular some of the warnings about tests
        # being expected to fail, but passing.
        '--passthrough',
        '-v',
        '--stable-jobs',
        '--extra-browser-args=%s' % ' '.join(extra_browser_args),
        '--enforce-browser-version',
    ] + args
    result['args'] = self.maybe_fixup_args_array(
        self.substitute_gpu_args(tester_config, result, args))
    return result

  # pylint: disable=line-too-long
  # LINT.ThenChange(//infra/config/lib/targets-internal/test-types/gpu_telemetry_test.star)
  # pylint: enable=line-too-long

  def get_default_isolate_name(self, tester_config, is_android_webview):
    if self.is_android(tester_config):
      if is_android_webview:
        return 'telemetry_gpu_integration_test_android_webview'
      return (
          'telemetry_gpu_integration_test' +
          BROWSER_CONFIG_TO_TARGET_SUFFIX_MAP[tester_config['browser_config']])
    if self.is_fuchsia(tester_config):
      return 'telemetry_gpu_integration_test_fuchsia'
    return 'telemetry_gpu_integration_test'

  def get_test_generator_map(self):
    return {
        'android_webview_gpu_telemetry_tests':
        GPUTelemetryTestGenerator(self, is_android_webview=True),
        'cast_streaming_tests':
        GPUTelemetryTestGenerator(self, is_cast_streaming=True),
        'gpu_telemetry_tests':
        GPUTelemetryTestGenerator(self),
        'gtest_tests':
        GTestGenerator(self),
        'isolated_scripts':
        IsolatedScriptTestGenerator(self),
        'scripts':
        ScriptGenerator(self),
        'skylab_tests':
        SkylabGenerator(self),
        'skylab_gpu_telemetry_tests':
        SkylabGPUTelemetryTestGenerator(self),
    }

  def get_test_type_remapper(self):
    return {
        # These are a specialization of isolated_scripts with a bunch of
        # boilerplate command line arguments added to each one.
        'android_webview_gpu_telemetry_tests': 'isolated_scripts',
        'cast_streaming_tests': 'isolated_scripts',
        'gpu_telemetry_tests': 'isolated_scripts',
        # These are the same as existing test types, just configured to run
        # in Skylab instead of via normal swarming.
        'skylab_gpu_telemetry_tests': 'skylab_tests',
    }

  def check_composition_type_test_suites(self, test_type,
                                         additional_validators=None):
    """Pre-pass to catch errors reliabily for compound/matrix suites"""
    validators = [check_compound_references,
                  check_basic_references,
                  check_conflicting_definitions]
    if additional_validators:
      validators += additional_validators

    target_suites = self.test_suites.get(test_type, {})
    other_test_type = ('compound_suites'
                       if test_type == 'matrix_compound_suites'
                       else 'matrix_compound_suites')
    other_suites = self.test_suites.get(other_test_type, {})
    basic_suites = self.test_suites.get('basic_suites', {})

    for suite, suite_def in target_suites.items():
      if suite in basic_suites:
        raise BBGenErr('%s names may not duplicate basic test suite names '
                       '(error found while processsing %s)'
                       % (test_type, suite))

      seen_tests = {}
      for sub_suite in suite_def:
        for validator in validators:
          validator(
                    basic_suites=basic_suites,
                    other_test_suites=other_suites,
                    seen_tests=seen_tests,
                    sub_suite=sub_suite,
                    suite=suite,
                    suite_def=suite_def,
                    target_test_suites=target_suites,
                    test_type=test_type,
                    all_variants=self.variants
                    )

  def flatten_test_suites(self):
    new_test_suites = {}
    test_types = ['basic_suites', 'compound_suites', 'matrix_compound_suites']
    for category in test_types:
      for name, value in self.test_suites.get(category, {}).items():
        new_test_suites[name] = value
    self.test_suites = new_test_suites

  def resolve_test_id_prefixes(self):
    for suite in self.test_suites['basic_suites'].values():
      for key, test in suite.items():
        assert isinstance(test, dict)

        isolate_name = test.get('test') or key
        gn_entry = self.gn_isolate_map.get(isolate_name)
        if gn_entry:
          label = gn_entry['label']

          if label.count(':') != 1:
            raise BBGenErr(
              'Malformed GN label "%s" in gn_isolate_map for key "%s",'
              ' implicit names (like //f/b meaning //f/b:b) are disallowed.' %
              (label, isolate_name))
          if label.split(':')[1] != isolate_name:
            raise BBGenErr(
              'gn_isolate_map key name "%s" doesn\'t match GN target name in'
              ' label "%s" see http://crbug.com/1071091 for details.' %
              (isolate_name, label))

          test['test_id_prefix'] = 'ninja:%s/' % label
        else:  # pragma: no cover
          # Some tests do not have an entry gn_isolate_map.pyl, such as
          # telemetry tests.
          # TODO(crbug.com/40112160): require an entry in gn_isolate_map.
          pass

  def resolve_composition_test_suites(self):
    self.check_composition_type_test_suites('compound_suites')

    compound_suites = self.test_suites.get('compound_suites', {})
    # check_composition_type_test_suites() checks that all basic suites
    # referenced by compound suites exist.
    basic_suites = self.test_suites.get('basic_suites')

    for name, value in compound_suites.items():
      # Resolve this to a dictionary.
      full_suite = {}
      for entry in value:
        suite = basic_suites[entry]
        full_suite.update(suite)
      compound_suites[name] = full_suite

  def resolve_variants(self, basic_test_definition, variants, mixins):
    """ Merge variant-defined configurations to each test case definition in a
    test suite.

    The output maps a unique test name to an array of configurations because
    there may exist more than one definition for a test name using variants. The
    test name is referenced while mapping machines to test suites, so unpacking
    the array is done by the generators.

    Args:
      basic_test_definition: a {} defined test suite in the format
        test_name:test_config
      variants: an [] of {} defining configurations to be applied to each test
        case in the basic test_definition

    Return:
      a {} of test_name:[{}], where each {} is a merged configuration
    """

    # Each test in a basic test suite will have a definition per variant.
    test_suite = {}
    for variant in variants:
      # Unpack the variant from variants.pyl if it's string based.
      if isinstance(variant, str):
        variant = self.variants[variant]

      # If 'enabled' is set to False, we will not use this variant; otherwise if
      # the variant doesn't include 'enabled' variable or 'enabled' is set to
      # True, we will use this variant
      if not variant.get('enabled', True):
        continue

      # Make a shallow copy of the variant to remove variant-specific fields,
      # leaving just mixin fields
      variant = copy.copy(variant)
      variant.pop('enabled', None)
      identifier = variant.pop('identifier')
      variant_mixins = variant.pop('mixins', [])
      variant_skylab = variant.pop('skylab', {})

      for test_name, test_config in basic_test_definition.items():
        new_test = copy.copy(test_config)

        # The identifier is used to make the name of the test unique.
        # Generators in the recipe uniquely identify a test by it's name, so we
        # don't want to have the same name for each variant.
        new_test['name'] = f'{test_name} {identifier}'

        # Attach the variant identifier to the test config so downstream
        # generators can make modifications based on the original name. This
        # is mainly used in generate_gpu_telemetry_test().
        new_test['variant_id'] = identifier

        # Save the variant details and mixins to be applied in
        # apply_common_transformations to match the order that starlark will
        # apply things
        new_test['*variant*'] = variant
        new_test['*variant_mixins*'] = variant_mixins + mixins

        # TODO: crbug.com/40258588 - When skylab support is implemented in
        # starlark, these fields should be incorporated into mixins and handled
        # consistently with other fields
        for k, v in variant_skylab.items():
          # cros_chrome_version is the ash chrome version in the cros img in the
          # variant of cros_board. We don't want to include it in the final json
          # files; so remove it.
          if k != 'cros_chrome_version':
            new_test[k] = v

        # For skylab, we need to pop the correct `autotest_name`. This field
        # defines what wrapper we use in OS infra. e.g. for gtest it's
        # https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/third_party/autotest/files/server/site_tests/chromium/chromium.py
        if variant_skylab and 'autotest_name' not in new_test:
          if 'tast_expr' in test_config:
            if 'lacros' in test_config['name']:
              new_test['autotest_name'] = 'tast.lacros-from-gcs'
            else:
              new_test['autotest_name'] = 'tast.chrome-from-gcs'
          elif 'benchmark' in test_config:
            new_test['autotest_name'] = 'chromium_Telemetry'
          else:
            new_test['autotest_name'] = 'chromium'

        test_suite.setdefault(test_name, []).append(new_test)

    return test_suite

  def resolve_matrix_compound_test_suites(self):
    self.check_composition_type_test_suites('matrix_compound_suites',
                                            [check_matrix_identifier])

    matrix_compound_suites = self.test_suites.get('matrix_compound_suites', {})
    # check_composition_type_test_suites() checks that all basic suites are
    # referenced by matrix suites exist.
    basic_suites = self.test_suites.get('basic_suites')

    def update_tests_uncurried(full_suite, expanded):
      for test_name, new_tests in expanded.items():
        if not isinstance(new_tests, list):
          new_tests = [new_tests]
        tests_for_name = full_suite.setdefault(test_name, [])
        for t in new_tests:
          if t not in tests_for_name:
            tests_for_name.append(t)

    for matrix_suite_name, matrix_config in matrix_compound_suites.items():
      full_suite = {}

      for test_suite, mtx_test_suite_config in matrix_config.items():
        basic_test_def = copy.deepcopy(basic_suites[test_suite])

        update_tests = functools.partial(update_tests_uncurried, full_suite)

        mixins = mtx_test_suite_config.get('mixins', [])
        if (variants := mtx_test_suite_config.get('variants')):
          result = self.resolve_variants(basic_test_def, variants, mixins)
          update_tests(result)
        else:
          suite = copy.deepcopy(basic_suites[test_suite])
          for test_config in suite.values():
            test_config['mixins'] = test_config.get('mixins', []) + mixins
          update_tests(suite)
      matrix_compound_suites[matrix_suite_name] = full_suite

  def link_waterfalls_to_test_suites(self):
    for waterfall in self.waterfalls:
      for tester_name, tester in waterfall['machines'].items():
        for suite, value in tester.get('test_suites', {}).items():
          if not value in self.test_suites:
            # Hard / impossible to cover this in the unit test.
            raise self.unknown_test_suite(
              value, tester_name, waterfall['name']) # pragma: no cover
          tester['test_suites'][suite] = self.test_suites[value]

  def load_configuration_files(self):
    self.waterfalls = self.load_pyl_file(self.args.waterfalls_pyl_path)
    self.test_suites = self.load_pyl_file(self.args.test_suites_pyl_path)
    self.exceptions = self.load_pyl_file(
        self.args.test_suite_exceptions_pyl_path)
    self.mixins = self.load_pyl_file(self.args.mixins_pyl_path)
    self.gn_isolate_map = self.load_pyl_file(self.args.gn_isolate_map_pyl_path)
    for isolate_map in self.args.isolate_map_files:
      isolate_map = self.load_pyl_file(isolate_map)
      duplicates = set(isolate_map).intersection(self.gn_isolate_map)
      if duplicates:
        raise BBGenErr('Duplicate targets in isolate map files: %s.' %
                       ', '.join(duplicates))
      self.gn_isolate_map.update(isolate_map)

    self.variants = self.load_pyl_file(self.args.variants_pyl_path)

  def resolve_configuration_files(self):
    self.resolve_mixins()
    self.resolve_test_names()
    self.resolve_isolate_names()
    self.resolve_dimension_sets()
    self.resolve_test_id_prefixes()
    self.resolve_composition_test_suites()
    self.resolve_matrix_compound_test_suites()
    self.flatten_test_suites()
    self.link_waterfalls_to_test_suites()

  def resolve_mixins(self):
    for mixin in self.mixins.values():
      mixin.pop('fail_if_unused', None)

  def resolve_test_names(self):
    for suite_name, suite in self.test_suites.get('basic_suites').items():
      for test_name, test in suite.items():
        if 'name' in test:
          raise BBGenErr(
              f'The name field is set in test {test_name} in basic suite '
              f'{suite_name}, this is not supported, the test name is the key '
              'within the basic suite')
        # When a test is expanded with variants, this will be overwritten, but
        # this ensures every test definition has the name field set
        test['name'] = test_name

  def resolve_isolate_names(self):
    for suite_name, suite in self.test_suites.get('basic_suites').items():
      for test_name, test in suite.items():
        if 'isolate_name' in test:
          raise BBGenErr(
              f'The isolate_name field is set in test {test_name} in basic '
              f'suite {suite_name}, the test field should be used instead')

  def resolve_dimension_sets(self):

    def definitions():
      for suite_name, suite in self.test_suites.get('basic_suites', {}).items():
        for test_name, test in suite.items():
          yield test, f'test {test_name} in basic suite {suite_name}'

      for mixin_name, mixin in self.mixins.items():
        yield mixin, f'mixin {mixin_name}'

      for waterfall in self.waterfalls:
        for builder_name, builder in waterfall.get('machines', {}).items():
          yield (
              builder,
              f'builder {builder_name} in waterfall {waterfall["name"]}',
          )

      for test_name, exceptions in self.exceptions.items():
        modifications = exceptions.get('modifications', {})
        for builder_name, mods in modifications.items():
          yield (
              mods,
              f'exception for test {test_name} on builder {builder_name}',
          )

    for definition, location in definitions():
      for swarming_attr in (
          'swarming',
          'android_swarming',
          'chromeos_swarming',
      ):
        if (swarming :=
            definition.get(swarming_attr)) and 'dimension_sets' in swarming:
          raise BBGenErr(
              f'dimension_sets is no longer supported (set in {location}),'
              ' instead, use set dimensions to a single dict')

  def unknown_bot(self, bot_name, waterfall_name):
    return BBGenErr(
      'Unknown bot name "%s" on waterfall "%s"' % (bot_name, waterfall_name))

  def unknown_test_suite(self, suite_name, bot_name, waterfall_name):
    return BBGenErr(
      'Test suite %s from machine %s on waterfall %s not present in '
      'test_suites.pyl' % (suite_name, bot_name, waterfall_name))

  def unknown_test_suite_type(self, suite_type, bot_name, waterfall_name):
    return BBGenErr(
      'Unknown test suite type ' + suite_type + ' in bot ' + bot_name +
      ' on waterfall ' + waterfall_name)

  def ensure_valid_mixin_list(self, mixins, location):
    if not isinstance(mixins, list):
      raise BBGenErr(
          f"got '{mixins}', should be a list of mixin names: {location}")
    for mixin in mixins:
      if not mixin in self.mixins:
        raise BBGenErr(f'bad mixin {mixin}: {location}')

  def apply_mixins(self, test, mixins, mixins_to_ignore, builder=None):
    for mixin in mixins:
      if mixin not in mixins_to_ignore:
        test = self.apply_mixin(self.mixins[mixin], test, builder)
    return test

  def apply_mixin(self, mixin, test, builder=None):
    """Applies a mixin to a test.

    A mixin is applied by copying all fields from the mixin into the
    test with the following exceptions:
    * For the various *args keys, the test's existing value (an empty
      list if not present) will be extended with the mixin's value.
    * The sub-keys of the swarming value will be copied to the test's
      swarming value with the following exceptions:
      * For the named_caches sub-keys, the test's existing value (an
        empty list if not present) will be extended with the mixin's
        value.
      * For the dimensions sub-key, the tests's existing value (an empty
        dict if not present) will be updated with the mixin's value.
    """

    new_test = copy.deepcopy(test)
    mixin = copy.deepcopy(mixin)

    if 'description' in mixin:
      description = []
      if 'description' in new_test:
        description.append(new_test['description'])
      description.append(mixin.pop('description'))
      new_test['description'] = '\n'.join(description)

    if 'swarming' in mixin:
      self.merge_swarming(new_test.setdefault('swarming', {}),
                          mixin.pop('swarming'))

    for a in ('args', 'precommit_args', 'non_precommit_args'):
      if (value := mixin.pop(a, None)) is None:
        continue
      if not isinstance(value, list):
        raise BBGenErr(f'"{a}" must be a list')
      new_test.setdefault(a, []).extend(value)

    # At this point, all keys that require merging are taken care of, so the
    # remaining entries can be copied over. The os-conditional entries will be
    # resolved immediately after and they are resolved before any mixins are
    # applied, so there's are no concerns about overwriting the corresponding
    # entry in the test.
    new_test.update(mixin)
    if builder:
      self.resolve_os_conditional_values(new_test, builder)

    if 'args' in new_test:
      new_test['args'] = self.maybe_fixup_args_array(new_test['args'])

    return new_test

  def generate_output_tests(self, waterfall):
    """Generates the tests for a waterfall.

    Args:
      waterfall: a dictionary parsed from a master pyl file
    Returns:
      A dictionary mapping builders to test specs
      """
    return {
        name: self.get_tests_for_config(waterfall, name, config)
        for name, config in waterfall['machines'].items()
    }

  def get_tests_for_config(self, waterfall, name, config):
    generator_map = self.get_test_generator_map()
    test_type_remapper = self.get_test_type_remapper()

    tests = {}
    # Copy only well-understood entries in the machine's configuration
    # verbatim into the generated JSON.
    if 'additional_compile_targets' in config:
      tests['additional_compile_targets'] = config[
        'additional_compile_targets']
    for test_type, input_tests in config.get('test_suites', {}).items():
      if test_type not in generator_map:
        raise self.unknown_test_suite_type(
          test_type, name, waterfall['name']) # pragma: no cover
      test_generator = generator_map[test_type]
      # Let multiple kinds of generators generate the same kinds
      # of tests. For example, gpu_telemetry_tests are a
      # specialization of isolated_scripts.
      new_tests = test_generator.generate(
        waterfall, name, config, input_tests)
      remapped_test_type = test_type_remapper.get(test_type, test_type)
      tests.setdefault(remapped_test_type, []).extend(new_tests)

    for test_type, tests_for_type in tests.items():
      if test_type == 'additional_compile_targets':
        continue
      tests[test_type] = sorted(tests_for_type, key=lambda t: t['name'])

    return tests

  def jsonify(self, all_tests):
    return json.dumps(
        all_tests, indent=2, separators=(',', ': '),
        sort_keys=True) + '\n'

  def generate_outputs(self): # pragma: no cover
    self.load_configuration_files()
    self.resolve_configuration_files()
    filters = self.args.waterfall_filters
    result = collections.defaultdict(dict)

    if os.path.exists(self.args.autoshard_exceptions_json_path):
      autoshards = json.loads(
          self.read_file(self.args.autoshard_exceptions_json_path))
    else:
      autoshards = {}

    required_fields = ('name',)
    for waterfall in self.waterfalls:
      for field in required_fields:
        # Verify required fields
        if field not in waterfall:
          raise BBGenErr('Waterfall %s has no %s' % (waterfall['name'], field))

      # Handle filter flag, if specified
      if filters and waterfall['name'] not in filters:
        continue

      # Join config files and hardcoded values together
      all_tests = self.generate_output_tests(waterfall)
      result[waterfall['name']] = all_tests

      if not autoshards:
        continue
      for builder, test_spec in all_tests.items():
        for target_type, test_list in test_spec.items():
          if target_type == 'additional_compile_targets':
            continue
          for test_dict in test_list:
            # Suites that apply variants or other customizations will create
            # test_dicts that have "name" value that is different from the
            # "test" value.
            # e.g. name = vulkan_swiftshader_content_browsertests, but
            # test = content_browsertests and
            # test_id_prefix = "ninja://content/test:content_browsertests/"
            test_name = test_dict['name']
            shard_info = autoshards.get(waterfall['name'],
                                        {}).get(builder, {}).get(test_name)
            if shard_info:
              test_dict['swarming'].update(
                  {'shards': int(shard_info['shards'])})

    # Add do not edit warning
    for tests in result.values():
      tests['AAAAA1 AUTOGENERATED FILE DO NOT EDIT'] = {}
      tests['AAAAA2 See generate_buildbot_json.py to make changes'] = {}

    return result

  def write_json_result(self, result): # pragma: no cover
    suffix = '.json'
    if self.args.new_files:
      suffix = '.new' + suffix

    for filename, contents in result.items():
      jsonstr = self.jsonify(contents)
      file_path = os.path.join(self.args.output_dir, filename + suffix)
      self.write_file(file_path, jsonstr)

  def get_valid_bot_names(self):
    # Extract bot names from infra/config/generated/luci/luci-milo.cfg.
    # NOTE: This reference can cause issues; if a file changes there, the
    # presubmit here won't be run by default. A manually maintained list there
    # tries to run presubmit here when luci-milo.cfg is changed. If any other
    # references to configs outside of this directory are added, please change
    # their presubmit to run `generate_buildbot_json.py -c`, so that the tree
    # never ends up in an invalid state.

    # Get the generated project.pyl so we can check if we should be enforcing
    # that the specs are for builders that actually exist
    # If not, return None to indicate that we won't enforce that builders in
    # waterfalls.pyl are defined in LUCI
    project_pyl_path = os.path.join(self.args.infra_config_dir, 'generated',
                                    'project.pyl')
    if os.path.exists(project_pyl_path):
      settings = ast.literal_eval(self.read_file(project_pyl_path))
      if not settings.get('validate_source_side_specs_have_builder', True):
        return None

    bot_names = set()
    milo_configs = glob.glob(
        os.path.join(self.args.infra_config_dir, 'generated', 'luci',
                     'luci-milo*.cfg'))
    for c in milo_configs:
      for l in self.read_file(c).splitlines():
        if (not 'name: "buildbucket/luci.chromium.' in l and
            not 'name: "buildbucket/luci.chrome.' in l):
          continue
        # l looks like
        # `name: "buildbucket/luci.chromium.try/win_chromium_dbg_ng"`
        # Extract win_chromium_dbg_ng part.
        bot_names.add(l[l.rindex('/') + 1:l.rindex('"')])
    return bot_names

  def get_internal_waterfalls(self):
    # Similar to get_builders_that_do_not_actually_exist above, but for
    # waterfalls defined in internal configs.
    return [
        'chrome', 'chrome.pgo', 'chrome.gpu.fyi', 'internal.chrome.fyi',
        'internal.chromeos.fyi', 'internal.optimization_guide', 'internal.soda',
        'chromeos.preuprev'
    ]

  def check_input_file_consistency(self, verbose=False):
    self.check_input_files_sorting(verbose)

    self.load_configuration_files()
    self.check_composition_type_test_suites('compound_suites')
    self.check_composition_type_test_suites('matrix_compound_suites',
                                            [check_matrix_identifier])
    self.resolve_test_id_prefixes()

    # All test suites must be referenced. Check this before flattening the test
    # suites so that we can transitively check the basic suites for compound
    # suites and matrix compound suites (otherwise we would determine a basic
    # suite is used if it shared a name with a test present in a basic suite
    # that is used).
    all_suites = set(
        itertools.chain(*(self.test_suites.get(a, {}) for a in (
            'basic_suites',
            'compound_suites',
            'matrix_compound_suites',
        ))))
    unused_suites = set(all_suites)
    generator_map = self.get_test_generator_map()
    for waterfall in self.waterfalls:
      for bot_name, tester in waterfall['machines'].items():
        for suite_type, suite in tester.get('test_suites', {}).items():
          if suite_type not in generator_map:
            raise self.unknown_test_suite_type(suite_type, bot_name,
                                               waterfall['name'])
          if suite not in all_suites:
            raise self.unknown_test_suite(suite, bot_name, waterfall['name'])
          unused_suites.discard(suite)
    # For each compound suite or matrix compound suite, if the suite was used,
    # remove all of the basic suites that it composes from the set of unused
    # suites
    for a in ('compound_suites', 'matrix_compound_suites'):
      for suite, sub_suites in self.test_suites.get(a, {}).items():
        if suite not in unused_suites:
          unused_suites.difference_update(sub_suites)
    if unused_suites:
      raise BBGenErr('The following test suites were unreferenced by bots on '
                     'the waterfalls: ' + str(unused_suites))

    self.flatten_test_suites()

    # All bots should exist.
    bot_names = self.get_valid_bot_names()
    if bot_names is not None:
      internal_waterfalls = self.get_internal_waterfalls()
      for waterfall in self.waterfalls:
        # TODO(crbug.com/41474799): Remove the need for this exception.
        if waterfall['name'] in internal_waterfalls:
          continue  # pragma: no cover
        for bot_name in waterfall['machines']:
          if bot_name not in bot_names:
            if waterfall['name'] in [
                'client.v8.chromium', 'client.v8.fyi', 'tryserver.v8'
            ]:
              # TODO(thakis): Remove this once these bots move to luci.
              continue  # pragma: no cover
            if waterfall['name'] in ['tryserver.webrtc',
                                     'webrtc.chromium.fyi.experimental']:
              # These waterfalls have their bot configs in a different repo.
              # so we don't know about their bot names.
              continue  # pragma: no cover
            if waterfall['name'] in ['client.devtools-frontend.integration',
                                     'tryserver.devtools-frontend',
                                     'chromium.devtools-frontend']:
              continue  # pragma: no cover
            if waterfall['name'] in ['client.openscreen.chromium']:
              continue  # pragma: no cover
            raise self.unknown_bot(bot_name, waterfall['name'])

    # All test suite exceptions must refer to bots on the waterfall.
    all_bots = set()
    missing_bots = set()
    for waterfall in self.waterfalls:
      for bot_name, tester in waterfall['machines'].items():
        all_bots.add(bot_name)
        # In order to disambiguate between bots with the same name on
        # different waterfalls, support has been added to various
        # exceptions for concatenating the waterfall name after the bot
        # name.
        all_bots.add(bot_name + ' ' + waterfall['name'])
    for exception in self.exceptions.values():
      removals = (exception.get('remove_from', []) +
                  exception.get('remove_gtest_from', []) +
                  list(exception.get('modifications', {}).keys()))
      for removal in removals:
        if removal not in all_bots:
          missing_bots.add(removal)

    if missing_bots:
      raise BBGenErr('The following nonexistent machines were referenced in '
                     'the test suite exceptions: ' + str(missing_bots))

    for name, mixin in self.mixins.items():
      if '$mixin_append' in mixin:
        raise BBGenErr(
            f'$mixin_append is no longer supported (set in mixin "{name}"),'
            ' args and named caches specified as normal will be appended')

    # All mixins must be referenced
    seen_mixins = set()
    for waterfall in self.waterfalls:
      seen_mixins = seen_mixins.union(waterfall.get('mixins', set()))
      for bot_name, tester in waterfall['machines'].items():
        seen_mixins = seen_mixins.union(tester.get('mixins', set()))
    for suite in self.test_suites.values():
      if isinstance(suite, list):
        # Don't care about this, it's a composition, which shouldn't include a
        # swarming mixin.
        continue

      for test in suite.values():
        assert isinstance(test, dict)
        seen_mixins = seen_mixins.union(test.get('mixins', set()))
        seen_mixins = seen_mixins.union(
            test.get('test_common', {}).get('mixins', set()))

    for variant in self.variants:
      # Unpack the variant from variants.pyl if it's string based.
      if isinstance(variant, str):
        variant = self.variants[variant]
      seen_mixins = seen_mixins.union(variant.get('mixins', set()))

    missing_mixins = set()
    for name, mixin_value in self.mixins.items():
      if name not in seen_mixins and mixin_value.get('fail_if_unused', True):
        missing_mixins.add(name)
    if missing_mixins:
      raise BBGenErr('The following mixins are unreferenced: %s. They must be'
                     ' referenced in a waterfall, machine, or test suite.' % (
                         str(missing_mixins)))

    # All variant references must be referenced
    seen_variants = set()
    for suite in self.test_suites.values():
      if isinstance(suite, list):
        continue

      for test in suite.values():
        if isinstance(test, dict):
          for variant in test.get('variants', []):
            if isinstance(variant, str):
              seen_variants.add(variant)

    missing_variants = set(self.variants.keys()) - seen_variants
    if missing_variants:
      raise BBGenErr('The following variants were unreferenced: %s. They must '
                     'be referenced in a matrix test suite under the variants '
                     'key.' % str(missing_variants))


  def type_assert(self, node, typ, file_path, verbose=False):
    """Asserts that the Python AST node |node| is of type |typ|.

    If verbose is set, it prints out some helpful context lines, showing where
    exactly the error occurred in the file.
    """
    if not isinstance(node, typ):
      if verbose:
        lines = [''] + self.read_file(file_path).splitlines()

        context = 2
        lines_start = max(node.lineno - context, 0)
        # Add one to include the last line
        lines_end = min(node.lineno + context, len(lines)) + 1
        lines = itertools.chain(
            ['== %s ==\n' % file_path],
            ['<snip>\n'],
            [
                '%d %s' % (lines_start + i, line)
                for i, line in enumerate(lines[lines_start:lines_start +
                                               context])
            ],
            ['-' * 80 + '\n'],
            ['%d %s' % (node.lineno, lines[node.lineno])],
            [
                '-' * (node.col_offset + 3) + '^' + '-' *
                (80 - node.col_offset - 4) + '\n'
            ],
            [
                '%d %s' % (node.lineno + 1 + i, line)
                for i, line in enumerate(lines[node.lineno + 1:lines_end])
            ],
            ['<snip>\n'],
        )
        # Print out a useful message when a type assertion fails.
        for l in lines:
          self.print_line(l.strip())

      node_dumped = ast.dump(node, annotate_fields=False)
      # If the node is huge, truncate it so everything fits in a terminal
      # window.
      if len(node_dumped) > 60: # pragma: no cover
        node_dumped = node_dumped[:30] + '  <SNIP>  ' + node_dumped[-30:]
      raise BBGenErr(
          "Invalid .pyl file '%s'. Python AST node %r on line %s expected to"
          ' be %s, is %s' %
          (file_path, node_dumped, node.lineno, typ, type(node)))

  def check_ast_list_formatted(self,
                               keys,
                               file_path,
                               verbose,
                               check_sorting=True):
    """Checks if a list of ast keys are correctly formatted.

    Currently only checks to ensure they're correctly sorted, and that there
    are no duplicates.

    Args:
      keys: An python list of AST nodes.

            It's a list of AST nodes instead of a list of strings because
            when verbose is set, it tries to print out context of where the
            diffs are in the file.
      file_path: The path to the file this node is from.
      verbose: If set, print out diff information about how the keys are
               incorrectly formatted.
      check_sorting: If true, checks if the list is sorted.
    Returns:
      If the keys are correctly formatted.
    """
    if not keys:
      return True

    assert isinstance(keys[0], ast.Str)

    keys_strs = [k.s for k in keys]
    # Keys to diff against. Used below.
    keys_to_diff_against = None
    # If the list is properly formatted.
    list_formatted = True

    # Duplicates are always bad.
    if len(set(keys_strs)) != len(keys_strs):
      list_formatted = False
      keys_to_diff_against = list(collections.OrderedDict.fromkeys(keys_strs))

    if check_sorting and sorted(keys_strs) != keys_strs:
      list_formatted = False
    if list_formatted:
      return True

    if verbose:
      line_num = keys[0].lineno
      keys = [k.s for k in keys]
      if check_sorting:
        # If we have duplicates, sorting this will take care of it anyways.
        keys_to_diff_against = sorted(set(keys))
      # else, keys_to_diff_against is set above already

      self.print_line('=' * 80)
      self.print_line('(First line of keys is %s)' % line_num)
      for line in difflib.context_diff(keys,
                                       keys_to_diff_against,
                                       fromfile='current (%r)' % file_path,
                                       tofile='sorted',
                                       lineterm=''):
        self.print_line(line)
      self.print_line('=' * 80)

    return False

  def check_ast_dict_formatted(self, node, file_path, verbose):
    """Checks if an ast dictionary's keys are correctly formatted.

    Just a simple wrapper around check_ast_list_formatted.
    Args:
      node: An AST node. Assumed to be a dictionary.
      file_path: The path to the file this node is from.
      verbose: If set, print out diff information about how the keys are
               incorrectly formatted.
      check_sorting: If true, checks if the list is sorted.
    Returns:
      If the dictionary is correctly formatted.
    """
    keys = []
    # The keys of this dict are ordered as ordered in the file; normal python
    # dictionary keys are given an arbitrary order, but since we parsed the
    # file itself, the order as given in the file is preserved.
    for key in node.keys:
      self.type_assert(key, ast.Str, file_path, verbose)
      keys.append(key)

    return self.check_ast_list_formatted(keys, file_path, verbose)

  def check_input_files_sorting(self, verbose=False):
    # TODO(crbug.com/41415841): Add the ability for this script to
    # actually format the files, rather than just complain if they're
    # incorrectly formatted.
    bad_files = set()

    def parse_file(file_path):
      """Parses and validates a .pyl file.

      Returns an AST node representing the value in the pyl file."""
      parsed = ast.parse(self.read_file(file_path))

      # Must be a module.
      self.type_assert(parsed, ast.Module, file_path, verbose)
      module = parsed.body

      # Only one expression in the module.
      self.type_assert(module, list, file_path, verbose)
      if len(module) != 1: # pragma: no cover
        raise BBGenErr('Invalid .pyl file %s' % file_path)
      expr = module[0]
      self.type_assert(expr, ast.Expr, file_path, verbose)

      return expr.value

    # Handle this separately
    value = parse_file(self.args.waterfalls_pyl_path)
    # Value should be a list.
    self.type_assert(value, ast.List, self.args.waterfalls_pyl_path, verbose)

    keys = []
    for elm in value.elts:
      self.type_assert(elm, ast.Dict, self.args.waterfalls_pyl_path, verbose)
      waterfall_name = None
      for key, val in zip(elm.keys, elm.values):
        self.type_assert(key, ast.Str, self.args.waterfalls_pyl_path, verbose)
        if key.s == 'machines':
          if not self.check_ast_dict_formatted(
              val, self.args.waterfalls_pyl_path, verbose):
            bad_files.add(self.args.waterfalls_pyl_path)

        if key.s == 'name':
          self.type_assert(val, ast.Str, self.args.waterfalls_pyl_path, verbose)
          waterfall_name = val
      assert waterfall_name
      keys.append(waterfall_name)

    if not self.check_ast_list_formatted(keys, self.args.waterfalls_pyl_path,
                                         verbose):
      bad_files.add(self.args.waterfalls_pyl_path)

    for file_path in (
        self.args.mixins_pyl_path,
        self.args.test_suites_pyl_path,
        self.args.test_suite_exceptions_pyl_path,
    ):
      value = parse_file(file_path)
      # Value should be a dictionary.
      self.type_assert(value, ast.Dict, file_path, verbose)

      if not self.check_ast_dict_formatted(value, file_path, verbose):
        bad_files.add(file_path)

      if file_path == self.args.test_suites_pyl_path:
        expected_keys = ['basic_suites',
                         'compound_suites',
                         'matrix_compound_suites']
        actual_keys = [node.s for node in value.keys]
        assert all(key in expected_keys for key in actual_keys), (
            'Invalid %r file; expected keys %r, got %r' %
            (file_path, expected_keys, actual_keys))
        suite_dicts = list(value.values)
        # Only two keys should mean only 1 or 2 values
        assert len(suite_dicts) <= 3
        for suite_group in suite_dicts:
          if not self.check_ast_dict_formatted(suite_group, file_path, verbose):
            bad_files.add(file_path)

        for key, suite in zip(value.keys, value.values):
          # The compound suites are checked in
          # 'check_composition_type_test_suites()'
          if key.s == 'basic_suites':
            for group in suite.values:
              if not self.check_ast_dict_formatted(group, file_path, verbose):
                bad_files.add(file_path)
            break

      elif file_path == self.args.test_suite_exceptions_pyl_path:
        # Check the values for each test.
        for test in value.values:
          for kind, node in zip(test.keys, test.values):
            if isinstance(node, ast.Dict):
              if not self.check_ast_dict_formatted(node, file_path, verbose):
                bad_files.add(file_path)
            elif kind.s == 'remove_from':
              # Don't care about sorting; these are usually grouped, since the
              # same bug can affect multiple builders. Do want to make sure
              # there aren't duplicates.
              if not self.check_ast_list_formatted(
                  node.elts, file_path, verbose, check_sorting=False):
                bad_files.add(file_path)

    if bad_files:
      raise BBGenErr(
          'The following files have invalid keys: %s\n. They are either '
          'unsorted, or have duplicates. Re-run this with --verbose to see '
          'more details.' % ', '.join(bad_files))

  def check_output_file_consistency(self, verbose=False):
    self.load_configuration_files()
    # All waterfalls/bucket .json files must have been written
    # by this script already.
    self.resolve_configuration_files()
    ungenerated_files = set()
    outputs = self.generate_outputs()
    for filename, expected_contents in outputs.items():
      expected = self.jsonify(expected_contents)
      file_path = os.path.join(self.args.output_dir, filename + '.json')
      current = self.read_file(file_path)
      if expected != current:
        ungenerated_files.add(filename)
        if verbose: # pragma: no cover
          self.print_line('File ' +  filename +
                 '.json did not have the following expected '
                 'contents:')
          for line in difflib.unified_diff(
              expected.splitlines(),
              current.splitlines(),
              fromfile='expected', tofile='current'):
            self.print_line(line)

    if ungenerated_files:
      raise BBGenErr(
          'The following files have not been properly '
           'autogenerated by generate_buildbot_json.py: ' +
           ', '.join([filename + '.json' for filename in ungenerated_files]))

    for builder_group, builders in outputs.items():
      for builder, step_types in builders.items():
        for test_type in ('gtest_tests', 'isolated_scripts'):
          for step_data in step_types.get(test_type, []):
            step_name = step_data['name']
            self._check_swarming_config(builder_group, builder, step_name,
                                        step_data)

  def _check_swarming_config(self, filename, builder, step_name, step_data):
    # TODO(crbug.com/40179524): Ensure all swarming tests specify cpu, not
    # just mac tests.
    if 'swarming' in step_data:
      dimensions = step_data['swarming'].get('dimensions')
      if not dimensions:
        raise BBGenErr('%s: %s / %s : dimensions must be specified for all '
                       'swarmed tests' % (filename, builder, step_name))
      if not dimensions.get('os'):
        raise BBGenErr('%s: %s / %s : os must be specified for all '
                       'swarmed tests' % (filename, builder, step_name))
      if 'Mac' in dimensions.get('os') and not dimensions.get('cpu'):
        raise BBGenErr('%s: %s / %s : cpu must be specified for mac '
                       'swarmed tests' % (filename, builder, step_name))

  def check_consistency(self, verbose=False):
    self.check_input_file_consistency(verbose) # pragma: no cover
    self.check_output_file_consistency(verbose) # pragma: no cover

  def does_test_match(self, test_info, params_dict):
    """Checks to see if the test matches the parameters given.

    Compares the provided test_info with the params_dict to see
    if the bot matches the parameters given. If so, returns True.
    Else, returns false.

    Args:
      test_info (dict): Information about a specific bot provided
                       in the format shown in waterfalls.pyl
      params_dict (dict): Dictionary of parameters and their values
                          to look for in the bot
        Ex: {
          'device_os':'android',
          '--flag':True,
          'mixins': ['mixin1', 'mixin2'],
          'ex_key':'ex_value'
        }

    """
    DIMENSION_PARAMS = ['device_os', 'device_type', 'os',
                        'kvm', 'pool', 'integrity'] # dimension parameters
    SWARMING_PARAMS = ['shards', 'hard_timeout', 'idempotent',
                       'can_use_on_swarming_builders']
    for param in params_dict:
      # if dimension parameter
      if param in DIMENSION_PARAMS or param in SWARMING_PARAMS:
        if not 'swarming' in test_info:
          return False
        swarming = test_info['swarming']
        if param in SWARMING_PARAMS:
          if not param in swarming:
            return False
          if not str(swarming[param]) == params_dict[param]:
            return False
        else:
          if not 'dimensions' in swarming:
            return False
          dimensions = swarming['dimensions']
          # only looking at the first dimension set
          if not param in dimensions:
            return False
          if not dimensions[param] == params_dict[param]:
            return False

      # if flag
      elif param.startswith('--'):
        if not 'args' in test_info:
          return False
        if not param in test_info['args']:
          return False

      # not dimension parameter/flag/mixin
      else:
        if not param in test_info:
          return False
        if not test_info[param] == params_dict[param]:
          return False
    return True
  def error_msg(self, msg):
    """Prints an error message.

    In addition to a catered error message, also prints
    out where the user can find more help. Then, program exits.
    """
    self.print_line(msg +  (' If you need more information, ' +
                  'please run with -h or --help to see valid commands.'))
    sys.exit(1)

  def find_bots_that_run_test(self, test, bots):
    matching_bots = []
    for bot in bots:
      bot_info = bots[bot]
      tests = self.flatten_tests_for_bot(bot_info)
      for test_info in tests:
        test_name = test_info['name']
        if not test_name == test:
          continue
        matching_bots.append(bot)
    return matching_bots

  def find_tests_with_params(self, tests, params_dict):
    matching_tests = []
    for test_name in tests:
      test_info = tests[test_name]
      if not self.does_test_match(test_info, params_dict):
        continue
      if not test_name in matching_tests:
        matching_tests.append(test_name)
    return matching_tests

  def flatten_waterfalls_for_query(self, waterfalls):
    bots = {}
    for waterfall in waterfalls:
      waterfall_tests = self.generate_output_tests(waterfall)
      for bot in waterfall_tests:
        bot_info = waterfall_tests[bot]
        bots[bot] = bot_info
    return bots

  def flatten_tests_for_bot(self, bot_info):
    """Returns a list of flattened tests.

    Returns a list of tests not grouped by test category
    for a specific bot.
    """
    TEST_CATS = self.get_test_generator_map().keys()
    tests = []
    for test_cat in TEST_CATS:
      if not test_cat in bot_info:
        continue
      test_cat_tests = bot_info[test_cat]
      tests = tests + test_cat_tests
    return tests

  def flatten_tests_for_query(self, test_suites):
    """Returns a flattened dictionary of tests.

    Returns a dictionary of tests associate with their
    configuration, not grouped by their test suite.
    """
    tests = {}
    for test_suite in test_suites.values():
      for test in test_suite:
        test_info = test_suite[test]
        test_name = test
        tests[test_name] = test_info
    return tests

  def parse_query_filter_params(self, params):
    """Parses the filter parameters.

    Creates a dictionary from the parameters provided
    to filter the bot array.
    """
    params_dict = {}
    for p in params:
      # flag
      if p.startswith('--'):
        params_dict[p] = True
      else:
        pair = p.split(':')
        if len(pair) != 2:
          self.error_msg('Invalid command.')
        # regular parameters
        if pair[1].lower() == 'true':
          params_dict[pair[0]] = True
        elif pair[1].lower() == 'false':
          params_dict[pair[0]] = False
        else:
          params_dict[pair[0]] = pair[1]
    return params_dict

  def get_test_suites_dict(self, bots):
    """Returns a dictionary of bots and their tests.

    Returns a dictionary of bots and a list of their associated tests.
    """
    test_suite_dict = dict()
    for bot in bots:
      bot_info = bots[bot]
      tests = self.flatten_tests_for_bot(bot_info)
      test_suite_dict[bot] = tests
    return test_suite_dict

  def output_query_result(self, result, json_file=None):
    """Outputs the result of the query.

    If a json file parameter name is provided, then
    the result is output into the json file. If not,
    then the result is printed to the console.
    """
    output = json.dumps(result, indent=2)
    if json_file:
      self.write_file(json_file, output)
    else:
      self.print_line(output)

  # pylint: disable=inconsistent-return-statements
  def query(self, args):
    """Queries tests or bots.

    Depending on the arguments provided, outputs a json of
    tests or bots matching the appropriate optional parameters provided.
    """
    # split up query statement
    query = args.query.split('/')
    self.load_configuration_files()
    self.resolve_configuration_files()

    # flatten bots json
    tests = self.test_suites
    bots = self.flatten_waterfalls_for_query(self.waterfalls)

    cmd_class = query[0]

    # For queries starting with 'bots'
    if cmd_class == 'bots':
      if len(query) == 1:
        return self.output_query_result(bots, args.json)
      # query with specific parameters
      if len(query) == 2:
        if query[1] == 'tests':
          test_suites_dict = self.get_test_suites_dict(bots)
          return self.output_query_result(test_suites_dict, args.json)
        self.error_msg('This query should be in the format: bots/tests.')

      else:
        self.error_msg('This query should have 0 or 1 "/"", found %s instead.' %
                       str(len(query) - 1))

    # For queries starting with 'bot'
    elif cmd_class == 'bot':
      if not len(query) == 2 and not len(query) == 3:
        self.error_msg('Command should have 1 or 2 "/"", found %s instead.' %
                       str(len(query) - 1))
      bot_id = query[1]
      if not bot_id in bots:
        self.error_msg('No bot named "' + bot_id + '" found.')
      bot_info = bots[bot_id]
      if len(query) == 2:
        return self.output_query_result(bot_info, args.json)
      if not query[2] == 'tests':
        self.error_msg('The query should be in the format:'
                       'bot/<bot-name>/tests.')

      bot_tests = self.flatten_tests_for_bot(bot_info)
      return self.output_query_result(bot_tests, args.json)

    # For queries starting with 'tests'
    elif cmd_class == 'tests':
      if not len(query) == 1 and not len(query) == 2:
        self.error_msg('The query should have 0 or 1 "/", found %s instead.' %
                       str(len(query) - 1))
      flattened_tests = self.flatten_tests_for_query(tests)
      if len(query) == 1:
        return self.output_query_result(flattened_tests, args.json)

      # create params dict
      params = query[1].split('&')
      params_dict = self.parse_query_filter_params(params)
      matching_bots = self.find_tests_with_params(flattened_tests, params_dict)
      return self.output_query_result(matching_bots)

    # For queries starting with 'test'
    elif cmd_class == 'test':
      if not len(query) == 2 and not len(query) == 3:
        self.error_msg('The query should have 1 or 2 "/", found %s instead.' %
                       str(len(query) - 1))
      test_id = query[1]
      if len(query) == 2:
        flattened_tests = self.flatten_tests_for_query(tests)
        for test in flattened_tests:
          if test == test_id:
            return self.output_query_result(flattened_tests[test], args.json)
        self.error_msg('There is no test named %s.' % test_id)
      if not query[2] == 'bots':
        self.error_msg('The query should be in the format: '
                       'test/<test-name>/bots')
      bots_for_test = self.find_bots_that_run_test(test_id, bots)
      return self.output_query_result(bots_for_test)

    else:
      self.error_msg('Your command did not match any valid commands. '
                     'Try starting with "bots", "bot", "tests", or "test".')

  # pylint: enable=inconsistent-return-statements

  def main(self):  # pragma: no cover
    if self.args.check:
      self.check_consistency(verbose=self.args.verbose)
    elif self.args.query:
      self.query(self.args)
    else:
      self.write_json_result(self.generate_outputs())
    return 0


if __name__ == '__main__':  # pragma: no cover
  generator = BBJSONGenerator(BBJSONGenerator.parse_args(sys.argv[1:]))
  sys.exit(generator.main())
