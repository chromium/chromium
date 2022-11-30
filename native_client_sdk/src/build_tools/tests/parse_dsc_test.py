#!/usr/bin/env python
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import copy
import datetime
import os
import posixpath
import subprocess
import sys
import unittest

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
BUILD_TOOLS_DIR = os.path.dirname(SCRIPT_DIR)

sys.path.append(BUILD_TOOLS_DIR)
import generate_make
import parse_dsc

BASIC_DESC = {
  'TOOLS': ['clang-newlib', 'glibc'],
  'TARGETS': [
    {
      'NAME' : 'hello_world',
      'TYPE' : 'main',
      'SOURCES' : ['hello_world.c'],
    },
  ],
  'DEST' : 'examples/api'
}

class TestValidateFormat(unittest.TestCase):
  def _validate(self, src, expected_failure):
    try:
      parse_dsc.ValidateFormat(src, parse_dsc.DSC_FORMAT)
    except parse_dsc.ValidationError as e:
      if expected_failure:
        self.assertEqual(str(e), expected_failure)
        return
      raise

  def testGoodDesc(self):
    testdesc = copy.deepcopy(BASIC_DESC)
    self._validate(testdesc, None)

  def testMissingKey(self):
    testdesc = copy.deepcopy(BASIC_DESC)
    del testdesc['TOOLS']
    self._validate(testdesc, 'Missing required key TOOLS.')

    testdesc = copy.deepcopy(BASIC_DESC)
    del testdesc['TARGETS'][0]['NAME']
    self._validate(testdesc, 'Missing required key NAME.')

  def testNonEmpty(self):
    testdesc = copy.deepcopy(BASIC_DESC)
    testdesc['TOOLS'] = []
    self._validate(testdesc, 'Expected non-empty value for TOOLS.')

    testdesc = copy.deepcopy(BASIC_DESC)
    testdesc['TARGETS'] = []
    self._validate(testdesc, 'Expected non-empty value for TARGETS.')

    testdesc = copy.deepcopy(BASIC_DESC)
    testdesc['TARGETS'][0]['NAME'] = ''
    self._validate(testdesc, 'Expected non-empty value for NAME.')

  def testBadValue(self):
    testdesc = copy.deepcopy(BASIC_DESC)
    testdesc['TOOLS'] = ['clang-newlib', 'glibc', 'badtool']
    self._validate(testdesc, 'Value badtool not expected in TOOLS.')

  def testExpectStr(self):
    testdesc = copy.deepcopy(BASIC_DESC)
    testdesc['TOOLS'] = ['clang-newlib', True, 'glibc']
    self._validate(testdesc, 'Value True not expected in TOOLS.')

  def testExpectList(self):
    testdesc = copy.deepcopy(BASIC_DESC)
    testdesc['TOOLS'] = 'clang-newlib'
    self._validate(testdesc, 'Key TOOLS expects LIST not STR.')

# TODO(bradnelson):  Add test which generates a real make and runs it.

if __name__ == '__main__':
  unittest.main()
