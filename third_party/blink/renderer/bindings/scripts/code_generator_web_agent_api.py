# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=import-error,print-statement,relative-import

"""Generates Web Agent API bindings.

The Web Agent API bindings provide a stable, IDL-generated interface for the
Web Agents.

The Web Agents are the high-level services like Autofill,
Autocomplete, Translate, Distiller, Phishing Detector, and others. Web Agents
typically want to introspec the document and rendering infromation to implement
browser features.

The bindings are meant to be as simple and as ephemeral as possible, mostly just
wrapping existing DOM classes. Their primary goal is to avoid leaking the actual
DOM classes to the Web Agents layer.
"""

import os
import posixpath
import sys

from code_generator import CodeGeneratorBase, render_template, normalize_and_sort_includes
# TODO(dglazkov): Move TypedefResolver to code_generator.py
from code_generator_v8 import TypedefResolver
from utilities import to_header_guard

sys.path.append(os.path.join(os.path.dirname(__file__),
                             '..', '..', 'build', 'scripts'))
from blinkbuild.name_style_converter import NameStyleConverter

MODULE_PYNAME = os.path.splitext(os.path.basename(__file__))[0] + '.py'

STRING_INCLUDE_PATH = 'third_party/blink/renderer/platform/wtf/text/wtf_string.h'
WEB_AGENT_API_IDL_ATTRIBUTE = 'WebAgentAPI'


def interface_context(idl_interface, type_resolver):
    builder = InterfaceContextBuilder(MODULE_PYNAME, type_resolver)
    builder.set_class_name(idl_interface.name)
    builder.set_inheritance(idl_interface.parent)

    for idl_attribute in idl_interface.attributes:
        builder.add_attribute(idl_attribute)

    for idl_operation in idl_interface.operations:
        builder.add_operation(idl_operation)

    return builder.build()


class TypeResolver(object):
    """Resolves Web IDL types into corresponding C++ types and include paths
       to the generated and existing files."""

    def __init__(self, interfaces_info):
        self.interfaces_info = interfaces_info

    def includes_from_interface(self, interface_name):
        interface_info = self.interfaces_info.get(interface_name)
        if interface_info is None:
            raise KeyError('Unknown interface "%s".' % interface_name)
        return set([interface_info['include_path']])

    def _includes_from_type(self, idl_type):
        if idl_type.is_void:
            return set()
        if idl_type.is_primitive_type:
            return set()
        if idl_type.is_string_type:
            return set([STRING_INCLUDE_PATH])

        # TODO(dglazkov): Handle complex/weird types.
        return self.includes_from_interface(idl_type.base_type)

    def includes_from_definition(self, idl_definition):
        return self._includes_from_type(idl_definition.idl_type)

    def type_from_definition(self, idl_definition):
        # TODO(dglazkov): The output of this method must be a reasonable C++
        # type that can be used directly in the jinja2 template.
        return idl_definition.idl_type.base_type

    def base_class_includes(self):
        return set(['third_party/blink/renderer/platform/heap/handle.h'])


class MethodOverloadSplitter(object):
    """Because of union and optional types being used as arguments, some
       operations may result in more than one generated method. This class
       contains the logic for spliting an operation into multiple C++ overloads.
    """

    def __init__(self, idl_operation):
        self.idl_operation = idl_operation

    def _update_argument_lists(self, argument_lists, idl_types):
        """Given a list of IdlTypes and an existing list of argument lists (yes,
           this is a list of lists), produces a next generation of the list of
           lists. This is where the actual splitting into overloads happens.
        """
        result = []
        for argument_list in argument_lists:
            for idl_type in idl_types:
                new_argument_list = list(argument_list)
                if idl_type is not None:
                    new_argument_list.append(idl_type)
                result.append(new_argument_list)
        return result

    def _enumerate_argument_types(self, idl_argument):
        """Given an IdlArgument, returns a list of types that are included
           in this argument. If optional, the list will include a 'None'."""
        argument_type = idl_argument.idl_type
        # TODO(dglazkov): What should we do with primitive nullable args?
        if (argument_type.is_nullable and
                argument_type.inner_type.is_primitive_type):
            raise ValueError('Primitive nullable types are not supported.')

        idl_types = []
        if idl_argument.is_optional:
            idl_types.append(None)  # None is used to convey optionality.
        if argument_type.is_union_type:
            idl_types = idl_types + argument_type.member_types
        else:
            idl_types.append(argument_type)
        return idl_types

    def split_into_overloads(self):
        """Splits an operation into one or more overloads that correctly reflect
           the WebIDL semantics of the operation arguments. For example,
           running this method on an IdlOperation that represents this WebIDL
           definition:

           void addEventListener(
                DOMString type,
                EventListener? listener,
                optional (AddEventListenerOptions or boolean) options)

            will produce a list of 3 argument lists:

            1) [DOMString, EventListener], since the third argument is optional,
            2) [DOMString, EventListener, AddEventListenerOptions], since the
               third argument is a union type with AddEventListenerOptions as
               one of its member types, and
            3) [DOMString, EventListener, boolean], since the other union member
               type of the third argument is boolean.

            This example is also captured as test in
            MethodOverloadSplitterTest.test_split_add_event_listener.
        """

        argument_lists = [[]]
        for idl_argument in self.idl_operation.arguments:
            idl_types = self._enumerate_argument_types(idl_argument)
            argument_lists = self._update_argument_lists(argument_lists,
                                                         idl_types)
        return argument_lists


