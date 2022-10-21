#!/usr/bin/env python
# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Helper script to update the test error expectations based on actual results.

This is useful for regenerating test expectations after making changes to the
error format.

To use this run the affected tests, and then pass the input to this script
(either via stdin, or as the first argument). For instance:

  $ ./out/Release/net_unittests --gtest_filter="*VerifyCertificateChain*" | \
     net/data/verify_certificate_chain_unittest/rebase-errors.py

The script works by scanning the stdout looking for gtest failures having a
particular format.  The C++ test side should have been instrumented to dump out
the test file's path on mismatch.

This script will then update the corresponding test/error file that contains the
error expectation.
"""

import os
import sys
import re

# Regular expression to find the failed errors in test stdout.
#  * Group 1 of the match is file path (relative to //src) where the
#    expected errors were read from.
#  * Group 2 of the match is the actual error text
failed_test_regex = re.compile(r"""
Cert path errors don't match expectations \((.+?)\)

EXPECTED:

(?:.|\n)*?
ACTUAL:

((?:.|\n)*?)
===> Use net/data/verify_certificate_chain_unittest/rebase-errors.py to rebaseline.
""", re.MULTILINE)


def read_file_to_string(path):
  """Reads a file entirely to a string"""
  with open(path, 'r') as f:
    return f.read()


def write_string_to_file(data, path):
  """Writes a string to a file"""
  print("Writing file %s ..." % (path))
  with open(path, "w") as f:
    f.write(data)


def get_src_root():
  """Returns the path to the enclosing //src directory. This assumes the
  current script is inside the source tree."""
  cur_dir = os.path.dirname(os.path.realpath(__file__))

  while True:
    parent_dir, dirname = os.path.split(cur_dir)
    # Check if it looks like the src/ root.
    if dirname == "src" and os.path.isdir(os.path.join(cur_dir, "net")):
      return cur_dir
    if not parent_dir or parent_dir == cur_dir:
      break
    cur_dir = parent_dir

  print("Couldn't find src dir")
  sys.exit(1)


def get_abs_path(rel_path):
  """Converts |rel_path| (relative to src) to a full path"""
  return os.path.join(get_src_root(), rel_path)


def fixup_errors_for_file(actual_errors, test_file_path):
  """Updates the errors in |test_file_path| to match |actual_errors|"""
  contents = read_file_to_string(test_file_path)

  header = "\nexpected_errors:\n"
  index = contents.find(header)
  if index < 0:
    print("Couldn't find expected_errors")
    sys.exit(1)

  # The rest of the file contains the errors (overwrite).
  contents = contents[0:index] + header + actual_errors

  write_string_to_file(contents, test_file_path)


def main():
  if len(sys.argv) > 2:
    print('Usage: %s [path-to-unittest-stdout]' % (sys.argv[0]))
    sys.exit(1)

  # Read the input either from a file, or from stdin.
  test_stdout = None
  if len(sys.argv) == 2:
    test_stdout = read_file_to_string(sys.argv[1])
  else:
    print('Reading input from stdin...')
    test_stdout = sys.stdin.read()

  for m in failed_test_regex.finditer(test_stdout):
    src_relative_errors_path = m.group(1)
    errors_path = get_abs_path(src_relative_errors_path)
    actual_errors = m.group(2)

    if errors_path.endswith(".test"):
      fixup_errors_for_file(actual_errors, errors_path)
    elif errors_path.endswith(".txt"):
      write_string_to_file(actual_errors, errors_path)
    else:
      print('Unknown file extension')
      sys.exit(1)



if __name__ == "__main__":
  main()
