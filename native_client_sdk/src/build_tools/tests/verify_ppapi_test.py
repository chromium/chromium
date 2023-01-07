#!/usr/bin/env vpython3
# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
BUILD_TOOLS_DIR = os.path.dirname(SCRIPT_DIR)
CHROME_SRC = os.path.dirname(os.path.dirname(os.path.dirname(BUILD_TOOLS_DIR)))

import mock

sys.path.append(BUILD_TOOLS_DIR)
import verify_ppapi


class TestPartition(unittest.TestCase):
  def testBasic(self):
    filenames = [
        os.path.join('ppapi', 'c', 'ppb_foo.h'),
        os.path.join('ppapi', 'cpp', 'foo.h'),
        os.path.join('ppapi', 'cpp', 'foo.cc'),
    ]
    result = verify_ppapi.PartitionFiles(filenames)
    self.assertTrue(filenames[0] in result['ppapi'])
    self.assertTrue(filenames[1] in result['ppapi_cpp'])
    self.assertTrue(filenames[2] in result['ppapi_cpp'])
    self.assertEqual(0, len(result['ppapi_cpp_private']))

  def testIgnoreDocumentation(self):
    filenames = [
        os.path.join('ppapi', 'c', 'documentation', 'Doxyfile'),
        os.path.join('ppapi', 'c', 'documentation', 'index.dox'),
        os.path.join('ppapi', 'cpp', 'documentation', 'footer.html'),
    ]
    result = verify_ppapi.PartitionFiles(filenames)
    self.assertEqual(0, len(result['ppapi']))
    self.assertEqual(0, len(result['ppapi_cpp']))
    self.assertEqual(0, len(result['ppapi_cpp_private']))

  def testIgnoreTrusted(self):
    filenames = [
        os.path.join('ppapi', 'c', 'trusted', 'ppb_broker_trusted.h'),
        os.path.join('ppapi', 'cpp', 'trusted', 'file_chooser_trusted.cc'),
    ]
    result = verify_ppapi.PartitionFiles(filenames)
    self.assertEqual(0, len(result['ppapi']))
    self.assertEqual(0, len(result['ppapi_cpp']))
    self.assertEqual(0, len(result['ppapi_cpp_private']))

  def testIgnoreIfNotSourceOrHeader(self):
    filenames = [
        os.path.join('ppapi', 'c', 'DEPS'),
        os.path.join('ppapi', 'c', 'blah', 'foo.xml'),
        os.path.join('ppapi', 'cpp', 'DEPS'),
        os.path.join('ppapi', 'cpp', 'foobar.py'),
    ]
    result = verify_ppapi.PartitionFiles(filenames)
    self.assertEqual(0, len(result['ppapi']))
    self.assertEqual(0, len(result['ppapi_cpp']))
    self.assertEqual(0, len(result['ppapi_cpp_private']))

  def testIgnoreOtherDirectories(self):
    ignored_directories = ['api', 'examples', 'generators', 'host', 'lib',
        'native_client', 'proxy', 'shared_impl', 'tests', 'thunk']

    # Generate some random files in the ignored directories.
    filenames = []
    for dirname in ignored_directories:
      filenames = os.path.join('ppapi', dirname, 'foo.cc')
      filenames = os.path.join('ppapi', dirname, 'subdir', 'foo.h')
      filenames = os.path.join('ppapi', dirname, 'DEPS')

    result = verify_ppapi.PartitionFiles(filenames)
    self.assertEqual(0, len(result['ppapi']))
    self.assertEqual(0, len(result['ppapi_cpp']))
    self.assertEqual(0, len(result['ppapi_cpp_private']))


