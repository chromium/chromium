#!/usr/bin/env python
# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import json
import os
import sys

import common


def main(argv):
  parser = argparse.ArgumentParser()
  parser.add_argument('--output', required=True)
  parser.add_argument('args', nargs=argparse.REMAINDER)

  args = parser.parse_args(argv)

  passthrough_args = args.args
  if passthrough_args[0] == '--':
    passthrough_args = passthrough_args[1:]

  results = {}

  for filename in os.listdir(common.SCRIPT_DIR):
    if not filename.endswith('.py'):
      continue
    if filename in ('common.py', 'get_compile_targets.py',
                    'gpu_integration_test_adapter.py', 'PRESUBMIT.py',
                    'sizes_common.py', 'variations_seed_access_helper.py',
                    'run_variations_smoke_tests.py',
                    'run_performance_tests_unittest.py'):
      continue

    with common.temporary_file() as tempfile_path:
      rc = common.run_command(
          [sys.executable,
           os.path.join(common.SCRIPT_DIR, filename)] + passthrough_args +
          ['compile_targets', '--output', tempfile_path])
      if rc != 0:
        return rc

      with open(tempfile_path) as f:
        # json.load() throws a ValueError for empty files
        try:
          results[filename] = json.load(f)
        except ValueError:
          pass

  with open(args.output, 'w') as f:
    json.dump(results, f)

  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
