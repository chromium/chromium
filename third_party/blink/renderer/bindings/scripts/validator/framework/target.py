# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Creates constants representing the target types of validation.
All the possible target types are defined here.

Basically, all the IDL fragments defined in Web IDL can be a target.
Example: Interfaces, Callback functions, constants, etc.

You can also define customized target types if necessary.
Example: Function likes mean all the callables like callback functions or
operations in interfaces.
"""

from .target_type import TargetType


def _get_arguments(target_store):
    arguments = []
    for function_like in target_store.get(FUNCTION_LIKES):
        arguments.extend(function_like.arguments)
    return arguments


def _get_async_iterables(target_store):
    async_iterables = []
    for interface in target_store.get(INTERFACES):
        if interface.async_iterable:
            async_iterables.append(interface.async_iterable)
    return async_iterables


def _get_attributes(target_store):
    attributes = []
    for interface in target_store.get(INTERFACES):
        attributes.extend(interface.attributes)
    for namespace in target_store.get(NAMESPACES):
        attributes.extend(namespace.attributes)
    return attributes


def _get_callback_functions(target_store):
    return target_store.web_idl_database.callback_functions


def _get_callback_interfaces(target_store):
    return target_store.web_idl_database.callback_interfaces


def _get_constants(target_store):
    constants = []
    for callback_interface in target_store.get(CALLBACK_INTERFACES):
        constants.extend(callback_interface.constants)
    for interface in target_store.get(INTERFACES):
        constants.extend(interface.constants)
    return constants


def _get_dictionaries(target_store):
    return target_store.web_idl_database.dictionaries


def _get_dictionary_members(target_store):
    dictionary_members = []
    for dictionary in target_store.get(DICTIONARIES):
        dictionary_members.extend(dictionary.own_members)
    return dictionary_members


def _get_enumerations(target_store):
    return target_store.web_idl_database.enumerations


def _get_function_likes(target_store):
    function_likes = []
    function_likes.extend(target_store.get(CALLBACK_FUNCTIONS))
    for callback_interface in target_store.get(CALLBACK_INTERFACES):
        function_likes.extend(callback_interface.operations)
    for interface in target_store.get(INTERFACES):
        function_likes.extend(interface.operations)
        function_likes.extend(interface.constructors)
        function_likes.extend(interface.legacy_factory_functions)
        if interface.iterable:
            function_likes.extend(interface.iterable.operations)
        if interface.maplike:
            function_likes.extend(interface.maplike.operations)
        if interface.setlike:
            function_likes.extend(interface.setlike.operations)
    for namespace in target_store.get(NAMESPACES):
        function_likes.extend(namespace.operations)
    return function_likes


def _get_idl_types(target_store):
    idl_types = []
    idl_types.extend(
        list(map(lambda x: x.idl_type, target_store.get(ARGUMENTS))))
    idl_types.extend(
        list(map(lambda x: x.idl_type, target_store.get(ATTRIBUTES))))
    idl_types.extend(
        list(map(lambda x: x.idl_type, target_store.get(CONSTANTS))))
    idl_types.extend(
        list(map(lambda x: x.idl_type, target_store.get(DICTIONARY_MEMBERS))))
    idl_types.extend(
        list(map(lambda x: x.return_type, target_store.get(FUNCTION_LIKES))))
    return idl_types


def _get_interfaces(target_store):
    return target_store.web_idl_database.interfaces


def _get_iterables(target_store):
    iterables = []
    for interface in target_store.get(INTERFACES):
        if interface.iterable:
            iterables.append(interface.iterable)
    return iterables


def _get_legacy_window_aliases(target_store):
    legacy_window_aliases = []
    for interface in target_store.get(INTERFACES):
        legacy_window_aliases.extend(interface.legacy_window_aliases)
    return legacy_window_aliases


def _get_map_likes(target_store):
    map_likes = []
    for interface in target_store.get(INTERFACES):
        if interface.maplike:
            map_likes.append(interface.maplike)
    return iterables


def _get_namespaces(target_store):
    return target_store.web_idl_database.namespaces


def _get_observable_arrays(target_store):
    return target_store.web_idl_database.observable_arrays


def _get_objects_with_extended_attributes(target_store):
    objects = []
    objects.extend(target_store.get(ASYNC_ITERABLES))
    objects.extend(target_store.get(ATTRIBUTES))
    objects.extend(target_store.get(CALLBACK_FUNCTIONS))
    objects.extend(target_store.get(CALLBACK_INTERFACES))
    objects.extend(target_store.get(CONSTANTS))
    objects.extend(target_store.get(DICTIONARIES))
    objects.extend(target_store.get(DICTIONARY_MEMBERS))
    objects.extend(target_store.get(ENUMERATIONS))
    objects.extend(target_store.get(FUNCTION_LIKES))
    objects.extend(target_store.get(INTERFACES))
    objects.extend(target_store.get(ITERABLES))
    objects.extend(target_store.get(LEGACY_WINDOW_ALIASES))
    objects.extend(target_store.get(NAMESPACES))
    return objects


def _get_set_likes(target_store):
    set_likes = []
    for interface in target_store.get(INTERFACES):
        if interface.setlike:
            set_likes.append(interface.setlike)
    return iterables


"""
TargetType constants. These are used as a key of dictionary
in TargetStore and RuleStore.
"""
ARGUMENTS = TargetType("arguments", _get_arguments)
ASYNC_ITERABLES = TargetType("async_iterables", _get_async_iterables)
ATTRIBUTES = TargetType("attributes", _get_attributes)
CALLBACK_FUNCTIONS = TargetType("callback_functions", _get_callback_functions)
CALLBACK_INTERFACES = TargetType("callback_interfaces",
                                 _get_callback_interfaces)
CONSTANTS = TargetType("constants", _get_constants)
DICTIONARIES = TargetType("dictionaries", _get_dictionaries)
DICTIONARY_MEMBERS = TargetType("dictionary_members", _get_dictionary_members)
ENUMERATIONS = TargetType("enumerations", _get_enumerations)
FUNCTION_LIKES = TargetType("function_likes", _get_function_likes)
IDL_TYPES = TargetType("idl_types", _get_idl_types)
INTERFACES = TargetType("interfaces", _get_interfaces)
ITERABLES = TargetType("iterables", _get_iterables)
LEGACY_WINDOW_ALIASES = TargetType("legacy_window_aliases",
                                   _get_legacy_window_aliases)
MAP_LIKES = TargetType("map_likes", _get_map_likes)
NAMESPACES = TargetType("namespaces", _get_namespaces)
# Target objects which have extended attributes except for web_idl.IdlType
OBJECTS_WITH_EXTENDED_ATTRIBUTES = TargetType(
    "objects_with_extended_attributes", _get_objects_with_extended_attributes)
OBSERVABLE_ARRAYS = TargetType("observable_arrays", _get_observable_arrays)
SET_LIKES = TargetType("set_likes", _get_set_likes)