class TestGetChangedAndRemoved(unittest.TestCase):
  def testBasic(self):
    modified_filenames = [
        os.path.join('ppapi', 'cpp', 'audio.cc'),
        os.path.join('ppapi', 'cpp', 'graphics_2d.cc'),
        os.path.join('ppapi', 'cpp', 'foobar.cc'),
        os.path.join('ppapi', 'cpp', 'var.cc'),
    ]
    directory_list = [
        os.path.join('ppapi', 'cpp', 'audio.cc'),
        os.path.join('ppapi', 'cpp', 'graphics_2d.cc'),
    ]
    changed, removed = verify_ppapi.GetChangedAndRemovedFilenames(
        modified_filenames, directory_list)
    self.assertTrue(modified_filenames[0] in changed)
    self.assertTrue(modified_filenames[1] in changed)
    self.assertTrue(modified_filenames[2] in removed)
    self.assertTrue(modified_filenames[3] in removed)


class TestVerify(unittest.TestCase):
  def testBasic(self):
    dsc_filename = 'native_client_sdk/src/libraries/ppapi/library.dsc'
    # The .dsc files typically uses basenames, not full paths.
    dsc_sources_and_headers = [
        'ppb_audio.h',
        'ppb_console.h',
        'ppb_gamepad.h',
        'ppb.h',
        'ppp_zoom_dev.h',
    ]
    changed_filenames = [
        os.path.join('ppapi', 'c', 'ppb_audio.h'),
        os.path.join('ppapi', 'c', 'ppb_console.h'),
    ]
    removed_filenames = []
    # Should not raise.
    verify_ppapi.Verify(dsc_filename, dsc_sources_and_headers,
                        changed_filenames, removed_filenames)

    # Raise, because we removed ppp_zoom_dev.h.
    removed_filenames = [
        os.path.join('ppapi', 'c', 'ppb_console.h'),
    ]
    self.assertRaises(verify_ppapi.VerifyException, verify_ppapi.Verify,
                      dsc_filename, dsc_sources_and_headers, changed_filenames,
                      removed_filenames)

    # Raise, because we added ppb_foo.h.
    removed_filenames = []
    changed_filenames = [
        os.path.join('ppapi', 'c', 'ppb_audio.h'),
        os.path.join('ppapi', 'c', 'ppb_console.h'),
        os.path.join('ppapi', 'c', 'ppb_foo.h'),
    ]
    self.assertRaises(verify_ppapi.VerifyException, verify_ppapi.Verify,
                      dsc_filename, dsc_sources_and_headers, changed_filenames,
                      removed_filenames)

  def testVerifyPrivate(self):
    dsc_filename = \
        'native_client_sdk/src/libraries/ppapi_cpp_private/library.dsc'
    # The .dsc files typically uses basenames, not full paths.
    dsc_sources_and_headers = [
        'ext_crx_file_system_private.cc',
        'file_io_private.cc',
        'ppb_ext_crx_file_system_private.h',
        'ppb_file_io_private.h',
        'host_resolver_private.h',
        'net_address_private.h',
    ]
    changed_filenames = [
        os.path.join('ppapi', 'c', 'private', 'ppb_foo_private.h'),
    ]
    removed_filenames = []

    with mock.patch('sys.stderr') as sys_stderr:
      # When a new private file is added, just print to stderr, but don't fail.
      result = verify_ppapi.VerifyOrPrintError(
          dsc_filename, dsc_sources_and_headers, changed_filenames,
          removed_filenames, is_private=True)
      self.assertTrue(result)
      self.assertTrue(sys_stderr.write.called)

      # If is_private is False, then adding a new interface without updating the
      # .dsc is an error.
      sys_stderr.reset_mock()
      result = verify_ppapi.VerifyOrPrintError(
          dsc_filename, dsc_sources_and_headers, changed_filenames,
          removed_filenames, is_private=False)
      self.assertFalse(result)
      self.assertTrue(sys_stderr.write.called)

      # Removing a file without updating the .dsc is always an error.
      sys_stderr.reset_mock()
      changed_filenames = []
      removed_filenames = [
          os.path.join('ppapi', 'c', 'private', 'net_address_private.h'),
      ]
      result = verify_ppapi.VerifyOrPrintError(
          dsc_filename, dsc_sources_and_headers, changed_filenames,
          removed_filenames, is_private=True)
      self.assertFalse(result)
      self.assertTrue(sys_stderr.write.called)


if __name__ == '__main__':
  unittest.main()
