#!/usr/bin/env vpython3
# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Simple helper script to run pytype on GPU Python code."""

import os
import subprocess
import sys

if sys.platform != 'linux':
  print('pytype is currently only supported on Linux, see '
        'https://github.com/google/pytype/issues/1154')
  sys.exit(0)

# pytype looks for a 'python' or 'python3' executable in PATH, so make sure that
# the Python 3 executable from vpython is in the path.
executable_dir = os.path.dirname(sys.executable)
os.environ['PATH'] = executable_dir + os.pathsep + os.environ['PATH']
subprocess.run([sys.executable, '-m', 'pytype'] + sys.argv[1:], check=True)
