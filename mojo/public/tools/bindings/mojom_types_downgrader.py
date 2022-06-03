#!/usr/bin/env python
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Downgrades *.mojom files to the old mojo types for remotes and receivers."""

import argparse
import fnmatch
import os
import re
import shutil
import sys
import tempfile

# List of patterns and replacements to match and use against the contents of a
# mojo file. Each replacement string will be used with Python string's format()
# function, so the '{}' substring is used to mark where the mojo type should go.
_MOJO_REPLACEMENTS = {
    r'pending_remote': r'{}',
    r'pending_receiver': r'{}&',
    r'pending_associated_remote': r'associated {}',
    r'pending_associated_receiver': r'associated {}&',
}

# Pre-compiled regular expression that matches against any of the replacements.
_REGEXP_PATTERN = re.compile(
    r'|'.join(
        ['{}\s*<\s*(.*?)\s*>'.format(k) for k in _MOJO_REPLACEMENTS.keys()]),
    flags=re.DOTALL)


def ReplaceFunction(match_object):
  """Returns the right replacement for the string matched against the regexp."""
  for index, (match, repl) in enumerate(_MOJO_REPLACEMENTS.items(), 1):
    if match_object.group(0).startswith(match):
      return repl.format(match_object.group(index))


def DowngradeFile(path, output_dir=None):
  """Downgrades the mojom file specified by |path| to the old mojo types.

  Optionally pass |output_dir| to place the result under a separate output
  directory, preserving the relative path to the file included in |path|.
  """
  # Use a temporary file to dump the new contents after replacing the patterns.
  with open(path) as src_mojo_file:
    with tempfile.NamedTemporaryFile(mode='w', delete=False) as tmp_mojo_file:
      tmp_contents = _REGEXP_PATTERN.sub(ReplaceFunction, src_mojo_file.read())
      tmp_mojo_file.write(tmp_contents)

  # Files should be placed in the desired output directory
  if output_dir:
    output_filepath = os.path.join(output_dir, os.path.basename(path))
    if not os.path.exists(output_dir):
      os.makedirs(output_dir)
  else:
    output_filepath = path

  # Write the new contents preserving the original file's attributes.
  shutil.copystat(path, tmp_mojo_file.name)
  shutil.move(tmp_mojo_file.name, output_filepath)

  # Make sure to "touch" the new file so that access, modify and change times
  # are always newer than the source file's, otherwise Modify time will be kept
  # as per the call to shutil.copystat(), causing unnecessary generations of the
  # output file in subsequent builds due to ninja considering it dirty.
  os.utime(output_filepath, None)


def DowngradeDirectory(path, output_dir=None):
  """Downgrades mojom files inside directory |path| to the old mojo types.

  Optionally pass |output_dir| to place the result under a separate output
  directory, preserving the relative path to the file included in |path|.
  """
  # We don't have recursive glob.glob() nor pathlib.Path.rglob() in Python 2.7
  mojom_filepaths = []
  for dir_path, _, filenames in os.walk(path):
    for filename in fnmatch.filter(filenames, "*mojom"):
      mojom_filepaths.append(os.path.join(dir_path, filename))

  for path in mojom_filepaths:
    absolute_dirpath = os.path.dirname(os.path.abspath(path))
    if output_dir:
      dest_dirpath = output_dir + absolute_dirpath
    else:
      dest_dirpath = absolute_dirpath
    DowngradeFile(path, dest_dirpath)


def DowngradePath(src_path, output_dir=None):
  """Downgrades the mojom files pointed by |src_path| to the old mojo types.

  Optionally pass |output_dir| to place the result under a separate output
  directory, preserving the relative path to the file included in |path|.
  """
  if os.path.isdir(src_path):
    DowngradeDirectory(src_path, output_dir)
  elif os.path.isfile(src_path):
    DowngradeFile(src_path, output_dir)
  else:
    print(">>> {} not pointing to a valid file or directory".format(src_path))
    sys.exit(1)


def main():
  parser = argparse.ArgumentParser(
      description="Downgrade *.mojom files to use the old mojo types.")
  parser.add_argument(
      "srcpath", help="path to the file or directory to apply the conversion")
  parser.add_argument(
      "--outdir", help="the directory to place the converted file(s) under")
  args = parser.parse_args()

  DowngradePath(args.srcpath, args.outdir)


if __name__ == "__main__":
  sys.exit(main())
