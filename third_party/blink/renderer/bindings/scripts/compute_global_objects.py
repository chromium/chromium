#!/usr/bin/env python
#
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Compute global objects.

Global objects are defined by interfaces with [Global] on
their definition: http://heycam.github.io/webidl/#Global

Design document: http://www.chromium.org/developers/design-documents/idl-build
"""

# pylint: disable=relative-import

import optparse
import os
import sys

from utilities import get_file_contents
from utilities import get_interface_extended_attributes_from_idl
from utilities import get_first_interface_name_from_idl
from utilities import read_file_to_list
from utilities import read_pickle_files
from utilities import write_pickle_file


def parse_options():
    usage = 'Usage: %prog [options]  [GlobalObjects.pickle]'
    parser = optparse.OptionParser(usage=usage)
    parser.add_option('--idl-files-list', help='file listing IDL files')
    parser.add_option(
        '--global-objects-component-files',
        action='append',
        help='optionally preceeded input pickle filename.')

    options, args = parser.parse_args()

    if options.idl_files_list is None:
        parser.error(
            'Must specify a file listing IDL files using --idl-files-list.')
    if options.global_objects_component_files is None:
        options.global_objects_component_files = []
    if len(args) != 1:
        parser.error('Must specify an output pickle filename as an argument')

    return options, args


def dict_union(dicts):
    return dict((k, v) for d in dicts for k, v in d.items())


def idl_file_to_global_names(idl_filename):
    """Returns global names, if any, for an IDL file.

    The identifier argument or identifier list argument the [Global] extended
    attribute is declared with define the interface's global names
    (http://heycam.github.io/webidl/#Global).
    """
    full_path = os.path.realpath(idl_filename)
    idl_file_contents = get_file_contents(full_path)
    extended_attributes = get_interface_extended_attributes_from_idl(
        idl_file_contents)
    interface_name = get_first_interface_name_from_idl(idl_file_contents)

    if 'Global' not in extended_attributes:
        return
    global_value = extended_attributes['Global']
    if not global_value:
        raise ValueError(
            '[Global] must take an indentifier or an identifier list.\n' +
            full_path)
    return map(str.strip, global_value.strip('()').split(','))


def idl_files_to_interface_name_global_names(idl_files):
    """Yields pairs (interface_name, global_names) found in IDL files."""
    for idl_filename in idl_files:
        interface_name = get_first_interface_name_from_idl(
            get_file_contents(idl_filename))
        global_names = idl_file_to_global_names(idl_filename)
        if global_names:
            yield interface_name, global_names


################################################################################


def main():
    options, args = parse_options()
    output_global_objects_filename = args.pop()
    interface_name_global_names = dict_union(
        existing_interface_name_global_names
        for existing_interface_name_global_names in read_pickle_files(
            options.global_objects_component_files))

    # File paths of input IDL files are passed in a file, which is generated at
    # GN time. It is OK because the target IDL files themselves are static.
    idl_files = read_file_to_list(options.idl_files_list)
    interface_name_global_names.update(
        idl_files_to_interface_name_global_names(idl_files))

    write_pickle_file(output_global_objects_filename,
                      interface_name_global_names)


if __name__ == '__main__':
    sys.exit(main())
