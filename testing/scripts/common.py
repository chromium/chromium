# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import contextlib
import io
import json
import os
import logging
import subprocess
import sys
import tempfile
import time
import traceback

# Add src/testing/ into sys.path for importing xvfb.
sys.path.append(os.path.join(os.path.dirname(__file__), '..'))
import xvfb
import test_env

# Unfortunately we need to copy these variables from ../test_env.py.
# Importing it and using its get_sandbox_env breaks test runs on Linux
# (it seems to unset DISPLAY).
CHROME_SANDBOX_ENV = 'CHROME_DEVEL_SANDBOX'
CHROME_SANDBOX_PATH = '/opt/chromium/chrome_sandbox'


SCRIPT_DIR = os.path.abspath(os.path.dirname(__file__))
SRC_DIR = os.path.abspath(
    os.path.join(SCRIPT_DIR, os.path.pardir, os.path.pardir))


# run_web_tests.py returns the number of failures as the return
# code, but caps the return code at 101 to avoid overflow or colliding
# with reserved values from the shell.
MAX_FAILURES_EXIT_STATUS = 101


# Exit code to indicate infrastructure issue.
INFRA_FAILURE_EXIT_CODE = 87


def run_script(argv, funcs):
  def parse_json(path):
    with open(path) as f:
      return json.load(f)
  parser = argparse.ArgumentParser()
  # TODO(phajdan.jr): Make build-config-fs required after passing it in recipe.
  parser.add_argument('--build-config-fs')
  parser.add_argument('--paths', type=parse_json, default={})
  # Properties describe the environment of the build, and are the same per
  # script invocation.
  parser.add_argument('--properties', type=parse_json, default={})
  # Args contains per-invocation arguments that potentially change the
  # behavior of the script.
  parser.add_argument('--args', type=parse_json, default=[])

  parser.add_argument(
      '--use-src-side-runtest-py', action='store_true',
      help='Use the src-side copy of runtest.py, as opposed to the build-side '
           'one')

  subparsers = parser.add_subparsers()

  run_parser = subparsers.add_parser('run')
  run_parser.add_argument(
      '--output', type=argparse.FileType('w'), required=True)
  run_parser.add_argument('--filter-file', type=argparse.FileType('r'))
  run_parser.set_defaults(func=funcs['run'])

  run_parser = subparsers.add_parser('compile_targets')
  run_parser.add_argument(
      '--output', type=argparse.FileType('w'), required=True)
  run_parser.set_defaults(func=funcs['compile_targets'])

  args = parser.parse_args(argv)
  return args.func(args)


def run_command(argv, env=None, cwd=None):
  print 'Running %r in %r (env: %r)' % (argv, cwd, env)
  rc = test_env.run_command(argv, env=env, cwd=cwd)
  print 'Command %r returned exit code %d' % (argv, rc)
  return rc


def run_runtest(cmd_args, runtest_args):
  env = os.environ.copy()
  env['CHROME_HEADLESS'] = '1'

  return run_command([
      sys.executable,
      os.path.join(
          cmd_args.paths['checkout'], 'infra', 'scripts', 'runtest_wrapper.py'),
      '--',
      '--target', cmd_args.build_config_fs,
      '--xvfb',
      '--builder-name', cmd_args.properties['buildername'],
      '--slave-name', cmd_args.properties['slavename'],
      '--build-number', str(cmd_args.properties['buildnumber']),
      '--build-properties', json.dumps(cmd_args.properties),
  ] + runtest_args, env=env)


@contextlib.contextmanager
def temporary_file():
  fd, path = tempfile.mkstemp()
  os.close(fd)
  try:
    yield path
  finally:
    os.remove(path)


