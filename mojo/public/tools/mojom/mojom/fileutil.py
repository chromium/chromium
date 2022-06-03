# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import errno
import imp
import os.path
import sys


def _GetDirAbove(dirname):
  """Returns the directory "above" this file containing |dirname| (which must
  also be "above" this file)."""
  path = os.path.abspath(__file__)
  while True:
    path, tail = os.path.split(path)
    if not tail:
      return None
    if tail == dirname:
      return path


def EnsureDirectoryExists(path, always_try_to_create=False):
  """A wrapper for os.makedirs that does not error if the directory already
  exists. A different process could be racing to create this directory."""

  if not os.path.exists(path) or always_try_to_create:
    try:
      os.makedirs(path)
    except OSError as e:
      # There may have been a race to create this directory.
      if e.errno != errno.EEXIST:
        raise


def AddLocalRepoThirdPartyDirToModulePath():
  """Helper function to find the top-level directory of this script's repository
  assuming the script falls somewhere within a 'mojo' directory, and insert the
  top-level 'third_party' directory early in the module search path. Used to
  ensure that third-party dependencies provided within the repository itself
  (e.g. Chromium sources include snapshots of jinja2 and ply) are preferred over
  locally installed system library packages."""
  toplevel_dir = _GetDirAbove('mojo')
  if toplevel_dir:
    sys.path.insert(1, os.path.join(toplevel_dir, 'third_party'))
