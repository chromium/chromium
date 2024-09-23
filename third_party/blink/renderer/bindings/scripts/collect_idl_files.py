# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Collects Web IDL definitions in IDL files into a Python object per Blink
component.

Collected IDL definitions are parsed into ASTs and saved into a file with
a label of Blink component.
"""

import optparse
import shlex

import web_idl

from idl_parser import idl_parser
from idl_parser import idl_lexer

_VALID_COMPONENTS = ('core', 'modules', 'extensions_chromeos',
                     'extensions_webview')


def parse_options():
    parser = optparse.OptionParser()
    parser.add_option(
        '--idl_list_file',
        type='string',
        help="a file path which lists IDL file paths to process")
    parser.add_option(
        '--component',
        type='choice',
        choices=_VALID_COMPONENTS,
        help="specify a component name")
    parser.add_option(
        '--for_testing',
        action='store_true',
        help=("specify this option if the IDL definitions are meant for "
              "testing only"))
    parser.add_option('--output', type='string', help="the output file path")
    options, args = parser.parse_args()

    required_option_names = ('idl_list_file', 'component', 'output')
    for opt_name in required_option_names:
        if getattr(options, opt_name) is None:
            parser.error("--{} is a required option.".format(opt_name))

    if args:
        parser.error("Unknown arguments {}".format(args))

    return options, args


def main():
    options, _ = parse_options()

    with open(options.idl_list_file, encoding='utf-8') as idl_list_file:
        filepaths = shlex.split(idl_list_file)
    lexer = idl_lexer.IDLLexer()
    parser = idl_parser.IDLParser(lexer)
    ast_group = web_idl.AstGroup(
        component=web_idl.Component(options.component),
        for_testing=bool(options.for_testing))
    for filepath in filepaths:
        ast_group.add_ast_node(idl_parser.ParseFile(parser, filepath))
    ast_group.write_to_file(options.output)


if __name__ == '__main__':
    main()
