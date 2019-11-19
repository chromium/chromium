#!/usr/bin/env python
#
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This is just a wrapper script around monochrome_apk_checker.py that
# understands and uses the isolated-script arguments

import argparse
import json
import os
import sys
import subprocess
import time

import common


def _PathExists(path):
  return path is not None and os.path.exists(path)


def _ForwardOptionalArgs(args):
  forwardable_args = []
  if _PathExists(args.monochrome_pathmap):
    forwardable_args += ['--monochrome-pathmap', args.monochrome_pathmap]
  if _PathExists(args.chrome_pathmap):
    forwardable_args += ['--chrome-pathmap', args.chrome_pathmap]
  if _PathExists(args.system_webview_pathmap):
    forwardable_args += [
        '--system-webview-pathmap', args.system_webview_pathmap
    ]
  return forwardable_args


def main():
  parser = argparse.ArgumentParser(prog='monochrome_apk_checker_wrapper')

  parser.add_argument('--script',
                      required=True,
                      help='The path to the monochrome_apk_checker.py script')
  parser.add_argument('--isolated-script-test-output',
                      required=True)
  # Only run one test, we check that it's the test we expect.
  parser.add_argument('--isolated-script-test-filter')
  # Ignored, but required to satisfy the isolated_script interface.
  # We aren't a perf test, so don't have any perf output.
  parser.add_argument('--isolated-script-test-perf-output')

  # We must intercept the pathmap args since they are always passed by the
  # trybots but the files in question may not actually exist (path shortening
  # may not be enabled for all apks). Check if the files exist and forward the
  # arg iff they are.
  parser.add_argument(
      '--monochrome-pathmap', help='The monochrome APK resources pathmap path.')
  parser.add_argument(
      '--chrome-pathmap', help='The chrome APK resources pathmap path.')
  parser.add_argument(
      '--system-webview-pathmap',
      help='The system webview APK resources pathmap path.')

  args, extra = parser.parse_known_args(sys.argv[1:])

  if args.isolated_script_test_filter and (
      'monochrome_apk_checker' not in args.isolated_script_test_filter):
    parser.error('isolated-script-test-filter has invalid test: %s' %
                 (args.isolated_script_test_filter))

  cmd = [args.script] + extra + _ForwardOptionalArgs(args)

  start_time = time.time()
  ret = subprocess.call(cmd)
  success = ret == 0

  # Schema is at //docs/testing/json_test_results_format.md
  with open(args.isolated_script_test_output, 'w') as fp:
    test = {
      'expected': 'PASS',
      'actual': 'PASS' if success else 'FAIL',
    }
    if not success:
      test['is_unexpected'] = True

    json.dump({
      'version': 3,
      'interrupted': False,
      'path_delimiter': '/',
      'seconds_since_epoch': start_time,
      'num_failures_by_type': {
        'PASS': int(success),
        'FAIL': int(not success),
      },
      'tests': {
        'monochrome_apk_checker': test,
      }
    }, fp)

  return ret

# This is not really a "script test" so does not need to manually add
# any additional compile targets.
def main_compile_targets(args):
  json.dump([], args.output)

if __name__ == '__main__':
  # Conform minimally to the protocol defined by ScriptTest.
  if 'compile_targets' in sys.argv:
    funcs = {
      'run': None,
      'compile_targets': main_compile_targets,
    }
    sys.exit(common.run_script(sys.argv[1:], funcs))

  sys.exit(main())
