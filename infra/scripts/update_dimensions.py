#!/usr/bin/env python
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# TODO(yyanagisawa): remove this script after the migration. (crbug.com/954450)
"""
Script to change bots dimensions from trusty to xenial (crbug.com/954450).
It also marks the builders to use builderless bots.

How to use:
  $ ./update_dimensions.py <cr-buildbucket.cfg> <builder name>
  $ git cl issue 0
  $ git cl upload

  e.g.
  $ ./update_dimensions.py cr-buildbucket.cfg android-marshmallow-arm64-rel
"""

import cStringIO as StringIO
import os
import re
import subprocess
import sys


BUILDERS_PATTERN = re.compile(r'\s*builders\s*{\s*')


def InsertMixin(builder_info, mixin_name):
  """Insert given mixin_name to info.

  Args:
    builder_info: contents of builder section.
    mixin_name: mixin name to be inserted.
  """
  for line in builder_info.splitlines():
    if 'mixin' in line:
      idx = line.find('mixin')
  builder_info += '%smixins: "%s"\n' % (' ' * idx, mixin_name)
  return builder_info


def UpdateConfig(config_filename, target_builder):
  """Update cr-buildbucket.cfg to use xenial instead of trusty.

  Args:
    config_filename: cr-buildbucket.cfg filename.
    target_builder: target builder name to update.
  """
  in_builders = False
  depth = 0
  builder_name_line = 'name: "%s"' % target_builder
  builder_info = ''
  should_replace = False

  out = StringIO.StringIO()

  with open(config_filename) as f:
    for line in f:
      if BUILDERS_PATTERN.match(line) and not '}' in line:
        in_builders = True
        depth = 1
      elif in_builders and '{' in line:
        depth += 1
      elif in_builders and '}' in line:
        depth -= 1
      if depth == 0:
        in_builders = False
        if should_replace:
          builder_info = InsertMixin(builder_info, 'builderless')
          out.write(builder_info.replace('Ubuntu-14.04', 'Ubuntu-16.04'))
        else:
          out.write(builder_info)
        builder_info = ''
        should_replace = False
      if in_builders:
        builder_info += line
      else:
        out.write(line)

      if in_builders and builder_name_line in line:
        should_replace = True
  with open(config_filename, 'w') as f:
    f.write(out.getvalue())


def main(argv):
  if len(argv) != 3:
    raise Exception('unexpected args')

  UpdateConfig(argv[1], argv[2])
  message = """switch trusty to xenial for %s CQ/CI builder

Bug: 954450""" % argv[2]
  subprocess.check_call(['git', 'add', os.path.basename(argv[1])],
                        cwd=os.path.dirname(os.path.abspath(argv[1])))
  subprocess.check_call(
      ['git', 'commit', '-m', message],
      cwd=os.path.dirname(os.path.abspath(argv[1])))

if __name__ == '__main__':
  main(sys.argv)
