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

  exe = os.path.join(find_depot_tools.DEPOT_TOOLS_PATH, 'dirmd')
  if sys.platform == 'win32':
    exe = exe + '.bat'

  try:
    return subprocess.call([
        exe,
        'location-tags',
        '-out', args.out,
        '-root', SRC_DIR,
        '-repo', 'https://chromium.googlesource.com/chromium/src',
        ])
  except OSError as e:
    # TODO(crbug.com/1191087): Figure out what exactly is going on on Win7
    # and whether this should work, be a hard error, or can be safely
    # ignored.
    print('%s not found: %s' % (exe, repr(e)))
    if (e.errno == errno.ENOENT and sys.platform == 'win32'
            and sys.getwindowsversion().major < 10):
        pass
    raise


if __name__ == '__main__':
  sys.exit(main())
