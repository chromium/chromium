#!/usr/bin/env python
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""//testing/scripts wrapper for the grit unittests. This script is used to run
test_suite_all.py on the trybots to ensure that grit is working correctly on
all platforms."""

import json
import os
import sys

import common


def main_run(args):
  rc = common.run_command([
      sys.executable,
      os.path.join(common.SRC_DIR, 'tools', 'grit', 'grit',
                   'test_suite_all.py'),
  ])

  json.dump(
      {
          'valid': True,
          'failures': ['Please refer to stdout for errors.'] if rc else [],
      }, args.output)

  return rc


def main_compile_targets(args):
  json.dump([], args.output)


if __name__ == '__main__':
  funcs = {
      'run': main_run,
      'compile_targets': main_compile_targets,
  }
  sys.exit(common.run_script(sys.argv[1:], funcs))
