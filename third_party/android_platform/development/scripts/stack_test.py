#!/usr/bin/env python
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import mock
import os
import re
import shutil
import subprocess
import sys
import tempfile
import textwrap
import unittest
import zipfile

sys.path.insert(
    1,
    os.path.join(
        os.path.dirname(__file__), '..', '..', '..', '..', 'build', 'android'))
import devil_chromium
from pylib import constants

sys.path.insert(1,
                constants.host_paths.ANDROID_PLATFORM_DEVELOPMENT_SCRIPTS_PATH)
import stack

# Use Python-based zipalign so that these tests can run on the Presubmit bot.
sys.path.insert(
    1, os.path.join(constants.DIR_SOURCE_ROOT, 'build'))
import zip_helpers


# These tests exercise stack.py by generating fake APKs (zip-aligned archives),
# full of fake .so files (with ELF headers), and using a fake symbolizer.
#
# The symbolizer returns deterministic function descriptions given an address
# and library name, so that test cases can be easily contrived. Eg:
#
#   libchrome.so at 0x174 --> chrome::Func_174 at chrome.cc:1:1
#
# All libraries generated are slightly under 4K in size (0x1000). This means
# that when a fake APK is generated, libraries within it will reside at
# consecutive 4K boundaries. Eg., an APK with libfoo.so and libbar.so:
#
#   libfoo.so will reside at APK offset 0x1000
#   libbar.so will reside at APK offset 0x2000
#
# Each test invokes stack.py with a given test input (fudged trace lines), runs
# them through stack.py with the fake symbolizer, grabs output, and matches it
# line-for-line with the expected output. Whitespace is ignored at that step, so
# that test expectations don't have to be column-accurate.


class FakeSymbolizer:

  def __init__(self, directory):
    self._lib_directory = directory

  def GetSymbolInformation(self, library, address):
    basename = os.path.basename(library)
    local_file = os.path.join(self._lib_directory, basename)

    # If the library doesn't exist, the LLVM symbolizer wrapper script
    # intercepts the call and returns <UNKNOWN>.
    if not os.path.exists(local_file):
      return [('<UNKNOWN>', library)]

    # If the address isn't in the library, LLVM symbolizer yields ??.
    lib_size = os.stat(local_file).st_size
    if address >= lib_size:
      return [('??', '??:0:0')]

    namespace = basename.split('.')[0].replace('lib', '', 1)

    # Determine if the lib is a secondary ABI library, in which case, preface
    # its namespace with '32'.
    if 'android_clang_' in library:
      namespace += '32'

    method_name = '{}::Func_{:X}'.format(namespace, address)
    return [(method_name, '{}.cc:1:1'.format(namespace))]

  @staticmethod
  def IsValidTarget(path):
    # pylint: disable=unused-argument
    return True

