#!/usr/bin/env vpython3
# Copyright (c) 2011 Google Inc. All rights reserved.
# Copyright (C) 2010 Chris Jerdonek (cjerdonek@webkit.org)
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

import os
import sys

from blinkpy.common import path_finder
from blinkpy.common.system.filesystem import FileSystem
path_finder.add_typ_dir_to_sys_path()

import typ


def main():
    finder = path_finder.PathFinder(FileSystem())
    os.environ['COVERAGE_RCFILE'] = finder.path_from_blink_tools('.coveragerc')
    return typ.main(
        coverage_source=[
            # Do not measure coverage of executables under `blink/tools`.
            finder.path_from_blink_tools('blinkpy'),
        ],
        coverage_omit=[
            '*/blinkpy/presubmit/*',
            # Exclude non-production code.
            '*/PRESUBMIT.py',
            '*_unittest.py',
            '*mock*.py',
            # Temporarily exclude python2 code that Coverage.py cannot import in
            # python3.
            '*/blinkpy/style/*',
        ],
        top_level_dirs=[
            path_finder.get_blink_tools_dir(),
            path_finder.get_build_scripts_dir(),
        ],
        path=[
            path_finder.get_blinkpy_thirdparty_dir(),
            finder.path_from_chromium_base('third_party', 'pyjson5', 'src')
        ])


if __name__ == "__main__":
    sys.exit(main())
