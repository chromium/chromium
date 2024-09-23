#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import sys

from mojom.format.format import mojom_format


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--dry-run',
      action='store_true',
      help=(
          'Runs the formatter and reports if any changes would be made. '
          'Returns on stdout the list of files that are not formatted '
          'properly. Exits non-zero if any files are not formatted correctly.'))
  parser.add_argument(
      'files',
      nargs='*',
      help=('The files to format. If there are no arguments, reads the content '
            'from stdin and writes the formatted result on stdout.'))
  args = parser.parse_args()

  if not args.files:
    try:
      print(mojom_format(filename="stdin", contents=sys.stdin.read()), end="")
    except Exception as e:
      print('Failed to format the data from stdin', file=sys.stderr)
      raise e

  exit_code = 0
  for file in args.files:
    try:
      output = mojom_format(file)
      if args.dry_run:
        with open(file, 'r') as f:
          current = f.read()
          if current != output:
            print(file)
            exit_code = 1
      else:
        with open(file, 'w') as f:
          f.write(output)
    except Exception as e:
      print(f'Failed to format {file}', file=sys.stderr)
      raise e

  return exit_code


if __name__ == '__main__':
  sys.exit(main())
