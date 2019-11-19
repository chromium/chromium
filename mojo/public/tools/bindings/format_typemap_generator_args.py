#!/usr/bin/env python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import sys

# This utility converts mojom dependencies into their corresponding typemap
# paths and formats them to be consumed by generate_type_mappings.py.


def FormatTypemap(typemap_filename):
  # A simple typemap is valid Python with a minor alteration.
  with open(typemap_filename) as f:
    typemap_content = f.read().replace('=\n', '=')
  typemap = {}
  exec typemap_content in typemap

  for header in typemap.get('public_headers', []):
    yield 'public_headers=%s' % header
  for header in typemap.get('traits_headers', []):
    yield 'traits_headers=%s' % header
  for header in typemap.get('type_mappings', []):
    yield 'type_mappings=%s' % header


def main():
  typemaps = sys.argv[1:]
  print(' '.join('--start-typemap %s' % ' '.join(FormatTypemap(typemap))
                 for typemap in typemaps))


if __name__ == '__main__':
  sys.exit(main())
