#!/usr/bin/env vpython3
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import stat
import sys

sys.path.append(
    os.path.join(os.path.dirname(__file__), os.pardir, os.pardir, os.pardir,
                 'build', 'android', 'gyp'))
from util import build_utils


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

    return parser.parse_args(args=args)


def _find_test_executables(args):
    exes = set()
    input_filepath = args.rust_test_executables
    with open(input_filepath) as f:
        for line in f:
            exe_name = line.strip()
            # TODO(https://crbug.com/1271215): Append ".exe" extension when
            # *targeting* Windows.  (The "targeting" part means that we can't
            # just detect whether the build is *hosted* on Windows.)
            if exe_name in exes:
                raise ValueError("Duplicate entry ('{}') in {}".format(
                    exe_name, input_filepath))
            exes.add(exe_name)
    if not exes:
        raise ValueError("Unexpectedly empty file: {}".format(input_filepath))
    exes = sorted(exes)  # For stable results in unit tests.
    return exes


def _validate_if_test_executables_exist(exes):
    for exe in exes:
        if not os.path.isfile(exe):
            raise ValueError("File not found: {}".format(exe))


def _generate_script(args, should_validate_if_exes_exist=True):
    res = '#!/bin/bash\n'

    script_dir = os.path.dirname(args.script_path)
    res += 'SCRIPT_DIR=`dirname $0`\n'

    exe_dir = os.path.relpath(args.exe_dir, start=script_dir)
    exe_dir = os.path.normpath(exe_dir)
    res += 'EXE_DIR="$SCRIPT_DIR/{}"\n'.format(exe_dir)

    generator_script_dir = os.path.dirname(__file__)
    lib_dir = os.path.relpath(generator_script_dir, script_dir)
    lib_dir = os.path.normpath(lib_dir)
    res += 'LIB_DIR="$SCRIPT_DIR/{}"\n'.format(lib_dir)

    exes = _find_test_executables(args)
    if should_validate_if_exes_exist:
        _validate_if_test_executables_exist(exes)

    res += 'env vpython3 "$LIB_DIR/rust_main_program.py" \\\n'
    for exe in exes:
        res += '    "--rust-test-executable=$EXE_DIR/{}" \\\n'.format(exe)
    res += '    "$@"'

    return res


def _main():
    args = _parse_args(sys.argv[1:])

    # AtomicOutput will ensure we only write to the file on disk if what we give
    # to write() is different than what's currently on disk.
    with build_utils.AtomicOutput(args.script_path) as f:
        f.write(_generate_script(args).encode())

    # chmod a+x
    st = os.stat(args.script_path)
    if (not st.st_mode & stat.S_IXUSR) or (not st.st_mode & stat.S_IXGRP) or \
       (not st.st_mode & stat.S_IXOTH):
        os.chmod(args.script_path,
                 st.st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)


if __name__ == '__main__':
    _main()
