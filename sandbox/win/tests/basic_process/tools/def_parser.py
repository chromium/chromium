#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Shared parser for module-definition exports."""

import collections
import re


Export = collections.namedtuple('Export', ('name', 'target', 'ordinal'))

_EXPORT_RE = re.compile(
    r'^(\S+)(?:\s*=\s*(\S+))?(?:\s+@(\d+))?'
    r'(?:\s+(?:DATA|NONAME|PRIVATE))*$')


def parse_def_exports(path):
  """Returns the ordered exports from a module-definition file."""
  exports = []
  in_exports = False
  with open(path, 'r', encoding='utf-8') as source:
    for line_number, line in enumerate(source, start=1):
      stripped = line.split(';', 1)[0].strip()
      if stripped.upper() == 'EXPORTS':
        in_exports = True
        continue
      if not in_exports or not stripped:
        continue
      match = _EXPORT_RE.match(stripped)
      if not match:
        raise ValueError(f'{path}:{line_number}: invalid export: {stripped}')
      exports.append(
          Export(match.group(1), match.group(2),
                 int(match.group(3)) if match.group(3) else None))
  if not in_exports:
    raise ValueError(f'{path}: missing EXPORTS section')
  return exports
