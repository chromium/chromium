#!/usr/bin/env python
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Create binary protobuf encoding for fuzzer seeds.

This script is used to generate binary encoded protobuf seeds for fuzzers
related to Zucchini-gen and -apply, which take pairs of files are arguments. The
binary protobuf format is faster to parse so it is the preferred method for
encoding the seeds. For gen related fuzzers this should only need to be run
once. For any apply related fuzzers this should be rerun whenever the patch
format is changed.
"""

import argparse
import logging
import os
import subprocess
import sys

ABS_PATH = os.path.abspath(os.path.join(os.path.dirname(__file__)))
PROTO_DEFINITION_FILE = 'file_pair.proto'

def parse_args():
  """Parse commandline args."""
  parser = argparse.ArgumentParser()
  parser.add_argument('protoc_path', help='Path to protoc.')
  parser.add_argument('old_file', help='Old file to generate/apply patch.')
  parser.add_argument('new_or_patch_file',
                      help='New file to generate or patch to apply.')
  parser.add_argument('output_file',
                      help='File to write binary protobuf to.')
  parser.add_argument('--imposed_matches',
                      help='Equivalence matches to impose when generating '
                      'the patch.')
  return parser.parse_args()


def read_to_proto_escaped_string(filename):
  """Reads a file and converts it to hex escape sequences."""
  with open(filename, 'rb') as f:
    # Note that unicode-escape escapes all non-ASCII printable characters
    # excluding ", which needs to be manually escaped.
    return f.read().decode('latin1').encode('unicode-escape').replace(
               b'"', b'\\"')


def main():
  args = parse_args()
  # Create an ASCII string representing a protobuf.
  content = [b'old_file: "%s"' % read_to_proto_escaped_string(args.old_file),
             b'new_or_patch_file: "%s"' % read_to_proto_escaped_string(
                                               args.new_or_patch_file)]

  if args.imposed_matches:
    content.append(b'imposed_matches: "%s"' %
                       args.imposed_matches.encode('unicode-escape'))

  # Encode the ASCII protobuf as a binary protobuf.
  ps = subprocess.Popen([args.protoc_path, '--proto_path=%s' % ABS_PATH,
                         '--encode=zucchini.fuzzers.FilePair',
                         os.path.join(ABS_PATH, PROTO_DEFINITION_FILE)],
                        stdin=subprocess.PIPE,
                        stdout=subprocess.PIPE)
  # Write the string to the subprocess. Single line IO is fine as protoc returns
  # a string.
  output = ps.communicate(input=b'\n'.join(content))
  ps.wait()
  if ps.returncode:
    logging.error('Binary protobuf encoding failed.')
    return ps.returncode

  # Write stdout of the subprocess for protoc to the |output_file|.
  with open(args.output_file, 'wb') as f:
    f.write(output[0])
  return 0


if __name__ == '__main__':
  sys.exit(main())
