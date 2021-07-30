#!/usr/bin/env vpython
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A smoke test to verify Chrome doesn't crash and basic rendering is functional
when parsing a newly given Finch seed.
"""

import json
import logging
import os
import sys

import common


def main_run(args):
  """Runs the Finch smoke tests."""
  logging.basicConfig(level=logging.INFO)
  common.record_local_script_results('run_finch_smoke_tests', args.output, [],
                                     True)
  return 0


def main_compile_targets(args):
  """Returns the list of targets to compile in order to run this test."""
  json.dump(['chrome', 'chromedriver'], args.output)
  return 0


if __name__ == '__main__':
  funcs = {
      'run': main_run,
      'compile_targets': main_compile_targets,
  }
  sys.exit(common.run_script(sys.argv[1:], funcs))
