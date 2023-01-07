#!/usr/bin/env vpython3
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import shutil
import sys
import tempfile
import unittest

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
TOOLS_DIR = os.path.dirname(SCRIPT_DIR)
CHROME_SRC = os.path.dirname(os.path.dirname(os.path.dirname(TOOLS_DIR)))

import mock

# For getos, the module under test
sys.path.append(TOOLS_DIR)
import getos
import oshelpers


class TestCaseExtended(unittest.TestCase):
  """Monkey patch some 2.7-only TestCase features."""
  # TODO(sbc): remove this once we switch to python2.7 everywhere
  def assertIn(self, expr1, expr2, msg=None):
    if hasattr(super(TestCaseExtended, self), 'assertIn'):
      return super(TestCaseExtended, self).assertIn(expr1, expr2, msg)
    if expr1 not in expr2:
      self.fail(msg or '%r not in %r' % (expr1, expr2))


class TestGetos(TestCaseExtended):
  def setUp(self):
    self.patch1 = mock.patch.dict('os.environ',
                                  {'NACL_SDK_ROOT': os.path.dirname(TOOLS_DIR)})
    self.patch1.start()
    self.patch2 = mock.patch.object(oshelpers, 'FindExeInPath',
                                    return_value='/bin/ls')
    self.patch2.start()

  def tearDown(self):
    self.patch1.stop()
    self.patch2.stop()

  def testGetSDKPath(self):
    """honors environment variable."""
    with mock.patch.dict('os.environ', {'NACL_SDK_ROOT': 'dummy'}):
      self.assertEqual(getos.GetSDKPath(), 'dummy')

  def testGetSDKPathDefault(self):
    """defaults to relative path."""
    del os.environ['NACL_SDK_ROOT']
    self.assertEqual(getos.GetSDKPath(), os.path.dirname(TOOLS_DIR))

  def testGetPlatform(self):
    """returns a valid platform."""
    platform = getos.GetPlatform()
    self.assertIn(platform, ('mac', 'linux', 'win'))

  def testGetSystemArch(self):
    """returns a valid architecture."""
    arch = getos.GetSystemArch(getos.GetPlatform())
    self.assertIn(arch, ('x86_64', 'x86_32', 'arm'))

  def testGetChromePathEnv(self):
    """honors CHROME_PATH environment."""
    with mock.patch.dict('os.environ', {'CHROME_PATH': '/dummy/file'}):
      expect = "Invalid CHROME_PATH.*/dummy/file"
      platform = getos.GetPlatform()
      if hasattr(self, 'assertRaisesRegexp'):
        with self.assertRaisesRegexp(getos.Error, expect):
          getos.GetChromePath(platform)
      else:
        # TODO(sbc): remove this path once we switch to python2.7 everywhere
        self.assertRaises(getos.Error, getos.GetChromePath, platform)

  def testGetChromePathCheckExists(self):
    """checks that existence of explicitly CHROME_PATH is checked."""
    mock_location = '/bin/ls'
    platform = getos.GetPlatform()
    if platform == 'win':
      mock_location = 'c:\\nowhere'
    with mock.patch.dict('os.environ', {'CHROME_PATH': mock_location}):
      with mock.patch('os.path.exists') as mock_exists:
        chrome = getos.GetChromePath(platform)
        self.assertEqual(chrome, mock_location)
        mock_exists.assert_called_with(chrome)

  def testGetNaClArch(self):
    """returns a valid architecture."""
    platform = getos.GetPlatform()
    # Since the unix implementation of GetNaClArch will run objdump on the
    # chrome binary, and we want to be able to run this test without chrome
    # installed we mock the GetChromePath call to return a known system binary,
    # which objdump will work with.
    with mock.patch('getos.GetChromePath') as mock_chrome_path:
      mock_chrome_path.return_value = '/bin/ls'
      arch = getos.GetNaClArch(platform)
      self.assertIn(arch, ('x86_64', 'x86_32', 'arm'))

  def testMainInvalidArgs(self):
    with self.assertRaises(SystemExit):
      with mock.patch('sys.stderr'):
        getos.main('--foo')

  @mock.patch('sys.stdout', mock.Mock())
  @mock.patch('getos.GetPlatform')
  def testMainNoArgs(self, mock_get_platform):
    mock_get_platform.return_value = 'platform'
    getos.main([])

  @mock.patch('sys.stdout', mock.Mock())
  @mock.patch('getos.GetSystemArch')
  def testMainArgsParsing(self, mock_system_arch):
    mock_system_arch.return_value = 'dummy'
    getos.main(['--arch'])
    mock_system_arch.assert_called()


class TestGetosWithTempdir(TestCaseExtended):
  def setUp(self):
    self.tempdir = tempfile.mkdtemp("_sdktest")
    self.patch = mock.patch.dict('os.environ',
                                  {'NACL_SDK_ROOT': self.tempdir})
    self.patch.start()

  def tearDown(self):
    shutil.rmtree(self.tempdir)
    self.patch.stop()

  def testGetSDKVersion(self):
    """correctly parses README to find SDK version."""
    expected_version = (16, 100, 'f00baacabba6e-refs/heads/master@{#100}')
    with open(os.path.join(self.tempdir, 'README'), 'w') as out:
      out.write('Version: %d\n' % expected_version[0])
      out.write('Chrome Revision: %d\n' % expected_version[1])
      out.write('Chrome Commit Position: %s\n' % expected_version[2])

    version = getos.GetSDKVersion()
    self.assertEqual(version, expected_version)

  def testParseVersion(self):
    """correctly parses a version given to --check-version."""
    check_version_string = '15.100'
    self.assertEquals((15, 100), getos.ParseVersion(check_version_string))

  def testCheckVersion(self):
    """correctly rejects SDK versions earlier than the required one."""
    actual_version = (16, 100, 'f00baacabba6e-refs/heads/master@{#100}')
    with open(os.path.join(self.tempdir, 'README'), 'w') as out:
      out.write('Version: %d\n' % actual_version[0])
      out.write('Chrome Revision: %d\n' % actual_version[1])
      out.write('Chrome Commit Position: %s\n' % actual_version[2])

    required_version = (15, 150)
    getos.CheckVersion(required_version)

    required_version = (16, 99)
    getos.CheckVersion(required_version)

    required_version = (16, 100)
    getos.CheckVersion(required_version)

    required_version = (16, 101)
    self.assertRaisesRegexp(
        getos.Error,
        r'SDK version too old \(current: 16.100, required: 16.101\)',
        getos.CheckVersion,
        required_version)

    required_version = (17, 50)
    self.assertRaisesRegexp(
        getos.Error,
        r'SDK version too old \(current: 16.100, required: 17.50\)',
        getos.CheckVersion,
        required_version)


if __name__ == '__main__':
  unittest.main()
