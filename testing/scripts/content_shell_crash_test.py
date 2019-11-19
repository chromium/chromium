#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import json
import os
import sys


import common

# Add src/testing/ into sys.path for importing xvfb.
sys.path.append(os.path.join(os.path.dirname(__file__), '..'))
import xvfb


# Unfortunately we need to copy these variables from ../test_env.py.
# Importing it and using its get_sandbox_env breaks test runs on Linux
# (it seems to unset DISPLAY).
CHROME_SANDBOX_ENV = 'CHROME_DEVEL_SANDBOX'
CHROME_SANDBOX_PATH = '/opt/chromium/chrome_sandbox'


def main(argv):
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--isolated-script-test-output', type=str,
      required=True)
  parser.add_argument(
      '--isolated-script-test-chartjson-output', type=str,
      required=False)
  parser.add_argument(
      '--isolated-script-test-perf-output', type=str,
      required=False)
  parser.add_argument(
      '--isolated-script-test-filter', type=str,
      required=False)
  parser.add_argument(
      '--platform', type=str, default=sys.platform, required=False)

  args = parser.parse_args(argv)

  env = os.environ.copy()
  # Assume we want to set up the sandbox environment variables all the
  # time; doing so is harmless on non-Linux platforms and is needed
  # all the time on Linux.
  env[CHROME_SANDBOX_ENV] = CHROME_SANDBOX_PATH

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
                     'Content Shell Framework.framework', 'Versions',
                     'Current', 'Content Shell Framework')
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
        '--build-dir', '.',
        '--binary', exe,
        '--json', tempfile_path,
        '--platform', args.platform,
    ] + additional_args, env)

    with open(tempfile_path) as f:
      failures = json.load(f)

  with open(args.isolated_script_test_output, 'w') as fp:
    json.dump({
        'valid': True,
        'failures': failures,
    }, fp)

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
