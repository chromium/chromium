#!/usr/bin/env python
# Copyright 2016 The Chromium Authors. All rights reserved.
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
import itertools
import json
import os
import string
import sys
import traceback

THIS_DIR = os.path.dirname(os.path.abspath(__file__))


class BBGenErr(Exception):
  def __init__(self, message):
    super(BBGenErr, self).__init__(message)


# This class is only present to accommodate certain machines on
# chromium.android.fyi which run certain tests as instrumentation
# tests, but not as gtests. If this discrepancy were fixed then the
# notion could be removed.
class TestSuiteTypes(object):
  GTEST = 'gtest'


class BaseGenerator(object):
  def __init__(self, bb_gen):
    self.bb_gen = bb_gen

  def generate(self, waterfall, tester_name, tester_config, input_tests):
    raise NotImplementedError()

  def sort(self, tests):
    raise NotImplementedError()


def cmp_tests(a, b):
  # Prefer to compare based on the "test" key.
  val = cmp(a['test'], b['test'])
  if val != 0:
    return val
  if 'name' in a and 'name' in b:
    return cmp(a['name'], b['name']) # pragma: no cover
  if 'name' not in a and 'name' not in b:
    return 0 # pragma: no cover
  # Prefer to put variants of the same test after the first one.
  if 'name' in a:
    return 1
  # 'name' is in b.
  return -1 # pragma: no cover


class GPUTelemetryTestGenerator(BaseGenerator):

  def __init__(self, bb_gen, is_android_webview=False):
    super(GPUTelemetryTestGenerator, self).__init__(bb_gen)
    self._is_android_webview = is_android_webview

  def generate(self, waterfall, tester_name, tester_config, input_tests):
    isolated_scripts = []
    for test_name, test_config in sorted(input_tests.iteritems()):
      test = self.bb_gen.generate_gpu_telemetry_test(
          waterfall, tester_name, tester_config, test_name, test_config,
          self._is_android_webview)
      if test:
        isolated_scripts.append(test)
    return isolated_scripts

  def sort(self, tests):
    return sorted(tests, key=lambda x: x['name'])


class GTestGenerator(BaseGenerator):
  def __init__(self, bb_gen):
    super(GTestGenerator, self).__init__(bb_gen)

  def generate(self, waterfall, tester_name, tester_config, input_tests):
    # The relative ordering of some of the tests is important to
    # minimize differences compared to the handwritten JSON files, since
    # Python's sorts are stable and there are some tests with the same
    # key (see gles2_conform_d3d9_test and similar variants). Avoid
    # losing the order by avoiding coalescing the dictionaries into one.
    gtests = []
    for test_name, test_config in sorted(input_tests.iteritems()):
      test = self.bb_gen.generate_gtest(
        waterfall, tester_name, tester_config, test_name, test_config)
      if test:
        # generate_gtest may veto the test generation on this tester.
        gtests.append(test)
    return gtests

  def sort(self, tests):
    return sorted(tests, cmp=cmp_tests)


class IsolatedScriptTestGenerator(BaseGenerator):
  def __init__(self, bb_gen):
    super(IsolatedScriptTestGenerator, self).__init__(bb_gen)

  def generate(self, waterfall, tester_name, tester_config, input_tests):
    isolated_scripts = []
    for test_name, test_config in sorted(input_tests.iteritems()):
      test = self.bb_gen.generate_isolated_script_test(
        waterfall, tester_name, tester_config, test_name, test_config)
      if test:
        isolated_scripts.append(test)
    return isolated_scripts

  def sort(self, tests):
    return sorted(tests, key=lambda x: x['name'])


class ScriptGenerator(BaseGenerator):
  def __init__(self, bb_gen):
    super(ScriptGenerator, self).__init__(bb_gen)

  def generate(self, waterfall, tester_name, tester_config, input_tests):
    scripts = []
    for test_name, test_config in sorted(input_tests.iteritems()):
      test = self.bb_gen.generate_script_test(
        waterfall, tester_name, tester_config, test_name, test_config)
      if test:
        scripts.append(test)
    return scripts

  def sort(self, tests):
    return sorted(tests, key=lambda x: x['name'])


class JUnitGenerator(BaseGenerator):
  def __init__(self, bb_gen):
    super(JUnitGenerator, self).__init__(bb_gen)

  def generate(self, waterfall, tester_name, tester_config, input_tests):
    scripts = []
    for test_name, test_config in sorted(input_tests.iteritems()):
      test = self.bb_gen.generate_junit_test(
        waterfall, tester_name, tester_config, test_name, test_config)
      if test:
        scripts.append(test)
    return scripts

  def sort(self, tests):
    return sorted(tests, key=lambda x: x['test'])


class CTSGenerator(BaseGenerator):
  def __init__(self, bb_gen):
    super(CTSGenerator, self).__init__(bb_gen)

  def generate(self, waterfall, tester_name, tester_config, input_tests):
    # These only contain one entry and it's the contents of the input tests'
    # dictionary, verbatim.
    cts_tests = []
    cts_tests.append(input_tests)
    return cts_tests

  def sort(self, tests):
    return tests


class InstrumentationTestGenerator(BaseGenerator):
  def __init__(self, bb_gen):
    super(InstrumentationTestGenerator, self).__init__(bb_gen)

  def generate(self, waterfall, tester_name, tester_config, input_tests):
    scripts = []
    for test_name, test_config in sorted(input_tests.iteritems()):
      test = self.bb_gen.generate_instrumentation_test(
        waterfall, tester_name, tester_config, test_name, test_config)
      if test:
        scripts.append(test)
    return scripts

  def sort(self, tests):
    return sorted(tests, cmp=cmp_tests)


