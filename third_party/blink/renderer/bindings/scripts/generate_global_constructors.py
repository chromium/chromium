#!/usr/bin/python
#
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generates interface properties on global objects.

Concretely these are implemented as "constructor attributes", meaning
"attributes whose name ends with Constructor" (special-cased by code generator),
hence "global constructors" for short.

For reference on global objects, see:
http://heycam.github.io/webidl/#Global
http://heycam.github.io/webidl/#Exposed

Design document: http://www.chromium.org/developers/design-documents/idl-build
"""

# pylint: disable=relative-import

import itertools
import optparse
import os
import re
import sys

from collections import defaultdict
from utilities import get_file_contents
from utilities import get_first_interface_name_from_idl
from utilities import get_interface_exposed_arguments
from utilities import get_interface_extended_attributes_from_idl
from utilities import is_non_legacy_callback_interface_from_idl
from utilities import is_interface_mixin_from_idl
from utilities import read_file_to_list
from utilities import read_pickle_file
from utilities import should_generate_impl_file_from_idl
from utilities import write_file
from v8_utilities import EXPOSED_EXECUTION_CONTEXT_METHOD

interface_name_to_global_names = {}
global_name_to_constructors = defaultdict(list)


HEADER_FORMAT = """// Stub header file for {{idl_basename}}
// Required because the IDL compiler assumes that a corresponding header file
// exists for each IDL file.
"""

def parse_options():
    parser = optparse.OptionParser()
    parser.add_option('--idl-files-list', help='file listing IDL files')
    parser.add_option('--global-objects-file', help='pickle file of global objects')
    options, args = parser.parse_args()

    if options.idl_files_list is None:
        parser.error('Must specify a file listing IDL files using --idl-files-list.')
    if options.global_objects_file is None:
        parser.error('Must specify a pickle file of global objects using --global-objects-file.')

    return options, args


def flatten_list(iterable):
    return list(itertools.chain.from_iterable(iterable))


def interface_name_to_constructors(interface_name):
    """Returns constructors for an interface."""
    global_names = interface_name_to_global_names[interface_name]
    return flatten_list(global_name_to_constructors[global_name]
                        for global_name in global_names)


def record_global_constructors(idl_filename):
    full_path = os.path.realpath(idl_filename)
    idl_file_contents = get_file_contents(full_path)
    extended_attributes = get_interface_extended_attributes_from_idl(idl_file_contents)
    interface_name = get_first_interface_name_from_idl(idl_file_contents)

    # An interface property is produced for every non-callback interface
    # that does not have [NoInterfaceObject].
    # http://heycam.github.io/webidl/#es-interfaces
    if (not should_generate_impl_file_from_idl(idl_file_contents) or
            is_non_legacy_callback_interface_from_idl(idl_file_contents) or
            is_interface_mixin_from_idl(idl_file_contents) or
            'NoInterfaceObject' in extended_attributes):
        return

    exposed_arguments = get_interface_exposed_arguments(idl_file_contents)
    if exposed_arguments:
        # Exposed(Arguments) case
        for argument in exposed_arguments:
            if 'RuntimeEnabled' in extended_attributes:
                raise ValueError('RuntimeEnabled should not be used with Exposed(Arguments)')
            attributes = extended_attributes.copy()
            attributes['RuntimeEnabled'] = argument['runtime_enabled']
            new_constructors_list = generate_global_constructors_list(interface_name, attributes)
            global_name_to_constructors[argument['exposed']].extend(new_constructors_list)
    else:
        # Exposed=env or Exposed=(env1,...) case
        exposed_value = extended_attributes.get('Exposed', 'Window')
        exposed_global_names = map(str.strip, exposed_value.strip('()').split(','))
        new_constructors_list = generate_global_constructors_list(interface_name, extended_attributes)
        for name in exposed_global_names:
            global_name_to_constructors[name].extend(new_constructors_list)


def generate_global_constructors_list(interface_name, extended_attributes):
    extended_attributes_list = [
        name + (('=' + extended_attributes[name]) if extended_attributes[name] else '')
        for name in 'RuntimeEnabled', 'ContextEnabled', 'SecureContext'
        if name in extended_attributes]
    if extended_attributes_list:
        extended_string = '[%s] ' % ', '.join(extended_attributes_list)
    else:
        extended_string = ''

    attribute_string = 'attribute {interface_name}Constructor {interface_name}'.format(interface_name=interface_name)
    attributes_list = [extended_string + attribute_string]

    # In addition to the usual interface property, for every [NamedConstructor]
    # extended attribute on an interface, a corresponding property MUST exist
    # on the ECMAScript global object.
    # http://heycam.github.io/webidl/#NamedConstructor
    if 'NamedConstructor' in extended_attributes:
        named_constructor = extended_attributes['NamedConstructor']
        # Extract function name, namely everything before opening '('
        constructor_name = re.sub(r'\(.*', '', named_constructor)
        # Note the reduplicated 'ConstructorConstructor'
        # FIXME: rename to NamedConstructor
        attribute_string = 'attribute %sConstructorConstructor %s' % (interface_name, constructor_name)
        attributes_list.append(extended_string + attribute_string)

    return attributes_list


def write_global_constructors_partial_interface(interface_name, idl_filename, constructor_attributes_list):
    idl_basename = os.path.basename(idl_filename)
    basename = os.path.splitext(idl_basename)[0]
    # FIXME: replace this with a simple Jinja template
    lines = (['[\n',
              '    ImplementedAs=%s\n' % basename,
              '] partial interface %s {\n' % interface_name] +
             ['    %s;\n' % constructor_attribute
              # FIXME: sort by interface name (not first by extended attributes)
              for constructor_attribute in sorted(constructor_attributes_list)] +
             ['};\n'])
    write_file(''.join(lines), idl_filename)
    header_filename = os.path.splitext(idl_filename)[0] + '.h'
    write_file(HEADER_FORMAT.format(idl_basename=idl_basename), header_filename)


################################################################################

def main():
    options, args = parse_options()

    # File paths of input IDL files are passed in a file, which is generated at
    # GN time. It is OK because the target IDL files are static.
    idl_files = read_file_to_list(options.idl_files_list)

    # Output IDL files (to generate) are passed at the command line, since
    # these are in the build directory, which is determined at build time, not
    # GN time.
    # These are passed as pairs of GlobalObjectName, global_object.idl
    interface_name_idl_filename = [(args[i], args[i + 1])
                                   for i in range(0, len(args), 2)]

    interface_name_to_global_names.update(read_pickle_file(options.global_objects_file))

    for idl_filename in idl_files:
        record_global_constructors(idl_filename)

    # Check for [Exposed] / [Global] mismatch.
    known_global_names = EXPOSED_EXECUTION_CONTEXT_METHOD.keys()
    exposed_global_names = frozenset(global_name_to_constructors)
    if not exposed_global_names.issubset(known_global_names):
        unknown_global_names = exposed_global_names.difference(known_global_names)
        raise ValueError('The following global names were used in '
                         '[Exposed=xxx] but do not match any global names: %s'
                         % list(unknown_global_names))

    # Write partial interfaces containing constructor attributes for each
    # global interface.
    for interface_name, idl_filename in interface_name_idl_filename:
        constructors = interface_name_to_constructors(interface_name)
        write_global_constructors_partial_interface(
            interface_name, idl_filename, constructors)


if __name__ == '__main__':
    sys.exit(main())
