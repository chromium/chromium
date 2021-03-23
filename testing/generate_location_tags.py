#!/usr/bin/env python
# Copyright (c) 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Generates the directory->tags mapping used by ResultDB."""

# pylint: disable=line-too-long
#
# For more on the tags, see
# https://source.chromium.org/chromium/infra/infra/+/master:go/src/go.chromium.org/luci/resultdb/sink/proto/v1/location_tag.proto
#
# pylint: enable=line-too-long

from __future__ import print_function

import argparse
import os
import subprocess
import sys

THIS_DIR = os.path.dirname(__file__)
SRC_DIR = os.path.dirname(THIS_DIR)
BUILD_DIR = os.path.join(SRC_DIR, 'build')
sys.path.insert(0, BUILD_DIR)
import find_depot_tools

def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('-o', '--out', required=True,
                      help='path to write location tag metadata to')
  args = parser.parse_args()

  if sys.platform == 'win32' and sys.getwindowsversion().major < 6:
    # dirmd isn't working on Win7, but the only Win7 bots we have are
    # testers, which don't need to generate the location metadata.
    return 0

  exe = os.path.join(find_depot_tools.DEPOT_TOOLS_PATH, 'dirmd')
  if sys.platform == 'win32':
    exe = exe + '.bat'

  return subprocess.call([
      exe,
      'location-tags',
      '-out', args.out,
      '-root', SRC_DIR,
      '-repo', 'https://chromium.googlesource.com/chromium/src',
      ])


if __name__ == '__main__':
  sys.exit(main())
