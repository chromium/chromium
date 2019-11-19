# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Collects Web IDL definitions in IDL files into a Python object per Blink
component.

Collected IDL definitions are parsed into ASTs and saved into a file with
a label of Blink component.
"""

import optparse

import blink_idl_parser
import utilities
import web_idl


_VALID_COMPONENTS = ('core', 'modules')


def parse_options():
    parser = optparse.OptionParser()
    parser.add_option('--idl-list-file', type='string',
                      help="a file path which lists IDL file paths to process")
    parser.add_option('--component', type='choice', choices=_VALID_COMPONENTS,
                      help="specify a component name")
    parser.add_option('--output', type='string',
                      help="the output file path")
    options, args = parser.parse_args()

    if options.idl_list_file is None:
        parser.error("Specify a file listing IDL files with --idl-list-file.")
    if options.output is None:
        parser.error("Specify the output file path with --output.")
    if options.component is None:
        parser.error("Specify a component with --component.")

    if args:
        parser.error("Unknown arguments {}".format(args))

    return options, args


def main():
    options, _ = parse_options()

    filepaths = utilities.read_idl_files_list_from_file(options.idl_list_file)
    parser = blink_idl_parser.BlinkIDLParser()
    ast_group = web_idl.AstGroup(web_idl.Component(options.component))
    for filepath in filepaths:
        ast_group.add_ast_node(blink_idl_parser.parse_file(parser, filepath))
    ast_group.write_to_file(options.output)


if __name__ == '__main__':
    main()
