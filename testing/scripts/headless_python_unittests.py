#!/usr/bin/env python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import sys


import common


def main_run(args):
  typ_path = os.path.abspath(os.path.join(
      os.path.dirname(__file__), os.path.pardir, os.path.pardir,
      'third_party', 'catapult', 'third_party', 'typ'))
  _AddToPathIfNeeded(typ_path)
  import typ

  top_level_dir = os.path.join(
      common.SRC_DIR, 'headless', 'lib', 'browser', 'devtools_api')
  with common.temporary_file() as tempfile_path:
    rc = typ.main(
        argv=[],
        top_level_dir=top_level_dir,
        write_full_results_to=tempfile_path,
        coverage_source=[top_level_dir])

    with open(tempfile_path) as f:
      results = json.load(f)

  parsed_results = common.parse_common_test_results(results, test_separator='.')
  failures = parsed_results['unexpected_failures']

  json.dump({
      'valid': bool(rc <= common.MAX_FAILURES_EXIT_STATUS and
                   ((rc == 0) or failures)),
      'failures': failures.keys(),
  }, args.output)

  return rc


def main_compile_targets(args):
  json.dump([], args.output)


def _AddToPathIfNeeded(path):
  if path not in sys.path:
    sys.path.insert(0, path)


if __name__ == '__main__':
  funcs = {
    'run': main_run,
    'compile_targets': main_compile_targets,
  }
  sys.exit(common.run_script(sys.argv[1:], funcs))
