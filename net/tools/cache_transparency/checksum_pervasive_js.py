# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Calculates the checksum for pervasive.js.

Usage:
  python3 checksum_pervasive_js.py pervasive.js

"""

import re
import sys
import pervasive_checksum


def main(argv):
  if len(argv) != 2:
    print('Supply the path to pervasive.js as the sole command-line argument')
    sys.exit(1)

  filename = argv[1]
  with open(filename, mode='rb') as f:
    raw_body = f.read()

  headers = []
  with open(f'{filename}.mock-http-headers', mode='r') as lines:
    for line in lines:
      if line.startswith('HTTP/'):
        continue
      match = re.match(r'^([A-Za-z0-9-]+): *(.*)$', line)
      if not match:
        print(f'Failed to parse header line: {line}')
        continue
      headers.append((match.group(1), match.group(2)))

  print(pervasive_checksum.calculate_checksum(headers, raw_body))


if __name__ == '__main__':
  main(sys.argv)
