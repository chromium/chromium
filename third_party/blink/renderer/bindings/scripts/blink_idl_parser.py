# Copyright (C) 2013 Google Inc. All rights reserved.
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
"""Parser for Blink IDL.

The parser uses the PLY (Python Lex-Yacc) library to build a set of parsing
rules which understand the Blink dialect of Web IDL.
It derives from a standard Web IDL parser, overriding rules where Blink IDL
differs syntactically or semantically from the base parser, or where the base
parser diverges from the Web IDL standard.

Web IDL:
    http://www.w3.org/TR/WebIDL/
Web IDL Grammar:
    http://www.w3.org/TR/WebIDL/#idl-grammar
PLY:
    http://www.dabeaz.com/ply/

Design doc:
http://www.chromium.org/developers/design-documents/idl-compiler#TOC-Front-end
"""

# Disable check for line length and Member as Function due to how grammar rules
# are defined with PLY
#
# pylint: disable=R0201
# pylint: disable=C0301
#
# Disable attribute validation, as lint can't import parent class to check
# pylint: disable=E1101
#

from __future__ import print_function

import os.path
import sys

# PLY is in Chromium src/third_party/ply
module_path, module_name = os.path.split(__file__)
third_party = os.path.join(module_path, os.pardir, os.pardir, os.pardir,
                           os.pardir)
# Insert at front to override system libraries, and after path[0] == script dir
sys.path.insert(1, third_party)
from ply import yacc

# Base parser is in Chromium src/tools/idl_parser
tools_dir = os.path.join(module_path, os.pardir, os.pardir, os.pardir,
                         os.pardir, os.pardir, 'tools')
sys.path.append(tools_dir)
from idl_parser.idl_parser import IDLParser  # pylint: disable=import-error
from idl_parser.idl_parser import ParseFile as parse_file

from blink_idl_lexer import BlinkIDLLexer
import blink_idl_lexer


class BlinkIDLParser(IDLParser):
    def __init__(
            self,
            # common parameters
            debug=False,
            # local parameters
            rewrite_tables=False,
            # idl_parser parameters
            lexer=None,
            verbose=False,
            mute_error=False,
            # yacc parameters
            outputdir='',
            optimize=True,
            write_tables=False,
            picklefile=None):
        if debug:
            # Turn off optimization and caching, and write out tables,
            # to help debugging
            optimize = False
            outputdir = None
            picklefile = None
            write_tables = True
        if outputdir:
            picklefile = picklefile or os.path.join(outputdir,
                                                    'parsetab.pickle')
            if rewrite_tables:
                try:
                    os.unlink(picklefile)
                except OSError:
                    pass

        lexer = lexer or BlinkIDLLexer(
            debug=debug, outputdir=outputdir, optimize=optimize)
        self.lexer = lexer
        self.tokens = lexer.KnownTokens()
        # Optimized mode substantially decreases startup time (by disabling
        # error checking), and also allows use of Python's optimized mode.
        # See: Using Python's Optimized Mode
        # http://www.dabeaz.com/ply/ply.html#ply_nn38
        #
        # |picklefile| allows simpler importing than |tabmodule| (parsetab.py),
        # as we don't need to modify sys.path; virtually identical speed.
        # See: CHANGES, Version 3.2
        # http://ply.googlecode.com/svn/trunk/CHANGES
        self.yaccobj = yacc.yacc(
            module=self,
            debug=debug,
            optimize=optimize,
            write_tables=write_tables,
            picklefile=picklefile)
        # See IDLParser.__init__() why we set defaulted_states.
        self.yaccobj.defaulted_states = {}
        self.parse_debug = debug
        self.verbose = verbose
        self.mute_error = mute_error
        self._parse_errors = 0
        self._parse_warnings = 0
        self._last_error_msg = None
        self._last_error_lineno = 0
        self._last_error_pos = 0


################################################################################


def main(argv):
    # If file itself executed, cache lex/parse tables
    try:
        outputdir = argv[1]
    except IndexError as err:
        print('Usage: %s OUTPUT_DIR' % argv[0])
        return 1
    blink_idl_lexer.main(argv)
    # Important: rewrite_tables=True causes the cache file to be deleted if it
    # exists, thus making sure that PLY doesn't load it instead of regenerating
    # the parse table.
    parser = BlinkIDLParser(outputdir=outputdir, rewrite_tables=True)


if __name__ == '__main__':
    sys.exit(main(sys.argv))
