# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import web_idl

from . import name_style
from .code_node import CodeNode
from .code_node import SymbolDefinitionNode
from .code_node import SymbolNode
from .code_node import SymbolScopeNode
from .code_node import TextNode
from .code_node import UnlikelyExitNode

_format = CodeNode.format_template


def blink_class_name(idl_definition):
    """
    Returns the class name of Blink implementation.
    """
    try:
        class_name = idl_definition.extended_attributes.get(
            "ImplementedAs").value
    except:
        class_name = idl_definition.identifier

    if isinstance(idl_definition,
                  (web_idl.CallbackFunction, web_idl.CallbackInterface)):
        return name_style.class_("v8", class_name)
    else:
        return name_style.class_(class_name)


def blink_type_info(idl_type):
    """
    Returns the types of Blink implementation corresponding to the given IDL
    type.  The returned object has the following attributes.

      member_t: The type of a member variable.  E.g. T => Member<T>
      ref_t: The type of a local variable that references to an already-existing
          value.  E.g. String => String&
      value_t: The type of a variable that behaves as a value.  E.g. String =>
          String
      is_nullable: True if the Blink implementation type can represent IDL null
          value by itself.
    """
    assert isinstance(idl_type, web_idl.IdlType)

    class TypeInfo(object):
        def __init__(self,
                     typename,
                     member_fmt="{}",
                     ref_fmt="{}",
                     value_fmt="{}",
                     is_nullable=False):
            self.member_t = member_fmt.format(typename)
            self.ref_t = ref_fmt.format(typename)
            self.value_t = value_fmt.format(typename)
            # Whether Blink impl type can represent IDL null or not.
            self.is_nullable = is_nullable

    real_type = idl_type.unwrap(typedef=True)

    if real_type.is_boolean or real_type.is_numeric:
        cxx_type = {
            "boolean": "bool",
            "byte": "int8_t",
            "octet": "uint8_t",
            "short": "int16_t",
            "unsigned short": "uint16_t",
            "long": "int32_t",
            "unsigned long": "uint32_t",
            "long long": "int64_t",
            "unsigned long long": "uint64_t",
            "float": "float",
            "unrestricted float": "float",
            "double": "double",
            "unrestricted double": "double",
        }
        return TypeInfo(cxx_type[real_type.keyword_typename])

    if real_type.is_string:
        return TypeInfo("String", ref_fmt="{}&", is_nullable=True)

    if real_type.is_symbol:
        assert False, "Blink does not support/accept IDL symbol type."

    if real_type.is_any or real_type.is_object:
        return TypeInfo("ScriptValue", ref_fmt="{}&", is_nullable=True)

    if real_type.is_void:
        assert False, "Blink does not support/accept IDL void type."

    if real_type.type_definition_object is not None:
        type_def_obj = real_type.type_definition_object
        blink_impl_type = (
            type_def_obj.code_generator_info.receiver_implemented_as
            or name_style.class_(type_def_obj.identifier))
        return TypeInfo(
            blink_impl_type,
            member_fmt="Member<{}>",
            ref_fmt="{}*",
            is_nullable=True)

    if (real_type.is_sequence or real_type.is_frozen_array
            or real_type.is_variadic):
        element_type = blink_type_info(real_type.element_type)
        return TypeInfo(
            "VectorOf<{}>".format(element_type.value_t), ref_fmt="{}&")

    if real_type.is_record:
        key_type = blink_type_info(real_type.key_type)
        value_type = blink_type_info(real_type.value_type)
        return TypeInfo(
            "VectorOfPairs<{}, {}>".format(key_type.value_t,
                                           value_type.value_t),
            ref_fmt="{}&")

    if real_type.is_promise:
        return TypeInfo("ScriptPromise", ref_fmt="{}&")

    if real_type.is_union:
        return TypeInfo("ToBeImplementedUnion")

    if real_type.is_nullable:
        inner_type = blink_type_info(real_type.inner_type)
        if inner_type.is_nullable:
            return inner_type
        return TypeInfo(
            "base::Optional<{}>".format(inner_type.value_t), ref_fmt="{}&")


def native_value_tag(idl_type):
    """Returns the tag type of NativeValueTraits."""
    assert isinstance(idl_type, web_idl.IdlType)

    real_type = idl_type.unwrap(typedef=True)

    if (real_type.is_boolean or real_type.is_numeric or real_type.is_string
            or real_type.is_any or real_type.is_object):
        return "IDL{}".format(real_type.type_name)

    if real_type.is_symbol:
        assert False, "Blink does not support/accept IDL symbol type."

    if real_type.is_void:
        assert False, "Blink does not support/accept IDL void type."

    if real_type.type_definition_object is not None:
        return blink_type_info(real_type).value_t

    if real_type.is_sequence:
        return "IDLSequence<{}>".format(
            native_value_tag(real_type.element_type))

    if real_type.is_record:
        return "IDLRecord<{}, {}>".format(
            native_value_tag(real_type.key_type),
            native_value_tag(real_type.value_type))

    if real_type.is_promise:
        return "IDLPromise"

    if real_type.is_union:
        return blink_type_info(real_type).value_t

    if real_type.is_nullable:
        return "IDLNullable<{}>".format(native_value_tag(real_type.inner_type))


def make_v8_to_blink_value(blink_var_name, v8_value_expr, idl_type):
    """
    Returns a SymbolNode whose definition converts a v8::Value to a Blink value.
    """
    assert isinstance(blink_var_name, str)
    assert isinstance(v8_value_expr, str)
    assert isinstance(idl_type, web_idl.IdlType)

    pattern = "NativeValueTraits<{_1}>::NativeValue({_2})"
    _1 = native_value_tag(idl_type)
    _2 = ["${isolate}", v8_value_expr, "${exception_state}"]
    blink_value = _format(pattern, _1=_1, _2=", ".join(_2))

    pattern = "const auto& ${{{_1}}} = {_2};"
    _1 = blink_var_name
    _2 = blink_value
    text = _format(pattern, _1=_1, _2=_2)

    def create_definition(symbol_node):
        return SymbolDefinitionNode(symbol_node, [
            TextNode(text),
            UnlikelyExitNode(
                cond=TextNode("${exception_state}.HadException()"),
                body=SymbolScopeNode([TextNode("return;")])),
        ])

    return SymbolNode(blink_var_name, definition_constructor=create_definition)
