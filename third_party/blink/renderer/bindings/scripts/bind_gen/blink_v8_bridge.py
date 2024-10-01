# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import web_idl

from . import name_style
from .code_node import FormatNode
from .code_node import Likeliness
from .code_node import SymbolDefinitionNode
from .code_node import SymbolNode
from .code_node import TextNode
from .code_node_cxx import CxxIfElseNode
from .code_node_cxx import CxxLikelyIfNode
from .code_node_cxx import CxxUnlikelyIfNode
from .codegen_format import format_template as _format


def blink_class_name(idl_definition):
    """
    Returns the class name of Blink implementation.
    """
    class_name = idl_definition.code_generator_info.receiver_implemented_as
    if class_name:
        return class_name

    assert idl_definition.identifier[0].isupper()
    # Do not apply |name_style.class_| in order to respect the original name
    # (Web spec'ed name) as much as possible.  For example, |interface EXTsRGB|
    # is implemented as |class EXTsRGB|, not as |ExtSRgb| nor |ExtsRgb|.
    if isinstance(idl_definition,
                  (web_idl.CallbackFunction, web_idl.CallbackInterface,
                   web_idl.Enumeration, web_idl.Typedef)):
        return "V8{}".format(idl_definition.identifier)
    elif isinstance(idl_definition, web_idl.ObservableArray):
        return "V8ObservableArray{}".format(
            idl_definition.element_type.
            type_name_with_extended_attribute_key_values)
    elif isinstance(idl_definition, web_idl.Union):
        # Technically this name is not guaranteed to be unique because
        # (X or sequence<Y or Z>) and (X or Y or sequence<Z>) have the same
        # name, but it's highly unlikely to cause a conflict in the actual use
        # cases.  Plus, we prefer a simple naming rule conformant to the
        # Chromium coding style.  So, we go with this way.
        return "V8Union{}".format("Or".join(idl_definition.member_tokens))
    elif isinstance(idl_definition, web_idl.AsyncIterator):
        return "AsyncIterator<{}>".format(
            blink_class_name(idl_definition.interface))
    elif isinstance(idl_definition, web_idl.SyncIterator):
        return "SyncIterator<{}>".format(
            blink_class_name(idl_definition.interface))
    else:
        return idl_definition.identifier


def v8_bridge_class_name(idl_definition):
    """
    Returns the name of V8-from/to-Blink bridge class.
    """
    assert isinstance(
        idl_definition,
        (web_idl.AsyncIterator, web_idl.CallbackInterface, web_idl.Interface,
         web_idl.Namespace, web_idl.SyncIterator))

    assert idl_definition.identifier[0].isupper()
    # Do not apply |name_style.class_| due to the same reason as
    # |blink_class_name|.
    if isinstance(idl_definition, web_idl.AsyncIterator):
        return "V8AsyncIterator{}".format(idl_definition.interface.identifier)
    if isinstance(idl_definition, web_idl.SyncIterator):
        return "V8SyncIterator{}".format(idl_definition.interface.identifier)
    return "V8{}".format(idl_definition.identifier)


