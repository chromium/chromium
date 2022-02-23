#!/usr/bin/env python3
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Usage: run_with_dummy_home.py <command>

Helper for running a test with a dummy $HOME, populated with just enough for
tests to run and pass. Useful for isolating tests from the real $HOME, which
can contain config files that negatively affect test performance.
"""

import os
import shutil
import subprocess
import sys
import tempfile


def _set_up_dummy_home(original_home, dummy_home):
  """Sets up a dummy $HOME that Chromium tests can run in.

  Files are copied, while directories are symlinked.
  """
  for filename in ['.Xauthority']:
    original_path = os.path.join(original_home, filename)
    if not os.path.exists(original_path):
      continue
    shutil.copyfile(original_path, os.path.join(dummy_home, filename))

  # Prevent fontconfig etc. from reconstructing the cache and symlink rr
  # trace directory.
  for dirpath in [['.cache'], ['.local', 'share', 'rr'], ['.vpython'],
                  ['.vpython_cipd_cache'], ['.vpython-root']]:
    original_path = os.path.join(original_home, *dirpath)
    if not os.path.exists(original_path):
      continue
    dummy_parent_path = os.path.join(dummy_home, *dirpath[:-1])
    if not os.path.isdir(dummy_parent_path):
      os.makedirs(dummy_parent_path)
    os.symlink(original_path, os.path.join(dummy_home, *dirpath))


def main():
  try:
    dummy_home = tempfile.mkdtemp()
    print('Creating dummy home in %s' % dummy_home)

    original_home = os.environ['HOME']
    os.environ['HOME'] = dummy_home
    _set_up_dummy_home(original_home, dummy_home)

    return subprocess.call(sys.argv[1:])
  finally:
    shutil.rmtree(dummy_home)


if __name__ == '__main__':
  sys.exit(main())
