#!/usr/bin/env vpython3
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import json
import os
import sys

import common

sys.path.append(
    os.path.abspath(os.path.join(os.path.dirname(__file__), os.path.pardir)))
# //testing imports.
import xvfb


def main(argv):
  parser = argparse.ArgumentParser()
  parser.add_argument('--isolated-script-test-output', type=str, required=False)
  parser.add_argument('--isolated-script-test-chartjson-output',
                      type=str,
                      required=False)
  parser.add_argument('--isolated-script-test-perf-output',
                      type=str,
                      required=False)
  parser.add_argument('--isolated-script-test-filter', type=str, required=False)
  parser.add_argument('--platform',
                      type=str,
                      default=sys.platform,
                      required=False)

  args = parser.parse_args(argv)

  env = os.environ.copy()

  additional_args = []
  if args.platform == 'win32':
    exe = os.path.join('.', 'content_shell.exe')
  elif args.platform == 'darwin':
    exe = os.path.join('.', 'Content Shell.app', 'Contents', 'MacOS',
                       'Content Shell')
    # The Content Shell binary does not directly link against
    # the Content Shell Framework (it is loaded at runtime). Ensure that
    # symbols are dumped for the Framework too.
    additional_args = [
        '--additional-binary',
        os.path.join('.', 'Content Shell.app', 'Contents', 'Frameworks',
                     'Content Shell Framework.framework', 'Versions', 'Current',
                     'Content Shell Framework')
    ]
  elif args.platform == 'android':
    exe = os.path.join('.', 'lib.unstripped',
                       'libcontent_shell_content_view.so')
  else:
    exe = os.path.join('.', 'content_shell')

  with common.temporary_file() as tempfile_path:
    env['CHROME_HEADLESS'] = '1'
    rc = xvfb.run_executable([
        sys.executable,
        os.path.join(common.SRC_DIR, 'content', 'shell', 'tools',
                     'breakpad_integration_test.py'),
        '--verbose',
        '--build-dir',
        '.',
        '--binary',
        exe,
        '--json',
        tempfile_path,
        '--platform',
        args.platform,
    ] + additional_args, env)

    with open(tempfile_path) as f:
      failures = json.load(f)

  if args.isolated_script_test_output:
    with open(args.isolated_script_test_output, 'w') as fp:
      common.record_local_script_results('content_shell_crash_test', fp,
                                         failures, True)

  return rc


def main_compile_targets(args):
  json.dump(['content_shell_crash_test'], args.output)


if __name__ == '__main__':
  # Conform minimally to the protocol defined by ScriptTest.
  if 'compile_targets' in sys.argv:
    funcs = {
        'run': None,
        'compile_targets': main_compile_targets,
    }
    sys.exit(common.run_script(sys.argv[1:], funcs))
  sys.exit(main(sys.argv[1:]))