def blink_type_info(idl_type):
    """
    Returns an object that represents the types of Blink implementation
    corresponding to the given IDL type, such as reference type, value type,
    member type, etc.
    """
    assert isinstance(idl_type, web_idl.IdlType)

    class TypeInfo(object):
        def __init__(self,
                     typename,
                     member_fmt="{}",
                     ref_fmt="{}",
                     const_ref_fmt="const {}",
                     value_fmt="{}",
                     has_null_value=False,
                     is_gc_type=False,
                     is_heap_vector_type=False,
                     is_move_effective=False,
                     is_traceable=False,
                     clear_member_var_fmt="{}.Clear()"):
            self._typename = typename
            self._has_null_value = has_null_value
            self._is_gc_type = is_gc_type
            self._is_heap_vector_type = is_heap_vector_type
            self._is_move_effective = is_move_effective
            self._is_traceable = (is_gc_type or is_heap_vector_type
                                  or is_traceable)
            self._is_member_t_cppgc_member = member_fmt == "Member<{}>"
            self._clear_member_var_fmt = clear_member_var_fmt

            self._ref_t = ref_fmt.format(typename)
            self._const_ref_t = const_ref_fmt.format(typename)
            self._value_t = value_fmt.format(typename)
            self._member_t = member_fmt.format(typename)
            self._member_ref_t = (self._ref_t
                                  if self._is_gc_type else self._const_ref_t)

        @property
        def typename(self):
            """Returns the internal-use-only name.  Do not use this."""
            return self._typename

        @property
        def ref_t(self):
            """
            Returns the type of a local variable that references to an existing
            value.  E.g. String => String&
            """
            return self._ref_t

        @property
        def const_ref_t(self):
            """
            Returns the const-qualified version of |ref_t|.  E.g. String =>
            const String&
            """
            return self._const_ref_t

        @property
        def value_t(self):
            """
            Returns the type of a variable that behaves as a value. E.g. String
            => String
            """
            return self._value_t

        @property
        def member_t(self):
            """
            Returns the type of a member variable.  E.g. Node => Member<Node>
            """
            return self._member_t

        @property
        def member_ref_t(self):
            """
            Returns the type used for input to and output from a member
            variable.  E.g. Node* for Member<Node> and const String& for String
            """
            return self._member_ref_t

        @property
        def has_null_value(self):
            """
            Returns True if the Blink implementation type can represent IDL
            null value without use of std::optional<T>.  E.g. pointer type =>
            True and int32_t => False
            """
            return self._has_null_value

        @property
        def is_gc_type(self):
            """
            Returns True if the Blink implementation type is a GarbageCollected
            type.
            """
            return self._is_gc_type

        @property
        def is_heap_vector_type(self):
            """
            Returns True if the Blink implementation type is HeapVector<T>.

            HeapVector is very special because HeapVector is GarbageCollected
            but it's used as a value type rather than a reference type for the
            most cases because HeapVector had been implemented as a non-GC type
            for a long time and it turned into a GC type in 2021 January.
            """
            return self._is_heap_vector_type

        @property
        def is_move_effective(self):
            """
            Returns True if support of std::move is effective and desired.
            E.g. Vector => True and int32_t => False
            """
            return self._is_move_effective

        @property
        def is_traceable(self):
            """
            Returns True if the Blink implementation type has Trace method.
            E.g. ScriptValue => True and int32_t => False
            """
            return self._is_traceable

        def member_var_to_ref_expr(self, var_name):
            """
            Returns an expression to convert the given member variable into
            a reference type. E.g. Member<T> => var_name.Get()
            """
            if self._is_member_t_cppgc_member:
                return "{}.Get()".format(var_name)
            return var_name

        def clear_member_var_expr(self, var_name):
            """
            Returns an expression to reset the given member variable.  E.g.
            Vector => var_name.clear() and int32_t => var_name = 0
            """
            return self._clear_member_var_fmt.format(var_name)

    real_type = idl_type.unwrap(typedef=True)

    if real_type.is_boolean:
        return TypeInfo("bool",
                        const_ref_fmt="{}",
                        clear_member_var_fmt="{} = false")

    if real_type.is_numeric:
        return TypeInfo(numeric_type(real_type.keyword_typename),
                        const_ref_fmt="{}",
                        clear_member_var_fmt="{} = 0")

    if real_type.is_bigint:
        return TypeInfo("BigInt",
                        ref_fmt="{}&",
                        const_ref_fmt="const {}&",
                        clear_member_var_fmt="{} = BigInt()")

    if real_type.is_string:
        return TypeInfo("String",
                        ref_fmt="{}&",
                        const_ref_fmt="const {}&",
                        has_null_value=True,
                        is_move_effective=True,
                        clear_member_var_fmt="{} = String()")

    if real_type.is_array_buffer:
        if "AllowShared" in idl_type.effective_annotations:
            # DOMArrayBufferBase is the common base class of DOMArrayBuffer and
            # DOMSharedArrayBuffer, so it works as [AllowShared] ArrayBuffer.
            typename = "DOMArrayBufferBase"
        else:
            typename = "DOMArrayBuffer"
        return TypeInfo(typename,
                        member_fmt="Member<{}>",
                        ref_fmt="{}*",
                        const_ref_fmt="const {}*",
                        value_fmt="{}*",
                        has_null_value=True,
                        is_gc_type=True)

    if real_type.is_buffer_source_type:
        if "AllowShared" in idl_type.effective_annotations:
            return TypeInfo("MaybeShared<DOM{}>".format(
                real_type.keyword_typename),
                            has_null_value=True,
                            is_gc_type=True)
        else:
            return TypeInfo("NotShared<DOM{}>".format(
                real_type.keyword_typename),
                            has_null_value=True,
                            is_gc_type=True)

    if real_type.is_symbol:
        assert False, "Blink does not support/accept IDL symbol type."

    if real_type.is_any or real_type.is_object:
        return TypeInfo("ScriptValue",
                        ref_fmt="{}&",
                        const_ref_fmt="const {}&",
                        has_null_value=True,
                        is_traceable=True)

    if real_type.is_undefined:
        assert False, "Blink does not support/accept IDL undefined type."

    if real_type.type_definition_object:
        typename = blink_class_name(real_type.type_definition_object)
        if real_type.is_enumeration:
            return TypeInfo(typename,
                            ref_fmt="{}",
                            const_ref_fmt="{}",
                            clear_member_var_fmt="")
        return TypeInfo(typename,
                        member_fmt="Member<{}>",
                        ref_fmt="{}*",
                        const_ref_fmt="const {}*",
                        value_fmt="{}*",
                        has_null_value=True,
                        is_gc_type=True)

    if (real_type.is_sequence or real_type.is_frozen_array
            or real_type.is_variadic):
        element_type = blink_type_info(real_type.element_type)
        if element_type.is_traceable:
            # HeapVector is GarbageCollected but we'd like to treat it as
            # a value type (is_gc_type=False, has_null_value=False) rather than
            # a reference type (is_gc_type=True, has_null_value=True) by
            # default.
            typename = "HeapVector<{}>".format(element_type.member_t)
            return TypeInfo(typename,
                            ref_fmt="{}&",
                            const_ref_fmt="const {}&",
                            has_null_value=False,
                            is_gc_type=False,
                            is_move_effective=True,
                            is_heap_vector_type=True,
                            clear_member_var_fmt="{}.clear()")
        else:
            return TypeInfo("Vector<{}>".format(element_type.value_t),
                            ref_fmt="{}&",
                            const_ref_fmt="const {}&",
                            is_move_effective=True,
                            clear_member_var_fmt="{}.clear()")

    if real_type.is_observable_array:
        typename = blink_class_name(
            real_type.observable_array_definition_object)
        return TypeInfo(typename,
                        member_fmt="Member<{}>",
                        ref_fmt="{}*",
                        const_ref_fmt="const {}*",
                        value_fmt="{}*",
                        has_null_value=True,
                        is_gc_type=True)

    if real_type.is_record:
        assert real_type.key_type.is_string
        key_type = blink_type_info(real_type.key_type)
        value_type = blink_type_info(real_type.value_type)
        if value_type.is_traceable:
            # HeapVector is GarbageCollected but we'd like to treat it as
            # a value type (is_gc_type=False, has_null_value=False) rather than
            # a reference type (is_gc_type=True, has_null_value=True) by
            # default.
            typename = ("HeapVector<std::pair<{}, {}>>".format(
                key_type.member_t, value_type.member_t))
            return TypeInfo(typename,
                            ref_fmt="{}&",
                            const_ref_fmt="const {}&",
                            has_null_value=False,
                            is_gc_type=False,
                            is_move_effective=True,
                            is_heap_vector_type=True,
                            clear_member_var_fmt="{}.clear()")
        else:
            typename = "Vector<std::pair<{}, {}>>".format(
                key_type.value_t, value_type.value_t)
            return TypeInfo(typename,
                            ref_fmt="{}&",
                            const_ref_fmt="const {}&",
                            is_move_effective=True,
                            clear_member_var_fmt="{}.clear()")

    if real_type.is_promise:
        if "IDLTypeImplementedAsV8Promise" in real_type.extended_attributes:
            type_name = "v8::Local<v8::Promise>"
        else:
            type_name = "ScriptPromise<{}>".format(
                native_value_tag(real_type.result_type))
        return TypeInfo(type_name,
                        ref_fmt="{}&",
                        const_ref_fmt="const {}&",
                        is_traceable=True)

    if real_type.is_union:
        if real_type.is_phantom:
            return TypeInfo("v8::Local<v8::Value>",
                            ref_fmt="{}*",
                            value_fmt="{}",
                            has_null_value=True,
                            is_gc_type=True)

        typename = blink_class_name(real_type.union_definition_object)
        return TypeInfo(typename,
                        member_fmt="Member<{}>",
                        ref_fmt="{}*",
                        const_ref_fmt="const {}*",
                        value_fmt="{}*",
                        has_null_value=True,
                        is_gc_type=True)

    if real_type.is_nullable:
        inner_type = blink_type_info(real_type.inner_type)
        if inner_type.has_null_value:
            return inner_type
        if inner_type.is_heap_vector_type:
            return TypeInfo(inner_type.typename,
                            member_fmt="Member<{}>",
                            ref_fmt="{}*",
                            const_ref_fmt="const {}*",
                            value_fmt="{}*",
                            has_null_value=True,
                            is_gc_type=True,
                            is_move_effective=False,
                            is_heap_vector_type=False)
        assert not inner_type.is_traceable
        return TypeInfo("std::optional<{}>".format(inner_type.value_t),
                        ref_fmt="{}&",
                        const_ref_fmt="const {}&",
                        is_move_effective=inner_type.is_move_effective,
                        clear_member_var_fmt="{}.reset()")

    assert False, "Unknown type: {}".format(idl_type.syntactic_form)


