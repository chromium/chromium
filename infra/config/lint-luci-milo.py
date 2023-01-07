#!/usr/bin/env python3
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Checks that the main console and subconsole configs are consistent."""

import collections
import difflib
import os
import sys

THIS_DIR = os.path.dirname(os.path.abspath(__file__))
SRC_ROOT = os.path.join(THIS_DIR, '..', '..')
sys.path.insert(1, os.path.join(
    SRC_ROOT, "third_party", "protobuf", "python"))

import google.protobuf.text_format
import project_pb2


def compare_builders(name, main_builders, sub_builders):
  # Checks that the builders on a subwaterfall on the main waterfall
  # are consistent with the builders on that subwaterfall's main page.
  # For example, checks that the builders on the "chromium.win" section
  # are the same as on the dedicated standalone chromium.win waterfall.
  def to_list(builders, category_prefix=''):
    desc_list = []
    for builder in builders:
      desc_list.append('name: ' + ', '.join(builder.name))
      # A bot with "chromium.win|foo|bar" on the main waterfall should have
      # a category of "foo|bar" on the "chromium.win" subwaterfall.
      category = builder.category
      if category_prefix:
        if category:
          category = category_prefix + '|' + category
        else:
          category = category_prefix
      desc_list.append('category: ' + category)
      desc_list.append('short_name: ' + builder.short_name)
    return desc_list
  main_desc = to_list(main_builders)
  sub_desc = to_list(sub_builders, name)

  if main_desc != sub_desc:
    print('bot lists different between main waterfall ' +
          'and stand-alone %s waterfall:' % name)
    print('\n'.join(
        difflib.unified_diff(main_desc,
                             sub_desc,
                             fromfile='main',
                             tofile=name,
                             lineterm='')))
    print('')
    return False
  return True


def main():
  project = project_pb2.Project()
  with open(os.path.join(THIS_DIR, 'generated', 'luci', 'luci-milo.cfg'),
            'rb') as f:
    google.protobuf.text_format.Parse(f.read(), project,
                                      allow_unknown_field=True)

  # Maps subwaterfall name to list of builders on that subwaterfall
  # on the main waterfall.
  subwaterfalls = collections.defaultdict(list)
  for console in project.consoles:
    if console.id == 'main':
      # Chromium main waterfall console.
      for builder in console.builders:
        subwaterfall = builder.category.split('|', 1)[0]
        subwaterfalls[subwaterfall].append(builder)

  # subwaterfalls contains the waterfalls referenced by the main console
  # Check that every referenced subwaterfall has its own console, unless it's
  # explicitly excluded below.
  excluded_names = [
      # This is the chrome console in src-internal.
      'chrome',
  ]
  all_console_names = [console.id for console in project.consoles]
  referenced_names = set(subwaterfalls.keys())
  missing_names = referenced_names - set(all_console_names + excluded_names)
  if missing_names:
    print('Missing subwaterfall console for', missing_names)
    return 1

  # Check that the bots on a subwaterfall match the corresponding bots on the
  # main waterfall
  all_good = True
  for console in project.consoles:
    if console.id in subwaterfalls:
      if not compare_builders(console.id, subwaterfalls[console.id],
                              console.builders):
        all_good = False
  return 0 if all_good else 1


if __name__ == '__main__':
  sys.exit(main())
