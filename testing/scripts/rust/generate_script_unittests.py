#!/usr/bin/env vpython3

# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import tempfile
import unittest

from pyfakefs import fake_filesystem_unittest

from generate_script import _parse_args
from generate_script import _generate_script


class Tests(fake_filesystem_unittest.TestCase):
    def test_parse_args(self):
        raw_args = [
            '--script-path=./bin/run_foobar', '--exe-dir=.',
            '--rust-test-executables=metadata.json'
        ]
        parsed_args = _parse_args(raw_args)
        self.assertEqual('./bin/run_foobar', parsed_args.script_path)
        self.assertEqual('.', parsed_args.exe_dir)
        self.assertEqual('metadata.json', parsed_args.rust_test_executables)

    def test_generate_script(self):
        lib_dir = os.path.dirname(__file__)
        out_dir = os.path.join(lib_dir, '../../../out/rust')
        args = type('', (), {})()
        args.make_bat = False
        args.script_path = os.path.join(out_dir, 'bin/run_foo_bar')
        args.exe_dir = out_dir

        # pylint: disable=unexpected-keyword-arg
        with tempfile.NamedTemporaryFile(delete=False,
                                         mode='w',
                                         encoding='utf-8') as f:
            filepath = f.name
            f.write('foo\n')
            f.write('bar\n')
        try:
            args.rust_test_executables = filepath
            actual = _generate_script(args,
                                      should_validate_if_exes_exist=False)
        finally:
            os.remove(filepath)

        expected = """
#!/bin/bash
env vpython3 \
"$(dirname $0)/../../../testing/scripts/rust/rust_main_program.py" \\
    "--rust-test-executable=$(dirname $0)/../bar" \\
    "--rust-test-executable=$(dirname $0)/../foo" \\
    "$@"
""".strip()

        self.assertEqual(expected, actual)

    def test_generate_bat(self):
        lib_dir = os.path.dirname(__file__)
        out_dir = os.path.join(lib_dir, '../../../out/rust')
        args = type('', (), {})()
        args.make_bat = True
        args.script_path = os.path.join(out_dir, 'bin/run_foo_bar')
        args.exe_dir = out_dir

        # pylint: disable=unexpected-keyword-arg
        with tempfile.NamedTemporaryFile(delete=False,
                                         mode='w',
                                         encoding='utf-8') as f:
            filepath = f.name
            f.write('foo\n')
            f.write('bar\n')
        try:
            args.rust_test_executables = filepath
            actual = _generate_script(args,
                                      should_validate_if_exes_exist=False)
        finally:
            os.remove(filepath)

        expected = """
@echo off
vpython3 "%~dp0\\../../../testing/scripts/rust\\rust_main_program.py" ^
    "--rust-test-executable=%~dp0\\..\\bar.exe" ^
    "--rust-test-executable=%~dp0\\..\\foo.exe" ^
    %*
""".strip()

        self.assertEqual(expected, actual)


if __name__ == '__main__':
    unittest.main()
