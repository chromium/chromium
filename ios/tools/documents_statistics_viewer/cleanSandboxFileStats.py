#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Removes the file and directory names of Downloaded items from within a
Sandbox File Statistics json output file.
'''

import argparse
import json
import os
import sys

def clean(stats_json):
  for item in stats_json:
    for attribute, value in item.items():
      if attribute == 'name':
        item['name'] = '##DOWNLOADED_ITEM##'
      elif attribute == 'contents':
        clean(item['contents'])

def main():
  description = 'Removes file names in downloads directory stats.'
  parser = argparse.ArgumentParser(description=description)

  parser.add_argument('stats_json_path', nargs=1,
                      help='path to file statistics json file')

  options, extra_options = parser.parse_known_args()
  if len(extra_options):
    print >> sys.stderr, 'Unknown options: ', extra_options
    return 1

  stats_json_path = options.stats_json_path[0]

  if not os.path.isfile(stats_json_path):
    print('The input file does not exist: ' + stats_json_path, file=sys.stderr)
    return 1

  stats_json_splitext = os.path.splitext(stats_json_path)
  out_json_path = stats_json_splitext[0] + '_clean' + stats_json_splitext[1]

  if os.path.exists(out_json_path):
    print('The output file already exists: ' + out_json_path, file=sys.stderr)
    return 1

  stats_json = None
  with open(stats_json_path, 'r') as json_it:
    stats_json = json.load(json_it)
    for item in stats_json['contents']:
      if item['name'] == 'Documents':
        clean(item['contents'])

  with open(out_json_path, 'x') as out_file:
    json.dump(stats_json, out_file)

  return 0

if __name__ == '__main__':
  sys.exit(main())
