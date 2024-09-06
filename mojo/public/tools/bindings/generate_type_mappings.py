#!/usr/bin/env python
# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Generates a JSON typemap from its command-line arguments and dependencies.

Each typemap should be specified in an command-line argument of the form
key=value, with an argument of "--start-typemap" preceding each typemap.

For example,
generate_type_mappings.py --output=foo.typemap --start-typemap \\
    public_headers=foo.h traits_headers=foo_traits.h \\
    type_mappings=mojom.Foo=FooImpl

generates a foo.typemap containing
{
  "c++": {
    "mojom.Foo": {
      "typename": "FooImpl",
      "traits_headers": [
        "foo_traits.h"
      ],
      "public_headers": [
        "foo.h"
      ]
    }
  }
}

Then,
generate_type_mappings.py --dependency foo.typemap --output=bar.typemap \\
    --start-typemap public_headers=bar.h traits_headers=bar_traits.h \\
    type_mappings=mojom.Bar=BarImpl

generates a bar.typemap containing
{
  "c++": {
    "mojom.Bar": {
      "typename": "BarImpl",
      "traits_headers": [
        "bar_traits.h"
      ],
      "public_headers": [
        "bar.h"
      ]
    },
    "mojom.Foo": {
      "typename": "FooImpl",
      "traits_headers": [
        "foo_traits.h"
      ],
      "public_headers": [
        "foo.h"
      ]
    }
  }
}
"""

import argparse
import json
import os
import re
import sys

sys.path.insert(
    0,
    os.path.join(
        os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "mojom"))

from mojom.generate.generator import WriteFile

def ReadTypemap(path):
  with open(path) as f:
    return json.load(f)['c++']


def LoadCppTypemapConfig(path):
  configs = {}
  with open(path) as f:
    for config in json.load(f):
      for entry in config['types']:
        configs[entry['mojom']] = {
            'typename': entry['cpp'],
            'forward_declaration': entry.get('forward_declaration', None),
            'public_headers': config.get('traits_headers', []),
            'traits_headers': config.get('traits_private_headers', []),
            'copyable_pass_by_value': entry.get('copyable_pass_by_value',
                                                False),
            'default_constructible': entry.get('default_constructible', True),
            'force_serialize': entry.get('force_serialize', False),
            'hashable': entry.get('hashable', False),
            'move_only': entry.get('move_only', False),
            'non_const_ref': entry.get('non_const_ref', False),
            'nullable_is_same_type': entry.get('nullable_is_same_type', False),
            'non_copyable_non_movable': False,
        }
  return configs


def LoadTsTypemapConfig(path):
  configs = {}
  with open(path) as f:
    for config in json.load(f):
      for entry in config['types']:
        configs[entry['mojom']] = {
            'typename': entry['ts'],
            'type_import': entry.get('ts_import', None),
            'converter_import': entry['import'],
            'converter': entry['converter'],
        }
  return configs


def main():
  parser = argparse.ArgumentParser(
      description=__doc__,
      formatter_class=argparse.RawDescriptionHelpFormatter)
  parser.add_argument(
      '--dependency',
      type=str,
      action='append',
      default=[],
      help=('A path to another JSON typemap to merge into the output. '
            'This may be repeated to merge multiple typemaps.'))
  parser.add_argument(
      '--cpp-typemap-config',
      type=str,
      action='store',
      dest='cpp_config_path',
      help=('A path to a single JSON-formatted typemap config as emitted by'
            'GN when processing a mojom_cpp_typemap build rule.'))
  parser.add_argument(
      '--ts-typemap-config',
      type=str,
      action='store',
      dest='ts_config_path',
      help=('A path to a single JSON-formatted typemap config as emitted by'
            'GN when processing a mojom_ts_typemap build rule.'))
  parser.add_argument('--output',
                      type=str,
                      required=True,
                      help='The path to which to write the generated JSON.')
  params, _ = parser.parse_known_args()

  cpp_typemaps = {}
  if params.cpp_config_path:
    cpp_typemaps = LoadCppTypemapConfig(params.cpp_config_path)
  missing = [path for path in params.dependency if not os.path.exists(path)]
  if missing:
    raise IOError('Missing dependencies: %s' % ', '.join(missing))
  for path in params.dependency:
    cpp_typemaps.update(ReadTypemap(path))

  ts_typemaps = {}
  if params.ts_config_path:
    ts_typemaps = LoadTsTypemapConfig(params.ts_config_path)

  WriteFile(
      json.dumps({
          'c++': cpp_typemaps,
          'typescript': ts_typemaps
      }, indent=2), params.output)


if __name__ == '__main__':
  main()
