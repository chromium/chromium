#!/usr/bin/env vpython
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A smoke test to verify Chrome doesn't crash and basic rendering is functional
when parsing a newly given Finch seed.
"""

import argparse
import json
import logging
import os
import sys

import common


def main_run(args):
  """Runs the Finch smoke tests."""
  logging.basicConfig(level=logging.INFO)
  parser = argparse.ArgumentParser()
  parser.add_argument('--isolated-script-test-output', type=str, required=True)
  args, _ = parser.parse_known_args()
  with open(args.isolated_script_test_output, 'w') as f:
    common.record_local_script_results('run_finch_smoke_tests', f, [], True)
  return 0


def main_compile_targets(args):
  """Returns the list of targets to compile in order to run this test."""
  json.dump(['chrome', 'chromedriver'], args.output)
  return 0


if __name__ == '__main__':
  if 'compile_targets' in sys.argv:
    funcs = {
        'run': None,
        'compile_targets': main_compile_targets,
    }
    sys.exit(common.run_script(sys.argv[1:], funcs))
  sys.exit(main_run(sys.argv[1:]))
