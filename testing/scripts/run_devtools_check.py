#!/usr/bin/env python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs a python script under an isolate

This script attempts to emulate the contract of gtest-style tests
invoked via recipes.

If optional argument --isolated-script-test-output=[FILENAME] is passed
to the script, json is written to that file in the format detailed in
//docs/testing/json-test-results-format.md.

This script is intended to be the base command invoked by the isolate,
followed by a subsequent Python script."""

import argparse
import json
import os
import sys


import common

# Add src/testing/ into sys.path for importing xvfb.
sys.path.append(os.path.join(os.path.dirname(__file__), '..'))
import xvfb


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--isolated-script-test-output', type=str)
  args, rest_args = parser.parse_known_args()
  # Remove the isolated script extra args this script doesn't care about.
  should_ignore_arg = lambda arg: any(to_ignore in arg for to_ignore in (
    '--isolated-script-test-chartjson-output',
    '--isolated-script-test-perf-output',
    '--isolated-script-test-filter',
    ))
  rest_args = [arg for arg in rest_args if not should_ignore_arg(arg)]

  ret = common.run_command([sys.executable] + rest_args)
  if args.isolated_Script_test_output:
    with open(args.isolated_script_test_output, 'w') as fp:
      json.dump({'valid': True,
                 'failures': ['failed'] if ret else []}, fp)
  return ret


# This is not really a "script test" so does not need to manually add
# any additional compile targets.
def main_compile_targets(args):
  json.dump([''], args.output)


if __name__ == '__main__':
  # Conform minimally to the protocol defined by ScriptTest.
  if 'compile_targets' in sys.argv:
    funcs = {
      'run': None,
      'compile_targets': main_compile_targets,
    }
    sys.exit(common.run_script(sys.argv[1:], funcs))
  sys.exit(main())
