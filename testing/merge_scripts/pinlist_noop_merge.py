#!/usr/bin/env python
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import shutil
import sys

import merge_api
from noop_merge import noop_merge


def main(raw_args):
  parser = merge_api.ArgumentParser()
  parser.add_argument('--pinlist-dir',
                      required=True,
                      help='where to store the merged data')
  args = parser.parse_args(raw_args)

  rc = noop_merge(args.output_json, args.jsons_to_merge)
  if rc != 0:
    return rc

  pinlists = []
  for dir_path, _sub_dirs, file_names in os.walk(args.task_output_dir):
    for fn in file_names:
      if fn == 'pinlist.meta':
        pinlists.append(os.path.join(dir_path, fn))
  assert len(pinlists) == 1, f'Zero or more than one pinlist found: {pinlists}'

  src = pinlists[0]
  dst = os.path.join(args.pinlist_dir, 'pinlist.meta')
  shutil.copyfile(src, dst)

  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
