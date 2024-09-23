#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re
import shutil
import sys

load_regexp = re.compile(r'^\s*utils\.load\([\'"]([^\'"]+)[\'"]\);\s*$')


def resolve_loads(output_file, input_lines, loaded_files, load_root):
  for line in input_lines:
    load_match = load_regexp.match(line)
    if not load_match:
      output_file.write(line)
      continue
    load_file(output_file, load_match.group(1), loaded_files, load_root)


def load_file(output_file, input_file, loaded_files, load_root):
  if input_file in loaded_files:
    sys.exit("Recursive load of '{}'".format(input_file))
  loaded_files.add(input_file)
  output_file.write("\n// Loaded from '{}':\n".format(input_file))
  with open(os.path.join(load_root, input_file)) as file:
    resolve_loads(output_file, file.readlines(), loaded_files, load_root)


def generate_content(output_file, input_file, load_root):
  # The fuzzer does not provide the same methods on 'utils' as the
  # inspector-test executable. Thus mock out non-existing ones via a proxy.
  output_file.write("""
utils = new Proxy(utils, {
    get: function(target, prop) {
      if (prop in target) return target[prop];
      return i=>i;
    }
  });
""".lstrip())

  # Always prepend the 'protocol-test.js' file, which is always loaded first
  # by the test runner for inspector tests.
  protocol_test_file = os.path.join('test', 'inspector', 'protocol-test.js')
  load_file(output_file, protocol_test_file, set(), load_root)

  # Then load the actual input file, inlining all recursively loaded files.
  load_file(output_file, input_file, set(), load_root)


def main():
  if len(sys.argv) != 3:
    print(
        'Usage: {} <path to input directory> <path to output directory>'.format(
            sys.argv[0]))
    sys.exit(1)

  input_root = sys.argv[1]
  output_root = sys.argv[2]
  # Start with a clean output directory.
  if os.path.exists(output_root):
    shutil.rmtree(output_root)
  os.makedirs(output_root)

  # Loaded files are relative to the v8 root, which is two levels above the
  # inspector test directory.
  load_root = os.path.dirname(os.path.dirname(os.path.normpath(input_root)))

  for parent, _, files in os.walk(input_root):
    for filename in files:
      if filename.endswith('.js'):
        output_file = os.path.join(output_root, filename)
        output_dir = os.path.dirname(output_file)
        if not os.path.exists(output_dir):
          os.makedirs(os.path.dirname(output_file))
        with open(output_file, 'w') as output_file:
          abs_input_file = os.path.join(parent, filename)
          rel_input_file = os.path.relpath(abs_input_file, load_root)
          generate_content(output_file, rel_input_file, load_root)

  # Done.
  sys.exit(0)


if __name__ == '__main__':
  main()