def native_value_tag(idl_type, argument=None, apply_optional_to_last_arg=True):
    """Returns the tag type of NativeValueTraits."""
    assert isinstance(idl_type, web_idl.IdlType)
    assert argument is None or isinstance(argument, web_idl.Argument)

    if (idl_type.is_optional and argument
            and not (idl_type.is_nullable or argument.default_value)
            and (apply_optional_to_last_arg
                 or argument != argument.owner.arguments[-1])):
        return "IDLOptional<{}>".format(_native_value_tag_impl(idl_type))

    return _native_value_tag_impl(idl_type)


def _pass_as_span_conversion_arguments(idl_type):
    real_type = idl_type.unwrap(typedef=True)
    types = real_type.flattened_member_types if real_type.is_union else [
        real_type
    ]
    sequence_types = set(
        map(lambda t: t.element_type.unwrap(typedef=True),
            filter(lambda t: t.is_sequence, types)))
    assert len(
        sequence_types
    ) < 2, "Unions of sequence types of different types are not supported with [PassAsSpan]"
    typed_arrays = set(filter(lambda t: t.is_typed_array_type, types))
    assert len(
        typed_arrays
    ) < 2, "Unions of typed arrays of different types are not supported with [PassAsSpan]"
    native_type = None
    if typed_arrays:
        typed_array_type = typed_array_element_type(list(typed_arrays)[0])
        native_type = numeric_type(typed_array_type)
        if sequence_types:
            seq_element_type = list(sequence_types)[0].keyword_typename
            types_are_compatible = seq_element_type == typed_array_type
            assert types_are_compatible, "Sequence and typed array types are incompatible (%s vs %s)" % (
                seq_element_type, typed_array_type)
    else:
        assert (not sequence_types
                ), "Plain sequence<> types are not supported with [PassAsSpan]"
        native_type = "void"
        is_buffer_source_type = all(t.is_buffer_source_type for t in types)
        assert is_buffer_source_type, "All types must be buffer"

    flags = []
    if sequence_types:
        flags.append("PassAsSpanMarkerBase::Flags::kAllowSequence")
    allow_shared = "AllowShared" in idl_type.effective_annotations or any(
        "AllowShared" in t.effective_annotations for t in types)
    if allow_shared:
        flags.append("PassAsSpanMarkerBase::Flags::kAllowShared")

    return [
        " | ".join(flags) or "PassAsSpanMarkerBase::Flags::kNone", native_type
    ]


