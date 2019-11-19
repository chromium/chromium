#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This script should be run manually on occasion to make sure the gyp file and
the includes tests are up to date.

It does the following:
 - Verifies that all source code is in ppapi.gyp
 - Verifies that all sources in ppapi.gyp really do exist
 - Generates tests/test_c_includes.c
 - Generates tests/test_cpp_includes.cc
These tests are checked in to SVN.
"""
# TODO(dmichael):  Make this script execute as a gyp action, move the include
#                  tests to some 'generated' area, and remove them from version
#                  control.

from __future__ import print_function

import re
import os
import sys
import posixpath

# A simple regular expression that should match source files for C++ and C.
SOURCE_FILE_RE = re.compile('.+\.(cc|c|h)$')

# IGNORE_RE is a regular expression that matches directories which contain
# source that we don't (currently) expect to be in ppapi.gyp.  This script will
# not check whether source files under these directories are in the gyp file.
# TODO(dmichael): Put examples back in the build.
# TODO(brettw): Put proxy in the build when it's ready.
IGNORE_RE = re.compile('^(examples|GLES2|proxy|tests\/clang).*')

GYP_TARGETS_KEY = 'targets'
GYP_SOURCES_KEY = 'sources'
GYP_TARGET_NAME_KEY = 'target_name'


# Return a set containing all source files found given an object read from a gyp
# file.
def GetAllGypSources(gyp_file_data):
  sources = set([])
  for target in gyp_file_data[GYP_TARGETS_KEY]:
    # Get a list of sources in the target that are not ignored, and 'normalize'
    # them.  The main reason for this is to turn the forward slashes in the gyp
    # file in to backslashes when the script is run on Windows.
    source_list = [posixpath.normpath(src) for src in target[GYP_SOURCES_KEY]
                   if not IGNORE_RE.match(src)]
    sources |= set(source_list)
  return sources


# Search the directory named start_root and all its subdirectories for source
# files.
# Return a set containing the string names of all the source files found,
# relative to start_root.
def GetFileSources(start_root):
  file_set = set([])
  for root, dirs, files in os.walk(start_root):
    relative_root = os.path.relpath(root, start_root)
    if not IGNORE_RE.match(relative_root):
      for source in files:
        if SOURCE_FILE_RE.match(source):
          file_set |= set([os.path.join(relative_root, source)])
  return file_set


# Make sure all source files are in the given gyp object (evaluated from a gyp
# file), and that all source files listed in the gyp object exist in the
# directory.
def VerifyGypFile(gyp_file_data):
  gyp_sources = GetAllGypSources(gyp_file_data)
  file_sources = GetFileSources('.')
  in_gyp_not_file = gyp_sources - file_sources
  in_file_not_gyp = file_sources - gyp_sources
  if len(in_gyp_not_file):
    print('Found source file(s) in ppapi.gyp but not in the directory:', \
      in_gyp_not_file)
  if len(in_file_not_gyp):
    print('Found source file(s) in the directory but not in ppapi.gyp:', \
      in_file_not_gyp)
  error_count = len(in_gyp_not_file) + len(in_file_not_gyp)
  if error_count:
    sys.exit(error_count)


def WriteLines(filename, lines):
  outfile = open(filename, 'w')
  for line in lines:
    outfile.write(line)
  outfile.write('\n')


COPYRIGHT_STRING_C = \
"""/* Copyright (c) 2010 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * This test simply includes all the C headers to ensure they compile with a C
 * compiler.  If it compiles, it passes.
 */
"""

COPYRIGHT_STRING_CC = \
"""// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This test simply includes all the C++ headers to ensure they compile with a
// C++ compiler.  If it compiles, it passes.
//
"""


# Get the source file names out of the given gyp file data object (as evaluated
# from a gyp file) for the given target name.  Return the string names in
# sorted order.
def GetSourcesForTarget(target_name, gyp_file_data):
  for target in gyp_file_data[GYP_TARGETS_KEY]:
    if target[GYP_TARGET_NAME_KEY] == target_name:
      sources = target[GYP_SOURCES_KEY]
      sources.sort()
      return sources
  print('Warning: no target named ', target, ' found.')
  return []


# Generate all_c_includes.h, which includes all C headers.  This is part of
# tests/test_c_sizes.c, which includes all C API files to ensure that all
# the headers in ppapi/c can be compiled with a C compiler, and also asserts
# (with compile-time assertions) that all structs and enums are a particular
# size.
def GenerateCIncludeTest(gyp_file_data):
  c_sources = GetSourcesForTarget('ppapi_c', gyp_file_data)
  lines = [COPYRIGHT_STRING_C]
  lines.append('#ifndef PPAPI_TESTS_ALL_C_INCLUDES_H_\n')
  lines.append('#define PPAPI_TESTS_ALL_C_INCLUDES_H_\n\n')
  for source in c_sources:
    lines.append('#include "ppapi/' + source + '"\n')
  lines.append('\n#endif  /* PPAPI_TESTS_ALL_C_INCLUDES_H_ */\n')
  WriteLines('tests/all_c_includes.h', lines)


# Generate all_cpp_includes.h, which is used by test_cpp_includes.cc to ensure
# that all the headers in ppapi/cpp can be compiled with a C++ compiler.
def GenerateCCIncludeTest(gyp_file_data):
  cc_sources = GetSourcesForTarget('ppapi_cpp_objects', gyp_file_data)
  header_re = re.compile('.+\.h$')
  lines = [COPYRIGHT_STRING_CC]
  lines.append('#ifndef PPAPI_TESTS_ALL_CPP_INCLUDES_H_\n')
  lines.append('#define PPAPI_TESTS_ALL_CPP_INCLUDES_H_\n\n')
  for source in cc_sources:
    if header_re.match(source):
      lines.append('#include "ppapi/' + source + '"\n')
  lines.append('\n#endif  // PPAPI_TESTS_ALL_CPP_INCLUDES_H_\n')
  WriteLines('tests/all_cpp_includes.h', lines)


def main():
  ppapi_gyp_file_name = 'ppapi.gyp'
  gyp_file_contents = open(ppapi_gyp_file_name).read()
  gyp_file_data = eval(gyp_file_contents)
  VerifyGypFile(gyp_file_data)
  GenerateCIncludeTest(gyp_file_data)
  GenerateCCIncludeTest(gyp_file_data)
  return 0


if __name__ == '__main__':
  sys.exit(main())
