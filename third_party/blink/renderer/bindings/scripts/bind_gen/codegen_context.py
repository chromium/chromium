# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import copy

from . import name_style
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
    ALL_WORLDS = "all"

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
            "operation": None,
            "operation_group": None,

            # Main world or all worlds
            "for_world": cls.ALL_WORLDS,
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
            "v8_class",
        )

        # Define public readonly properties of this class.
        for attr in cls._context_attrs.iterkeys():

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

        for arg in kwargs.iterkeys():
            assert arg in self._context_attrs, "Unknown argument: {}".format(
                arg)

        for attr, default_value in self._context_attrs.iteritems():
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
        for arg in kwargs.iterkeys():
            assert arg in self._context_attrs, "Unknown argument: {}".format(
                arg)

        new_object = copy.copy(self)

        for attr, new_value in kwargs.iteritems():
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

        for attr in self._context_attrs.iterkeys():
            value = getattr(self, attr)
            if value is not None:
                bindings[attr] = value

        for attr in self._computational_attrs:
            value = getattr(self, attr)
            if value is not None:
                bindings[attr.strip("_")] = value

        return bindings

    @property
    def class_like(self):
        return (self.callback_interface or self.dictionary or self.interface
                or self.namespace)

    @property
    def function_like(self):
        return (self.callback_function or self.constructor or self.operation)

    @property
    def idl_definition(self):
        return (self.callback_function or self.callback_interface
                or self.dictionary or self.enumeration or self.interface
                or self.namespace or self.typedef or self.union)

    @property
    def idl_location(self):
        idl_def = self.member_like or self.idl_definition
        if idl_def:
            location = idl_def.debug_info.location
            text = PathManager.relpath_to_project_root(location.filepath)
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
        if self.return_type is None:
            return None
        return_type = self.return_type.unwrap()
        return return_type.is_dictionary or return_type.is_union

    @property
    def may_throw_exception(self):
        if not self.member_like:
            return None
        ext_attr = self.member_like.extended_attributes.get("RaisesException")
        if not ext_attr:
            return False
        return (not ext_attr.values
                or (self.attribute_get and "Getter" in ext_attr.values)
                or (self.attribute_set and "Setter" in ext_attr.values))

    @property
    def member_like(self):
        return (self.attribute or self.constant or self.constructor
                or self.dict_member or self.operation)

    @property
    def property_(self):
        return (self.attribute or self.constant or self.constructor_group
                or self.dict_member or self.operation_group)

    @property
    def return_type(self):
        if self.attribute_get:
            return self.attribute.idl_type
        if self.callback_function:
            return self.callback_function.return_type
        if self.operation:
            return self.operation.return_type
        return None

    @property
    def v8_class(self):
        if not self.idl_definition:
            return None
        return name_style.class_("v8", self.idl_definition.identifier)


CodeGenContext.init()
