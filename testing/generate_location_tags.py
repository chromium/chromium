#!/usr/bin/env python
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Generates the directory->tags mapping used by ResultDB."""

# pylint: disable=line-too-long
#
# For more on the tags, see
# https://source.chromium.org/chromium/infra/infra/+/main:go/src/go.chromium.org/luci/resultdb/sink/proto/v1/location_tag.proto
#
# pylint: enable=line-too-long

import argparse
import logging
import os
import subprocess
import sys

THIS_DIR = os.path.dirname(__file__)
SRC_DIR = os.path.abspath(os.path.dirname(THIS_DIR))

# //build imports.
BUILD_DIR = os.path.join(SRC_DIR, 'build')
sys.path.insert(0, BUILD_DIR)
import find_depot_tools


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('-o',
                      '--out',
                      required=True,
                      help='path to write location tag metadata to')
  args = parser.parse_args()

  logging.basicConfig(level=logging.WARNING)

  # ".git" is a directory in full checkouts, but a file in work trees.
  if not os.path.exists(os.path.join(SRC_DIR, '.git')):
    logging.warning('Skip generating location tags because the script is not '
                    'running in a git repository')
    return 0

  exe = os.path.join(find_depot_tools.DEPOT_TOOLS_PATH, 'dirmd')
  if sys.platform == 'win32':
    exe = exe + '.bat'

  return subprocess.call([
      exe,
      'location-tags',
      '-out',
      args.out,
      '-root',
      SRC_DIR,
      '-repo',
      'https://chromium.googlesource.com/chromium/src',
  ])


if __name__ == '__main__':
  sys.exit(main())
