#!/usr/bin/env vpython3
# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import ntpath
import posixpath
import sys
import collections
import unittest

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
BUILD_TOOLS_DIR = os.path.dirname(SCRIPT_DIR)
CHROME_SRC = os.path.dirname(os.path.dirname(os.path.dirname(BUILD_TOOLS_DIR)))

from mock import call, patch, Mock

sys.path.append(BUILD_TOOLS_DIR)
import build_artifacts


class BasePosixTestCase(unittest.TestCase):
  def setUp(self):
    self.addCleanup(patch.stopall)
    patch('build_artifacts.PLATFORM', 'posix').start()
    patch('build_artifacts.BUILD_ARCHIVE_DIR', '/archive_dir/').start()
    patch('os.path.join', posixpath.join).start()


class PosixTestCase(BasePosixTestCase):
  def setUp(self):
    BasePosixTestCase.setUp(self)

  def testGetToolchainNaClLib(self):
    tests = [
        (('glibc_x86', 'x86_32'), 'foo/x86_64-nacl/lib32'),
        (('glibc_x86', 'x86_64'), 'foo/x86_64-nacl/lib'),
        (('glibc_arm', 'arm'), 'foo/arm-nacl/lib'),
        (('pnacl', None), 'foo/le32-nacl/lib'),
    ]

    for test in tests:
      self.assertEqual(
        build_artifacts.GetToolchainNaClLib(test[0][0], 'foo', test[0][1]),
        test[1])

  def testGetGypBuiltLib(self):
    tests = [
        (('glibc_x86', 'x86_32'), 'foo/Release/gen/tc_glibc/lib32'),
        (('glibc_x86', 'x86_64'), 'foo/Release/gen/tc_glibc/lib64'),
        (('glibc_arm', 'arm'), 'foo/Release/gen/tc_glibc/libarm'),
        (('pnacl', None), 'foo/Release/gen/tc_pnacl_newlib/lib')
    ]

    for test in tests:
      self.assertEqual(
        build_artifacts.GetGypBuiltLib('foo', test[0][0], test[0][1]),
        test[1])

  def testGetGypToolchainLib(self):
    tests = [
        (('glibc_x86', 'x86_32'),
         'foo/Release/gen/sdk/posix_x86/nacl_x86_glibc/x86_64-nacl/lib32'),
        (('glibc_x86', 'x86_64'),
         'foo/Release/gen/sdk/posix_x86/nacl_x86_glibc/x86_64-nacl/lib'),
        (('glibc_arm', 'arm'),
         'foo/Release/gen/sdk/posix_x86/nacl_arm_glibc/arm-nacl/lib'),
        (('pnacl', None),
         'foo/Release/gen/sdk/posix_x86/pnacl_newlib/le32-nacl/lib'),
    ]

    for tc_info, expected in tests:
      self.assertEqual(
        build_artifacts.GetGypToolchainLib('foo', tc_info[0], tc_info[1]),
        expected)

  @patch('build_artifacts.all_archives', ['foo.tar.bz2', 'bar.tar.bz2'])
  @patch('build_version.ChromeMajorVersion', Mock(return_value='40'))
  @patch('build_version.ChromeRevision', Mock(return_value='302630'))
  @patch('build_version.ChromeCommitPosition', Mock(return_value=
             '1492c3d296476fe12cafecabba6ebabe-refs/heads/master@{#302630}'))
  @patch('buildbot_common.Archive')
  def testUploadArchives(self, archive_mock):
    build_artifacts.UploadArchives()
    cwd = '/archive_dir/'
    bucket_path = 'native-client-sdk/archives/40-302630-1492c3d29'
    archive_mock.assert_has_calls([
        call('foo.tar.bz2', bucket_path, cwd=cwd, step_link=False),
        call('foo.tar.bz2.sha1', bucket_path, cwd=cwd, step_link=False),
        call('bar.tar.bz2', bucket_path, cwd=cwd, step_link=False),
        call('bar.tar.bz2.sha1', bucket_path, cwd=cwd, step_link=False)
    ])


