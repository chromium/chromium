#!/usr/bin/env python
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import sys

import common


def main_run(args):
  with common.temporary_file() as tempfile_path:
    rc = common.run_command([
        sys.executable,
        os.path.join(common.SRC_DIR, 'build', 'check_gn_headers.py'),
        '--out-dir',
        args.build_dir,
        '--whitelist',
        os.path.join(common.SRC_DIR, 'build', 'check_gn_headers_whitelist.txt'),
        '--json',
        tempfile_path,
        '--verbose',
    ],
                            cwd=common.SRC_DIR)

    with open(tempfile_path) as f:
      failures = json.load(f)

  json.dump({
      'valid': True,
      'failures': failures,
  }, args.output)

  return rc


def main_compile_targets(args):
  json.dump([], args.output)


if __name__ == '__main__':
  funcs = {
      'run': main_run,
      'compile_targets': main_compile_targets,
  }
  common.run_script(sys.argv[1:], funcs)
