#!/usr/bin/env vpython3
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import sys
import tempfile
import unittest

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PARENT_DIR = os.path.dirname(SCRIPT_DIR)
DATA_DIR = os.path.join(SCRIPT_DIR, 'data')
CHROME_SRC = os.path.dirname(os.path.dirname(os.path.dirname(PARENT_DIR)))

sys.path.append(PARENT_DIR)

import fix_deps
import mock


class TestFixDeps(unittest.TestCase):
  def setUp(self):
    self.tempfile = None

  def tearDown(self):
    if self.tempfile:
      os.remove(self.tempfile)

  def testRequiresFile(self):
    with mock.patch('sys.stderr'):
      self.assertRaises(SystemExit, fix_deps.main, [])

  def testInvalidOption(self):
    with mock.patch('sys.stderr'):
      self.assertRaises(SystemExit, fix_deps.main, ['--foo', 'bar'])

  def testMissingFile(self):
    with mock.patch('sys.stderr'):
      self.assertRaises(fix_deps.Error, fix_deps.main, ['nonexistent.file'])

  def testAddsDeps(self):
    self.tempfile = tempfile.mktemp("_sdktest")
    with open(self.tempfile, 'w') as out:
      out.write('foo.o: foo.c foo.h bar.h\n')
    fix_deps.FixupDepFile(self.tempfile)
    with open(self.tempfile) as infile:
      contents = infile.read()
    lines = contents.splitlines()
    self.assertEqual(len(lines), 5)
    self.assertTrue('foo.c:' in lines)
    self.assertTrue('foo.h:' in lines)
    self.assertTrue('bar.h:' in lines)

  def testSpacesInFilenames(self):
    self.tempfile = tempfile.mktemp("_sdktest")
    with open(self.tempfile, 'w') as out:
      out.write('foo.o: foo\\ bar.h\n')
    fix_deps.FixupDepFile(self.tempfile)
    with open(self.tempfile) as infile:
      contents = infile.read()
    lines = contents.splitlines()
    self.assertEqual(len(lines), 3)
    self.assertEqual(lines[2], 'foo\\ bar.h:')

  def testColonInFilename(self):
    self.tempfile = tempfile.mktemp("_sdktest")
    with open(self.tempfile, 'w') as out:
      out.write('foo.o: c:foo.c\\\n c:bar.h\n')
    fix_deps.FixupDepFile(self.tempfile)
    with open(self.tempfile) as infile:
      contents = infile.read()
    lines = contents.splitlines()
    self.assertEqual(len(lines), 5)
    self.assertEqual(lines[3], 'c:foo.c:')
    self.assertEqual(lines[4], 'c:bar.h:')

  def testDoubleInvoke(self):
    self.tempfile = tempfile.mktemp("_sdktest")
    with open(self.tempfile, 'w') as out:
      out.write('foo.o: foo\\ bar.h\n')
    fix_deps.FixupDepFile(self.tempfile)
    self.assertRaises(fix_deps.Error, fix_deps.FixupDepFile, self.tempfile)


if __name__ == '__main__':
  unittest.main()