class BBJSONGenerator(object):
  def __init__(self):
    self.this_dir = THIS_DIR
    self.args = None
    self.waterfalls = None
    self.test_suites = None
    self.exceptions = None
    self.mixins = None

  def generate_abs_file_path(self, relative_path):
    return os.path.join(self.this_dir, relative_path) # pragma: no cover

  def print_line(self, line):
    # Exists so that tests can mock
    print line # pragma: no cover

  def read_file(self, relative_path):
    with open(self.generate_abs_file_path(
        relative_path)) as fp: # pragma: no cover
      return fp.read() # pragma: no cover

  def write_file(self, relative_path, contents):
    with open(self.generate_abs_file_path(
        relative_path), 'wb') as fp: # pragma: no cover
      fp.write(contents) # pragma: no cover

  def pyl_file_path(self, filename):
    if self.args and self.args.pyl_files_dir:
      return os.path.join(self.args.pyl_files_dir, filename)
    return filename

  def load_pyl_file(self, filename):
    try:
      return ast.literal_eval(self.read_file(
          self.pyl_file_path(filename)))
    except (SyntaxError, ValueError) as e: # pragma: no cover
      raise BBGenErr('Failed to parse pyl file "%s": %s' %
                     (filename, e)) # pragma: no cover

  # TOOD(kbr): require that os_type be specified for all bots in waterfalls.pyl.
  # Currently it is only mandatory for bots which run GPU tests. Change these to
  # use [] instead of .get().
  def is_android(self, tester_config):
    return tester_config.get('os_type') == 'android'

  def is_chromeos(self, tester_config):
    return tester_config.get('os_type') == 'chromeos'

  def is_linux(self, tester_config):
    return tester_config.get('os_type') == 'linux'

  def is_mac(self, tester_config):
    return tester_config.get('os_type') == 'mac'

  def is_win(self, tester_config):
    return tester_config.get('os_type') == 'win'

  def is_win64(self, tester_config):
    return (tester_config.get('os_type') == 'win' and
        tester_config.get('browser_config') == 'release_x64')

  def get_exception_for_test(self, test_name, test_config):
    # gtests may have both "test" and "name" fields, and usually, if the "name"
    # field is specified, it means that the same test is being repurposed
    # multiple times with different command line arguments. To handle this case,
    # prefer to lookup per the "name" field of the test itself, as opposed to
    # the "test_name", which is actually the "test" field.
    if 'name' in test_config:
      return self.exceptions.get(test_config['name'])
    else:
      return self.exceptions.get(test_name)

  def should_run_on_tester(self, waterfall, tester_name,test_name, test_config):
    # Currently, the only reason a test should not run on a given tester is that
    # it's in the exceptions. (Once the GPU waterfall generation script is
    # incorporated here, the rules will become more complex.)
    exception = self.get_exception_for_test(test_name, test_config)
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

  def get_test_modifications(self, test, test_name, tester_name):
    exception = self.get_exception_for_test(test_name, test)
    if not exception:
      return None
    return exception.get('modifications', {}).get(tester_name)

  def get_test_replacements(self, test, test_name, tester_name):
    exception = self.get_exception_for_test(test_name, test)
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
    return arr

  def dictionary_merge(self, a, b, path=None, update=True):
    """http://stackoverflow.com/questions/7204805/
        python-dictionaries-of-dictionaries-merge
    merges b into a
    """
    if path is None:
      path = []
    for key in b:
      if key in a:
        if isinstance(a[key], dict) and isinstance(b[key], dict):
          self.dictionary_merge(a[key], b[key], path + [str(key)])
        elif a[key] == b[key]:
          pass # same leaf value
        elif isinstance(a[key], list) and isinstance(b[key], list):
          # Args arrays are lists of strings. Just concatenate them,
          # and don't sort them, in order to keep some needed
          # arguments adjacent (like --time-out-ms [arg], etc.)
          if all(isinstance(x, str)
                 for x in itertools.chain(a[key], b[key])):
            a[key] = self.maybe_fixup_args_array(a[key] + b[key])
          else:
            # TODO(kbr): this only works properly if the two arrays are
            # the same length, which is currently always the case in the
            # swarming dimension_sets that we have to merge. It will fail
            # to merge / override 'args' arrays which are different
            # length.
            for idx in xrange(len(b[key])):
              try:
                a[key][idx] = self.dictionary_merge(a[key][idx], b[key][idx],
                                                    path + [str(key), str(idx)],
                                                    update=update)
              except (IndexError, TypeError): # pragma: no cover
                raise BBGenErr('Error merging list keys ' + str(key) +
                               ' and indices ' + str(idx) + ' between ' +
                               str(a) + ' and ' + str(b)) # pragma: no cover
        elif update:
          if b[key] is None:
            del a[key]
          else:
            a[key] = b[key]
        else:
          raise BBGenErr('Conflict at %s' % '.'.join(
            path + [str(key)])) # pragma: no cover
      elif b[key] is not None:
        a[key] = b[key]
    return a

  def initialize_args_for_test(
      self, generated_test, tester_config, additional_arg_keys=None):

    args = []
    args.extend(generated_test.get('args', []))
    args.extend(tester_config.get('args', []))

    def add_conditional_args(key, fn):
      val = generated_test.pop(key, [])
      if fn(tester_config):
        args.extend(val)

    add_conditional_args('desktop_args', lambda cfg: not self.is_android(cfg))
    add_conditional_args('linux_args', self.is_linux)
    add_conditional_args('android_args', self.is_android)
    add_conditional_args('chromeos_args', self.is_chromeos)
    add_conditional_args('mac_args', self.is_mac)
    add_conditional_args('win_args', self.is_win)
    add_conditional_args('win64_args', self.is_win64)

    for key in additional_arg_keys or []:
      args.extend(generated_test.pop(key, []))
      args.extend(tester_config.get(key, []))

    if args:
      generated_test['args'] = self.maybe_fixup_args_array(args)

  def initialize_swarming_dictionary_for_test(self, generated_test,
                                              tester_config):
    if 'swarming' not in generated_test:
      generated_test['swarming'] = {}
    if not 'can_use_on_swarming_builders' in generated_test['swarming']:
      generated_test['swarming'].update({
        'can_use_on_swarming_builders': tester_config.get('use_swarming', True)
      })
    if 'swarming' in tester_config:
      if ('dimension_sets' not in generated_test['swarming'] and
          'dimension_sets' in tester_config['swarming']):
        generated_test['swarming']['dimension_sets'] = copy.deepcopy(
          tester_config['swarming']['dimension_sets'])
      self.dictionary_merge(generated_test['swarming'],
                            tester_config['swarming'])
    # Apply any Android-specific Swarming dimensions after the generic ones.
    if 'android_swarming' in generated_test:
      if self.is_android(tester_config): # pragma: no cover
        self.dictionary_merge(
          generated_test['swarming'],
          generated_test['android_swarming']) # pragma: no cover
      del generated_test['android_swarming'] # pragma: no cover

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
    if not swarming_dict.get('can_use_on_swarming_builders', False):
      # Remove all other keys.
      for k in swarming_dict.keys(): # pragma: no cover
        if k != 'can_use_on_swarming_builders': # pragma: no cover
          del swarming_dict[k] # pragma: no cover

  def update_and_cleanup_test(self, test, test_name, tester_name, tester_config,
                              waterfall):
    # Apply swarming mixins.
    test = self.apply_all_mixins(
        test, waterfall, tester_name, tester_config)
    # See if there are any exceptions that need to be merged into this
    # test's specification.
    modifications = self.get_test_modifications(test, test_name, tester_name)
    if modifications:
      test = self.dictionary_merge(test, modifications)
    if 'swarming' in test:
      self.clean_swarming_dictionary(test['swarming'])
    # Ensure all Android Swarming tests run only on userdebug builds if another
    # build type was not specified.
    if 'swarming' in test and self.is_android(tester_config):
      for d in test['swarming'].get('dimension_sets', []):
        if d.get('os') == 'Android' and not d.get('device_os_type'):
          d['device_os_type'] = 'userdebug'
    self.replace_test_args(test, test_name, tester_name)

    return test

  def replace_test_args(self, test, test_name, tester_name):
    replacements = self.get_test_replacements(
        test, test_name, tester_name) or {}
    valid_replacement_keys = ['args', 'non_precommit_args', 'precommit_args']
    for key, replacement_dict in replacements.iteritems():
      if key not in valid_replacement_keys:
        raise BBGenErr(
            'Given replacement key %s for %s on %s is not in the list of valid '
            'keys %s' % (key, test_name, tester_name, valid_replacement_keys))
      for replacement_key, replacement_val in replacement_dict.iteritems():
        found_key = False
        for i, test_key in enumerate(test.get(key, [])):
          # Handle both the key/value being replaced being defined as two
          # separate items or as key=value.
          if test_key == replacement_key:
            found_key = True
            # Handle flags without values.
            if replacement_val == None:
              del test[key][i]
            else:
              test[key][i+1] = replacement_val
            break
          elif test_key.startswith(replacement_key + '='):
            found_key = True
            if replacement_val == None:
              del test[key][i]
            else:
              test[key][i] = '%s=%s' % (replacement_key, replacement_val)
            break
        if not found_key:
          raise BBGenErr('Could not find %s in existing list of values for key '
                         '%s in %s on %s' % (replacement_key, key, test_name,
                             tester_name))

  def add_common_test_properties(self, test, tester_config):
    if tester_config.get('use_multi_dimension_trigger_script'):
      # Assumes update_and_cleanup_test has already been called, so the
      # builder's mixins have been flattened into the test.
      test['trigger_script'] = {
        'script': '//testing/trigger_scripts/trigger_multiple_dimensions.py',
        'args': [
          '--multiple-trigger-configs',
          json.dumps(test['swarming']['dimension_sets'] +
                     tester_config.get('alternate_swarming_dimensions', [])),
          '--multiple-dimension-script-verbose',
          'True'
        ],
      }
    elif self.is_chromeos(tester_config) and tester_config.get('use_swarming',
                                                               True):
      # The presence of the "device_type" dimension indicates that the tests
      # are targetting CrOS hardware and so need the special trigger script.
      dimension_sets = tester_config['swarming']['dimension_sets']
      if all('device_type' in ds for ds in dimension_sets):
        test['trigger_script'] = {
          'script': '//testing/trigger_scripts/chromeos_device_trigger.py',
        }

  def add_android_presentation_args(self, tester_config, test_name, result):
    args = result.get('args', [])
    bucket = tester_config.get('results_bucket', 'chromium-result-details')
    args.append('--gs-results-bucket=%s' % bucket)
    if (result['swarming']['can_use_on_swarming_builders'] and not
        tester_config.get('skip_merge_script', False)):
      result['merge'] = {
        'args': [
          '--bucket',
          bucket,
          '--test-name',
          test_name
        ],
        'script': '//build/android/pylib/results/presentation/'
          'test_results_presentation.py',
      }
    if not tester_config.get('skip_cipd_packages', False):
      cipd_packages = result['swarming'].get('cipd_packages', [])
      cipd_packages.append(
        {
          'cipd_package': 'infra/tools/luci/logdog/butler/${platform}',
          'location': 'bin',
          'revision': 'git_revision:ff387eadf445b24c935f1cf7d6ddd279f8a6b04c',
        }
      )
      result['swarming']['cipd_packages'] = cipd_packages
    if not tester_config.get('skip_output_links', False):
      result['swarming']['output_links'] = [
        {
          'link': [
            'https://luci-logdog.appspot.com/v/?s',
            '=android%2Fswarming%2Flogcats%2F',
            '${TASK_ID}%2F%2B%2Funified_logcats',
          ],
          'name': 'shard #${SHARD_INDEX} logcats',
        },
      ]
    if args:
      result['args'] = args

  def generate_gtest(self, waterfall, tester_name, tester_config, test_name,
                     test_config):
    if not self.should_run_on_tester(
        waterfall, tester_name, test_name, test_config):
      return None
    result = copy.deepcopy(test_config)
    if 'test' in result:
      result['name'] = test_name
    else:
      result['test'] = test_name
    self.initialize_swarming_dictionary_for_test(result, tester_config)

    self.initialize_args_for_test(
        result, tester_config, additional_arg_keys=['gtest_args'])
    if self.is_android(tester_config) and tester_config.get('use_swarming',
                                                            True):
      self.add_android_presentation_args(tester_config, test_name, result)
      result['args'] = result.get('args', []) + ['--recover-devices']

    result = self.update_and_cleanup_test(
        result, test_name, tester_name, tester_config, waterfall)
    self.add_common_test_properties(result, tester_config)

    if not result.get('merge'):
      # TODO(https://crbug.com/958376): Consider adding the ability to not have
      # this default.
      result['merge'] = {
        'script': '//testing/merge_scripts/standard_gtest_merge.py',
        'args': [],
      }
    return result

  def generate_isolated_script_test(self, waterfall, tester_name, tester_config,
                                    test_name, test_config):
    if not self.should_run_on_tester(waterfall, tester_name, test_name,
                                     test_config):
      return None
    result = copy.deepcopy(test_config)
    result['isolate_name'] = result.get('isolate_name', test_name)
    result['name'] = test_name
    self.initialize_swarming_dictionary_for_test(result, tester_config)
    self.initialize_args_for_test(result, tester_config)
    if tester_config.get('use_android_presentation', False):
      self.add_android_presentation_args(tester_config, test_name, result)
    result = self.update_and_cleanup_test(
        result, test_name, tester_name, tester_config, waterfall)
    self.add_common_test_properties(result, tester_config)

    if not result.get('merge'):
      # TODO(https://crbug.com/958376): Consider adding the ability to not have
      # this default.
      result['merge'] = {
        'script': '//testing/merge_scripts/standard_isolated_script_merge.py',
        'args': [],
      }
    return result

  def generate_script_test(self, waterfall, tester_name, tester_config,
                           test_name, test_config):
    # TODO(https://crbug.com/953072): Remove this check whenever a better
    # long-term solution is implemented.
    if (waterfall.get('forbid_script_tests', False) or
        waterfall['machines'][tester_name].get('forbid_script_tests', False)):
      raise BBGenErr('Attempted to generate a script test on tester ' +
                     tester_name + ', which explicitly forbids script tests')
    if not self.should_run_on_tester(waterfall, tester_name, test_name,
                                     test_config):
      return None
    result = {
      'name': test_name,
      'script': test_config['script']
    }
    result = self.update_and_cleanup_test(
        result, test_name, tester_name, tester_config, waterfall)
    return result

  def generate_junit_test(self, waterfall, tester_name, tester_config,
                          test_name, test_config):
    if not self.should_run_on_tester(waterfall, tester_name, test_name,
                                     test_config):
      return None
    result = copy.deepcopy(test_config)
    result.update({
      'name': test_name,
      'test': test_config.get('test', test_name),
    })
    self.initialize_args_for_test(result, tester_config)
    result = self.update_and_cleanup_test(
        result, test_name, tester_name, tester_config, waterfall)
    return result

  def generate_instrumentation_test(self, waterfall, tester_name, tester_config,
                                    test_name, test_config):
    if not self.should_run_on_tester(waterfall, tester_name, test_name,
                                     test_config):
      return None
    result = copy.deepcopy(test_config)
    if 'test' in result and result['test'] != test_name:
      result['name'] = test_name
    else:
      result['test'] = test_name
    result = self.update_and_cleanup_test(
        result, test_name, tester_name, tester_config, waterfall)
    return result

  def substitute_gpu_args(self, tester_config, swarming_config, args):
    substitutions = {
      # Any machine in waterfalls.pyl which desires to run GPU tests
      # must provide the os_type key.
      'os_type': tester_config['os_type'],
      'gpu_vendor_id': '0',
      'gpu_device_id': '0',
    }
    dimension_set = swarming_config['dimension_sets'][0]
    if 'gpu' in dimension_set:
      # First remove the driver version, then split into vendor and device.
      gpu = dimension_set['gpu']
      # Handle certain specialized named GPUs.
      if gpu.startswith('nvidia-quadro-p400'):
        gpu = ['10de', '1cb3']
      elif gpu.startswith('intel-hd-630'):
        gpu = ['8086', '5912']
      elif gpu.startswith('intel-uhd-630'):
        gpu = ['8086', '3e92']
      else:
        gpu = gpu.split('-')[0].split(':')
      substitutions['gpu_vendor_id'] = gpu[0]
      substitutions['gpu_device_id'] = gpu[1]
    return [string.Template(arg).safe_substitute(substitutions) for arg in args]

  def generate_gpu_telemetry_test(self, waterfall, tester_name, tester_config,
                                  test_name, test_config, is_android_webview):
    # These are all just specializations of isolated script tests with
    # a bunch of boilerplate command line arguments added.

    # The step name must end in 'test' or 'tests' in order for the
    # results to automatically show up on the flakiness dashboard.
    # (At least, this was true some time ago.) Continue to use this
    # naming convention for the time being to minimize changes.
    step_name = test_config.get('name', test_name)
    if not (step_name.endswith('test') or step_name.endswith('tests')):
      step_name = '%s_tests' % step_name
    result = self.generate_isolated_script_test(
      waterfall, tester_name, tester_config, step_name, test_config)
    if not result:
      return None
    result['isolate_name'] = 'telemetry_gpu_integration_test'
    args = result.get('args', [])
    test_to_run = result.pop('telemetry_test_name', test_name)

    # These tests upload and download results from cloud storage and therefore
    # aren't idempotent yet. https://crbug.com/549140.
    result['swarming']['idempotent'] = False

    # The GPU tests act much like integration tests for the entire browser, and
    # tend to uncover flakiness bugs more readily than other test suites. In
    # order to surface any flakiness more readily to the developer of the CL
    # which is introducing it, we disable retries with patch on the commit
    # queue.
    result['should_retry_with_patch'] = False

    browser = ('android-webview-instrumentation'
               if is_android_webview else tester_config['browser_config'])
    args = [
        test_to_run,
        '--show-stdout',
        '--browser=%s' % browser,
        # --passthrough displays more of the logging in Telemetry when
        # run via typ, in particular some of the warnings about tests
        # being expected to fail, but passing.
        '--passthrough',
        '-v',
        '--extra-browser-args=--enable-logging=stderr --js-flags=--expose-gc',
    ] + args
    result['args'] = self.maybe_fixup_args_array(self.substitute_gpu_args(
      tester_config, result['swarming'], args))
    return result

  def get_test_generator_map(self):
    return {
        'android_webview_gpu_telemetry_tests':
            GPUTelemetryTestGenerator(self, is_android_webview=True),
        'cts_tests':
            CTSGenerator(self),
        'gpu_telemetry_tests':
            GPUTelemetryTestGenerator(self),
        'gtest_tests':
            GTestGenerator(self),
        'instrumentation_tests':
            InstrumentationTestGenerator(self),
        'isolated_scripts':
            IsolatedScriptTestGenerator(self),
        'junit_tests':
            JUnitGenerator(self),
        'scripts':
            ScriptGenerator(self),
    }

  def get_test_type_remapper(self):
    return {
      # These are a specialization of isolated_scripts with a bunch of
      # boilerplate command line arguments added to each one.
      'android_webview_gpu_telemetry_tests': 'isolated_scripts',
      'gpu_telemetry_tests': 'isolated_scripts',
    }

  def check_composition_test_suites(self):
    # Pre-pass to catch errors reliably.
    for suite, suite_def in self.test_suites.iteritems():
      if isinstance(suite_def, list):
        seen_tests = {}
        for sub_suite in suite_def:
          if isinstance(self.test_suites[sub_suite], list):
            raise BBGenErr('Composition test suites may not refer to other '
                           'composition test suites (error found while '
                           'processing %s)' % suite)
          else:
            # test name -> basic_suite that it came from
            basic_tests = {k: sub_suite for k in self.test_suites[sub_suite]}
            for test_name, test_suite in basic_tests.iteritems():
              if (test_name in seen_tests and
                  self.test_suites[test_suite][test_name] !=
                  self.test_suites[seen_tests[test_name]][test_name]):
                raise BBGenErr('Conflicting test definitions for %s from %s '
                               'and %s in Composition test suite (error found '
                               'while processing %s)' % (test_name,
                               seen_tests[test_name], test_suite, suite))
            seen_tests.update(basic_tests)

  def flatten_test_suites(self):
    new_test_suites = {}
    for name, value in self.test_suites.get('basic_suites', {}).iteritems():
      new_test_suites[name] = value
    for name, value in self.test_suites.get('compound_suites', {}).iteritems():
      if name in new_test_suites:
        raise BBGenErr('Composition test suite names may not duplicate basic '
                       'test suite names (error found while processsing %s' % (
                       name))
      new_test_suites[name] = value
    self.test_suites = new_test_suites

  def resolve_composition_test_suites(self):
    self.flatten_test_suites()

    self.check_composition_test_suites()
    for name, value in self.test_suites.iteritems():
      if isinstance(value, list):
        # Resolve this to a dictionary.
        full_suite = {}
        for entry in value:
          suite = self.test_suites[entry]
          full_suite.update(suite)
        self.test_suites[name] = full_suite

  def link_waterfalls_to_test_suites(self):
    for waterfall in self.waterfalls:
      for tester_name, tester in waterfall['machines'].iteritems():
        for suite, value in tester.get('test_suites', {}).iteritems():
          if not value in self.test_suites:
            # Hard / impossible to cover this in the unit test.
            raise self.unknown_test_suite(
              value, tester_name, waterfall['name']) # pragma: no cover
          tester['test_suites'][suite] = self.test_suites[value]

  def load_configuration_files(self):
    self.waterfalls = self.load_pyl_file('waterfalls.pyl')
    self.test_suites = self.load_pyl_file('test_suites.pyl')
    self.exceptions = self.load_pyl_file('test_suite_exceptions.pyl')
    self.mixins = self.load_pyl_file('mixins.pyl')

  def resolve_configuration_files(self):
    self.resolve_composition_test_suites()
    self.link_waterfalls_to_test_suites()

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

  def apply_all_mixins(self, test, waterfall, builder_name, builder):
    """Applies all present swarming mixins to the test for a given builder.

    Checks in the waterfall, builder, and test objects for mixins.
    """
    def valid_mixin(mixin_name):
      """Asserts that the mixin is valid."""
      if mixin_name not in self.mixins:
        raise BBGenErr("bad mixin %s" % mixin_name)
    def must_be_list(mixins, typ, name):
      """Asserts that given mixins are a list."""
      if not isinstance(mixins, list):
        raise BBGenErr("'%s' in %s '%s' must be a list" % (mixins, typ, name))

    if 'mixins' in waterfall:
      must_be_list(waterfall['mixins'], 'waterfall', waterfall['name'])
      for mixin in waterfall['mixins']:
        valid_mixin(mixin)
        test = self.apply_mixin(self.mixins[mixin], test)

    if 'mixins' in builder:
      must_be_list(builder['mixins'], 'builder', builder_name)
      for mixin in builder['mixins']:
        valid_mixin(mixin)
        test = self.apply_mixin(self.mixins[mixin], test)

    if not 'mixins' in test:
      return test

    test_name = test.get('name')
    if not test_name:
      test_name = test.get('test')
    if not test_name: # pragma: no cover
      # Not the best name, but we should say something.
      test_name = str(test)
    must_be_list(test['mixins'], 'test', test_name)
    for mixin in test['mixins']:
      valid_mixin(mixin)
      test = self.apply_mixin(self.mixins[mixin], test)
      del test['mixins']
    return test

  def apply_mixin(self, mixin, test):
    """Applies a mixin to a test.

    Mixins will not override an existing key. This is to ensure exceptions can
    override a setting a mixin applies.

    Swarming dimensions are handled in a special way. Instead of specifying
    'dimension_sets', which is how normal test suites specify their dimensions,
    you specify a 'dimensions' key, which maps to a dictionary. This dictionary
    is then applied to every dimension set in the test.

    """
    new_test = copy.deepcopy(test)
    mixin = copy.deepcopy(mixin)

    if 'swarming' in mixin:
      swarming_mixin = mixin['swarming']
      new_test.setdefault('swarming', {})
      if 'dimensions' in swarming_mixin:
        new_test['swarming'].setdefault('dimension_sets', [{}])
        for dimension_set in new_test['swarming']['dimension_sets']:
          dimension_set.update(swarming_mixin['dimensions'])
        del swarming_mixin['dimensions']

      # python dict update doesn't do recursion at all. Just hard code the
      # nested update we need (mixin['swarming'] shouldn't clobber
      # test['swarming'], but should update it).
      new_test['swarming'].update(swarming_mixin)
      del mixin['swarming']

    if '$mixin_append' in mixin:
      # Values specified under $mixin_append should be appended to existing
      # lists, rather than replacing them.
      mixin_append = mixin['$mixin_append']
      for key in mixin_append:
        new_test.setdefault(key, [])
        if not isinstance(mixin_append[key], list):
          raise BBGenErr(
              'Key "' + key + '" in $mixin_append must be a list.')
        if not isinstance(new_test[key], list):
          raise BBGenErr(
              'Cannot apply $mixin_append to non-list "' + key + '".')
        new_test[key].extend(mixin_append[key])
      if 'args' in mixin_append:
        new_test['args'] = self.maybe_fixup_args_array(new_test['args'])
      del mixin['$mixin_append']

    new_test.update(mixin)

    return new_test

  def generate_waterfall_json(self, waterfall):
    all_tests = {}
    generator_map = self.get_test_generator_map()
    test_type_remapper = self.get_test_type_remapper()
    for name, config in waterfall['machines'].iteritems():
      tests = {}
      # Copy only well-understood entries in the machine's configuration
      # verbatim into the generated JSON.
      if 'additional_compile_targets' in config:
        tests['additional_compile_targets'] = config[
          'additional_compile_targets']
      for test_type, input_tests in config.get('test_suites', {}).iteritems():
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
        tests[remapped_test_type] = test_generator.sort(
          tests.get(remapped_test_type, []) + new_tests)
      all_tests[name] = tests
    all_tests['AAAAA1 AUTOGENERATED FILE DO NOT EDIT'] = {}
    all_tests['AAAAA2 See generate_buildbot_json.py to make changes'] = {}
    return json.dumps(all_tests, indent=2, separators=(',', ': '),
                      sort_keys=True) + '\n'

  def generate_waterfalls(self): # pragma: no cover
    self.load_configuration_files()
    self.resolve_configuration_files()
    filters = self.args.waterfall_filters
    suffix = '.json'
    if self.args.new_files:
      suffix = '.new' + suffix
    for waterfall in self.waterfalls:
      should_gen = not filters or waterfall['name'] in filters
      if should_gen:
        file_path = waterfall['name'] + suffix
        self.write_file(self.pyl_file_path(file_path),
                        self.generate_waterfall_json(waterfall))

  def get_valid_bot_names(self):
    # Extract bot names from infra/config/luci-milo.cfg.
    # NOTE: This reference can cause issues; if a file changes there, the
    # presubmit here won't be run by default. A manually maintained list there
    # tries to run presubmit here when luci-milo.cfg is changed. If any other
    # references to configs outside of this directory are added, please change
    # their presubmit to run `generate_buildbot_json.py -c`, so that the tree
    # never ends up in an invalid state.
    bot_names = set()
    infra_config_dir = os.path.abspath(
        os.path.join(os.path.dirname(__file__),
                     '..', '..', 'infra', 'config'))
    milo_configs = [
        os.path.join(infra_config_dir, 'generated', 'luci-milo.cfg'),
        os.path.join(infra_config_dir, 'generated', 'luci-milo-dev.cfg'),
    ]
    for c in milo_configs:
      for l in self.read_file(c).splitlines():
        if (not 'name: "buildbucket/luci.chromium.' in l and
            not 'name: "buildbucket/luci.chrome.' in l and
            not 'name: "buildbot/chromium.' in l and
            not 'name: "buildbot/tryserver.chromium.' in l):
          continue
        # l looks like
        # `name: "buildbucket/luci.chromium.try/win_chromium_dbg_ng"`
        # Extract win_chromium_dbg_ng part.
        bot_names.add(l[l.rindex('/') + 1:l.rindex('"')])
    return bot_names

  def get_builders_that_do_not_actually_exist(self):
    # Some of the bots on the chromium.gpu.fyi waterfall in particular
    # are defined only to be mirrored into trybots, and don't actually
    # exist on any of the waterfalls or consoles.
    return [
      'GPU FYI Fuchsia Builder',
      'ANGLE GPU Android Release (Nexus 5X)',
      'ANGLE GPU Linux Release (Intel HD 630)',
      'ANGLE GPU Linux Release (NVIDIA)',
      'ANGLE GPU Mac Release (Intel)',
      'ANGLE GPU Mac Retina Release (AMD)',
      'ANGLE GPU Mac Retina Release (NVIDIA)',
      'ANGLE GPU Win10 x64 Release (Intel HD 630)',
      'ANGLE GPU Win10 x64 Release (NVIDIA)',
      'Optional Android Release (Nexus 5X)',
      'Optional Linux Release (Intel HD 630)',
      'Optional Linux Release (NVIDIA)',
      'Optional Mac Release (Intel)',
      'Optional Mac Retina Release (AMD)',
      'Optional Mac Retina Release (NVIDIA)',
      'Optional Win10 x64 Release (Intel HD 630)',
      'Optional Win10 x64 Release (NVIDIA)',
      'Win7 ANGLE Tryserver (AMD)',
      # chromium.fyi
      'linux-blink-rel-dummy',
      'mac10.10-blink-rel-dummy',
      'mac10.11-blink-rel-dummy',
      'mac10.12-blink-rel-dummy',
      'mac10.13_retina-blink-rel-dummy',
      'mac10.13-blink-rel-dummy',
      'win7-blink-rel-dummy',
      'win10-blink-rel-dummy',
      'Dummy WebKit Mac10.13',
      'WebKit Linux composite_after_paint Dummy Builder',
      'WebKit Linux layout_ng_disabled Builder',
      # chromium, due to https://crbug.com/878915
      'win-dbg',
      'win32-dbg',
      'win-archive-dbg',
      'win32-archive-dbg',
    ]

  def get_internal_waterfalls(self):
    # Similar to get_builders_that_do_not_actually_exist above, but for
    # waterfalls defined in internal configs.
    return ['chrome']

  def check_input_file_consistency(self, verbose=False):
    self.check_input_files_sorting(verbose)

    self.load_configuration_files()
    self.flatten_test_suites()
    self.check_composition_test_suites()

    # All bots should exist.
    bot_names = self.get_valid_bot_names()
    internal_waterfalls = self.get_internal_waterfalls()
    builders_that_dont_exist = self.get_builders_that_do_not_actually_exist()
    for waterfall in self.waterfalls:
      # TODO(crbug.com/991417): Remove the need for this exception.
      if waterfall['name'] in internal_waterfalls:
        continue  # pragma: no cover
      for bot_name in waterfall['machines']:
        if bot_name in builders_that_dont_exist:
          continue  # pragma: no cover
        if bot_name not in bot_names:
          if waterfall['name'] in ['client.v8.chromium', 'client.v8.fyi']:
            # TODO(thakis): Remove this once these bots move to luci.
            continue  # pragma: no cover
          if waterfall['name'] in ['tryserver.webrtc',
                                   'webrtc.chromium.fyi.experimental']:
            # These waterfalls have their bot configs in a different repo.
            # so we don't know about their bot names.
            continue  # pragma: no cover
          if waterfall['name'] in [
              'client.devtools-frontend.integration',
              'tryserver.devtools-frontend']:
            continue  # pragma: no cover
          raise self.unknown_bot(bot_name, waterfall['name'])

    # All test suites must be referenced.
    suites_seen = set()
    generator_map = self.get_test_generator_map()
    for waterfall in self.waterfalls:
      for bot_name, tester in waterfall['machines'].iteritems():
        for suite_type, suite in tester.get('test_suites', {}).iteritems():
          if suite_type not in generator_map:
            raise self.unknown_test_suite_type(suite_type, bot_name,
                                               waterfall['name'])
          if suite not in self.test_suites:
            raise self.unknown_test_suite(suite, bot_name, waterfall['name'])
          suites_seen.add(suite)
    # Since we didn't resolve the configuration files, this set
    # includes both composition test suites and regular ones.
    resolved_suites = set()
    for suite_name in suites_seen:
      suite = self.test_suites[suite_name]
      if isinstance(suite, list):
        for sub_suite in suite:
          resolved_suites.add(sub_suite)
      resolved_suites.add(suite_name)
    # At this point, every key in test_suites.pyl should be referenced.
    missing_suites = set(self.test_suites.keys()) - resolved_suites
    if missing_suites:
      raise BBGenErr('The following test suites were unreferenced by bots on '
                     'the waterfalls: ' + str(missing_suites))

    # All test suite exceptions must refer to bots on the waterfall.
    all_bots = set()
    missing_bots = set()
    for waterfall in self.waterfalls:
      for bot_name, tester in waterfall['machines'].iteritems():
        all_bots.add(bot_name)
        # In order to disambiguate between bots with the same name on
        # different waterfalls, support has been added to various
        # exceptions for concatenating the waterfall name after the bot
        # name.
        all_bots.add(bot_name + ' ' + waterfall['name'])
    for exception in self.exceptions.itervalues():
      removals = (exception.get('remove_from', []) +
                  exception.get('remove_gtest_from', []) +
                  exception.get('modifications', {}).keys())
      for removal in removals:
        if removal not in all_bots:
          missing_bots.add(removal)

    missing_bots = missing_bots - set(builders_that_dont_exist)
    if missing_bots:
      raise BBGenErr('The following nonexistent machines were referenced in '
                     'the test suite exceptions: ' + str(missing_bots))

    # All mixins must be referenced
    seen_mixins = set()
    for waterfall in self.waterfalls:
      seen_mixins = seen_mixins.union(waterfall.get('mixins', set()))
      for bot_name, tester in waterfall['machines'].iteritems():
        seen_mixins = seen_mixins.union(tester.get('mixins', set()))
    for suite in self.test_suites.values():
      if isinstance(suite, list):
        # Don't care about this, it's a composition, which shouldn't include a
        # swarming mixin.
        continue

      for test in suite.values():
        if not isinstance(test, dict):
          # Some test suites have top level keys, which currently can't be
          # swarming mixin entries. Ignore them
          continue

        seen_mixins = seen_mixins.union(test.get('mixins', set()))

    missing_mixins = set(self.mixins.keys()) - seen_mixins
    if missing_mixins:
      raise BBGenErr('The following mixins are unreferenced: %s. They must be'
                     ' referenced in a waterfall, machine, or test suite.' % (
                         str(missing_mixins)))


  def type_assert(self, node, typ, filename, verbose=False):
    """Asserts that the Python AST node |node| is of type |typ|.

    If verbose is set, it prints out some helpful context lines, showing where
    exactly the error occurred in the file.
    """
    if not isinstance(node, typ):
      if verbose:
        lines = [""] + self.read_file(filename).splitlines()

        context = 2
        lines_start = max(node.lineno - context, 0)
        # Add one to include the last line
        lines_end = min(node.lineno + context, len(lines)) + 1
        lines = (
            ['== %s ==\n' % filename] +
            ["<snip>\n"] +
            ['%d %s' % (lines_start + i, line) for i, line in enumerate(
                lines[lines_start:lines_start + context])] +
            ['-' * 80 + '\n'] +
            ['%d %s' % (node.lineno, lines[node.lineno])] +
            ['-' * (node.col_offset + 3) + '^' + '-' * (
                80 - node.col_offset - 4) + '\n'] +
            ['%d %s' % (node.lineno + 1 + i, line) for i, line in enumerate(
                lines[node.lineno + 1:lines_end])] +
            ["<snip>\n"]
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
          'Invalid .pyl file %r. Python AST node %r on line %s expected to'
          ' be %s, is %s' % (
              filename, node_dumped,
              node.lineno, typ, type(node)))

  def ensure_ast_dict_keys_sorted(self, node, filename, verbose):
    is_valid = True

    keys = []
    # The keys of this dict are ordered as ordered in the file; normal python
    # dictionary keys are given an arbitrary order, but since we parsed the
    # file itself, the order as given in the file is preserved.
    for key in node.keys:
      self.type_assert(key, ast.Str, filename, verbose)
      keys.append(key.s)

    keys_sorted = sorted(keys)
    if keys_sorted != keys:
      is_valid = False
      if verbose:
        for line in difflib.unified_diff(
            keys,
            keys_sorted, fromfile='current (%r)' % filename, tofile='sorted'):
          self.print_line(line)

    if len(set(keys)) != len(keys):
      for i in range(len(keys_sorted)-1):
        if keys_sorted[i] == keys_sorted[i+1]:
          self.print_line('Key %s is duplicated' % keys_sorted[i])
          is_valid = False
    return is_valid

  def check_input_files_sorting(self, verbose=False):
    # TODO(https://crbug.com/886993): Add the ability for this script to
    # actually format the files, rather than just complain if they're
    # incorrectly formatted.
    bad_files = set()

    for filename in (
        'mixins.pyl',
        'test_suites.pyl',
        'test_suite_exceptions.pyl',
    ):
      parsed = ast.parse(self.read_file(self.pyl_file_path(filename)))

      # Must be a module.
      self.type_assert(parsed, ast.Module, filename, verbose)
      module = parsed.body

      # Only one expression in the module.
      self.type_assert(module, list, filename, verbose)
      if len(module) != 1: # pragma: no cover
        raise BBGenErr('Invalid .pyl file %s' % filename)
      expr = module[0]
      self.type_assert(expr, ast.Expr, filename, verbose)

      # Value should be a dictionary.
      value = expr.value
      self.type_assert(value, ast.Dict, filename, verbose)

      if filename == 'test_suites.pyl':
        expected_keys = ['basic_suites', 'compound_suites']
        actual_keys = [node.s for node in value.keys]
        assert all(key in expected_keys for key in actual_keys), (
                    'Invalid %r file; expected keys %r, got %r' % (
                        filename, expected_keys, actual_keys))
        suite_dicts = [node for node in value.values]
        # Only two keys should mean only 1 or 2 values
        assert len(suite_dicts) <= 2
        for suite_group in suite_dicts:
          if not self.ensure_ast_dict_keys_sorted(
              suite_group, filename, verbose):
            bad_files.add(filename)

      else:
        if not self.ensure_ast_dict_keys_sorted(
            value, filename, verbose):
          bad_files.add(filename)

    # waterfalls.pyl is slightly different, just do it manually here
    filename = 'waterfalls.pyl'
    parsed = ast.parse(self.read_file(self.pyl_file_path(filename)))

    # Must be a module.
    self.type_assert(parsed, ast.Module, filename, verbose)
    module = parsed.body

    # Only one expression in the module.
    self.type_assert(module, list, filename, verbose)
    if len(module) != 1: # pragma: no cover
      raise BBGenErr('Invalid .pyl file %s' % filename)
    expr = module[0]
    self.type_assert(expr, ast.Expr, filename, verbose)

    # Value should be a list.
    value = expr.value
    self.type_assert(value, ast.List, filename, verbose)

    keys = []
    for val in value.elts:
      self.type_assert(val, ast.Dict, filename, verbose)
      waterfall_name = None
      for key, val in zip(val.keys, val.values):
        self.type_assert(key, ast.Str, filename, verbose)
        if key.s == 'machines':
          if not self.ensure_ast_dict_keys_sorted(val, filename, verbose):
            bad_files.add(filename)

        if key.s == "name":
          self.type_assert(val, ast.Str, filename, verbose)
          waterfall_name = val.s
      assert waterfall_name
      keys.append(waterfall_name)

    if sorted(keys) != keys:
      bad_files.add(filename)
      if verbose: # pragma: no cover
        for line in difflib.unified_diff(
            keys,
            sorted(keys), fromfile='current', tofile='sorted'):
          self.print_line(line)

    if bad_files:
      raise BBGenErr(
          'The following files have invalid keys: %s\n. They are either '
          'unsorted, or have duplicates.' % ', '.join(bad_files))

  def check_output_file_consistency(self, verbose=False):
    self.load_configuration_files()
    # All waterfalls must have been written by this script already.
    self.resolve_configuration_files()
    ungenerated_waterfalls = set()
    for waterfall in self.waterfalls:
      expected = self.generate_waterfall_json(waterfall)
      file_path = waterfall['name'] + '.json'
      current = self.read_file(self.pyl_file_path(file_path))
      if expected != current:
        ungenerated_waterfalls.add(waterfall['name'])
        if verbose: # pragma: no cover
          self.print_line('Waterfall ' +  waterfall['name'] +
                 ' did not have the following expected '
                 'contents:')
          for line in difflib.unified_diff(
              expected.splitlines(),
              current.splitlines(),
              fromfile='expected', tofile='current'):
            self.print_line(line)
    if ungenerated_waterfalls:
      raise BBGenErr('The following waterfalls have not been properly '
                     'autogenerated by generate_buildbot_json.py: ' +
                     str(ungenerated_waterfalls))

  def check_consistency(self, verbose=False):
    self.check_input_file_consistency(verbose) # pragma: no cover
    self.check_output_file_consistency(verbose) # pragma: no cover

  def parse_args(self, argv): # pragma: no cover

    # RawTextHelpFormatter allows for styling of help statement
    parser = argparse.ArgumentParser(formatter_class=
                                     argparse.RawTextHelpFormatter)

    group = parser.add_mutually_exclusive_group()
    group.add_argument(
      '-c', '--check', action='store_true', help=
      'Do consistency checks of configuration and generated files and then '
      'exit. Used during presubmit. Causes the tool to not generate any files.')
    group.add_argument(
      '--query', type=str, help=
        ("Returns raw JSON information of buildbots and tests.\n" +
        "Examples:\n" +
          "  List all bots (all info):\n" +
          "    --query bots\n\n" +
          "  List all bots and only their associated tests:\n" +
          "    --query bots/tests\n\n" +
          "  List all information about 'bot1' " +
               "(make sure you have quotes):\n" +
          "    --query bot/'bot1'\n\n" +
          "  List tests running for 'bot1' (make sure you have quotes):\n" +
          "    --query bot/'bot1'/tests\n\n" +
          "  List all tests:\n" +
          "    --query tests\n\n" +
          "  List all tests and the bots running them:\n" +
          "    --query tests/bots\n\n"+
          "  List all tests that satisfy multiple parameters\n" +
          "  (separation of parameters by '&' symbol):\n" +
          "    --query tests/'device_os:Android&device_type:hammerhead'\n\n" +
          "  List all tests that run with a specific flag:\n" +
          "    --query bots/'--test-launcher-print-test-studio=always'\n\n" +
          "  List specific test (make sure you have quotes):\n"
          "    --query test/'test1'\n\n"
          "  List all bots running 'test1' " +
               "(make sure you have quotes):\n" +
          "    --query test/'test1'/bots" ))
    parser.add_argument(
      '-n', '--new-files', action='store_true', help=
      'Write output files as .new.json. Useful during development so old and '
      'new files can be looked at side-by-side.')
    parser.add_argument(
      '-v', '--verbose', action='store_true', help=
      'Increases verbosity. Affects consistency checks.')
    parser.add_argument(
      'waterfall_filters', metavar='waterfalls', type=str, nargs='*',
      help='Optional list of waterfalls to generate.')
    parser.add_argument(
      '--pyl-files-dir', type=os.path.realpath,
      help='Path to the directory containing the input .pyl files.')
    parser.add_argument(
      '--json', help=
      ("Outputs results into a json file. Only works with query function.\n" +
      "Examples:\n" +
      "  Outputs file into specified json file: \n" +
      "    --json <file-name-here.json>"))
    self.args = parser.parse_args(argv)
    if self.args.json and not self.args.query:
      parser.error("The --json flag can only be used with --query.")

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
          if not 'dimension_sets' in swarming:
            return False
          d_set = swarming['dimension_sets']
          # only looking at the first dimension set
          if not param in d_set[0]:
            return False
          if not d_set[0][param] == params_dict[param]:
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
        test_name = ""
        if 'name' in test_info:
          test_name = test_info['name']
        elif 'test' in test_info:
          test_name = test_info['test']
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
      waterfall_json = json.loads(self.generate_waterfall_json(waterfall))
      for bot in waterfall_json:
        bot_info = waterfall_json[bot]
        if 'AAAAA' not in bot:
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
    for test_suite in test_suites.itervalues():
      for test in test_suite:
        test_info = test_suite[test]
        test_name = test
        if 'name' in test_info:
          test_name = test_info['name']
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
      if p.startswith("--"):
        params_dict[p] = True
      else:
        pair = p.split(":")
        if len(pair) != 2:
          self.error_msg('Invalid command.')
        # regular parameters
        if pair[1].lower() == "true":
          params_dict[pair[0]] = True
        elif pair[1].lower() == "false":
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
    return

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
    if cmd_class == "bots":
      if len(query) == 1:
        return self.output_query_result(bots, args.json)
      # query with specific parameters
      elif len(query) == 2:
        if query[1] == 'tests':
          test_suites_dict = self.get_test_suites_dict(bots)
          return self.output_query_result(test_suites_dict, args.json)
        else:
          self.error_msg("This query should be in the format: bots/tests.")

      else:
        self.error_msg("This query should have 0 or 1 '/', found %s instead."
                        % str(len(query)-1))

    # For queries starting with 'bot'
    elif cmd_class == "bot":
      if not len(query) == 2 and not len(query) == 3:
        self.error_msg("Command should have 1 or 2 '/', found %s instead."
                        % str(len(query)-1))
      bot_id = query[1]
      if not bot_id in bots:
        self.error_msg("No bot named '" + bot_id + "' found.")
      bot_info = bots[bot_id]
      if len(query) == 2:
        return self.output_query_result(bot_info, args.json)
      if not query[2] == 'tests':
        self.error_msg("The query should be in the format:" +
                       "bot/<bot-name>/tests.")

      bot_tests = self.flatten_tests_for_bot(bot_info)
      return self.output_query_result(bot_tests, args.json)

    # For queries starting with 'tests'
    elif cmd_class == "tests":
      if not len(query) == 1 and not len(query) == 2:
        self.error_msg("The query should have 0 or 1 '/', found %s instead."
                        % str(len(query)-1))
      flattened_tests = self.flatten_tests_for_query(tests)
      if len(query) == 1:
        return self.output_query_result(flattened_tests, args.json)

      # create params dict
      params = query[1].split('&')
      params_dict = self.parse_query_filter_params(params)
      matching_bots = self.find_tests_with_params(flattened_tests, params_dict)
      return self.output_query_result(matching_bots)

    # For queries starting with 'test'
    elif cmd_class == "test":
      if not len(query) == 2 and not len(query) == 3:
        self.error_msg("The query should have 1 or 2 '/', found %s instead."
                        % str(len(query)-1))
      test_id = query[1]
      if len(query) == 2:
        flattened_tests = self.flatten_tests_for_query(tests)
        for test in flattened_tests:
          if test == test_id:
            return self.output_query_result(flattened_tests[test], args.json)
        self.error_msg("There is no test named %s." % test_id)
      if not query[2] == 'bots':
        self.error_msg("The query should be in the format: " +
                       "test/<test-name>/bots")
      bots_for_test = self.find_bots_that_run_test(test_id, bots)
      return self.output_query_result(bots_for_test)

    else:
      self.error_msg("Your command did not match any valid commands." +
                     "Try starting with 'bots', 'bot', 'tests', or 'test'.")

  def main(self, argv): # pragma: no cover
    self.parse_args(argv)
    if self.args.check:
      self.check_consistency(verbose=self.args.verbose)
    elif self.args.query:
      self.query(self.args)
    else:
      self.generate_waterfalls()
    return 0

if __name__ == "__main__": # pragma: no cover
  generator = BBJSONGenerator()
  sys.exit(generator.main(sys.argv[1:]))
