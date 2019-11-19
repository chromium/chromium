#!/usr/bin/python
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

"""Compute global interface information for individual IDL files.

Auxiliary module for compute_interfaces_info_overall, which consolidates
this individual information, computing info that spans multiple files
(dependencies and ancestry).

This distinction is so that individual interface info can be computed
separately for each component (avoiding duplicated reading of individual
files), then consolidated using *only* the info visible to a given component.

Design doc: http://www.chromium.org/developers/design-documents/idl-build
"""

from collections import defaultdict
import optparse
import os
import posixpath
import sys

from idl_definitions import Visitor
from idl_reader import IdlReader
from utilities import idl_filename_to_component
from utilities import idl_filename_to_basename
from utilities import merge_dict_recursively
from utilities import read_idl_files_list_from_file
from utilities import shorten_union_name
from utilities import to_snake_case
from utilities import read_pickle_file
from utilities import write_pickle_file


module_path = os.path.dirname(__file__)
source_path = os.path.normpath(os.path.join(module_path, os.pardir, os.pardir))
gen_path = os.path.join('gen', 'blink')


class IdlBadFilenameError(Exception):
    """Raised if an IDL filename disagrees with the interface name in the file."""
    pass


def parse_options():
    usage = 'Usage: %prog [options]'
    parser = optparse.OptionParser(usage=usage)
    parser.add_option('--cache-directory', help='cache directory')
    parser.add_option('--idl-files-list', help='file listing IDL files')
    parser.add_option('--interfaces-info-file', help='interface info pickle file')
    parser.add_option('--component-info-file', help='component wide info pickle file')
    parser.add_option('--runtime-enabled-features-file', help='runtime-enabled features pickle file')

    options, args = parser.parse_args()
    if options.interfaces_info_file is None:
        parser.error('Must specify an output file using --interfaces-info-file.')
    if options.idl_files_list is None:
        parser.error('Must specify a file listing IDL files using --idl-files-list.')
    return options, args


################################################################################
# Computations
################################################################################

def relative_dir_posix(idl_filename, base_path):
    """Returns relative path to the directory of idl_file in POSIX format."""
    relative_path_local = os.path.relpath(idl_filename, base_path)
    relative_dir_local = os.path.dirname(relative_path_local)
    return relative_dir_local.replace(os.path.sep, posixpath.sep)


def include_path(idl_filename, implemented_as=None):
    """Returns relative path to header file in POSIX format; used in includes.

    POSIX format is used for consistency of output, so reference tests are
    platform-independent.
    """
    if idl_filename.startswith(gen_path):
        relative_dir = relative_dir_posix(idl_filename, gen_path)
    else:
        relative_dir = relative_dir_posix(idl_filename, source_path)

    # IDL file basename is used even if only a partial interface file
    output_file_basename = implemented_as or idl_filename_to_basename(idl_filename)
    output_file_basename = to_snake_case(output_file_basename)
    return posixpath.join(relative_dir, output_file_basename + '.h')


def get_includes_from_definitions(definitions, definition_name):
    interfaces = []
    mixins = []
    for include in definitions.includes:
        if definition_name == include.interface:
            mixins.append(include.mixin)
        elif definition_name == include.mixin:
            interfaces.append(include.interface)
        else:
            raise IdlBadFilenameError(
                'includes statement found in unrelated IDL file.\n'
                'Statement is:\n'
                '    %s includes %s;\n'
                'but filename is unrelated "%s.idl"' %
                (include.interface, include.mixin, definition_name))
    return interfaces, mixins


def get_put_forward_interfaces_from_definition(definition):
    return sorted(set(attribute.idl_type.base_type
                      for attribute in definition.attributes
                      if 'PutForwards' in attribute.extended_attributes))


def get_unforgeable_attributes_from_definition(definition):
    return sorted(attribute for attribute in definition.attributes
                  if 'Unforgeable' in attribute.extended_attributes)


