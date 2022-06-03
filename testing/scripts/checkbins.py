#!/usr/bin/env python3
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Launches //tools/checkbins/checkbins.py for trybots.

To run locally on `out/release`, create /tmp/config.json

{
  "checkout": "."
}

python3 testing/scripts/checkbins.py \
  --paths /tmp/config.json \
  --build-config-fs release run \
  --output -
"""

import json
import os
import sys


import common

WIN_PY3_TARGETS = ['python3.exe', 'python3.bat']

def with_python3():
  if sys.version_info.major >= 3:
    return sys.executable
  # `env` should have worked on other platforms.
  if sys.platform == 'win32':
    # non-exhaustive, we expect depot_tools somewhere.
    for d in os.environ['PATH'].split(';'):
      for maybe_py3 in WIN_PY3_TARGETS:
        if os.path.exists(os.path.join(d, maybe_py3)):
          return os.path.join(d, maybe_py3)
  raise Exception("Cannot find python3 to launch checkbins.py")

def main_run(args):
  print(sys.executable)
  with common.temporary_file() as tempfile_path:
    rc = common.run_command([
        with_python3(),
        os.path.join(common.SRC_DIR, 'tools', 'checkbins', 'checkbins.py'),
        '--verbose',
        '--json', tempfile_path,
        os.path.join(args.paths['checkout'], 'out', args.build_config_fs),
    ])

    with open(tempfile_path) as f:
      checkbins_results = json.load(f)

  common.record_local_script_results(
      'checkbins', args.output, checkbins_results, True)

  return rc


def main_compile_targets(args):
  json.dump([], args.output)


if __name__ == '__main__':
  funcs = {
    'run': main_run,
    'compile_targets': main_compile_targets,
  }
  sys.exit(common.run_script(sys.argv[1:], funcs))
