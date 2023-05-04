#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import subprocess
import tempfile

if (not os.path.isfile('viewer.ts')):
  raise Exception("Run this script directly from the containing directory.")

with tempfile.TemporaryDirectory() as tmpdirname:
  cmd = [
    'python3', '../../../tools/typescript/ts_library.py',
    '--output_suffix', 'build_ts',
    '--root_gen_dir', '.',
    '--root_src_dir', '.',
    '--root_dir', '.',
    '--out_dir', './tsc',
    '--gen_dir', tmpdirname,
    '--in_files', 'viewer.ts']
  subprocess.check_call(cmd)