def _native_value_tag_impl(idl_type):
    """Returns the tag type of NativeValueTraits."""
    assert isinstance(idl_type, web_idl.IdlType)

    if idl_type.is_event_handler:
        return "IDL{}".format(idl_type.identifier)

    real_type = idl_type.unwrap(typedef=True)

    if "PassAsSpan" in idl_type.effective_annotations:
        conversion_arguments = _pass_as_span_conversion_arguments(idl_type)
        return "PassAsSpan<{}>".format(", ".join(conversion_arguments))

    if (real_type.is_boolean or real_type.is_numeric or real_type.is_string
            or real_type.is_any or real_type.is_object or real_type.is_bigint
            or real_type.is_undefined):
        return "IDL{}".format(
            idl_type.type_name_with_extended_attribute_key_values)

    if real_type.is_array_buffer:
        if "BufferSourceTypeNoSizeLimit" in real_type.effective_annotations:
            return "IDLBufferSourceTypeNoSizeLimit<{}>".format(
                blink_type_info(real_type).typename)
        return blink_type_info(real_type).typename

    if real_type.is_buffer_source_type:
        if "BufferSourceTypeNoSizeLimit" in real_type.effective_annotations:
            return "IDLBufferSourceTypeNoSizeLimit<{}>".format(
                blink_type_info(real_type).value_t)
        return blink_type_info(real_type).value_t

    if real_type.is_symbol:
        assert False, "Blink does not support/accept IDL symbol type."

    if real_type.type_definition_object:
        return blink_class_name(real_type.type_definition_object)

    if real_type.is_sequence:
        return "IDLSequence<{}>".format(
            _native_value_tag_impl(real_type.element_type))

    if real_type.is_frozen_array:
        return "IDLArray<{}>".format(
            _native_value_tag_impl(real_type.element_type))

    if real_type.is_observable_array:
        return blink_class_name(real_type.observable_array_definition_object)

    if real_type.is_record:
        return "IDLRecord<{}, {}>".format(
            _native_value_tag_impl(real_type.key_type),
            _native_value_tag_impl(real_type.value_type))

    if real_type.is_promise:
        return "IDLPromise<{}>".format(
            _native_value_tag_impl(real_type.result_type))

    if real_type.is_union:
        return blink_class_name(real_type.union_definition_object)

    if real_type.is_nullable:
        return "IDLNullable<{}>".format(
            _native_value_tag_impl(real_type.inner_type))

    assert False, "Unknown type: {}".format(idl_type.syntactic_form)


