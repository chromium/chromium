#!/usr/bin/env python
# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Helper script for PPAPI's PRESUBMIT.py to detect if additions or removals of
PPAPI interfaces have been propagated to the Native Client libraries (.dsc
files).

For example, if a user adds "ppapi/c/foo.h", we check that the interface has
been added to "native_client_sdk/src/libraries/ppapi/library.dsc".
"""

import argparse
import os
import sys

from build_paths import PPAPI_DIR, SRC_DIR, SDK_LIBRARY_DIR
import parse_dsc


# Add a file to this list if it should not be added to a .dsc file; i.e. if it
# should not be included in the Native Client SDK. This will silence the
# presubmit warning.
#
# Some examples of files that should not be added to the SDK are: Dev and
# Private interfaces that are either not available to NaCl plugins or are only
# available to Flash or other privileged plugins.
IGNORED_FILES = set([
  'pp_video_dev.h',
])


class VerifyException(Exception):
  def __init__(self, lib_path, expected, unexpected):
    self.expected = expected
    self.unexpected = unexpected

    msg = 'In %s:\n' % lib_path
    if expected:
      msg += '  these files are missing and should be added:\n'
      for filename in sorted(expected):
        msg += '    %s\n' % filename
    if unexpected:
      msg += '  these files no longer exist and should be removed:\n'
      for filename in sorted(unexpected):
        msg += '    %s\n' % filename

    Exception.__init__(self, msg)


def PartitionFiles(filenames):
  c_filenames = set()
  cpp_filenames = set()
  private_filenames = set()

  for filename in filenames:
    if os.path.splitext(filename)[1] not in ('.cc', '.h'):
      continue

    parts = filename.split(os.sep)
    basename = os.path.basename(filename)
    if basename in IGNORED_FILES:
      continue

    if 'private' in filename:
      if 'flash' in filename:
        continue
      private_filenames.add(filename)
    elif parts[0:2] == ['ppapi', 'c']:
      if len(parts) >= 2 and parts[2] in ('documentation', 'trusted'):
        continue
      c_filenames.add(filename)
    elif (parts[0:2] == ['ppapi', 'cpp'] or
          parts[0:2] == ['ppapi', 'utility']):
      if len(parts) >= 2 and parts[2] in ('documentation', 'trusted'):
        continue
      cpp_filenames.add(filename)
    else:
      continue

  return {
      'ppapi': c_filenames,
      'ppapi_cpp': cpp_filenames,
      'ppapi_cpp_private': private_filenames
  }


def GetDirectoryList(directory_path, relative_to):
  result = []
  for root, _, files in os.walk(directory_path):
    rel_root = os.path.relpath(root, relative_to)
    if rel_root == '.':
      rel_root = ''
    for base_name in files:
      result.append(os.path.join(rel_root, base_name))
  return result


def GetDscSourcesAndHeaders(dsc):
  result = []
  for headers_info in dsc.get('HEADERS', []):
    result.extend(headers_info['FILES'])
  for targets_info in dsc.get('TARGETS', []):
    result.extend(targets_info['SOURCES'])
  return result


def GetChangedAndRemovedFilenames(modified_filenames, directory_list):
  changed = set()
  removed = set()
  directory_list_set = set(directory_list)
  for filename in modified_filenames:
    if filename in directory_list_set:
      # We can't know if a file was added (that would require knowing the
      # previous state of the working directory). Instead, we assume that a
      # changed file may have been added, and check it accordingly.
      changed.add(filename)
    else:
      removed.add(filename)
  return changed, removed


def GetDscFilenameFromLibraryName(lib_name):
  return os.path.join(SDK_LIBRARY_DIR, lib_name, 'library.dsc')


def Verify(dsc_filename, dsc_sources_and_headers, changed_filenames,
           removed_filenames):
  expected_filenames = set()
  unexpected_filenames = set()

  for filename in changed_filenames:
    basename = os.path.basename(filename)
    if basename not in dsc_sources_and_headers:
      expected_filenames.add(filename)

  for filename in removed_filenames:
    basename = os.path.basename(filename)
    if basename in dsc_sources_and_headers:
      unexpected_filenames.add(filename)

  if expected_filenames or unexpected_filenames:
    raise VerifyException(dsc_filename, expected_filenames,
                          unexpected_filenames)


def VerifyOrPrintError(dsc_filename, dsc_sources_and_headers, changed_filenames,
                       removed_filenames, is_private=False):
  try:
    Verify(dsc_filename, dsc_sources_and_headers, changed_filenames,
           removed_filenames)
  except VerifyException as e:
    should_fail = True
    if is_private and e.expected:
      # For ppapi_cpp_private, we don't fail if there are expected filenames...
      # we may not want to include them. We still want to fail if there are
      # unexpected filenames, though.
      sys.stderr.write('>>> WARNING: private interface files changed. '
          'Should they be added to the Native Client SDK? <<<\n')
      if not e.unexpected:
        should_fail = False
    sys.stderr.write(str(e) + '\n')
    if should_fail:
      return False
  return True


def main(args):
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('sources', nargs='+')
  options = parser.parse_args(args)

  retval = 0
  lib_files = PartitionFiles(options.sources)
  directory_list = GetDirectoryList(PPAPI_DIR, relative_to=SRC_DIR)
  for lib_name, filenames in iter(lib_files.items()):
    if not filenames:
      continue

    changed_filenames, removed_filenames = \
        GetChangedAndRemovedFilenames(filenames, directory_list)

    dsc_filename = GetDscFilenameFromLibraryName(lib_name)
    dsc = parse_dsc.LoadProject(dsc_filename)
    dsc_sources_and_headers = GetDscSourcesAndHeaders(dsc)

    # Use the relative path to the .dsc to make the error messages shorter.
    rel_dsc_filename = os.path.relpath(dsc_filename, SRC_DIR)
    is_private = lib_name == 'ppapi_cpp_private'
    if not VerifyOrPrintError(rel_dsc_filename, dsc_sources_and_headers,
                              changed_filenames, removed_filenames,
                              is_private=is_private):
      retval = 1
  return retval


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