def parse_common_test_results(json_results, test_separator='/'):
  def convert_trie_to_flat_paths(trie, prefix=None):
    # Also see blinkpy.web_tests.layout_package.json_results_generator
    result = {}
    for name, data in trie.iteritems():
      if prefix:
        name = prefix + test_separator + name
      if len(data) and not 'actual' in data and not 'expected' in data:
        result.update(convert_trie_to_flat_paths(data, name))
      else:
        result[name] = data
    return result

  results = {
    'passes': {},
    'unexpected_passes': {},
    'failures': {},
    'unexpected_failures': {},
    'flakes': {},
    'unexpected_flakes': {},
  }

  # TODO(dpranke): crbug.com/357866 - we should simplify the handling of
  # both the return code and parsing the actual results, below.

  passing_statuses = ('PASS', 'SLOW', 'NEEDSREBASELINE')

  for test, result in convert_trie_to_flat_paths(
      json_results['tests']).iteritems():
    key = 'unexpected_' if result.get('is_unexpected') else ''
    data = result['actual']
    actual_results = data.split()
    last_result = actual_results[-1]
    expected_results = result['expected'].split()

    if (len(actual_results) > 1 and
        (last_result in expected_results or last_result in passing_statuses)):
      key += 'flakes'
    elif last_result in passing_statuses:
      key += 'passes'
      # TODO(dpranke): crbug.com/357867 ...  Why are we assigning result
      # instead of actual_result here. Do we even need these things to be
      # hashes, or just lists?
      data = result
    else:
      key += 'failures'
    results[key][test] = data

  return results


def write_interrupted_test_results_to(filepath, test_start_time):
  """Writes a test results JSON file* to filepath.

  This JSON file is formatted to explain that something went wrong.

  *src/docs/testing/json_test_results_format.md

  Args:
    filepath: A path to a file to write the output to.
    test_start_time: The start time of the test run expressed as a
      floating-point offset in seconds from the UNIX epoch.
  """
  with open(filepath, 'w') as fh:
    output = {
        'interrupted': True,
        'num_failures_by_type': {},
        'seconds_since_epoch': test_start_time,
        'tests': {},
        'version': 3,
    }
    json.dump(output, fh)


def get_gtest_summary_passes(output):
  """Returns a mapping of test to boolean indicating if the test passed.

  Only partially parses the format. This code is based on code in tools/build,
  specifically
  https://chromium.googlesource.com/chromium/tools/build/+/17fef98756c5f250b20bf716829a0004857235ff/scripts/slave/recipe_modules/test_utils/util.py#189
  """
  if not output:
    return {}

  mapping = {}

  for cur_iteration_data in output.get('per_iteration_data', []):
    for test_fullname, results in cur_iteration_data.iteritems():
      # Results is a list with one entry per test try. Last one is the final
      # result.
      last_result = results[-1]

      if last_result['status'] == 'SUCCESS':
        mapping[test_fullname] = True
      elif last_result['status'] != 'SKIPPED':
        mapping[test_fullname] = False

  return mapping


def extract_filter_list(filter_list):
  """Helper for isolated script test wrappers. Parses the
  --isolated-script-test-filter command line argument. Currently, double-colon
  ('::') is used as the separator between test names, because a single colon may
  be used in the names of perf benchmarks, which contain URLs.
  """
  return filter_list.split('::')


