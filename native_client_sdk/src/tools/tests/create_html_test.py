#!/usr/bin/env vpython3
# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import unittest
import shutil
import sys
import tempfile

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PARENT_DIR = os.path.dirname(SCRIPT_DIR)
CHROME_SRC = os.path.dirname(os.path.dirname(os.path.dirname(PARENT_DIR)))

sys.path.append(PARENT_DIR)

import create_html
import mock

class TestCreateHtml(unittest.TestCase):
  def setUp(self):
    self.tempdir = None

  def tearDown(self):
    if self.tempdir:
      shutil.rmtree(self.tempdir)

  def testBadInput(self):
    # Non-existent file
    self.assertRaises(create_html.Error, create_html.main, ['foo.nexe'])
    # Existing file with wrong extension
    self.assertRaises(create_html.Error, create_html.main, [__file__])
    # Existing directory
    self.assertRaises(create_html.Error, create_html.main, [PARENT_DIR])

  def testCreatesOutput(self):
    self.tempdir = tempfile.mkdtemp("_sdktest")
    expected_html = os.path.join(self.tempdir, 'foo.html')
    nmf_file = os.path.join(self.tempdir, 'foo.nmf')
    with mock.patch('sys.stdout'):
      with mock.patch('os.path.exists'):
        with mock.patch('os.path.isfile'):
          options = mock.MagicMock(return_value=False)
          options.output = None
          create_html.CreateHTML([nmf_file], options)
    # Assert that the file was created
    self.assertTrue(os.path.exists(expected_html))
    # Assert that nothing else was created
    self.assertEqual(os.listdir(self.tempdir),
                     [os.path.basename(expected_html)])


if __name__ == '__main__':
  unittest.main()
