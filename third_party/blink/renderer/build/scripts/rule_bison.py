#!/usr/bin/env python
#
# Copyright (C) 2009 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# Copyright 2009 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# usage: rule_bison.py INPUT_FILE OUTPUT_DIR BISON_EXE
# INPUT_FILE is a path to *.y such as xpath_grammar.y.
# OUTPUT_DIR is where the bison-generated .cc and .h files should be placed.

import os
import os.path
import subprocess
import sys

from blinkbuild.name_style_converter import NameStyleConverter


def modify_file(path, prefix_lines, suffix_lines, replace_list=[]):
    prefix_lines = map(lambda s: s + '\n', prefix_lines)
    suffix_lines = map(lambda s: s + '\n', suffix_lines)
    with open(path, 'r') as f:
        old_lines = f.readlines()
    for i in range(len(old_lines)):
        for src, dest in replace_list:
            old_lines[i] = old_lines[i].replace(src, dest)
    new_lines = prefix_lines + old_lines + suffix_lines
    with open(path, 'w') as f:
        f.writelines(new_lines)


def main():
    assert len(sys.argv) == 4

    input_file = sys.argv[1]
    output_dir = sys.argv[2]
    bison_exe = sys.argv[3]

    path_to_bison = os.path.split(bison_exe)[0]
    if path_to_bison:
        # Make sure this path is in the path so that it can find its auxiliary
        # binaries (in particular, m4). To avoid other 'm4's being found, insert
        # at head, rather than tail.
        os.environ['PATH'] = path_to_bison + os.pathsep + os.environ['PATH']

    input_name = os.path.basename(input_file)

    # Output name without directory and extension.
    output_basename = os.path.splitext(input_name)[0] + '_generated'

    output_cc = os.path.join(output_dir, output_basename + '.cc')
    BISON_HEADER_EXT = '.hh'
    original_output_h = os.path.join(output_dir,
                                     output_basename + BISON_HEADER_EXT)

    return_code = subprocess.call(
        [bison_exe, '-d', input_file, '-o', output_cc])
    assert return_code == 0
    # If the file doesn't exist, this raise an OSError.
    os.stat(original_output_h)

    # The generated files contain references to the original "foo.hh" for
    # #include and #line. We replace them with "foo.h".
    common_replace_list = [(output_basename + BISON_HEADER_EXT,
                            output_basename + '.h')]

    # Rewrite the generated header with #include guards.
    CLANG_FORMAT_DISABLE_LINE = "// clang-format off"
    output_h = os.path.join(output_dir, output_basename + '.h')
    header_guard = NameStyleConverter(output_h).to_header_guard()
    modify_file(
        original_output_h, [
            CLANG_FORMAT_DISABLE_LINE,
            '#ifndef %s' % header_guard,
            '#define %s' % header_guard
        ], ['#endif  // %s' % header_guard],
        replace_list=common_replace_list)
    os.rename(original_output_h, output_h)

    modify_file(
        output_cc, [CLANG_FORMAT_DISABLE_LINE], [],
        replace_list=common_replace_list)


if __name__ == '__main__':
    main()