def collect_union_types_from_definitions(definitions):
    """Traverse definitions and collect all union types."""
    class UnionTypeCollector(Visitor):
        def collect(self, definitions):
            self._union_types = set()
            definitions.accept(self)
            return self._union_types

        def visit_typed_object(self, typed_object):
            for attribute_name in typed_object.idl_type_attributes:
                attribute = getattr(typed_object, attribute_name, None)
                if not attribute:
                    continue
                for idl_type in attribute.idl_types():
                    if idl_type.is_union_type:
                        self._union_types.add(idl_type)

    return UnionTypeCollector().collect(definitions)


class InterfaceInfoCollector(object):
    """A class that collects interface information from idl files."""
    def __init__(self, cache_directory=None):
        self.reader = IdlReader(interfaces_info=None, outputdir=cache_directory)
        self.interfaces_info = {}
        self.partial_interface_files = defaultdict(lambda: {
            'full_paths': [],
            'include_paths': [],
        })
        self.enumerations = {}
        self.union_types = set()
        self.typedefs = {}
        self.callback_functions = {}

    def add_paths_to_partials_dict(self, partial_interface_name, full_path,
                                   include_paths):
        paths_dict = self.partial_interface_files[partial_interface_name]
        paths_dict['full_paths'].append(full_path)
        paths_dict['include_paths'].extend(include_paths)

    def check_enum_consistency(self, enum):
        existing_enum = self.enumerations.get(enum.name)
        if not existing_enum:
            return True
        # TODO(bashi): Ideally we should not allow multiple enum declarations
        # but we allow them to work around core/module separation.
        if len(existing_enum.values) != len(enum.values):
            return False
        return all(value in existing_enum.values for value in enum.values)

    def collect_info(self, idl_filename):
        """Reads an idl file and collects information which is required by the
        binding code generation."""
        def collect_unforgeable_attributes(definition, idl_filename):
            """Collects [Unforgeable] attributes so that we can define them on
            sub-interfaces later.  The resulting structure is as follows.
                interfaces_info[interface_name] = {
                    'unforgeable_attributes': [IdlAttribute, ...],
                    ...
                }
            """
            interface_info = {}
            unforgeable_attributes = get_unforgeable_attributes_from_definition(definition)
            if not unforgeable_attributes:
                return interface_info

            if definition.is_partial:
                interface_basename = idl_filename_to_interface_name(idl_filename)
                # TODO(yukishiino): [PartialInterfaceImplementedAs] is treated
                # in interface_dependency_resolver.transfer_extended_attributes.
                # Come up with a better way to keep them consistent.
                for attr in unforgeable_attributes:
                    attr.extended_attributes['PartialInterfaceImplementedAs'] = definition.extended_attributes.get('ImplementedAs', interface_basename)
            interface_info['unforgeable_attributes'] = unforgeable_attributes
            return interface_info

        definitions = self.reader.read_idl_file(idl_filename)

        this_union_types = collect_union_types_from_definitions(definitions)
        self.union_types.update(this_union_types)
        self.typedefs.update(definitions.typedefs)
        for callback_function_name, callback_function in definitions.callback_functions.iteritems():
            # Set 'component_dir' to specify a directory that callback function files belong to
            self.callback_functions[callback_function_name] = {
                'callback_function': callback_function,
                'component_dir': idl_filename_to_component(idl_filename),
                'full_path': os.path.realpath(idl_filename),
            }
        # Check enum duplication.
        for enum in definitions.enumerations.values():
            if not self.check_enum_consistency(enum):
                raise Exception('Enumeration "%s" is defined more than once '
                                'with different valid values' % enum.name)
        self.enumerations.update(definitions.enumerations)

        if definitions.interfaces:
            definition = next(definitions.interfaces.itervalues())
            interface_info = {
                'is_callback_interface': definition.is_callback,
                'is_dictionary': False,
                # Interfaces that are referenced (used as types) and that we
                # introspect during code generation (beyond interface-level
                # data ([ImplementedAs], is_callback_interface, ancestors, and
                # inherited extended attributes): deep dependencies.
                # These cause rebuilds of referrers, due to the dependency,
                # so these should be minimized; currently only targets of
                # [PutForwards].
                'referenced_interfaces': get_put_forward_interfaces_from_definition(definition),
            }
        elif definitions.dictionaries:
            definition = next(definitions.dictionaries.itervalues())
            interface_info = {
                'is_callback_interface': False,
                'is_dictionary': True,
                'referenced_interfaces': None,
            }
        else:
            return

        if definition.name not in self.interfaces_info:
            self.interfaces_info[definition.name] = {}

        # Remember [Unforgeable] attributes.
        if definitions.interfaces:
            merge_dict_recursively(self.interfaces_info[definition.name],
                                   collect_unforgeable_attributes(definition, idl_filename))

        component = idl_filename_to_component(idl_filename)
        extended_attributes = definition.extended_attributes
        implemented_as = extended_attributes.get('ImplementedAs')
        full_path = os.path.realpath(idl_filename)
        if interface_info['is_dictionary']:
            this_include_path = include_path(idl_filename)
        else:
            this_include_path = include_path(idl_filename, implemented_as)
        if definition.is_partial:
            # We don't create interface_info for partial interfaces, but
            # adds paths to another dict.
            partial_include_paths = []
            if this_include_path:
                partial_include_paths.append(this_include_path)
            self.add_paths_to_partials_dict(definition.name, full_path, partial_include_paths)
            # Collects C++ header paths which should be included from generated
            # .cpp files.  The resulting structure is as follows.
            #   interfaces_info[interface_name] = {
            #       'cpp_includes': {
            #           'core': set(['core/foo/Foo.h', ...]),
            #           'modules': set(['modules/bar/Bar.h', ...]),
            #       },
            #       ...
            #   }
            if this_include_path:
                merge_dict_recursively(
                    self.interfaces_info[definition.name],
                    {'cpp_includes': {component: set([this_include_path])}})
            return

        # 'includes' statements can be included in either the file for the
        # interface (lhs of 'includes') or mixin (rhs of 'includes'). Store both
        # for now, then merge to the interface later.
        includes_interfaces, includes_mixins = get_includes_from_definitions(
            definitions, definition.name)

        interface_info.update({
            'extended_attributes': extended_attributes,
            'full_path': full_path,
            'union_types': this_union_types,
            'implemented_as': implemented_as,
            'included_by_interfaces': includes_interfaces,
            'including_mixins': includes_mixins,
            'include_path': this_include_path,
            # FIXME: temporary private field, while removing old treatement of
            # 'implements': http://crbug.com/360435
            'is_legacy_treat_as_partial_interface': 'LegacyTreatAsPartialInterface' in extended_attributes,
            'parent': definition.parent,
            'relative_dir': relative_dir_posix(idl_filename, source_path),
        })
        merge_dict_recursively(self.interfaces_info[definition.name], interface_info)

    def get_info_as_dict(self):
        """Returns info packaged as a dict."""
        return {
            'interfaces_info': self.interfaces_info,
            # Can't pickle defaultdict, convert to dict
            # FIXME: this should be included in get_component_info.
            'partial_interface_files': dict(self.partial_interface_files),
        }

    def get_component_info_as_dict(self, runtime_enabled_features):
        """Returns component wide information as a dict."""
        return {
            'callback_functions': self.callback_functions,
            'enumerations': dict((enum.name, enum.values)
                                 for enum in self.enumerations.values()),
            'runtime_enabled_features': runtime_enabled_features,
            'typedefs': self.typedefs,
            'union_types': self.union_types,
        }


################################################################################

def main():
    options, _ = parse_options()

    # IDL files are passed in a file, due to OS command line length limits
    idl_files = read_idl_files_list_from_file(options.idl_files_list)

    # Compute information for individual files
    # Information is stored in global variables interfaces_info and
    # partial_interface_files.
    info_collector = InterfaceInfoCollector(options.cache_directory)
    for idl_filename in idl_files:
        info_collector.collect_info(idl_filename)

    write_pickle_file(options.interfaces_info_file,
                      info_collector.get_info_as_dict())
    runtime_enabled_features = read_pickle_file(options.runtime_enabled_features_file)
    write_pickle_file(options.component_info_file,
                      info_collector.get_component_info_as_dict(runtime_enabled_features))

if __name__ == '__main__':
    sys.exit(main())
