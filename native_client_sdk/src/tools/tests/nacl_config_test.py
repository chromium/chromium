#!/usr/bin/env vpython3
# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
TOOLS_DIR = os.path.dirname(SCRIPT_DIR)
CHROME_SRC = os.path.dirname(os.path.dirname(os.path.dirname(TOOLS_DIR)))

import mock

# For nacl_config, the module under test
sys.path.append(TOOLS_DIR)
import nacl_config


class TestNaclConfig(unittest.TestCase):
  def setUp(self):
    self.patches = []

    get_sdk_path = self.AddAndStartPatch('getos.GetSDKPath')
    get_sdk_path.return_value = '/sdk_root'

    get_platform = self.AddAndStartPatch('getos.GetPlatform')
    get_platform.return_value = 'mac'

  def tearDown(self):
    for patch in self.patches:
      patch.stop()

  def AddAndStartPatch(self, name):
    patch = mock.patch(name)
    self.patches.append(patch)
    return patch.start()

  @mock.patch('nacl_config.GetCFlags')
  def testMainArgParsing(self, mock_get_cflags):
    mock_get_cflags.return_value = 'flags'
    with mock.patch('sys.stdout'):
      nacl_config.main(['--cflags'])
    mock_get_cflags.assert_called()

  def testCFlags(self):
    cases = {
        'glibc': '-I/sdk_root/include -I/sdk_root/include/glibc',
        'pnacl': '-I/sdk_root/include -I/sdk_root/include/pnacl',
        'win': '-I/sdk_root/include -I/sdk_root/include/win',
        'mac': '-I/sdk_root/include -I/sdk_root/include/mac',
        'linux': '-I/sdk_root/include -I/sdk_root/include/linux'
    }
    for toolchain, expected in cases.iteritems():
      self.assertEqual(expected, nacl_config.GetCFlags(toolchain))
    self.assertRaises(nacl_config.Error, nacl_config.GetCFlags, 'foo')

  def testIncludeDirs(self):
    cases = {
        'glibc': '/sdk_root/include /sdk_root/include/glibc',
        'pnacl': '/sdk_root/include /sdk_root/include/pnacl',
        'win': '/sdk_root/include /sdk_root/include/win',
        'mac': '/sdk_root/include /sdk_root/include/mac',
        'linux': '/sdk_root/include /sdk_root/include/linux'
    }
    for toolchain, expected in cases.iteritems():
      self.assertEqual(expected, nacl_config.GetIncludeDirs(toolchain))
    self.assertRaises(nacl_config.Error, nacl_config.GetIncludeDirs, 'foo')

  def testLDFlags(self):
    self.assertEqual('-L/sdk_root/lib', nacl_config.GetLDFlags())

  def _TestTool(self, tool, nacl_tool=None, pnacl_tool=None):
    nacl_tool = nacl_tool or tool
    pnacl_tool = pnacl_tool or tool

    cases = {
        ('glibc', 'arm'):
            '/sdk_root/toolchain/mac_arm_glibc/bin/arm-nacl-%s' % nacl_tool,
        ('glibc', 'x86_32'):
            '/sdk_root/toolchain/mac_x86_glibc/bin/i686-nacl-%s' % nacl_tool,
        ('glibc', 'x86_64'):
            '/sdk_root/toolchain/mac_x86_glibc/bin/x86_64-nacl-%s' % nacl_tool,

        'pnacl': '/sdk_root/toolchain/mac_pnacl/bin/pnacl-%s' % pnacl_tool,
        ('pnacl', 'pnacl'):
            '/sdk_root/toolchain/mac_pnacl/bin/pnacl-%s' % pnacl_tool,
    }

    for tc_arch, expected in cases.iteritems():
      if isinstance(tc_arch, tuple):
        toolchain = tc_arch[0]
        arch = tc_arch[1]
      else:
        toolchain = tc_arch
        arch = None
      self.assertEqual(expected, nacl_config.GetToolPath(toolchain, arch, tool))

    for toolchain in ('host', 'mac', 'win', 'linux'):
      self.assertRaises(nacl_config.Error,
                        nacl_config.GetToolPath, toolchain, None, tool)

    # Using toolchain=pnacl with any arch other than None, or 'pnacl' is an
    # error.
    for arch in ('x86_32', 'x86_64', 'arm', 'foobar'):
      self.assertRaises(nacl_config.Error,
                        nacl_config.GetToolPath, toolchain, arch, tool)

  def testCC(self):
    self._TestTool('cc', 'gcc', 'clang')

  def testCXX(self):
    self._TestTool('c++', 'g++', 'clang++')

  def testLD(self):
    self._TestTool('ld', 'g++', 'clang++')

  def testStandardTool(self):
    for tool in ('nm', 'strip', 'ar', 'ranlib'):
      self._TestTool(tool)

  def testGDB(self):
    # We always use the same gdb (it supports multiple toolchains/architectures)
    expected = '/sdk_root/toolchain/mac_x86_glibc/bin/x86_64-nacl-gdb'
    for toolchain in ('glibc', 'pnacl'):
      for arch in ('x86_32', 'x86_64', 'arm'):
        self.assertEqual(expected,
                         nacl_config.GetToolPath(toolchain, arch, 'gdb'))


if __name__ == '__main__':
  unittest.main()
