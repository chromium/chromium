# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import copy

import web_idl

from . import name_style
from .codegen_format import NonRenderable
from .path_manager import PathManager


class CodeGenContext(object):
    """
    Represents a context of code generation.

    Note that this is not relevant to Mako template context or any contexts.
    Also note that CodeGenContext's attributes will be global template
    variables.  |CodeGenContext.interface| will be available in templates as
    "${interface}".
    """

    # "for_world" attribute values
    MAIN_WORLD = "main"
    NON_MAIN_WORLDS = "other"
    ALL_WORLDS = "all"

    # "v8_callback_type" attribute values
    #
    # void (*)(const v8::FunctionCallbackInfo<v8::Value>&)
    V8_FUNCTION_CALLBACK = "v8::FunctionCallback"
    # void (*)(v8::Local<v8::Name>,
    #          const v8::PropertyCallbackInfo<v8::Value>&)
    V8_ACCESSOR_NAME_GETTER_CALLBACK = "v8::AccessorNameGetterCallback"
    # void (*)(v8::Local<v8::Name>, v8::Local<v8::Value>,
    #          const v8::PropertyCallbackInfo<void>&)
    V8_ACCESSOR_NAME_SETTER_CALLBACK = "v8::AccessorNameSetterCallback"
    # void (*)(v8::Local<v8::Name>, v8::Local<v8::Value>,
    #          const v8::PropertyCallbackInfo<v8::Value>&)
    V8_GENERIC_NAMED_PROPERTY_SETTER_CALLBACK = (
        "v8::GenericNamedPropertySetterCallback")
    # Others
    V8_OTHER_CALLBACK = "other callback type"

    @classmethod
    def init(cls):
        """Initialize the class.  Must be called exactly once."""
        assert not hasattr(cls, "_was_initialized"), "Do not call twice."
        cls._was_initialized = True

        # List of
        #   attribute name: default value
        cls._context_attrs = {
            # Top-level definition
            "callback_function": None,
            "callback_interface": None,
            "dictionary": None,
            "enumeration": None,
            "interface": None,
            "namespace": None,
            "typedef": None,
            "union": None,

            # Class-member-ish definition
            "attribute": None,
            "attribute_get": False,
            "attribute_set": False,
            "constant": None,
            "constructor": None,
            "constructor_group": None,
            "dict_member": None,
            "exposed_construct": None,
            "is_named_constructor": False,
            "legacy_window_alias": None,
            "operation": None,
            "operation_group": None,

            # Special member-ish definition
            "indexed_property_getter": None,
            "indexed_property_setter": None,
            "named_property_getter": None,
            "named_property_setter": None,
            "named_property_deleter": None,
            "stringifier": None,

            # Contents of union.
            "union_members": (),

            # The names of the class being generated and its base class.
            "base_class_name": None,
            "class_name": None,

            # Main world or all worlds
            # Used via [PerWorldBindings] to optimize the code path of the main
            # world.
            "for_world": cls.ALL_WORLDS,

            # Type of V8 callback function which implements IDL attribute,
            # IDL operation, etc.
            "v8_callback_type": cls.V8_FUNCTION_CALLBACK,
        }

        # List of computational attribute names
        cls._computational_attrs = (
            "class_like",
            "function_like",
            "idl_definition",
            "idl_location",
            "idl_location_and_name",
            "idl_name",
            "is_return_by_argument",
            "may_throw_exception",
            "member_like",
            "property_",
            "return_type",
        )

        # Define public readonly properties of this class.
        for attr in cls._context_attrs.keys():

            def make_get():
                _attr = cls._internal_attr(attr)

                def get(self):
                    return getattr(self, _attr)

                return get

            setattr(cls, attr, property(make_get()))

    @staticmethod
    def _internal_attr(attr):
        return "_{}".format(attr)

    def __init__(self, **kwargs):
        assert CodeGenContext._was_initialized

        for arg in kwargs.keys():
            assert arg in self._context_attrs, "Unknown argument: {}".format(
                arg)

        for attr, default_value in self._context_attrs.items():
            value = kwargs[attr] if attr in kwargs else default_value
            assert (default_value is None
                    or type(value) is type(default_value)), (
                        "Type mismatch at argument: {}".format(attr))
            setattr(self, self._internal_attr(attr), value)

    def make_copy(self, **kwargs):
        """
        Returns a copy of this context applying the updates given as the
        arguments.
        """
        for arg in kwargs.keys():
            assert arg in self._context_attrs, "Unknown argument: {}".format(
                arg)

        new_object = copy.copy(self)

        for attr, new_value in kwargs.items():
            old_value = getattr(self, attr)
            assert old_value is None or type(new_value) is type(old_value), (
                "Type mismatch at argument: {}".format(attr))
            setattr(new_object, self._internal_attr(attr), new_value)

        return new_object

    def template_bindings(self):
        """
        Returns a bindings object to be passed into
        |CodeNode.add_template_vars|.  Only properties with a non-None value are
        bound so that it's easy to detect invalid use cases (use of an unbound
        template variable raises a NameError).
        """
        bindings = {}

        for attr in self._context_attrs.keys():
            value = getattr(self, attr)
            if value is None:
                value = NonRenderable(attr)
            bindings[attr] = value

        for attr in self._computational_attrs:
            value = getattr(self, attr)
            if value is None:
                value = NonRenderable(attr)
            bindings[attr.strip("_")] = value

        return bindings

    @property
    def class_like(self):
        return (self.callback_interface or self.dictionary or self.interface
                or self.namespace)

    @property
    def does_override_idl_return_type(self):
        # Blink implementation returns in a type different from the IDL type.
        # Namely, IndexedPropertySetterResult, NamedPropertySetterResult, and
        # NamedPropertyDeleterResult are returned ignoring the operation's
        # return type.
        return (self.indexed_property_setter or self.named_property_setter
                or self.named_property_deleter)

    @property
    def function_like(self):
        return (self.callback_function or self.constructor or self.operation
                or self._indexed_or_named_property)

    @property
    def idl_definition(self):
        return (self.callback_function or self.callback_interface
                or self.dictionary or self.enumeration or self.interface
                or self.namespace or self.typedef or self.union)

    @property
    def idl_location(self):
        idl_def = self.member_like or self.idl_definition
        if idl_def and not isinstance(idl_def, web_idl.Union):
            location = idl_def.debug_info.location
            text = location.filepath
            if location.line_number is not None:
                text += ":{}".format(location.line_number)
            return text
        return "<<unknown path>>"

    @property
    def idl_location_and_name(self):
        return "{}: {}".format(self.idl_location, self.idl_name)

    @property
    def idl_name(self):
        member = self.member_like or self.property_
        if member:
            return "{}.{}".format(self.class_like.identifier,
                                  member.identifier)
        if self.idl_definition:
            return self.idl_definition.identifier
        return "<<unknown name>>"

    @property
    def is_return_by_argument(self):
        if self.does_override_idl_return_type:
            return False
        if self.return_type is None:
            return False
        return self.return_type.unwrap().is_union

    @property
    def is_return_type_promise_type(self):
        if self.attribute:
            return self.attribute.idl_type.unwrap().is_promise
        if self.operation_group:
            return self.operation_group[0].return_type.unwrap().is_promise
        return False

    @property
    def may_throw_exception(self):
        if not self.member_like:
            return False
        ext_attr = self.member_like.extended_attributes.get("RaisesException")
        if not ext_attr:
            return False
        return (not ext_attr.values
                or (self.attribute_get and "Getter" in ext_attr.values)
                or (self.attribute_set and "Setter" in ext_attr.values))

    @property
    def member_like(self):
        return (self.attribute or self.constant or self.constructor
                or self.dict_member or self.operation
                or self._indexed_or_named_property)

    @property
    def property_(self):
        return (self.attribute or self.constant or self.constructor_group
                or self.dict_member
                or (self.legacy_window_alias or self.exposed_construct)
                or self.operation_group
                or (self.stringifier and self.stringifier.operation)
                or self._indexed_or_named_property)

    @property
    def return_type(self):
        if self.attribute_get:
            return self.attribute.idl_type
        function_like = self.function_like
        if function_like:
            return function_like.return_type
        return None

    @property
    def _indexed_or_named_property(self):
        return (self.indexed_property_getter or self.indexed_property_setter
                or self.named_property_getter or self.named_property_setter
                or self.named_property_deleter)


CodeGenContext.init()