class InterfaceContextBuilder(object):
    def __init__(self, code_generator, type_resolver):
        self.result = {'code_generator': code_generator}
        self.type_resolver = type_resolver

    def set_class_name(self, class_name):
        converter = NameStyleConverter(class_name)
        self.result['class_name'] = converter.to_all_cases()
        self._ensure_set('cpp_includes').update(
            self.type_resolver.includes_from_interface(class_name))

    def set_inheritance(self, base_interface):
        if base_interface is None:
            self._ensure_set('header_includes').update(
                self.type_resolver.base_class_includes())
            return
        self.result['base_class'] = base_interface
        self._ensure_set('header_includes').update(
            self.type_resolver.includes_from_interface(base_interface))

    def _ensure_set(self, name):
        return self.result.setdefault(name, set())

    def _ensure_list(self, name):
        return self.result.setdefault(name, [])

    def add_attribute(self, idl_attribute):
        self._ensure_list('attributes').append(
            self.create_attribute(idl_attribute))
        self._ensure_set('cpp_includes').update(
            self.type_resolver.includes_from_definition(idl_attribute))

    def add_operation(self, idl_operation):
        if not idl_operation.name:
            return
        overload_splitter = MethodOverloadSplitter(idl_operation)
        overloads = overload_splitter.split_into_overloads()
        argument_names = [argument.name for argument
                          in idl_operation.arguments]
        for argument_types in overloads:
            arguments = []
            for position, argument_type in enumerate(argument_types):
                arguments.append(
                    self.create_argument(argument_names[position],
                                         argument_type))
            self._ensure_list('methods').append(
                self.create_method(idl_operation, arguments))
            self._ensure_set('cpp_includes').update(
                self.type_resolver.includes_from_definition(idl_operation))

    def create_argument(self, argument_name, argument_type):
        name_converter = NameStyleConverter(argument_name)
        return {
            'name': name_converter.to_snake_case(),
            'type': argument_type.base_type,
        }

    def create_method(self, idl_operation, arguments):
        name_converter = NameStyleConverter(idl_operation.name)
        return_type = self.type_resolver.type_from_definition(idl_operation)
        return {
            'name': name_converter.to_upper_camel_case(),
            'type': return_type,
            'arguments': arguments
        }

    def create_attribute(self, idl_attribute):
        name = idl_attribute.name
        return_type = self.type_resolver.type_from_definition(idl_attribute)
        return {
            'name': name,
            'type': return_type
        }

    def build(self):
        if 'cpp_includes' in self.result:
            self.result['cpp_includes'] = set(normalize_and_sort_includes(self.result['cpp_includes']))
        if 'header_includes' in self.result:
            self.result['header_includes'] = set(normalize_and_sort_includes(self.result['header_includes']))
        return self.result


class CodeGeneratorWebAgentAPI(CodeGeneratorBase):
    def __init__(self, info_provider, cache_dir, output_dir):
        CodeGeneratorBase.__init__(self, MODULE_PYNAME, info_provider,
                                   cache_dir, output_dir)
        self.type_resolver = TypeResolver(info_provider.interfaces_info)
        self.typedef_resolver = TypedefResolver(info_provider)

    def get_template(self, file_extension):
        template_filename = 'web_agent_api_interface.%s.tmpl' % file_extension
        return self.jinja_env.get_template(template_filename)

    def generate_file(self, template_context, file_extension):
        template = self.get_template(file_extension)
        path = posixpath.join(
            self.output_dir,
            '%s.%s' % (template_context['class_name']['snake_case'],
                       file_extension))
        if file_extension == 'h':
            this_include_header_path = self.normalize_this_header_path(path)
            template_context['header_guard'] = to_header_guard(this_include_header_path)

        text = render_template(template, template_context)
        return (path, text)

    def generate_interface_code(self, interface):
        # TODO(dglazkov): Implement callback interfaces.
        # TODO(dglazkov): Make sure partial interfaces are handled.
        if interface.is_callback or interface.is_partial:
            raise ValueError('Partial or callback interfaces are not supported')

        template_context = interface_context(interface, self.type_resolver)

        return (
            self.generate_file(template_context, 'h'),
            self.generate_file(template_context, 'cc')
        )

    def generate_code(self, definitions, definition_name):
        self.typedef_resolver.resolve(definitions, definition_name)

        # TODO(dglazkov): Implement dictionaries
        if definition_name not in definitions.interfaces:
            return None

        interface = definitions.interfaces[definition_name]
        if WEB_AGENT_API_IDL_ATTRIBUTE not in interface.extended_attributes:
            return None

        return self.generate_interface_code(interface)