def make_blink_to_v8_value(
        v8_var_name,
        blink_value_expr,
        idl_type,
        argument=None,
        error_exit_return_statement="return v8::MaybeLocal<v8::Value>();",
        creation_context_script_state="${script_state}"):
    """
    Returns a SymbolNode whose definition converts a Blink value to a v8::Value.
    """
    assert isinstance(v8_var_name, str)
    assert isinstance(blink_value_expr, str)
    assert isinstance(idl_type, web_idl.IdlType)
    assert argument is None or isinstance(argument, web_idl.Argument)
    assert isinstance(error_exit_return_statement, str)
    assert isinstance(creation_context_script_state, str)

    T = TextNode
    F = FormatNode

    if "NodeWrapInOwnContext" in idl_type.effective_annotations:
        assert native_value_tag(idl_type, argument=argument) == "Node"
        execution_context = blink_value_expr + "->GetExecutionContext()"
        creation_context_script_state = _format(
            "{_1} && {_1} != ToExecutionContext({_2}) ? "
            "ToScriptState({_1}, {_2}->World()) : {_2}",
            _1=execution_context,
            _2=creation_context_script_state)

    def create_definition(symbol_node):
        binds = {
            "blink_value_expr": blink_value_expr,
            "creation_context_script_state": creation_context_script_state,
            "native_value_tag": native_value_tag(idl_type, argument=argument),
            "v8_var_name": v8_var_name,
        }
        pattern = ("{v8_var_name} = ToV8Traits<{native_value_tag}>::ToV8("
                   "{creation_context_script_state}, {blink_value_expr});")
        nodes = [
            F("v8::Local<v8::Value> {v8_var_name};", **binds),
            F(pattern, **binds)
        ]
        return SymbolDefinitionNode(symbol_node, nodes)

    return SymbolNode(v8_var_name, definition_constructor=create_definition)