class StackDecodeTest(unittest.TestCase):
  def setUp(self):
    self._num_libraries = 0
    self._temp_dir = tempfile.mkdtemp()

  def tearDown(self):
    shutil.rmtree(self._temp_dir)

  def _MakeElf(self, library):
    # Make the unstripped lib directory in case stack.py looks for it.
    lib_dir = os.path.dirname(library)
    if not os.path.exists(lib_dir):
      os.makedirs(lib_dir)

    # Create a library slightly less than 4K in size, so that when added to an
    # APK archive, all libraries end up on 4K boundaries. Also, make each
    # library a slightly different size, since stack.py may utilize size when
    # matching up libraries.
    data = '\x7fELF' + ' ' * (0xE00 - self._num_libraries)
    self._num_libraries += 1
    with open(library, 'wb') as f:
      f.write(data.encode('utf-8'))

  # Build a dummy APK with native libraries in it.
  def _MakeApk(self, apk, libs, apk_dir, out_dir, crazy):
    apk_file = os.path.join(apk_dir, apk)
    with zipfile.ZipFile(apk_file, 'w') as archive:
      for lib in libs:
        # Make an ELF-format .so file. The fake symbolizer will fudge functions
        # for libraries that exist.
        path, name = os.path.split(lib)
        library_file = os.path.join(out_dir, path, 'lib.unstripped', name)
        self._MakeElf(library_file)

        # Add the library to the APK.
        name_in_apk = 'crazy.' + lib if crazy else lib
        zip_helpers.add_to_zip_hermetic(
            archive,
            name_in_apk,
            src_path=library_file,
            alignment=0x1000)

  # Accept either a multi-line string or a list of strings, strip leading and
  # trailing whitespace, and return the strings as a list.
  def _StripLines(self, text):
    if isinstance(text, str):
      lines = text.splitlines()
    else:
      assert isinstance(text, list)
      lines = text
    lines = [line.strip() for line in lines]
    return [line for line in lines if line]

  def _RunCase(self, logcat, expected, apks, crazy=False):
    # Set up staging directories.
    temp = self._temp_dir
    out_dir = os.path.join(temp, 'out', 'Debug')
    os.makedirs(out_dir)
    apk_dir = os.path.join(out_dir, 'apks')
    os.makedirs(apk_dir)

    input_file = os.path.join(temp, 'input.txt')
    output_file = os.path.join(temp, 'output.txt')

    # Create test APKs, with .so libraries in them, that are real enough to
    # trick the stack decoder.
    for name, libs in apks.items():
      self._MakeApk(name, libs, apk_dir, out_dir, crazy)

    symbolizer = FakeSymbolizer(os.path.join(out_dir, 'lib.unstripped'))

    # Put the input into a temp file.
    with open(input_file, 'w') as f:
      input_lines = self._StripLines(logcat)
      f.write('\n'.join(input_lines))

    # Run the stack script and capture its stdout in a file.
    # TODO(cjgrant): Figure out how to output to a stream buffer instead.
    stack_script_args = [
        '--output-directory',
        out_dir,
        '--apks-directory',
        apk_dir,
        input_file,
    ]
    with open(output_file, 'w') as f:
      old_stdout = sys.stdout
      sys.stdout = f
      try:
        stack.main(stack_script_args, test_symbolizer=symbolizer)
      except Exception:
        pass
      sys.stdout.flush()
      sys.stdout = old_stdout

    # Filter out all output lines before actual decoding starts.
    with open(output_file, 'r') as f:
      lines = f.readlines()
      delimiter = [l for l in lines if 'RELADDR' in l]
      if delimiter:
        index = lines.index(delimiter[-1])
        output_lines = lines[index + 1:]
        output_lines = self._StripLines(output_lines)
      else:
        output_lines = []

    # Tokenize the input and output so that we can ignore whitespace in the
    # validation. This way a test doesn't fail if a column shifts slightly.
    expected_lines = self._StripLines(expected)
    expected_tokens = [line.split() for line in expected_lines]
    actual_tokens = [line.split() for line in output_lines]

    self.assertEqual(len(expected_tokens), len(actual_tokens))
    for i in range(len(expected_tokens)):
      self.assertEqual(expected_tokens[i], actual_tokens[i])

  @mock.patch('stack_core._BuildIdFromElf')
  def test_BasicDecoding(self, patch_build_id):
    patch_build_id.side_effect = ['1', '1', '2', '2']
    apks = {
        'chrome.apk': ['libchrome.so', 'libfoo.so'],
    }
    input_trace = textwrap.dedent('''
      DEBUG : #01 pc 00000174  /path==/base.apk (offset 0x00001000)
      DEBUG : #02 pc 00000274  /path==/base.apk (offset 0x00002000)
      DEBUG : #03 pc 00000374  /path==/lib/arm/libchrome.so
      ''')
    expected_decode = textwrap.dedent('''
      00000174   chrome::Func_174         chrome.cc:1:1
      00000274   foo::Func_274            foo.cc:1:1
      00000374   chrome::Func_374         chrome.cc:1:1
      ''')
    self._RunCase(input_trace, expected_decode, apks)

  @mock.patch('stack_core._BuildIdFromElf')
  def test_OutOfRangeAddresses(self, patch_build_id):
    patch_build_id.side_effect = ['1', '1']
    apks = {
        'chrome.apk': ['libchrome.so'],
    }
    # Test offsets where the address is outside the range of a valid library,
    # and when the offset does not correspond to a valid library.
    input_trace = textwrap.dedent('''
      DEBUG : #01 pc 00777777  /path==/base.apk (offset 0x00001000)
      DEBUG : #02 pc 00000374  /path==/base.apk (offset 0x00003000)
      ''')
    expected_decode = textwrap.dedent('''
      00777777   ??                       ??:0:0
      00000374   offset 0x00003000        /path==/base.apk
      ''')
    self._RunCase(input_trace, expected_decode, apks)

  def test_SystemLibraries(self):
    apks = {
        'chrome.apk': [],
    }
    # Here, the frames are in an on-device system library. If the trace is able
    # to supply a symbol name, ensure it's preserved in the output.
    input_trace = textwrap.dedent('''
      DEBUG : #01 pc 00000474  /system/lib/libart.so (art_function+40)
      DEBUG : #02 pc 00000474  /system/lib/libart.so
      ''')
    expected_decode = textwrap.dedent('''
      00000474   art_function+40          /system/lib/libart.so
      00000474   <UNKNOWN>                /system/lib/libart.so
      ''')
    self._RunCase(input_trace, expected_decode, apks)

  @mock.patch('stack_core._BuildIdFromElf')
  def test_MultiArchPrimaryAbi(self, patch_build_id):
    # '_BuildIdFromElf' is invoked twice to find:
    #   1. Build ID of lib in apk at the offset
    #   2. Build ID of out/lib.unstripped/libmonochrome.so
    patch_build_id.side_effect = ['1', '1']
    apks = {
        'monochrome.apk': [
            'libmonochrome.so', 'android_clang_arm/libmonochrome.so'
        ],
    }
    # With both architectures present, verify that the correct ABI output and
    # directory is chosen, even if identically-named libraries exist in both.
    input_trace = textwrap.dedent('''
      DEBUG : #01 pc 00000174  /path==/base.apk (offset 0x00001000)
      ''')
    expected_decode = textwrap.dedent('''
      00000174   monochrome::Func_174         monochrome.cc:1:1
      ''')
    self._RunCase(input_trace, expected_decode, apks)

  @mock.patch('stack_core._BuildIdFromElf')
  def test_MultiArchSecondary(self, patch_build_id):
    # '_BuildIdFromElf' is invoked 3 times to find:
    #   1. Build ID of lib in apk at the offset
    #   2. Build ID of out/lib.unstripped/libmonochrome.so
    #   3. Build ID of out/android_clang_arm/lib.unstripped/libmonochrome.so
    patch_build_id.side_effect = ['1', '2', '1']
    apks = {
        'monochrome.apk': [
            'libmonochrome.so', 'android_clang_arm/libmonochrome.so'
        ],
    }
    # With both architectures present, verify that the secondary ABI output
    # directory is chosen when appropriate, even if identically-named libraries
    # exist in both. This must be a different test from the primary ABI case,
    # since in practice, traces are from a single ABI (and the script relies on
    # this).
    input_trace = textwrap.dedent('''
      DEBUG : #02 pc 00000274  /path==/base.apk (offset 0x00002000)
      ''')
    expected_decode = textwrap.dedent('''
      00000274   monochrome32::Func_274       monochrome32.cc:1:1
      ''')
    self._RunCase(input_trace, expected_decode, apks)

  @mock.patch('stack_core._BuildIdFromElf')
  def test_CrazyUncompressedLibraries(self, patch_build_id):
    patch_build_id.side_effect = ['1', '1']
    # Here, the library in the APK is prefixed with "crazy.", as in
    # ChromeModern.
    apks = {
        'chrome.apk': ['libchrome.so'],
    }
    input_trace = textwrap.dedent('''
      DEBUG : #01 pc 00000174  /path==/base.apk (offset 0x00001000)
      ''')
    expected_decode = textwrap.dedent('''
      00000174   chrome::Func_174         chrome.cc:1:1
      ''')
    self._RunCase(input_trace, expected_decode, apks, crazy=True)

  def test_AndroidQ(self):
    apks = {
        'chrome.apk': ['libchrome.so'],
    }
    # Android Q helpfully prints both APK and library, so we don't need to do
    # any matching, as long as we can parse this.
    input_trace = textwrap.dedent('''
      DEBUG : #01 pc 00000174  /path==/base.apk!libchrome.so (offset 0x00001000)
      ''')
    expected_decode = textwrap.dedent('''
      00000174   chrome::Func_174         chrome.cc:1:1
      ''')
    self._RunCase(input_trace, expected_decode, apks)


if __name__ == '__main__':
  unittest.main()
