#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Generates MSVC import libraries from .def files.  Assumes MSVC environment
# has been loaded.

import optparse
import os
import subprocess


def main():
    parser = optparse.OptionParser(usage='usage: %prog [options] input')
    parser.add_option('-o',
                      '--output',
                      dest='output',
                      default=None,
                      help=('output location'))
    (options, args) = parser.parse_args()

    if options.output == None:
        parser.error('Output location not specified')
    if len(args) == 0:
        parser.error('No inputs specified')

    # Make sure output directory exists.
    if not os.path.exists(options.output):
        os.makedirs(options.output)

    # Run lib.exe on each input def file.
    for input_path in args:
        input_name = os.path.basename(input_path)
        input_root = os.path.splitext(input_name)[0]
        output_path = os.path.join(options.output, input_root + '.lib')
        subprocess.call([
            'lib', '/nologo', '/machine:X86', '/def:' + input_path,
            '/out:' + output_path
        ])


if __name__ == '__main__':
    main()
