#!/usr/bin/env python
# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
BUILD_TOOLS_DIR = os.path.dirname(SCRIPT_DIR)

sys.path.append(BUILD_TOOLS_DIR)
import verify_filelist


def Verify(platform, rules_contents, directory_list):
  rules = verify_filelist.Rules('test', platform, rules_contents)
  rules.VerifyDirectoryList(directory_list)


class VerifyFilelistTestCase(unittest.TestCase):
  def testBasic(self):
    rules = """\
foo/file1
foo/file2
foo/file3
bar/baz/other
"""
    dirlist = ['foo/file1', 'foo/file2', 'foo/file3', 'bar/baz/other']
    Verify('linux', rules, dirlist)

  def testGlob(self):
    rules = 'foo/*'
    dirlist = ['foo/file1', 'foo/file2', 'foo/file3/and/subdir']
    Verify('linux', rules, dirlist)

  def testPlatformVar(self):
    rules = 'dir/${PLATFORM}/blah'
    dirlist = ['dir/linux/blah']
    Verify('linux', rules, dirlist)

  def testPlatformVarGlob(self):
    rules = 'dir/${PLATFORM}/*'
    dirlist = ['dir/linux/file1', 'dir/linux/file2']
    Verify('linux', rules, dirlist)

  def testPlatformRule(self):
    rules = """\
[linux]dir/linux/only
all/platforms
"""
    linux_dirlist = ['dir/linux/only', 'all/platforms']
    other_dirlist = ['all/platforms']
    Verify('linux', rules, linux_dirlist)
    Verify('mac', rules, other_dirlist)

  def testMultiPlatformRule(self):
    rules = """\
[linux,win]dir/no/macs
all/platforms
"""
    nonmac_dirlist = ['dir/no/macs', 'all/platforms']
    mac_dirlist = ['all/platforms']
    Verify('linux', rules, nonmac_dirlist)
    Verify('win', rules, nonmac_dirlist)
    Verify('mac', rules, mac_dirlist)

  def testPlatformRuleBadPlatform(self):
    rules = '[frob]bad/platform'
    self.assertRaises(verify_filelist.ParseException, Verify,
                      'linux', rules, [])

  def testMissingFile(self):
    rules = """\
foo/file1
foo/missing
"""
    dirlist = ['foo/file1']
    self.assertRaises(verify_filelist.VerifyException, Verify,
                      'linux', rules, dirlist)

  def testExtraFile(self):
    rules = 'foo/file1'
    dirlist = ['foo/file1', 'foo/extra_file']
    self.assertRaises(verify_filelist.VerifyException, Verify,
                      'linux', rules, dirlist)

  def testEmptyGlob(self):
    rules = 'foo/*'
    dirlist = ['foo']  # Directory existing is not enough!
    self.assertRaises(verify_filelist.VerifyException, Verify,
                      'linux', rules, dirlist)

  def testBadGlob(self):
    rules = '*/foo/bar'
    dirlist = []
    self.assertRaises(verify_filelist.ParseException, Verify,
                      'linux', rules, dirlist)

  def testUnknownPlatform(self):
    rules = 'foo'
    dirlist = ['foo']
    for platform in ('linux', 'mac', 'win'):
      Verify(platform, rules, dirlist)
    self.assertRaises(verify_filelist.ParseException, Verify,
                      'foobar', rules, dirlist)

  def testUnexpectedPlatformFile(self):
    rules = '[mac,win]foo/file1'
    dirlist = ['foo/file1']
    self.assertRaises(verify_filelist.VerifyException, Verify,
                      'linux', rules, dirlist)

  def testWindowsPaths(self):
    if os.path.sep != '/':
      rules = 'foo/bar/baz'
      dirlist = ['foo\\bar\\baz']
      Verify('win', rules, dirlist)
    else:
      rules = 'foo/bar/baz\\foo'
      dirlist = ['foo/bar/baz\\foo']
      Verify('linux', rules, dirlist)

  def testNestedGlobs(self):
    rules = """\
foo/*
foo/bar/*"""
    dirlist = ['foo/file', 'foo/bar/file']
    Verify('linux', rules, dirlist)

    rules = """\
foo/bar/*
foo/*"""
    dirlist = ['foo/file', 'foo/bar/file']
    Verify('linux', rules, dirlist)


if __name__ == '__main__':
  unittest.main()
