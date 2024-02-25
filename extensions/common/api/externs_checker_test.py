#!/usr/bin/env python
# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest

from externs_checker import ExternsChecker

sys.path.append(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..'))

from PRESUBMIT_test_mocks import (MockInputApi, MockOutputApi, MockFile,
                                  MockChange)


class ExternsCheckerTest(unittest.TestCase):
  API_PAIRS = {'a': '1', 'b': '2', 'c': '3'}

  def _runChecks(self, files, exists=lambda f: True):
    input_api = MockInputApi()
    input_api.os_path.exists = exists
    input_api.files = [MockFile(f, '') for f in files]
    input_api.change = MockChange(input_api.files)
    output_api = MockOutputApi()
    checker = ExternsChecker(input_api, output_api, self.API_PAIRS)
    return checker.RunChecks()

  def testModifiedSourceWithoutModifiedExtern(self):
    results = self._runChecks(['b', 'test', 'random'])
    self.assertEquals(1, len(results))
    self.assertEquals(1, len(results[0].items))
    self.assertEquals('b', results[0].items[0])
    self.assertEquals(
        'To update the externs, run:\n'
        ' src/ $ python3 tools/json_schema_compiler/compiler.py b --root=. '
        '--generator=externs > 2',
        results[0].long_text)

  def testModifiedSourceWithModifiedExtern(self):
    results = self._runChecks(['b', '2', 'test', 'random'])
    self.assertEquals(0, len(results))

  def testModifiedMultipleSourcesWithNoModifiedExterns(self):
    results = self._runChecks(['b', 'test', 'c', 'random'])
    self.assertEquals(1, len(results))
    self.assertEquals(2, len(results[0].items))
    self.assertTrue('b' in results[0].items)
    self.assertTrue('c' in results[0].items)
    self.assertEquals(
        'To update the externs, run:\n'
        ' src/ $ python3 tools/json_schema_compiler/compiler.py <source_file> '
        '--root=. --generator=externs > <output_file>',
        results[0].long_text)

  def testModifiedMultipleSourcesWithOneModifiedExtern(self):
    results = self._runChecks(['b', 'test', 'c', 'random', '2'])
    self.assertEquals(1, len(results))
    self.assertEquals(1, len(results[0].items))
    self.assertEquals('c', results[0].items[0])

  def testApiFileDoesNotExist(self):
    exists = lambda f: f in ['a', 'b', 'c', '1', '2']
    with self.assertRaises(OSError) as e:
      self._runChecks(['a'], exists)
    self.assertEqual('Path Not Found: 3', str(e.exception))


if __name__ == '__main__':
  unittest.main()