def make_default_value_expr(idl_type, default_value):
    """
    Returns a set of C++ expressions to be used for initialization with default
    values.  The returned object has the following attributes.

      initializer_expr: Used as "Type var{|initializer_expr|};".  This is None
          if "Type var;" sets an appropriate default value.
      initializer_deps: A list of symbol names that |initializer_expr| depends
          on.
      is_initialization_lightweight: True if a possibly-redundant initialization
          will not be more expensive than assignment.  See bellow for an
          example.
      assignment_value: Used as "var = |assignment_value|;".
      assignment_deps: A list of symbol names that |assignment_value| depends
          on.


    |is_initialization_lightweight| is True if

      Type var{${initializer_expr}};
      if (value_is_given)
        var = value;

    is not more expensive than

      Type var;
      if (value_is_given)
        var = value;
      else
        var = ${assignment_value};
    """
    assert isinstance(idl_type, web_idl.IdlType)
    assert (default_value is None
            or isinstance(default_value, web_idl.LiteralConstant))
    assert default_value.is_type_compatible_with(idl_type)

    class DefaultValueExpr(object):
        _ALLOWED_SYMBOLS_IN_DEPS = ("isolate")

        def __init__(self, initializer_expr, initializer_deps,
                     is_initialization_lightweight, assignment_value,
                     assignment_deps):
            assert initializer_expr is None or isinstance(
                initializer_expr, str)
            assert (isinstance(initializer_deps, (list, tuple)) and all(
                dependency in DefaultValueExpr._ALLOWED_SYMBOLS_IN_DEPS
                for dependency in initializer_deps))
            assert isinstance(is_initialization_lightweight, bool)
            assert isinstance(assignment_value, str)
            assert (isinstance(assignment_deps, (list, tuple)) and all(
                dependency in DefaultValueExpr._ALLOWED_SYMBOLS_IN_DEPS
                for dependency in assignment_deps))

            self.initializer_expr = initializer_expr
            self.initializer_deps = tuple(initializer_deps)
            self.is_initialization_lightweight = is_initialization_lightweight
            self.assignment_value = assignment_value
            self.assignment_deps = tuple(assignment_deps)

    if idl_type.unwrap(typedef=True).is_union:
        union_type = idl_type.unwrap(typedef=True)
        member_type = None
        for member_type in union_type.flattened_member_types:
            if default_value.is_type_compatible_with(member_type):
                member_type = member_type
                break
        assert not (member_type is None) or default_value.idl_type.is_nullable

        pattern = "MakeGarbageCollected<{}>({})"
        union_class_name = blink_class_name(union_type.union_definition_object)

        if default_value.idl_type.is_nullable:
            value = pattern.format(union_class_name, "nullptr")
            return DefaultValueExpr(initializer_expr=value,
                                    initializer_deps=[],
                                    is_initialization_lightweight=False,
                                    assignment_value=value,
                                    assignment_deps=[])
        else:
            member_default_expr = make_default_value_expr(
                member_type, default_value)
            value = pattern.format(union_class_name,
                                   member_default_expr.assignment_value)
            return DefaultValueExpr(
                initializer_expr=value,
                initializer_deps=member_default_expr.initializer_deps,
                is_initialization_lightweight=False,
                assignment_value=value,
                assignment_deps=member_default_expr.assignment_deps)

    type_info = blink_type_info(idl_type)

    is_initialization_lightweight = False
    initializer_deps = []
    assignment_deps = []
    if default_value.idl_type.is_nullable:
        if not type_info.has_null_value:
            initializer_expr = None  # !std::optional::has_value() by default
            assignment_value = "std::nullopt"
        elif idl_type.unwrap().type_definition_object is not None:
            initializer_expr = "nullptr"
            is_initialization_lightweight = True
            assignment_value = "nullptr"
        elif idl_type.unwrap().is_string:
            initializer_expr = None  # String::IsNull() by default
            assignment_value = "String()"
        elif idl_type.unwrap().is_buffer_source_type:
            initializer_expr = "nullptr"
            is_initialization_lightweight = True
            assignment_value = "nullptr"
        elif type_info.typename == "ScriptValue":
            initializer_expr = "${isolate}, v8::Null(${isolate})"
            initializer_deps = ["isolate"]
            assignment_value = "ScriptValue::CreateNull(${isolate})"
            assignment_deps = ["isolate"]
        elif idl_type.unwrap().is_union:
            initializer_expr = "nullptr"
            is_initialization_lightweight = True
            assignment_value = "nullptr"
        else:
            assert False
    elif default_value.idl_type.is_sequence:
        initializer_expr = None  # VectorOf<T>::size() == 0 by default
        assignment_value = "{}()".format(type_info.value_t)
    elif default_value.idl_type.is_object:
        dictionary = idl_type.unwrap().type_definition_object
        # Currently "isolate" is the only possible dependency, so whenever
        # .initializer_deps exists, it must be ["isolate"].
        if any((make_default_value_expr(member.idl_type,
                                        member.default_value).initializer_deps)
               for member in dictionary.members if member.default_value):
            value = _format("{}::Create(${isolate})",
                            blink_class_name(dictionary))
            initializer_expr = value
            initializer_deps = ["isolate"]
            assignment_value = value
            assignment_deps = ["isolate"]
        else:
            value = _format("{}::Create()", blink_class_name(dictionary))
            initializer_expr = value
            assignment_value = value
    elif default_value.idl_type.is_boolean:
        value = "true" if default_value.value else "false"
        initializer_expr = value
        is_initialization_lightweight = True
        assignment_value = value
    elif default_value.idl_type.is_integer:
        initializer_expr = default_value.literal
        is_initialization_lightweight = True
        assignment_value = default_value.literal
    elif default_value.idl_type.is_floating_point_numeric:
        if default_value.value == float("NaN"):
            value_fmt = "std::numeric_limits<{type}>::quiet_NaN()"
        elif default_value.value == float("Infinity"):
            value_fmt = "std::numeric_limits<{type}>::infinity()"
        elif default_value.value == float("-Infinity"):
            value_fmt = "-std::numeric_limits<{type}>::infinity()"
        else:
            value_fmt = "{value}"
        value = value_fmt.format(
            type=type_info.value_t, value=default_value.literal)
        initializer_expr = value
        is_initialization_lightweight = True
        assignment_value = value
    elif default_value.idl_type.is_string:
        if idl_type.unwrap().is_string:
            value = "\"{}\"".format(default_value.value)
            initializer_expr = value
            assignment_value = value
        elif idl_type.unwrap().is_enumeration:
            enum_class_name = blink_class_name(
                idl_type.unwrap().type_definition_object)
            enum_value_name = name_style.constant(default_value.value)
            initializer_expr = "{}::Enum::{}".format(enum_class_name,
                                                     enum_value_name)
            is_initialization_lightweight = True
            assignment_value = "{}({})".format(enum_class_name,
                                               initializer_expr)
        else:
            assert False
    else:
        assert False

    return DefaultValueExpr(
        initializer_expr=initializer_expr,
        initializer_deps=initializer_deps,
        is_initialization_lightweight=is_initialization_lightweight,
        assignment_value=assignment_value,
        assignment_deps=assignment_deps)


