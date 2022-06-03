#!/usr/bin/env python
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import subprocess
import sys


#  python run_with_pythonpath.py -I path1 ... -I pathN foo/bar/baz.py args...
#  ==> Run "python foo/bar/baz.py args..." with PYTHONPATH=path1:..:pathN
def main():
    python_paths = []
    args = sys.argv[1:]
    while len(args) >= 2 and args[0] == '-I':
        python_paths.append(args[1])
        args = args[2:]

    env = os.environ.copy()
    if len(python_paths) > 0:
        existing_pp = (
            os.pathsep + env['PYTHONPATH']) if 'PYTHONPATH' in env else ''
        env['PYTHONPATH'] = os.pathsep.join(python_paths) + existing_pp
    sys.exit(subprocess.call([sys.executable] + args, env=env))


if __name__ == '__main__':
    main()