class GypNinjaPosixTestCase(BasePosixTestCase):
  def setUp(self):
    BasePosixTestCase.setUp(self)
    patch('sys.executable', 'python').start()
    patch('build_artifacts.SRC_DIR', 'src_dir').start()
    patch('os.environ', {}).start()
    self.run_mock = patch('buildbot_common.Run').start()
    self.options_mock = patch('build_artifacts.options').start()
    self.options_mock.mac_sdk = False
    self.options_mock.no_arm_trusted = False
    self.gyp_defines_base = []

  def testSimple(self):
    build_artifacts.GypNinjaBuild(
        None, 'gyp.py', 'foo.gyp', 'target', 'out_dir')
    self.run_mock.assert_has_calls([
        call(['python', 'gyp.py', 'foo.gyp', '--depth=.', '-G',
              'output_dir=out_dir'],
             cwd='src_dir',
             env={'GYP_DEFINES': ' '.join(self.gyp_defines_base)}),
        call(['ninja', '-C', 'out_dir/Release', 'target'], cwd='src_dir')
    ])

  def testTargetArch(self):
    build_artifacts.GypNinjaBuild(
        'x64', 'gyp.py', 'foo.gyp', 'target', 'out_dir')
    self.run_mock.assert_has_calls([
        call(['python', 'gyp.py', 'foo.gyp', '--depth=.', '-G',
              'output_dir=out_dir'],
             cwd='src_dir',
             env={
                 'GYP_DEFINES': ' '.join(self.gyp_defines_base +
                                         ['target_arch=x64']),
             }),
        call(['ninja', '-C', 'out_dir/Release', 'target'], cwd='src_dir')
    ])

  def testMultipleTargets(self):
    build_artifacts.GypNinjaBuild(
        None, 'gyp.py', 'foo.gyp', ['target1', 'target2'], 'out_dir')
    self.run_mock.assert_has_calls([
        call(['python', 'gyp.py', 'foo.gyp', '--depth=.', '-G',
              'output_dir=out_dir'],
             cwd='src_dir',
             env={'GYP_DEFINES': ' '.join(self.gyp_defines_base)}),
        call(['ninja', '-C', 'out_dir/Release', 'target1', 'target2'],
             cwd='src_dir')
    ])

  def testMacSdk(self):
    build_artifacts.PLATFORM = 'mac'
    self.options_mock.mac_sdk = '10.6'
    build_artifacts.GypNinjaBuild(
        None, 'gyp.py', 'foo.gyp', 'target', 'out_dir')
    self.run_mock.assert_has_calls([
        call(['python', 'gyp.py', 'foo.gyp', '--depth=.', '-G',
              'output_dir=out_dir'],
             cwd='src_dir',
             env={
               'GYP_DEFINES': ' '.join(self.gyp_defines_base +
                                       ['mac_sdk=10.6']),
             }),
        call(['ninja', '-C', 'out_dir/Release', 'target'], cwd='src_dir')
    ])

  def testArmLinux(self):
    build_artifacts.PLATFORM = 'linux'
    build_artifacts.GypNinjaBuild(
        'arm', 'gyp.py', 'foo.gyp', 'target', 'out_dir')
    self.run_mock.assert_has_calls([
        call(['python', 'gyp.py', 'foo.gyp', '--depth=.', '-G',
              'output_dir=out_dir'],
             cwd='src_dir',
             env={
               'GYP_CROSSCOMPILE': '1',
               'GYP_DEFINES': ' '.join(self.gyp_defines_base +
                                       ['target_arch=arm']),
             }),
        call(['ninja', '-C', 'out_dir/Release', 'target'], cwd='src_dir')
    ])

  def testNoArmTrusted(self):
    build_artifacts.PLATFORM = 'linux'
    self.options_mock.no_arm_trusted = True
    build_artifacts.GypNinjaBuild(
        'arm', 'gyp.py', 'foo.gyp', 'target', 'out_dir')
    self.run_mock.assert_has_calls([
        call(['python', 'gyp.py', 'foo.gyp', '--depth=.', '-G',
              'output_dir=out_dir'],
             cwd='src_dir',
             env={
               'GYP_CROSSCOMPILE': '1',
               'GYP_DEFINES': ' '.join(self.gyp_defines_base +
                                       ['target_arch=arm',
                                        'disable_cross_trusted=1']),
             }),
        call(['ninja', '-C', 'out_dir/Release', 'target'], cwd='src_dir')
    ])


