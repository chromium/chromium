#!/usr/bin/env python
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re
import sys

kUsage = '''Usage: truncate_net_log.py INPUT_FILE OUTPUT_FILE TRUNCATED_SIZE

Creates a smaller version of INPUT_FILE (which is a chrome-net-export-log.json
formatted NetLog file) and saves it to OUTPUT_FILE. Note that this works by
reading the file line by line and not fully parsing the JSON, so it must match
the exact format (whitespace and all).

File truncation is done by dropping the oldest events and keeping everything
else.

Parameters:

  INPUT_FILE:
    Path to net-export JSON file

  OUTPUT_FILE:
    Path to save truncated file to

  TRUNCATED_SIZE:
    The desired (approximate) size for the truncated file. May use a suffix to
    indicate units. Examples:
          2003  -->  2003 bytes
          100K  -->  100 KiB
          8M    -->  8 MiB
          1.5m  -->  1.5 MiB
'''

def get_file_size(path):
  '''Returns the filesize of |path| in bytes'''
  return os.stat(path).st_size


def truncate_log_file(in_path, out_path, desired_size):
  '''Copies |in_path| to |out_path| such that it is approximately
  |desired_size| bytes large. This is accomplished by dropping the oldest
  events first. The final file size may not be exactly |desired_size| as only
  complete event lines are skipped.'''
  orig_size = get_file_size(in_path)
  bytes_to_truncate = orig_size - desired_size

  # This variable is True if the current line being processed is an Event line.
  inside_events = False
  with open(out_path, 'w') as out_file:
    with open(in_path, 'r') as in_file:
      for line in in_file:
        # The final line before polledData closes the events array, and hence
        # ends in "],". The check for polledData is more for documentation
        # sake.
        if inside_events and (line.startswith('"polledData": {' or
                              line.endswith('],\n'))):
          inside_events = False

        # If this is an event line and need to drop more bytes, go ahead and
        # skip the line. Otherwise copy it to the output file.
        if inside_events and bytes_to_truncate > 0:
          bytes_to_truncate -= len(line)
        else:
          out_file.write(line)

        # All lines after this are events (up until the closing square
        # bracket).
        if line.startswith('"events": ['):
          inside_events = True

  sys.stdout.write(
      'Truncated file from %d to %d bytes\n' % (orig_size,
                                                get_file_size(out_path)))

def parse_filesize_str(filesize_str):
  '''Parses a string representation of a file size into a byte value, or None
  on failure'''
  filesize_str = filesize_str.lower()
  m = re.match('([0-9\.]+)([km]?)', filesize_str)

  if not m:
    return None

  # Try to parse as decimal (regex above accepts some invalid decimals too).
  float_value = 0.0
  try:
    float_value = float(m.group(1))
  except ValueError:
    return None

  kSuffixValueBytes = {
    'k': 1024,
    'm': 1024 * 1024,
    '': 1,
  }

  suffix = m.group(2)
  return int(float_value * kSuffixValueBytes[suffix])


def main():
  if len(sys.argv) != 4:
    sys.stderr.write('ERROR: Requires 3 command line arguments\n')
    sys.stderr.write(kUsage)
    sys.exit(1)

  in_path = os.path.normpath(sys.argv[1])
  out_path = os.path.normpath(sys.argv[2])

  if in_path == out_path:
    sys.stderr.write('ERROR: OUTPUT_FILE must be different from INPUT_FILE\n')
    sys.stderr.write(kUsage)
    sys.exit(1)

  size_str = sys.argv[3]
  size_bytes = parse_filesize_str(size_str)
  if size_bytes is None:
    sys.stderr.write('ERROR: Could not parse TRUNCATED_SIZE: %s\n' % size_str)
    sys.stderr.write(kUsage)
    sys.exit(1)

  truncate_log_file(in_path, out_path, size_bytes)


if __name__ == '__main__':
  main()
