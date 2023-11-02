#!/usr/bin/env vpython3
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Run web_tests (aka LayoutTests)"""

import multiprocessing
import os
import sys

from blinkpy.web_tests import run_web_tests


def main():
    tools_dir = os.path.dirname(os.path.abspath(__file__))
    search_paths = os.environ.get('PYTHONPATH', tools_dir).split(os.pathsep)
    if tools_dir not in search_paths:
        search_paths.append(tools_dir)
    os.environ['PYTHONPATH'] = os.pathsep.join(search_paths)
    if tools_dir not in sys.path:
        sys.path.append(tools_dir)
    return run_web_tests.main(sys.argv[1:], sys.stderr)


if __name__ == '__main__':
    multiprocessing.set_start_method('spawn')
    sys.exit(main())
