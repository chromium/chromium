#!/usr/bin/env vpython3
# Copyright (C) 2010 Google Inc.  All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

import sys

from blinkpy.common import path_finder
path_finder.add_typ_dir_to_sys_path()

import typ


def create_argument_parser():
    argument_parser = typ.ArgumentParser()
    argument_parser.add_argument('--skip-unit-tests',
                                 default=False,
                                 action='store_true',
                                 help='Skip running unit tests.')
    return argument_parser


def main(argv):
    argument_parser = create_argument_parser()

    # Run bindings unit tests.
    runner = typ.Runner()
    runner.parse_args(argument_parser, argv[1:])
    if argument_parser.exit_status is not None:
        return argument_parser.exit_status

    args = runner.args
    args.top_level_dirs = [
        path_finder.get_bindings_scripts_dir(),
        path_finder.get_build_scripts_dir(),
    ]
    if not args.skip_unit_tests:
        return_code, _, _ = runner.run()
        if return_code != 0:
            return return_code

    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
