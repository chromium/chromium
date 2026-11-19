#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest

import PRESUBMIT

sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
from PRESUBMIT_test_mocks import MockAffectedFile, MockInputApi, MockOutputApi


class _NetMockInputApi(MockInputApi):
  """MockInputApi with HasAffectedFiles support for //net presubmit tests."""

  def HasAffectedFiles(
    self, *, extensions=None, path=None, recursive=True, include_deletes=True
  ):
    if (extensions is None) == (path is None):
      raise ValueError(
        "Must specify exactly one of 'extensions' or 'path' to "
        'HasAffectedFiles().'
      )
    if extensions is None:
      raise NotImplementedError('path filtering is not mocked')
    if isinstance(extensions, str):
      extensions = (extensions,)
    normalized_exts = tuple(
      ext.lower() if ext.startswith('.') else f'.{ext.lower()}'
      for ext in extensions
    )
    for f in self.AffectedFiles(include_deletes=include_deletes):
      if f.LocalPath().lower().endswith(normalized_exts):
        return True
    return False


class CheckNoShortKeywordInNetTest(unittest.TestCase):
  def _CheckNoShortKeyword(self, filename, lines, action='M'):
    input_api = _NetMockInputApi()
    input_api.files = [MockAffectedFile(filename, lines, action=action)]
    return PRESUBMIT.CheckNoShortKeywordInNet(input_api, MockOutputApi())

  def testShortKeywordInSourceAndHeader(self):
    for filename in ('net/foo.cc', 'net/foo.h'):
      errors = self._CheckNoShortKeyword(
        filename,
        [
          '  short x = 0;',
          '  unsigned short y = 1;',
          '  short int z = 2;',
        ],
      )
      self.assertEqual(1, len(errors), filename)
      self.assertIn('Do not use the "short" keyword', errors[0].message)
      self.assertEqual(
        [
          f'{filename}:1\n    short x = 0;',
          f'{filename}:2\n    unsigned short y = 1;',
          f'{filename}:3\n    short int z = 2;',
        ],
        errors[0].items,
      )

  def testNonNetFileIgnored(self):
    errors = self._CheckNoShortKeyword('base/foo.cc', ['short x = 0;'])
    self.assertEqual(0, len(errors))

  def testNonCppFilesIgnored(self):
    for filename in ('net/foo.py', 'net/BUILD.gn', 'net/README.md'):
      errors = self._CheckNoShortKeyword(filename, ['short x = 0;'])
      self.assertEqual(0, len(errors), filename)

  def testDeletedFileIgnored(self):
    errors = self._CheckNoShortKeyword(
      'net/foo.cc', ['short x = 0;'], action='D'
    )
    self.assertEqual(0, len(errors))

  def testPlatformSpecificFilesAllowed(self):
    for filename in (
      'net/foo_win.cc',
      'net/foo_linux.h',
      'net/foo_posix.cc',
      'net/win/foo.cc',
      'net/base/android/foo.h',
    ):
      errors = self._CheckNoShortKeyword(filename, ['short x = 0;'])
      self.assertEqual(0, len(errors), filename)

  def testIdentifiersContainingShortAllowed(self):
    errors = self._CheckNoShortKeyword(
      'net/foo.cc',
      [
        'int short_name = 0;',
        'int my_short = 1;',
        'int shortest = 2;',
      ],
    )
    self.assertEqual(0, len(errors))

  def testShortInStringLiteralAllowed(self):
    errors = self._CheckNoShortKeyword(
      'net/foo.cc',
      [
        'const char* s1 = "short";',
        'const char* s2 = "a \\"short\\" word";',
      ],
    )
    self.assertEqual(0, len(errors))

  def testShortAfterStringLiteralError(self):
    errors = self._CheckNoShortKeyword(
      'net/foo.cc', ['const char* s = "ok"; short x = 0;']
    )
    self.assertEqual(1, len(errors))
    self.assertEqual(1, len(errors[0].items))

  def testShortInCppCommentAllowed(self):
    errors = self._CheckNoShortKeyword(
      'net/foo.cc', ['int x = 0;  // short comment']
    )
    self.assertEqual(0, len(errors))

  def testShortBeforeCppCommentError(self):
    errors = self._CheckNoShortKeyword(
      'net/foo.cc', ['short x = 0;  // comment']
    )
    self.assertEqual(1, len(errors))
    self.assertEqual(1, len(errors[0].items))

  def testShortInCCommentAllowed(self):
    errors = self._CheckNoShortKeyword(
      'net/foo.cc',
      [
        '/* short */ int x = 0;',
        '/* short */ int y = /* short */ 1;',
      ],
    )
    self.assertEqual(0, len(errors))

  def testShortOutsideCCommentError(self):
    errors = self._CheckNoShortKeyword(
      'net/foo.cc',
      [
        '/* ok */ short x = 0;',
        'foo/* */short y = 0;',
      ],
    )
    self.assertEqual(1, len(errors))
    self.assertEqual(2, len(errors[0].items))

  def testCommentMarkersInsideStringDoNotHideShort(self):
    errors = self._CheckNoShortKeyword(
      'net/foo.cc',
      [
        'const char* s1 = "http://example.com"; short x = 0;',
        'const char* s2 = "/*"; short y = 0; // */',
      ],
    )
    self.assertEqual(1, len(errors))
    self.assertEqual(2, len(errors[0].items))


if __name__ == '__main__':
  unittest.main()
