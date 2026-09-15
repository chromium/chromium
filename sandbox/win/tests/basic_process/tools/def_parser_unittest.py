#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import tempfile
import unittest

from def_parser import parse_def_exports


class DefParserTest(unittest.TestCase):

  def setUp(self):
    self._temp_dir = tempfile.TemporaryDirectory()
    self.addCleanup(self._temp_dir.cleanup)
    self.path = os.path.join(self._temp_dir.name, 'exports.def')

  def _write(self, contents):
    with open(self.path, 'w', encoding='utf-8', newline='') as output:
      output.write(contents)

  def test_parses_names_targets_and_ordinals(self):
    self._write("""\
LIBRARY sample.dll
EXPORTS
  Plain
  Forwarded = kernel32.Forwarded
  Ordinal = ws2_32.Ordinal @7 NONAME
  DataSymbol DATA
""")

    exports = parse_def_exports(self.path)

    self.assertEqual(
        [
            ('Plain', None, None),
            ('Forwarded', 'kernel32.Forwarded', None),
            ('Ordinal', 'ws2_32.Ordinal', 7),
            ('DataSymbol', None, None),
        ], list(exports))

  def test_requires_exports_section(self):
    self._write('LIBRARY sample.dll\n')

    with self.assertRaisesRegex(ValueError, 'missing EXPORTS section'):
      parse_def_exports(self.path)

  def test_rejects_invalid_export(self):
    self._write('EXPORTS\n  Name =\n')

    with self.assertRaisesRegex(ValueError, 'invalid export'):
      parse_def_exports(self.path)


if __name__ == '__main__':
  unittest.main()
