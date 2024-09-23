#!/usr/bin/env python
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import json
import os
import re
import sys


def CheckCppTypemapConfigs(target_name, config_filename, out_filename):
  _SUPPORTED_CONFIG_KEYS = set([
      'types', 'traits_headers', 'traits_private_headers', 'traits_sources',
      'traits_deps', 'traits_public_deps'
  ])
  _SUPPORTED_TYPE_KEYS = set([
      'mojom', 'cpp', 'copyable_pass_by_value', 'force_serialize', 'hashable',
      'move_only', 'non_const_ref', 'nullable_is_same_type',
      'forward_declaration', 'default_constructible'
  ])
  with open(config_filename, 'r') as f:
    for config in json.load(f):
      for key in config.keys():
        if key not in _SUPPORTED_CONFIG_KEYS:
          raise ValueError('Invalid typemap property "%s" when processing %s' %
                           (key, target_name))

      types = config.get('types')
      if not types:
        raise ValueError('Typemap for %s must specify at least one type to map'
                         % target_name)

      for entry in types:
        for key in entry.keys():
          if key not in _SUPPORTED_TYPE_KEYS:
            raise IOError(
                'Invalid type property "%s" in typemap for "%s" on target %s' %
                (key, entry.get('mojom', '(unknown)'), target_name))

  with open(out_filename, 'w') as f:
    f.truncate(0)


def main():
  parser = argparse.ArgumentParser()
  _, args = parser.parse_known_args()
  if len(args) != 3:
    print('Usage: validate_typemap_config.py target_name config_filename '
          'stamp_filename')
    sys.exit(1)

  CheckCppTypemapConfigs(args[0], args[1], args[2])


if __name__ == '__main__':
  main()