def make_v8_to_blink_value(blink_var_name,
                           v8_value_expr,
                           idl_type,
                           argument=None,
                           error_exit_return_statement=None,
                           cg_context=None):
    """
    Returns a SymbolNode whose definition converts a v8::Value to a Blink value.
    """
    assert isinstance(blink_var_name, str)
    assert isinstance(v8_value_expr, str)
    assert isinstance(idl_type, web_idl.IdlType)
    assert argument is None or isinstance(argument, web_idl.Argument)
    assert isinstance(error_exit_return_statement, str)

    T = TextNode
    F = FormatNode

    # Use of fast path is a trade-off between speed and binary size, so apply
    # it only when it's effective.  This hack is most significant on Android.
    use_fast_path = (
        cg_context and cg_context.operation
        and not (cg_context.is_return_type_promise_type or
                 "RaisesException" in cg_context.operation.extended_attributes)
        and all((arg.idl_type.type_name == "String" or arg.idl_type.unwrap(
            typedef=True).is_callback_function)
                for arg in cg_context.operation.arguments))
    fast_path_cond = None
    fast_path_body_text = None
    if not use_fast_path:
        pass
    elif idl_type.type_name == "String":
        # A key point of this fast path is that it doesn't require an
        # ExceptionState.
        fast_path_cond = "{}->IsString()".format(v8_value_expr)
        fast_path_body_text = _format(
            "{}.Init(${isolate}, {}.As<v8::String>());", blink_var_name,
            v8_value_expr)
    elif idl_type.unwrap(typedef=True).is_callback_function:
        # A key point of this fast path is that it doesn't require an
        # ExceptionState.
        fast_path_cond = "{}->IsFunction()".format(v8_value_expr)
        fast_path_body_text = "{} = {}::Create({}.As<v8::Function>());".format(
            blink_var_name,
            blink_class_name(idl_type.unwrap().type_definition_object),
            v8_value_expr)

    def create_definition(symbol_node):
        if argument is None:
            func_name = "NativeValue"
            arguments = ["${isolate}", v8_value_expr, "${exception_state}"]
        else:
            func_name = "ArgumentValue"
            arguments = [
                "${isolate}",
                str(argument.index),
                v8_value_expr,
                "${exception_state}",
            ]
        if "StringContext" in idl_type.effective_annotations:
            arguments.append("${class_like_name}")
            arguments.append("${property_name}")
            arguments.append("${execution_context_of_document_tree}")
        blink_value_expr = _format("NativeValueTraits<{_1}>::{_2}({_3})",
                                   _1=native_value_tag(
                                       idl_type,
                                       argument=argument,
                                       apply_optional_to_last_arg=False),
                                   _2=func_name,
                                   _3=", ".join(arguments))
        if argument and argument.default_value:
            default_expr = make_default_value_expr(idl_type,
                                                   argument.default_value)
        else:
            default_expr = None
        exception_exit_node = CxxUnlikelyIfNode(
            cond="${exception_state}.HadException()",
            attribute="[[unlikely]]",
            body=T(error_exit_return_statement))

        if not (default_expr or fast_path_cond):
            return SymbolDefinitionNode(symbol_node, [
                F("auto&& ${{{}}} = {};", blink_var_name, blink_value_expr),
                exception_exit_node,
            ])

        blink_var_type = _format(
            "decltype(NativeValueTraits<{}>::NativeValue("
            "std::declval<v8::Isolate*>(), "
            "std::declval<v8::Local<v8::Value>>(), "
            "std::declval<ExceptionState&>()))",
            native_value_tag(idl_type,
                             argument=argument,
                             apply_optional_to_last_arg=False))
        if default_expr and default_expr.is_initialization_lightweight:
            pattern = "{} ${{{}}}{{{}}};"
            args = [
                blink_var_type, blink_var_name, default_expr.initializer_expr
            ]
        else:
            pattern = "{} ${{{}}};"
            args = [blink_var_type, blink_var_name]
        blink_var_def_node = F(pattern, *args)
        assignment = [
            F("${{{}}} = {};", blink_var_name, blink_value_expr),
            exception_exit_node,
        ]
        if not default_expr:
            pass
        elif (default_expr.initializer_expr is None
              or default_expr.is_initialization_lightweight):
            assignment = CxxLikelyIfNode(
                cond="!{}->IsUndefined()".format(v8_value_expr),
                attribute=None,
                body=assignment)
        else:
            assignment = CxxIfElseNode(
                cond="{}->IsUndefined()".format(v8_value_expr),
                attribute=None,
                then=F("${{{}}} = {};", blink_var_name,
                       default_expr.assignment_value),
                then_likeliness=Likeliness.LIKELY,
                else_=assignment,
                else_likeliness=Likeliness.LIKELY)
        if fast_path_cond:
            assignment = CxxIfElseNode(cond=fast_path_cond,
                                       attribute="[[likely]]",
                                       then=T(fast_path_body_text),
                                       then_likeliness=Likeliness.LIKELY,
                                       else_=assignment,
                                       else_likeliness=Likeliness.UNLIKELY)
        return SymbolDefinitionNode(symbol_node, [
            blink_var_def_node,
            assignment,
        ])

    return SymbolNode(blink_var_name, definition_constructor=create_definition)


