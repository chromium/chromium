#!/usr/bin/env vpython3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import stat
import sys

sys.path.append(
    os.path.join(os.path.dirname(__file__), os.pardir, os.pardir, os.pardir,
                 'build'))
# //build imports.
import action_helpers


def _parse_args(args):
    description = 'Generator of bash scripts that can invoke the Python ' \
                  'library for running Rust unit tests with support for ' \
                  'Chromium test filters, sharding, and test output.'
    parser = argparse.ArgumentParser(description=description)
    parser.add_argument('--script-path',
                        dest='script_path',
                        help='Where to write the bash script.',
                        metavar='FILEPATH',
                        required=True)
    parser.add_argument('--exe-dir',
                        dest='exe_dir',
                        help='Directory where the wrapped executables are',
                        metavar='PATH',
                        required=True)
    parser.add_argument('--rust-test-executables',
                        dest='rust_test_executables',
                        help='File listing one or more executables to wrap. ' \
                             '(basenames - no .exe extension or directory)',
                        metavar='FILEPATH',
                        required=True)
    parser.add_argument('--make-bat',
                        action='store_true',
                        help='Generate a .bat file instead of a bash script')
    return parser.parse_args(args=args)


def _find_test_executables(args):
    exes = set()
    input_filepath = args.rust_test_executables
    with open(input_filepath) as f:
        for line in f:
            exe_name = line.strip()
            # Append ".exe" extension when *targeting* Windows, not when this
            # script is running on windows. We do that by using `args.make_bat`
            # as a signal.
            if exe_name in exes:
                raise ValueError(
                    f'Duplicate entry "{exe_name}" in {input_filepath}')
            if args.make_bat:
                suffix = '.exe'
            else:
                suffix = ''
            exes.add(f'{exe_name}{suffix}')
    if not exes:
        raise ValueError(f'Unexpectedly empty file: {input_filepath}')
    exes = sorted(exes)  # For stable results in unit tests.
    return exes


def _validate_if_test_executables_exist(exes):
    for exe in exes:
        if not os.path.isfile(exe):
            raise ValueError(f'File not found: {exe}')


def _generate_script(args, should_validate_if_exes_exist=True):
    THIS_DIR = os.path.abspath(os.path.dirname(__file__))
    GEN_SCRIPT_DIR = os.path.dirname(args.script_path)

    # Path from the .bat or bash script to the test exes.
    exe_dir = os.path.relpath(args.exe_dir, start=GEN_SCRIPT_DIR)
    exe_dir = os.path.normpath(exe_dir)

    # Path from the .bat or bash script to the python main.
    main_dir = os.path.relpath(THIS_DIR, start=GEN_SCRIPT_DIR)
    main_dir = os.path.normpath(main_dir)

    exes = _find_test_executables(args)
    if should_validate_if_exes_exist:
        _validate_if_test_executables_exist(exes)

    if args.make_bat:
        res = '@echo off\n'
        res += f'vpython3 "%~dp0\\{main_dir}\\rust_main_program.py" ^\n'
        for exe in exes:
            res += f'    "--rust-test-executable=%~dp0\\{exe_dir}\\{exe}" ^\n'
        res += '    %*'
    else:
        res = '#!/bin/bash\n'
        res += (f'env vpython3 '
                f'"$(dirname $0)/{main_dir}/rust_main_program.py" \\\n')
        for exe in exes:
            res += (
                f'    '
                f'"--rust-test-executable=$(dirname $0)/{exe_dir}/{exe}" \\\n')
        res += '    "$@"'
    return res


def _main():
    args = _parse_args(sys.argv[1:])

    # atomic_output will ensure we only write to the file on disk if what we
    # give to write() is different than what's currently on disk.
    with action_helpers.atomic_output(args.script_path) as f:
        f.write(_generate_script(args).encode())

    # chmod a+x
    st = os.stat(args.script_path)
    if (not st.st_mode & stat.S_IXUSR) or (not st.st_mode & stat.S_IXGRP) or \
       (not st.st_mode & stat.S_IXOTH):
        os.chmod(args.script_path,
                 st.st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)


if __name__ == '__main__':
    _main()
