#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Generate initPartialInterfacesInModules(), which registers partial
interfaces in modules to core interfaces.
"""

# pylint: disable=relative-import

from __future__ import print_function

from optparse import OptionParser
import os
import posixpath
import sys

from utilities import get_file_contents
from utilities import get_first_interface_name_from_idl
from utilities import read_idl_files_list_from_file
from utilities import should_generate_impl_file_from_idl
from utilities import write_file
from v8_utilities import build_basename

_COPYRIGHT = """// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
"""

_INIT_PARTIAL_INTERFACE = """%s
%s

namespace blink {

void InitPartialInterfacesInModules() {
%s
}

} // namespace blink
"""


def parse_options():
    usage = 'Usage: %prog [options]'
    parser = OptionParser(usage=usage)
    parser.add_option(
        '--idl-files-list',
        help='a text file containing the IDL file paths, so the command '
        'line doesn\'t exceed OS length limits.')
    parser.add_option('--output')

    options, args = parser.parse_args()
    if options.output is None:
        parser.error('Must specify output file using --output.')
    if options.idl_files_list is None:
        parser.error(
            'Must specify a list of IDL files using --idl-files-list.')
    return options


def extract_meta_data(file_paths):
    """Extracts interface name from each IDL file."""
    meta_data_list = []

    for file_path in file_paths:
        if not file_path.endswith('.idl'):
            print('WARNING: non-IDL file passed: "%s"' % file_path)
            continue
        if not os.path.exists(file_path):
            print('WARNING: file not found: "%s"' % file_path)
            continue

        idl_file_contents = get_file_contents(file_path)
        if not should_generate_impl_file_from_idl(idl_file_contents):
            continue

        # Extract interface name from file content
        basename = get_first_interface_name_from_idl(idl_file_contents)

        meta_data = {
            'basename': basename,
        }
        meta_data_list.append(meta_data)

    return meta_data_list


def main():
    options = parse_options()

    idl_file_names = read_idl_files_list_from_file(options.idl_files_list)

    meta_data_list = extract_meta_data(idl_file_names)
    interface_names = [
        'V8%sPartial' % meta_data['basename'] for meta_data in meta_data_list
    ]
    interface_names.sort()

    includes = [
        '#include "third_party/blink/renderer/bindings/modules/v8/%s.h"' %
        build_basename(interface_name) for interface_name in interface_names
    ]
    initialize_calls = [
        '  %s::Initialize();' % interface_name
        for interface_name in interface_names
    ]

    content = _INIT_PARTIAL_INTERFACE % (_COPYRIGHT, '\n'.join(includes),
                                         '\n'.join(initialize_calls))

    write_file(content, options.output)


if __name__ == '__main__':
    sys.exit(main())
