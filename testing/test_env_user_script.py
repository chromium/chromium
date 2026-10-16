#!/usr/bin/env python
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Script for use in test_env unittests."""

import os
import sys

# //testing imports.
import test_env

HERE = os.path.dirname(os.path.abspath(__file__))
TEST_SCRIPT = os.path.join(HERE, 'test_env_test_script.py')


def main():
  hjob = test_env.setup_job_object()
  try:
    return test_env.run_command([sys.executable, TEST_SCRIPT] + sys.argv[1:])
  finally:
    test_env.cleanup_child_processes(hjob)


if __name__ == '__main__':
  sys.exit(main())
