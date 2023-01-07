#!/usr/bin/env python
#
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
"""Generate event interfaces .json5 file (EventInterfaces.json5).

The event interfaces .json5 file contains a list of all Event interfaces, i.e.,
all interfaces that inherit from Event, including Event itself,
together with certain extended attributes.

Paths are in POSIX format, and relative to the repository root.

This list is used in core/ to generate EventFactory and EventNames.
The .json5 format is documented in build/scripts/json5_generator.py.
"""

from optparse import OptionParser
import os
import posixpath
import sys

from utilities import (get_file_contents, get_first_interface_name_from_idl,
                       read_file_to_list, write_file,
                       get_interface_extended_attributes_from_idl)

EXPORTED_EXTENDED_ATTRIBUTES = (
    'ImplementedAs',
    'RuntimeEnabled',
)
module_path = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT_DIR = os.path.normpath(
    os.path.join(module_path, os.pardir, os.pardir, os.pardir, os.pardir,
                 os.pardir))


def parse_options():
    parser = OptionParser()
    parser.add_option(
        '--event-idl-files-list', help='file listing event IDL files')
    parser.add_option('--event-interfaces-file', help='output file')
    parser.add_option(
        '--suffix',
        help=
        'specify a suffix to the namespace, i.e., "Modules". Default is None.')
    parser.add_option(
        '--export-macro',
        help='specify an export to use for the symbols, i.e.,'
        '"MODULES_EXPORT". Default is suffix_EXPORT if --suffix is present or '
        'CORE_EXPORT if not.')

    options, args = parser.parse_args()
    if options.event_idl_files_list is None:
        parser.error(
            'Must specify a file listing event IDL files using --event-idl-files-list.'
        )
    if options.event_interfaces_file is None:
        parser.error(
            'Must specify an output file using --event-interfaces-file.')
    if args:
        parser.error('No arguments allowed, but %d given.' % len(args))
    return options


def write_event_interfaces_file(event_idl_files, destination_filename, suffix,
                                export_macro):
    def interface_line(full_path):
        relative_dir_local = os.path.dirname(
            os.path.relpath(full_path, REPO_ROOT_DIR))
        relative_dir_posix = relative_dir_local.replace(os.sep, posixpath.sep)

        idl_file_contents = get_file_contents(full_path)
        interface_name = get_first_interface_name_from_idl(idl_file_contents)
        extended_attributes = get_interface_extended_attributes_from_idl(
            idl_file_contents)
        extended_attributes_list = [(name, extended_attributes[name])
                                    for name in EXPORTED_EXTENDED_ATTRIBUTES
                                    if name in extended_attributes]

        return (relative_dir_posix, interface_name, extended_attributes_list)

    lines = ['{', 'metadata: {', '  namespace: "event_interface_names",']

    if suffix:
        lines.append('  suffix: "' + suffix + '",')

    if export_macro:
        lines.append('  export: "%s",' % export_macro)
    elif suffix:
        lines.append('  export: "%s_EXPORT",' % suffix.upper())
    else:
        lines.append('  export: "CORE_EXPORT",')
    lines.extend(['},', 'data: ['])
    interface_lines = [
        interface_line(event_idl_file) for event_idl_file in event_idl_files
    ]
    interface_lines.sort()
    for relative_dir, name, attributes in interface_lines:
        lines.extend([
            '  {',
            '    name: "%s",' % name,
            '    interfaceHeaderDir: "%s",' % relative_dir
        ])
        for param, value in attributes:
            if param == 'RuntimeEnabled':
                value += 'Enabled'
            lines.append('    %s: "%s",' % (param, value))
        lines.append('  },')
    lines.extend([']', '}'])
    write_file('\n'.join(lines), destination_filename)


################################################################################


def main():
    options = parse_options()
    event_idl_files = read_file_to_list(options.event_idl_files_list)
    write_event_interfaces_file(event_idl_files, options.event_interfaces_file,
                                options.suffix, options.export_macro)


if __name__ == '__main__':
    sys.exit(main())
