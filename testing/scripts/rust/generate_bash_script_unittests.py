#!/usr/bin/env vpython3

# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import unittest

from generate_bash_script import _parse_args
from generate_bash_script import _generate_script


class Tests(unittest.TestCase):
    def test_parse_args(self):
        raw_args = [
            '--script-path=./bin/run_foobar', '--exe-dir=.',
            '--rust-test-executable=foo', '--rust-test-executable=bar'
        ]
        parsed_args = _parse_args(raw_args)
        self.assertEqual('./bin/run_foobar', parsed_args.script_path)
        self.assertEqual('.', parsed_args.exe_dir)
        self.assertEqual(['foo', 'bar'], parsed_args.rust_test_executables)

    def test_generate_script(self):
        lib_dir = os.path.dirname(__file__)
        out_dir = os.path.join(lib_dir, '../../../out/rust')
        args = type('', (), {})()
        args.script_path = os.path.join(out_dir, 'bin/run_foo_bar')
        args.exe_dir = out_dir
        args.rust_test_executables = ['foo', 'bar']

        actual = _generate_script(args)
        expected = '''
#!/bin/bash
SCRIPT_DIR=`dirname $0`
EXE_DIR="$SCRIPT_DIR/.."
LIB_DIR="$SCRIPT_DIR/../../../testing/scripts/rust"
env vpython3 "$LIB_DIR/rust_main_program.py" \\
    "--rust-test-executable=$EXE_DIR/foo" \\
    "--rust-test-executable=$EXE_DIR/bar" \\
    "$@"
'''.strip()

        self.assertEqual(expected, actual)


if __name__ == '__main__':
    unittest.main()