class BaseIsolatedScriptArgsAdapter(object):
  """The base class for all script adapters that need to translate flags
  set by isolated script test contract into the specific test script's flags.
  """

  def __init__(self):
    self._parser = argparse.ArgumentParser()
    self._options = None
    self._rest_args = None
    self._parser.add_argument(
        '--isolated-script-test-output', type=str,
        required=True)
    self._parser.add_argument(
        '--isolated-script-test-filter', type=str,
        required=False)
    self._parser.add_argument(
        '--isolated-script-test-repeat', type=int,
        required=False)
    self._parser.add_argument(
        '--isolated-script-test-launcher-retry-limit', type=int,
        required=False)
    self._parser.add_argument(
        '--isolated-script-test-also-run-disabled-tests',
        default=False, action='store_true', required=False)

    self._parser.add_argument('--xvfb', help='start xvfb', action='store_true')

    # This argument is ignored for now.
    self._parser.add_argument(
        '--isolated-script-test-chartjson-output', type=str)
    # This argument is ignored for now.
    self._parser.add_argument('--isolated-script-test-perf-output', type=str)

    self.add_extra_arguments(self._parser)

  def add_extra_arguments(self, parser):
    pass

  def parse_args(self, args=None):
    self._options, self._rest_args = self._parser.parse_known_args(args)

  @property
  def parser(self):
    return self._parser

  @property
  def options(self):
    return self._options

  @property
  def rest_args(self):
    return self._rest_args

  def generate_test_output_args(self, output):
    del output  # unused
    raise RuntimeError('this method is not yet implemented')

  def generate_test_filter_args(self, test_filter_str):
    del test_filter_str  # unused
    raise RuntimeError('this method is not yet implemented')

  def generate_test_repeat_args(self, repeat_count):
    del repeat_count  # unused
    raise RuntimeError('this method is not yet implemented')

  def generate_test_launcher_retry_limit_args(self, retry_limit):
    del retry_limit  # unused
    raise RuntimeError('this method is not yet implemented')

  def generate_test_also_run_disabled_tests_args(self):
    raise RuntimeError('this method is not yet implemented')

  def generate_sharding_args(self, total_shard, shard_index):
    del total_shard, shard_index  # unused
    raise RuntimeError('this method is not yet implemented')

  def generate_isolated_script_cmd(self):
    isolated_script_cmd = [sys.executable] + self.rest_args

    isolated_script_cmd += self.generate_test_output_args(
        self.options.isolated_script_test_output)

    # Augment test filter args if needed
    if self.options.isolated_script_test_filter:
      isolated_script_cmd += self.generate_test_filter_args(
          self.options.isolated_script_test_filter)

    # Augment test repeat if needed
    if self.options.isolated_script_test_repeat is not None:
      isolated_script_cmd += self.generate_test_repeat_args(
          self.options.isolated_script_test_repeat)

    # Augment test launcher retry limit args if needed
    if self.options.isolated_script_test_launcher_retry_limit is not None:
      isolated_script_cmd += self.generate_test_launcher_retry_limit_args(
          self.options.isolated_script_test_launcher_retry_limit)

    # Augment test also run disable tests args if needed
    if self.options.isolated_script_test_also_run_disabled_tests:
      isolated_script_cmd += self.generate_test_also_run_disabled_tests_args()

    # Augment shard args if needed
    env = os.environ.copy()

    total_shards = None
    shard_index = None

    if 'GTEST_TOTAL_SHARDS' in env:
      total_shards = int(env['GTEST_TOTAL_SHARDS'])
    if 'GTEST_SHARD_INDEX' in env:
      shard_index = int(env['GTEST_SHARD_INDEX'])
    if total_shards is not None and shard_index is not None:
      isolated_script_cmd += self.generate_sharding_args(
          total_shards, shard_index)

    return isolated_script_cmd

  def clean_up_after_test_run(self):
    pass

  def run_test(self):
    self.parse_args()
    cmd = self.generate_isolated_script_cmd()

    env = os.environ.copy()

    # Assume we want to set up the sandbox environment variables all the
    # time; doing so is harmless on non-Linux platforms and is needed
    # all the time on Linux.
    env[CHROME_SANDBOX_ENV] = CHROME_SANDBOX_PATH
    valid = True
    try:
      env['CHROME_HEADLESS'] = '1'
      print 'Running command: %s\nwith env: %r' % (
          ' '.join(cmd), env)
      if self.options.xvfb:
        exit_code = xvfb.run_executable(cmd, env)
      else:
        exit_code = test_env.run_command(cmd, env=env)
      print 'Command returned exit code %d' % exit_code
      return exit_code
    except Exception:
      traceback.print_exc()
      valid = False
    finally:
      self.clean_up_after_test_run()

    if not valid:
      failures = ['(entire test suite)']
      with open(self.options.isolated_script_test_output, 'w') as fp:
        json.dump({
            'valid': valid,
            'failures': failures,
        }, fp)

    return 1
