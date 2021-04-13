# Copyright 2021 The Chromium Authors. All rights reserved.
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


def _get_callback_functions(target_store):
    return target_store.web_idl_database.callback_functions


def _get_callback_interfaces(target_store):
    return target_store.web_idl_database.callback_interfaces


def _get_function_likes(target_store):
    function_likes = []
    function_likes.extend(target_store.get(CALLBACK_FUNCTIONS))
    for callback_interface in target_store.get(CALLBACK_INTERFACES):
        function_likes.extend(callback_interface.operations)
    for interface in target_store.get(INTERFACES):
        function_likes.extend(interface.operations)
    return function_likes


def _get_interfaces(target_store):
    return target_store.web_idl_database.interfaces


"""
TargetType constants. These are used as a key of dictionary
in TargetStore and RuleStore.
"""
CALLBACK_FUNCTIONS = TargetType("callback_function", _get_callback_functions)
CALLBACK_INTERFACES = TargetType("callback_interface",
                                 _get_callback_interfaces)
FUNCTION_LIKES = TargetType("function_like", _get_function_likes)
INTERFACES = TargetType("callback_function", _get_interfaces)