def make_v8_to_blink_value_variadic(blink_var_name, v8_array,
                                    v8_array_start_index, idl_type):
    """
    Returns a SymbolNode whose definition converts an array of v8::Value
    (variadic arguments) to a Blink value.
    """
    assert isinstance(blink_var_name, str)
    assert isinstance(v8_array, str)
    assert isinstance(v8_array_start_index, int)
    assert isinstance(idl_type, web_idl.IdlType)

    pattern = ("auto&& ${{{_1}}} = "
               "bindings::VariadicArgumentsToNativeValues<{_2}>({_3});")
    arguments = [
        "${isolate}", v8_array,
        str(v8_array_start_index), "${exception_state}"
    ]
    if "StringContext" in idl_type.element_type.effective_annotations:
        arguments.append("${class_like_name}")
        arguments.append("${property_name}")
        arguments.append("${execution_context_of_document_tree}")
    text = _format(
        pattern,
        _1=blink_var_name,
        _2=native_value_tag(idl_type.element_type),
        _3=", ".join(arguments))

    def create_definition(symbol_node):
        return SymbolDefinitionNode(symbol_node, [
            TextNode(text),
            CxxUnlikelyIfNode(cond="${exception_state}.HadException()",
                              attribute="[[unlikely]]",
                              body=TextNode("return;")),
        ])

    return SymbolNode(blink_var_name, definition_constructor=create_definition)


def typed_array_element_type(idl_type):
    assert isinstance(idl_type, web_idl.IdlType), type(idl_type)
    assert idl_type.is_typed_array_type
    element_type_map = {
        'Int8Array': 'byte',
        'Int16Array': 'short',
        'Int32Array': 'long',
        'BigInt64Array': 'long long',
        'Uint8Array': 'octet',
        'Uint16Array': 'unsigned short',
        'Uint32Array': 'unsigned long',
        'BigUint64Array': 'unsigned long long',
        'Uint8ClampedArray': 'octet',
        'Float32Array': 'unrestricted float',
        'Float64Array': 'unrestricted double',
    }
    return element_type_map.get(idl_type.keyword_typename)


def numeric_type(type_keyword):
    assert isinstance(type_keyword, str)
    type_map = {
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
    return type_map[type_keyword]
