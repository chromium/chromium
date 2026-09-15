#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Verify that exported Apifw thunks are reachable from exports_logging.def."""

import argparse
import re
import sys

from def_parser import parse_def_exports


def parse_export_targets(def_path):
  return {
      export.target for export in parse_def_exports(def_path)
      if export.target and re.fullmatch(r'Apifw\w+', export.target)
  }


def parse_stub_exports(source_path):
  with open(source_path, 'r', encoding='utf-8') as f:
    text = f.read()
  exports = set(
      re.findall(r'\bBASIC_STUB_EXPORT\b[\s\S]*?\b(Apifw\w+)\s*\(', text))
  exports.update(
      'Apifw' + match
      for match in re.findall(r'\bAPIFW_FORWARD_\w+\s*\(\s*(\w+)', text))
  return exports


def main():
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('--logging-def', required=True,
                      help='Path to shim/exports_logging.def')
  parser.add_argument('--source', action='append', required=True,
                      help='Thunk source file to check')
  parser.add_argument('--stamp', help='Stamp file written on success')
  args = parser.parse_args()

  targets = parse_export_targets(args.logging_def)
  failed = False
  for source in args.source:
    unused = sorted(parse_stub_exports(source) - targets)
    if unused:
      failed = True
      sys.stderr.write(f'{source} defines thunks not exported by '
                       f'{args.logging_def}:\n')
      for name in unused:
        sys.stderr.write(f'  {name}\n')

  if failed:
    return 1
  if args.stamp:
    with open(args.stamp, 'w', encoding='utf-8') as f:
      f.write('ok\n')
  return 0


if __name__ == '__main__':
  sys.exit(main())