class ArchivePosixTestCase(BasePosixTestCase):
  def setUp(self):
    BasePosixTestCase.setUp(self)
    self.makedir_mock = patch('buildbot_common.MakeDir').start()
    self.copyfile_mock = patch('buildbot_common.CopyFile').start()
    self.copydir_mock = patch('buildbot_common.CopyDir').start()
    self.isdir_mock = patch('os.path.isdir').start()
    patch('os.path.exists', Mock(return_value=False)).start()

    def dummy_isdir(path):
      if path == '/archive_dir/posix_foo':
        return True
      return False
    self.isdir_mock.side_effect = dummy_isdir

    self.archive = build_artifacts.Archive('foo')

  def testInit(self):
    self.assertEqual(self.archive.name, 'posix_foo')
    self.assertEqual(self.archive.archive_name, 'posix_foo.tar.bz2')
    self.assertEqual(self.archive.archive_path,
                     '/archive_dir/posix_foo.tar.bz2')
    self.assertEqual(self.archive.dirname, '/archive_dir/posix_foo')
    self.makedir_mock.assert_called_once_with('/archive_dir/posix_foo')

  @patch('glob.glob', Mock(side_effect=lambda x: [x]))
  def testCopySimple(self):
    self.archive.Copy('/copy_from', ['file1', 'file2'])
    self.assertEqual(self.copydir_mock.call_count, 0)
    self.copyfile_mock.assert_has_calls([
      call('/copy_from/file1', '/archive_dir/posix_foo/file1'),
      call('/copy_from/file2', '/archive_dir/posix_foo/file2')])

  @patch('glob.glob')
  def testCopyGlob(self, glob_mock):
    glob_mock.return_value = ['/copy_from/foo', '/copy_from/bar']
    self.archive.Copy('/copy_from', [('*', '')])
    glob_mock.assert_called_once_with('/copy_from/*')
    self.assertEqual(self.copydir_mock.call_count, 0)
    self.copyfile_mock.assert_has_calls([
      call('/copy_from/foo', '/archive_dir/posix_foo/'),
      call('/copy_from/bar', '/archive_dir/posix_foo/')])

  @patch('glob.glob', Mock(side_effect=lambda x: [x]))
  def testCopyRename(self):
    self.archive.Copy('/copy_from', [('file1', 'file1_renamed')])
    self.assertEqual(self.copydir_mock.call_count, 0)
    self.copyfile_mock.assert_called_once_with(
        '/copy_from/file1', '/archive_dir/posix_foo/file1_renamed')

  @patch('glob.glob', Mock(side_effect=lambda x: [x]))
  def testCopyNewDir(self):
    self.archive.Copy('/copy_from', [('file1', 'todir/')])
    self.assertEqual(self.copydir_mock.call_count, 0)
    self.copyfile_mock.assert_called_once_with(
        '/copy_from/file1', '/archive_dir/posix_foo/todir/file1')

  @patch('glob.glob', Mock(side_effect=lambda x: [x]))
  def testCopyDir(self):
    self.isdir_mock.side_effect = lambda _: True
    self.archive.Copy('/copy_from', ['dirname'])
    self.assertEqual(self.copyfile_mock.call_count, 0)
    self.copydir_mock.assert_called_once_with(
        '/copy_from/dirname', '/archive_dir/posix_foo/dirname')


class WinTestCase(unittest.TestCase):
  def setUp(self):
    patch('build_artifacts.PLATFORM', 'win').start()
    patch('build_artifacts.BUILD_ARCHIVE_DIR', 'c:\\archive_dir\\').start()
    patch('os.path.join', ntpath.join).start()

  def tearDown(self):
    patch.stopall()

  @patch('os.path.exists', Mock(return_value=False))
  @patch('buildbot_common.MakeDir')
  def testArchiveInit(self, makedir_mock):
    archive = build_artifacts.Archive('foo')
    self.assertEqual(archive.name, 'win_foo')
    self.assertEqual(archive.archive_name, 'win_foo.tar.bz2')
    self.assertEqual(archive.archive_path, r'c:\archive_dir\win_foo.tar.bz2')
    self.assertEqual(archive.dirname, r'c:\archive_dir\win_foo')
    makedir_mock.assert_called_once_with(r'c:\archive_dir\win_foo')


if __name__ == '__main__':
  unittest.main()
