#!/usr/bin/env python3
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Create binary protobuf encoding for fuzzer seeds.

This script is used to generate binary encoded protobuf seeds for fuzzers
related to Zucchini-gen and -apply, which take pairs of files as arguments. The
binary protobuf format is faster to parse so it is the preferred method for
encoding the seeds. For gen related fuzzers this should only need to be run
once. For any apply related fuzzers this should be rerun whenever the patch
format is changed.
"""

# Keep the existing two-space indentation to limit the scope of this change.
# pylint: disable=bad-indentation

import argparse
import logging
import os
import pathlib
import subprocess
import sys

_SCRIPT_DIR = pathlib.Path(__file__).resolve().parent
_SRC_ROOT = _SCRIPT_DIR.parents[2]
sys.path.insert(0, str(_SRC_ROOT / 'build'))
sys.path.insert(
    0, str(_SRC_ROOT / 'third_party' / 'protobuf' / 'python'))

# Import after adding Chromium's vendored modules to sys.path.
# pylint: disable=wrong-import-position
import action_helpers  # noqa: E402
from google.protobuf import text_encoding  # noqa: E402
# pylint: enable=wrong-import-position

ABS_PATH = str(_SCRIPT_DIR)
PROTO_DEFINITION_FILE = 'file_pair.proto'


def parse_args(argv=None):
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
  return parser.parse_args(argv)


def proto_escape(value):
  """Escapes bytes for use in a protobuf text-format string."""
  return text_encoding.CEscape(value, as_utf8=False).encode('ascii')


def read_to_proto_escaped_string(filename):
  """Reads a file and escapes it for a protobuf text-format string."""
  with open(filename, 'rb') as f:
    return proto_escape(f.read())


def build_file_pair_text(old_file, new_or_patch_file, imposed_matches=None):
  """Builds a text-format FilePair protobuf."""
  content = [b'old_file: "%s"' % read_to_proto_escaped_string(old_file),
             b'new_or_patch_file: "%s"' % read_to_proto_escaped_string(
                                               new_or_patch_file)]

  if imposed_matches:
    content.append(b'imposed_matches: "%s"' %
                   proto_escape(imposed_matches.encode('utf-8')))

  return b'\n'.join(content)


def create_seed_file_pair(protoc_path,
                          old_file,
                          new_or_patch_file,
                          output_file,
                          imposed_matches=None):
  """Creates a binary encoded FilePair seed."""
  # Encode the ASCII protobuf as a binary protobuf.
  result = subprocess.run(
      [protoc_path, '--proto_path=%s' % ABS_PATH,
       '--encode=zucchini.fuzzers.FilePair',
       os.path.join(ABS_PATH, PROTO_DEFINITION_FILE)],
      input=build_file_pair_text(old_file, new_or_patch_file, imposed_matches),
      stdout=subprocess.PIPE,
      check=False)
  if result.returncode:
    logging.error('Binary protobuf encoding failed.')
    return result.returncode

  with action_helpers.atomic_output(output_file) as f:
    f.write(result.stdout)
  return 0


def main(argv=None):
  args = parse_args(argv)
  return create_seed_file_pair(args.protoc_path, args.old_file,
                               args.new_or_patch_file, args.output_file,
                               args.imposed_matches)


if __name__ == '__main__':
  sys.exit(main())
