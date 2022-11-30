#!/usr/bin/env python
# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Helper script to inline files as const char[] into a C header file.

Example:

Input file in_a.vert:
1"
2
3\

Input file in_b.frag:
4
5
6

shader_to_header.py "output.h" "in_a.vert" "in_b.frag" will generate this:

#ifndef OUTPUT_H_BSGN3bbEMBDD0ucC
#define OUTPUT_H_BSGN3bbEMBDD0ucC

const char kInAVert[] = "1\"\n2\n3\\\n";

const char kInBFrag[] = "4\n5\n6\n";

#endif  // OUTPUT_H_BSGN3bbEMBDD0ucC


"""

import os.path
import random
import string
import sys

RANDOM_STRING_LENGTH = 16
STRING_CHARACTERS = (string.ascii_uppercase +
                    string.ascii_lowercase +
                    string.digits)

def random_str():
  return ''.join(random.choice(STRING_CHARACTERS)
          for _ in range(RANDOM_STRING_LENGTH))


def escape_text(line):
  # encode('string-escape') doesn't escape double quote so you need to manually
  # escape it.
  return line.encode('string-escape').replace('"', '\\"')


def main():
  if len(sys.argv) < 3:
    print 'Usage: shader_to_header.py <output-file> <input-files...>'
    return 1

  output_path = sys.argv[1]
  include_guard = (os.path.basename(output_path).upper().replace('.', '_') +
                  '_' + random_str())

  with open(output_path, 'w') as output_file:
    output_file.write('#ifndef ' + include_guard + '\n' +
                      '#define ' + include_guard + '\n\n')

    existing_names = set()
    argc = len(sys.argv)
    for i in xrange(2, argc):
      input_path = sys.argv[i]

      with open(input_path, 'r') as input_file:
        # hello_world.vert -> kHelloWorldVert
        const_name = ('k' + os.path.basename(input_path).title()
                     .replace('_', '').replace('.', ''))
        if const_name in existing_names:
          print >> sys.stderr, ('Error: Constant name ' + const_name +
                ' is already used by a previous file. Files with the same' +
                ' name can\'t be inlined into the same header.')
          return 1

        existing_names.add(const_name)
        text = input_file.read()

        inlined = ('const char ' + const_name + '[] = "' + escape_text(text) +
                  '";\n\n');
        output_file.write(inlined)

    output_file.write('#endif  // ' + include_guard + '\n')

  return 0


if __name__ == '__main__':
  sys.exit(main())
