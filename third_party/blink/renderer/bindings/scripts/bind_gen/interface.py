# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import itertools

import web_idl

from . import name_style
from .blink_v8_bridge import blink_class_name
from .blink_v8_bridge import blink_type_info
from .blink_v8_bridge import make_v8_to_blink_value
from .blink_v8_bridge import make_v8_to_blink_value_variadic
from .blink_v8_bridge import v8_bridge_class_name
from .code_node import EmptyNode
from .code_node import ListNode
from .code_node import SequenceNode
from .code_node import SymbolDefinitionNode
from .code_node import SymbolNode
from .code_node import SymbolScopeNode
from .code_node import TextNode
from .code_node import WeakDependencyNode
from .code_node_cxx import CxxBlockNode
from .code_node_cxx import CxxBreakableBlockNode
from .code_node_cxx import CxxClassDefNode
from .code_node_cxx import CxxFuncDeclNode
from .code_node_cxx import CxxFuncDefNode
from .code_node_cxx import CxxLikelyIfNode
from .code_node_cxx import CxxMultiBranchesNode
from .code_node_cxx import CxxNamespaceNode
from .code_node_cxx import CxxSwitchNode
from .code_node_cxx import CxxUnlikelyIfNode
from .codegen_accumulator import CodeGenAccumulator
from .codegen_context import CodeGenContext
from .codegen_expr import CodeGenExpr
from .codegen_expr import expr_and
from .codegen_expr import expr_from_exposure
from .codegen_expr import expr_or
from .codegen_format import format_template as _format
from .codegen_utils import component_export
from .codegen_utils import component_export_header
from .codegen_utils import enclose_with_header_guard
from .codegen_utils import make_copyright_header
from .codegen_utils import make_forward_declarations
from .codegen_utils import make_header_include_directives
from .codegen_utils import write_code_node_to_file
from .mako_renderer import MakoRenderer
from .package_initializer import package_initializer
from .path_manager import PathManager
from .task_queue import TaskQueue


def _is_none_or_str(arg):
    return arg is None or isinstance(arg, str)


def backward_compatible_api_func(cg_context):
    """
    Returns the Blink function name compatible with the old bindings generator.
    """
    assert isinstance(cg_context, CodeGenContext)

    name = (cg_context.member_like.code_generator_info.property_implemented_as
            or cg_context.member_like.identifier
            or cg_context.property_.identifier)

    if cg_context.attribute_get:
        # modules/webaudio/biquad_filter_node.idl has readonly attribute "Q"
        # and somehow it's implemented as "q" in Blink.
        if name == "Q":
            name = "q"

    if cg_context.attribute_set:
        tokens = name_style.raw.tokenize(name)
        if tokens[0] in ("IDL", "css", "xml"):
            tokens[0] = tokens[0].upper()
        else:
            tokens[0] = tokens[0].capitalize()
        tokens.insert(0, "set")
        name = "".join(tokens)

    if cg_context.indexed_property_getter and not name:
        name = "AnonymousIndexedGetter"
    if cg_context.indexed_property_setter and not name:
        name = "AnonymousIndexedSetter"
    if cg_context.named_property_getter and not name:
        name = "AnonymousNamedGetter"
    if cg_context.named_property_setter and not name:
        name = "AnonymousNamedSetter"
    if cg_context.named_property_deleter and not name:
        name = "AnonymousNamedDeleter"

    return name


def callback_function_name(cg_context,
                           overload_index=None,
                           for_cross_origin=False,
                           no_alloc_direct_call=False):
    assert isinstance(cg_context, CodeGenContext)

    def _cxx_name(name):
        """
        Returns a property name that the bindings generator can use in
        generated code.

        Note that Web IDL allows '-' (hyphen-minus) and '_' (low line) in
        identifiers but C++ does not allow or recommend them.  This function
        encodes these characters.
        """
        # In Python3, we can use str.maketrans and str.translate.
        #
        # We're optimistic about name conflict.  It's highly unlikely that
        # these replacements will cause a conflict.
        assert "Dec45" not in name
        assert "Dec95" not in name
        name = name.replace("-", "Dec45")
        name = name.replace("_", "Dec95")
        return name

    if cg_context.constant:
        property_name = cg_context.property_.identifier
    else:
        property_name = _cxx_name(cg_context.property_.identifier)

    if cg_context.attribute_get:
        kind = "AttributeGet"
    elif cg_context.attribute_set:
        kind = "AttributeSet"
    elif cg_context.constant:
        kind = "Constant"
    elif cg_context.constructor_group:
        if cg_context.is_named_constructor:
            kind = "NamedConstructor"
        else:
            property_name = ""
            kind = "Constructor"
    elif cg_context.exposed_construct:
        if cg_context.is_named_constructor:
            kind = "NamedConstructorProperty"
        elif cg_context.legacy_window_alias:
            kind = "LegacyWindowAlias"
        else:
            kind = "ExposedConstruct"
    elif cg_context.operation_group:
        kind = "Operation"
    elif cg_context.stringifier:
        kind = "Operation"

    if for_cross_origin:
        suffix = "CrossOrigin"
    elif overload_index is not None:
        suffix = "Overload{}".format(overload_index + 1)
    elif no_alloc_direct_call:
        suffix = "NoAllocDirectCallback"
    else:
        suffix = "Callback"

    if cg_context.for_world == CodeGenContext.MAIN_WORLD:
        world_suffix = "ForMainWorld"
    elif cg_context.for_world == CodeGenContext.NON_MAIN_WORLDS:
        world_suffix = "ForNonMainWorlds"
    elif cg_context.for_world == CodeGenContext.ALL_WORLDS:
        world_suffix = ""

    return name_style.func(property_name, kind, suffix, world_suffix)


def constant_name(cg_context):
    assert isinstance(cg_context, CodeGenContext)
    assert cg_context.constant

    property_name = cg_context.property_.identifier.lower()

    return name_style.constant(property_name)


def custom_function_name(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    if cg_context.named_property_getter:
        return "NamedPropertyGetterCustom"
    if cg_context.named_property_setter:
        return "NamedPropertySetterCustom"
    if cg_context.named_property_deleter:
        return "NamedPropertyDeleterCustom"

    if cg_context.attribute_get:
        suffix = "AttributeGetterCustom"
    elif cg_context.attribute_set:
        suffix = "AttributeSetterCustom"
    elif cg_context.operation_group:
        suffix = "MethodCustom"
    else:
        assert False

    return name_style.func(cg_context.property_.identifier, suffix)


# ----------------------------------------------------------------------------
# Callback functions
# ----------------------------------------------------------------------------


def bind_blink_api_arguments(code_node, cg_context):
    assert isinstance(code_node, SymbolScopeNode)
    assert isinstance(cg_context, CodeGenContext)

    if cg_context.attribute_get:
        return

    if cg_context.attribute_set:
        real_type = cg_context.attribute.idl_type.unwrap(typedef=True)
        if real_type.is_enumeration:
            pattern = """\
// https://heycam.github.io/webidl/#dfn-attribute-setter
// step 4.6.1. Let S be ? ToString(V).
const auto&& arg1_value_string =
    NativeValueTraits<IDLStringV2>::NativeValue(
        ${isolate}, ${v8_property_value}, ${exception_state});
if (${exception_state}.HadException())
  return;
// step 4.6.2. If S is not one of the enumeration's values, then return
//   undefined.
const auto arg1_value_maybe_enum = {enum_type}::Create(arg1_value_string);
if (!arg1_value_maybe_enum) {{
  bindings::ReportInvalidEnumSetToAttribute(
      ${isolate}, arg1_value_string, "{enum_type_name}", ${exception_state});
  return;  // Return undefined.
}}
const auto ${arg1_value} = arg1_value_maybe_enum.value();
"""
            text = _format(pattern,
                           enum_type=blink_class_name(
                               real_type.type_definition_object),
                           enum_type_name=real_type.identifier)
            code_node.register_code_symbol(SymbolNode("arg1_value", text))
            return

        name = "arg1_value"
        v8_value = "${v8_property_value}"
        code_node.register_code_symbol(
            make_v8_to_blink_value(name, v8_value,
                                   cg_context.attribute.idl_type))
        return

    for index, argument in enumerate(cg_context.function_like.arguments):
        name = name_style.arg_f("arg{}_{}", index + 1, argument.identifier)
        if argument.is_variadic:
            code_node.register_code_symbol(
                make_v8_to_blink_value_variadic(name, "${info}", index,
                                                argument.idl_type))
        else:
            v8_value = "${{info}}[{}]".format(argument.index)
            code_node.register_code_symbol(
                make_v8_to_blink_value(name,
                                       v8_value,
                                       argument.idl_type,
                                       argument_index=index,
                                       default_value=argument.default_value,
                                       cg_context=cg_context))


def bind_callback_local_vars(code_node, cg_context):
    assert isinstance(code_node, SymbolScopeNode)
    assert isinstance(cg_context, CodeGenContext)

    S = SymbolNode
    T = TextNode

    local_vars = []
    template_vars = {}

    local_vars.extend([
        S("blink_property_name",
          ("const AtomicString& ${blink_property_name} = "
           "ToCoreAtomicString(${v8_property_name}.As<v8::String>());")),
        S("class_like_name", ("const char* const ${class_like_name} = "
                              "\"${class_like.identifier}\";")),
        S("current_context", ("v8::Local<v8::Context> ${current_context} = "
                              "${isolate}->GetCurrentContext();")),
        S("current_script_state", ("ScriptState* ${current_script_state} = "
                                   "ScriptState::From(${current_context});")),
        S("isolate", "v8::Isolate* ${isolate} = ${info}.GetIsolate();"),
        S("non_undefined_argument_length",
          ("const int ${non_undefined_argument_length} = "
           "bindings::NonUndefinedArgumentLength(${info});")),
        S("per_context_data", ("V8PerContextData* ${per_context_data} = "
                               "${script_state}->PerContextData();")),
        S("per_isolate_data", ("V8PerIsolateData* ${per_isolate_data} = "
                               "V8PerIsolateData::From(${isolate});")),
        S("property_name",
          "const char* const ${property_name} = \"${property.identifier}\";"),
        S("receiver_context", ("v8::Local<v8::Context> ${receiver_context} = "
                               "${v8_receiver}->CreationContext();")),
        S("receiver_script_state",
          ("ScriptState* ${receiver_script_state} = "
           "ScriptState::From(${receiver_context});")),
    ])

    is_receiver_context = not (
        (cg_context.member_like and cg_context.member_like.is_static)
        or cg_context.constructor)

    # creation_context
    pattern = "const v8::Local<v8::Context>& ${creation_context} = {_1};"
    _1 = "${receiver_context}" if is_receiver_context else "${current_context}"
    local_vars.append(S("creation_context", _format(pattern, _1=_1)))

    # creation_context_object
    text = ("${v8_receiver}"
            if is_receiver_context else "${current_context}->Global()")
    template_vars["creation_context_object"] = T(text)

    # script_state
    pattern = "ScriptState* ${script_state} = {_1};"
    _1 = ("${receiver_script_state}"
          if is_receiver_context else "${current_script_state}")
    local_vars.append(S("script_state", _format(pattern, _1=_1)))

    # execution_context
    pattern = "ExecutionContext* ${execution_context} = {_1};"
    _1 = ("${receiver_execution_context}"
          if is_receiver_context else "${current_execution_context}")
    local_vars.append(S("execution_context", _format(pattern, _1=_1)))
    node = S("current_execution_context",
             ("ExecutionContext* ${current_execution_context} = "
              "ExecutionContext::From(${current_context});"))
    node.accumulate(
        CodeGenAccumulator.require_include_headers([
            "third_party/blink/renderer/core/execution_context/execution_context.h"
        ]))
    local_vars.append(node)
    node = S("receiver_execution_context",
             ("ExecutionContext* ${receiver_execution_context} = "
              "ExecutionContext::From(${receiver_context});"))
    node.accumulate(
        CodeGenAccumulator.require_include_headers([
            "third_party/blink/renderer/core/execution_context/execution_context.h"
        ]))
    local_vars.append(node)

    # execution_context_of_document_tree
    pattern = "ExecutionContext* ${execution_context_of_document_tree} = {_1};"
    if is_receiver_context:
        _1 = "bindings::ExecutionContextFromV8Wrappable(${blink_receiver})"
    else:
        _1 = "${current_execution_context}"
    text = _format(pattern, _1=_1)
    local_vars.append(S("execution_context_of_document_tree", text))

    # exception_state_context_type
    pattern = (
        "const ExceptionState::ContextType ${exception_state_context_type} = "
        "{_1};")
    if cg_context.attribute_get:
        _1 = "ExceptionState::kGetterContext"
    elif cg_context.attribute_set:
        _1 = "ExceptionState::kSetterContext"
    elif cg_context.constructor_group:
        _1 = "ExceptionState::kConstructionContext"
    elif cg_context.indexed_property_getter:
        _1 = "ExceptionState::kIndexedGetterContext"
    elif cg_context.indexed_property_setter:
        _1 = "ExceptionState::kIndexedSetterContext"
    elif cg_context.named_property_getter:
        _1 = "ExceptionState::kNamedGetterContext"
    elif cg_context.named_property_setter:
        _1 = "ExceptionState::kNamedSetterContext"
    elif cg_context.named_property_deleter:
        _1 = "ExceptionState::kNamedDeletionContext"
    else:
        _1 = "ExceptionState::kExecutionContext"
    local_vars.append(
        S("exception_state_context_type", _format(pattern, _1=_1)))

    # exception_state
    pattern = "ExceptionState ${exception_state}({_1});{_2}"
    _1 = ["${isolate}", "${exception_state_context_type}"]
    if cg_context.is_named_constructor:
        _1.append("\"{}\"".format(cg_context.property_.identifier))
    else:
        _1.append("${class_like_name}")
    if (cg_context.property_ and cg_context.property_.identifier
            and not cg_context.constructor_group):
        _1.append("${property_name}")
    _2 = ""
    if cg_context.is_return_type_promise_type:
        _2 = ("\n"
              "ExceptionToRejectPromiseScope reject_promise_scope"
              "(${info}, ${exception_state});")
    local_vars.append(
        S("exception_state", _format(pattern, _1=", ".join(_1), _2=_2)))

    # blink_receiver
    if cg_context.class_like.identifier == "Window":
        # TODO(yukishiino): Window interface should be
        # [ImplementedAs=LocalDOMWindow] instead of [ImplementedAs=DOMWindow],
        # and [CrossOrigin] properties should be implemented specifically with
        # DOMWindow class.  Then, we'll have less hacks.
        if (not cg_context.member_like or
                "CrossOrigin" in cg_context.member_like.extended_attributes):
            text = ("DOMWindow* ${blink_receiver} = "
                    "${class_name}::ToWrappableUnsafe(${v8_receiver});")
        else:
            text = ("LocalDOMWindow* ${blink_receiver} = To<LocalDOMWindow>("
                    "${class_name}::ToWrappableUnsafe(${v8_receiver}));")
    else:
        pattern = ("{_1}* ${blink_receiver} = "
                   "${class_name}::ToWrappableUnsafe(${v8_receiver});")
        _1 = blink_class_name(cg_context.class_like)
        text = _format(pattern, _1=_1)
    local_vars.append(S("blink_receiver", text))

    # v8_property_value
    if cg_context.v8_callback_type == CodeGenContext.V8_FUNCTION_CALLBACK:
        # In case of V8_ACCESSOR_NAME_SETTER_CALLBACK, |v8_property_value| is
        # defined as an argument.  In case of V8_FUNCTION_CALLBACK (of IDL
        # attribute set function), |info[0]| is the value to be set.
        local_vars.append(
            S("v8_property_value",
              "v8::Local<v8::Value> ${v8_property_value} = ${info}[0];"))

    # v8_receiver
    if cg_context.v8_callback_type == CodeGenContext.V8_FUNCTION_CALLBACK:
        # In case of v8::FunctionCallbackInfo, This() is the receiver object.
        local_vars.append(
            S("v8_receiver",
              "v8::Local<v8::Object> ${v8_receiver} = ${info}.This();"))
    else:
        # In case of v8::PropertyCallbackInfo, Holder() is the object that has
        # the property being processed.
        local_vars.append(
            S("v8_receiver",
              "v8::Local<v8::Object> ${v8_receiver} = ${info}.Holder();"))

    # throw_security_error
    template_vars["throw_security_error"] = T(
        "BindingSecurity::FailedAccessCheckFor("
        "${info}.GetIsolate(), "
        "${class_name}::GetWrapperTypeInfo(), "
        "${info}.Holder());")

    code_node.register_code_symbols(local_vars)
    code_node.add_template_vars(template_vars)


def _make_reflect_content_attribute_key(code_node, cg_context):
    assert isinstance(code_node, SymbolScopeNode)
    assert isinstance(cg_context, CodeGenContext)

    name = (cg_context.attribute.extended_attributes.value_of("Reflect")
            or cg_context.attribute.identifier.lower())
    if cg_context.attribute_get and name in ("class", "id", "name"):
        return None

    if cg_context.class_like.identifier.startswith("SVG"):
        namespace = "svg_names"
        code_node.accumulate(
            CodeGenAccumulator.require_include_headers(
                ["third_party/blink/renderer/core/svg_names.h"]))
    else:
        namespace = "html_names"
        code_node.accumulate(
            CodeGenAccumulator.require_include_headers(
                ["third_party/blink/renderer/core/html_names.h"]))
    return "{}::{}".format(namespace, name_style.constant(name, "attr"))


def _make_reflect_accessor_func_name(cg_context):
    assert isinstance(cg_context, CodeGenContext)
    assert cg_context.attribute_get or cg_context.attribute_set

    if cg_context.attribute_get:
        name = (cg_context.attribute.extended_attributes.value_of("Reflect")
                or cg_context.attribute.identifier.lower())
        if name in ("class", "id", "name"):
            return name_style.func("get", name, "attribute")

        if "URL" in cg_context.attribute.extended_attributes:
            return "GetURLAttribute"

    FAST_ACCESSORS = {
        "boolean": ("FastHasAttribute", "SetBooleanAttribute"),
        "long": ("GetIntegralAttribute", "SetIntegralAttribute"),
        "unsigned long": ("GetUnsignedIntegralAttribute",
                          "SetUnsignedIntegralAttribute"),
    }
    idl_type = cg_context.attribute.idl_type.unwrap()
    accessors = FAST_ACCESSORS.get(idl_type.keyword_typename)
    if accessors:
        return accessors[0 if cg_context.attribute_get else 1]

    if (idl_type.is_interface
            and idl_type.type_definition_object.does_implement("Element")):
        if cg_context.attribute_get:
            return "GetElementAttribute"
        else:
            return "SetElementAttribute"

    if idl_type.element_type:
        element_type = idl_type.element_type.unwrap()
        if (element_type.is_interface and
                element_type.type_definition_object.does_implement("Element")):
            if cg_context.attribute_get:
                return "GetElementArrayAttribute"
            else:
                return "SetElementArrayAttribute"

    if cg_context.attribute_get:
        return "FastGetAttribute"
    else:
        return "setAttribute"


def _make_reflect_process_keyword_state(cg_context):
    # https://html.spec.whatwg.org/C/#keywords-and-enumerated-attributes

    assert isinstance(cg_context, CodeGenContext)
    assert cg_context.attribute_get or cg_context.attribute_set

    T = TextNode
    F = lambda *args, **kwargs: T(_format(*args, **kwargs))

    if not cg_context.attribute_get:
        return None

    ext_attrs = cg_context.attribute.extended_attributes
    keywords = ext_attrs.values_of("ReflectOnly")
    missing_default = ext_attrs.value_of("ReflectMissing")
    empty_default = ext_attrs.value_of("ReflectEmpty")
    invalid_default = ext_attrs.value_of("ReflectInvalid")

    def constant(keyword):
        if not keyword:
            return "g_empty_atom"
        return "keywords::{}".format(name_style.constant(keyword))

    branches = CxxMultiBranchesNode()
    branches.accumulate(
        CodeGenAccumulator.require_include_headers(
            ["third_party/blink/renderer/core/keywords.h"]))
    nodes = [
        T("// [ReflectOnly]"),
        T("const AtomicString reflect_value(${return_value}.LowerASCII());"),
        branches,
    ]

    if missing_default is not None:
        branches.append(
            cond="reflect_value.IsNull()",
            body=F("${return_value} = {};", constant(missing_default)))
    elif cg_context.return_type.unwrap(nullable=False).is_nullable:
        branches.append(
            cond="reflect_value.IsNull()",
            body=T("// Null string to IDL null."))

    if empty_default is not None:
        branches.append(
            cond="reflect_value.IsEmpty()",
            body=F("${return_value} = {};", constant(empty_default)))

    expr = " || ".join(
        map(lambda keyword: "reflect_value == {}".format(constant(keyword)),
            keywords))
    branches.append(cond=expr, body=T("${return_value} = reflect_value;"))

    if invalid_default is not None:
        branches.append(
            cond=True,
            body=F("${return_value} = {};", constant(invalid_default)))
    else:
        branches.append(
            cond=True, body=F("${return_value} = {};", constant("")))

    return SequenceNode(nodes)


def _make_blink_api_call(code_node,
                         cg_context,
                         num_of_args=None,
                         overriding_args=None):
    assert isinstance(code_node, SymbolScopeNode)
    assert isinstance(cg_context, CodeGenContext)
    assert num_of_args is None or isinstance(num_of_args, (int, long))
    assert (overriding_args is None
            or (isinstance(overriding_args, (list, tuple))
                and all(isinstance(arg, str) for arg in overriding_args)))

    arguments = []
    ext_attrs = cg_context.member_like.extended_attributes

    values = ext_attrs.values_of("CallWith") + (
        ext_attrs.values_of("GetterCallWith") if cg_context.attribute_get else
        ext_attrs.values_of("SetterCallWith") if cg_context.attribute_set else
        ())
    if "Isolate" in values:
        arguments.append("${isolate}")
    if "ScriptState" in values:
        arguments.append("${script_state}")
    if "ExecutionContext" in values:
        arguments.append("${execution_context}")
    if "Document" in values:
        arguments.append(
            "*bindings::ToDocumentFromExecutionContext(${execution_context})")
    if "ThisValue" in values:
        arguments.append("ScriptValue(${isolate}, ${v8_receiver})")

    code_generator_info = cg_context.member_like.code_generator_info
    is_partial = code_generator_info.defined_in_partial
    if (is_partial and
            not (cg_context.constructor or cg_context.member_like.is_static)):
        arguments.append("*${blink_receiver}")

    if "Reflect" in ext_attrs:  # [Reflect]
        key = _make_reflect_content_attribute_key(code_node, cg_context)
        if key:
            arguments.append(key)

    if overriding_args is not None:
        arguments.extend(overriding_args)
    elif cg_context.attribute_get:
        pass
    elif cg_context.attribute_set:
        arguments.append("${arg1_value}")
    else:
        for index, argument in enumerate(cg_context.function_like.arguments):
            if num_of_args is not None and index == num_of_args:
                break
            name = name_style.arg_f("arg{}_{}", index + 1, argument.identifier)
            arguments.append(_format("${{{}}}", name))

    if cg_context.is_return_by_argument:
        arguments.append("${return_value}")

    if cg_context.may_throw_exception:
        arguments.append("${exception_state}")

    func_name = backward_compatible_api_func(cg_context)
    if cg_context.constructor:
        if cg_context.is_named_constructor:
            func_name = "CreateForJSConstructor"
        else:
            func_name = "Create"
    if "Reflect" in ext_attrs:  # [Reflect]
        func_name = _make_reflect_accessor_func_name(cg_context)

    if (cg_context.constructor or cg_context.member_like.is_static
            or is_partial):
        class_like = cg_context.member_like.owner_mixin or cg_context.class_like
        class_name = (code_generator_info.receiver_implemented_as
                      or name_style.class_(class_like.identifier))
        func_designator = "{}::{}".format(class_name, func_name)
    else:
        func_designator = _format("${blink_receiver}->{}", func_name)

    return _format("{_1}({_2})", _1=func_designator, _2=", ".join(arguments))


def bind_return_value(code_node, cg_context, overriding_args=None):
    assert isinstance(code_node, SymbolScopeNode)
    assert isinstance(cg_context, CodeGenContext)
    assert (overriding_args is None
            or (isinstance(overriding_args, (list, tuple))
                and all(isinstance(arg, str) for arg in overriding_args)))

    T = TextNode
    F = lambda *args, **kwargs: T(_format(*args, **kwargs))

    def create_definition(symbol_node):
        api_calls = []  # Pairs of (num_of_args, api_call_text)
        if overriding_args is None:
            arguments = (cg_context.function_like.arguments
                         if cg_context.function_like else [])
            for index, arg in enumerate(arguments):
                if arg.is_optional and not arg.default_value:
                    api_calls.append((index,
                                      _make_blink_api_call(
                                          code_node, cg_context, index)))
            api_calls.append((None, _make_blink_api_call(
                code_node, cg_context)))
        else:
            api_calls.append((None,
                              _make_blink_api_call(
                                  code_node,
                                  cg_context,
                                  overriding_args=overriding_args)))

        nodes = []
        is_return_type_void = ((not cg_context.return_type
                                or cg_context.return_type.unwrap().is_void) and
                               not cg_context.does_override_idl_return_type)
        if not (is_return_type_void
                or cg_context.does_override_idl_return_type):
            return_type = blink_type_info(cg_context.return_type).value_t
        if len(api_calls) == 1:
            _, api_call = api_calls[0]
            if is_return_type_void:
                nodes.append(F("{};", api_call))
            elif cg_context.is_return_by_argument:
                nodes.append(F("{} ${return_value};", return_type))
                nodes.append(F("{};", api_call))
            elif "ReflectOnly" in cg_context.member_like.extended_attributes:
                # [ReflectOnly]
                nodes.append(F("auto ${return_value} = {};", api_call))
            else:
                nodes.append(F("auto&& ${return_value} = {};", api_call))
        else:
            branches = SequenceNode()
            for index, api_call in api_calls:
                if is_return_type_void or cg_context.is_return_by_argument:
                    assignment = "{};".format(api_call)
                else:
                    assignment = _format("${return_value} = {};", api_call)
                if index is not None:
                    branches.append(
                        CxxLikelyIfNode(
                            cond=_format(
                                "${non_undefined_argument_length} <= {}",
                                index),
                            body=[
                                T(assignment),
                                T("break;"),
                            ]))
                else:
                    branches.append(T(assignment))

            if not is_return_type_void:
                nodes.append(F("{} ${return_value};", return_type))
            nodes.append(CxxBreakableBlockNode(branches))

        if cg_context.may_throw_exception:
            nodes.append(
                CxxUnlikelyIfNode(
                    cond="${exception_state}.HadException()",
                    body=T("return;")))

        if "ReflectOnly" in cg_context.member_like.extended_attributes:
            # [ReflectOnly]
            node = _make_reflect_process_keyword_state(cg_context)
            if node:
                nodes.append(EmptyNode())
                nodes.append(node)

        return SymbolDefinitionNode(symbol_node, nodes)

    code_node.register_code_symbol(
        SymbolNode("return_value", definition_constructor=create_definition))


def make_bindings_trace_event(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    event_name = "{}.{}".format(cg_context.class_like.identifier,
                                cg_context.property_.identifier)
    if cg_context.attribute_get:
        event_name = "{}.{}".format(event_name, "get")
    elif cg_context.attribute_set:
        event_name = "{}.{}".format(event_name, "set")
    elif cg_context.constructor_group and not cg_context.is_named_constructor:
        event_name = "{}.{}".format(cg_context.class_like.identifier,
                                    "constructor")

    return TextNode("BLINK_BINDINGS_TRACE_EVENT(\"{}\");".format(event_name))


def make_check_argument_length(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    T = TextNode
    F = lambda *args, **kwargs: T(_format(*args, **kwargs))

    if cg_context.v8_callback_type != CodeGenContext.V8_FUNCTION_CALLBACK:
        return None

    if cg_context.attribute_get:
        num_of_required_args = 0
    elif cg_context.attribute_set:
        idl_type = cg_context.attribute.idl_type
        if not (idl_type.does_include_nullable_or_dict
                or idl_type.unwrap().is_any or "LegacyTreatNonObjectAsNull" in
                idl_type.unwrap().extended_attributes
                or "PutForwards" in cg_context.attribute.extended_attributes
                or "Replaceable" in cg_context.attribute.extended_attributes):
            # ES undefined in ${v8_property_value} will cause a TypeError
            # anyway, so omit the check against the number of arguments.
            return None
        num_of_required_args = 1
    elif cg_context.function_like:
        num_of_required_args = (
            cg_context.function_like.num_of_required_arguments)
    elif isinstance(cg_context.property_, web_idl.OverloadGroup):
        num_of_required_args = (
            cg_context.property_.min_num_of_required_arguments)
    else:
        assert False

    if num_of_required_args == 0:
        return None

    return CxxUnlikelyIfNode(
        cond=_format("UNLIKELY(${info}.Length() < {})", num_of_required_args),
        body=[
            F(("${exception_state}.ThrowTypeError("
               "ExceptionMessages::NotEnoughArguments"
               "({}, ${info}.Length()));"), num_of_required_args),
            T("return;"),
        ])


def make_check_constructor_call(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    T = TextNode

    node = SequenceNode([
        CxxUnlikelyIfNode(
            cond="!${info}.IsConstructCall()",
            body=T("${exception_state}.ThrowTypeError("
                   "ExceptionMessages::ConstructorCalledAsFunction());\n"
                   "return;")),
    ])
    if not cg_context.is_named_constructor:
        node.append(
            CxxLikelyIfNode(
                cond=("ConstructorMode::Current(${isolate}) == "
                      "ConstructorMode::kWrapExistingObject"),
                body=T("bindings::V8SetReturnValue(${info}, ${v8_receiver});\n"
                       "return;")))
    node.accumulate(
        CodeGenAccumulator.require_include_headers([
            "third_party/blink/renderer/platform/bindings/v8_object_constructor.h"
        ]))
    return node


def make_check_receiver(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    T = TextNode

    if cg_context.member_like.is_static:
        return None

    if (cg_context.attribute and
            "LegacyLenientThis" in cg_context.attribute.extended_attributes):
        return SequenceNode([
            T("// [LegacyLenientThis]"),
            CxxUnlikelyIfNode(
                cond="!${class_name}::HasInstance(${isolate}, ${v8_receiver})",
                body=T("return;")),
        ])

    if cg_context.is_return_type_promise_type:
        return SequenceNode([
            T("// Promise returning function: "
              "Convert a TypeError to a reject promise."),
            CxxUnlikelyIfNode(
                cond="!${class_name}::HasInstance(${isolate}, ${v8_receiver})",
                body=[
                    T("${exception_state}.ThrowTypeError("
                      "\"Illegal invocation\");"),
                    T("return;"),
                ])
        ])

    return None


def make_check_security_of_return_value(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    T = TextNode

    check_security = cg_context.member_like.extended_attributes.value_of(
        "CheckSecurity")
    if check_security != "ReturnValue":
        return None

    web_feature = _format(
        "WebFeature::{}",
        name_style.constant("CrossOrigin", cg_context.class_like.identifier,
                            cg_context.property_.identifier))
    use_counter = _format(
        "UseCounter::Count(${current_execution_context}, {});", web_feature)
    cond = T("!BindingSecurity::ShouldAllowAccessTo("
             "ToLocalDOMWindow(${current_context}), ${return_value}, "
             "BindingSecurity::ErrorReportOption::kDoNotReport)")
    body = [
        T(use_counter),
        T("bindings::V8SetReturnValue(${info}, nullptr);\n"
          "return;"),
    ]
    node = SequenceNode([
        T("// [CheckSecurity=ReturnValue]"),
        CxxUnlikelyIfNode(cond=cond, body=body),
    ])
    node.accumulate(
        CodeGenAccumulator.require_include_headers([
            "third_party/blink/renderer/bindings/core/v8/binding_security.h",
            "third_party/blink/renderer/core/frame/web_feature.h",
            "third_party/blink/renderer/platform/instrumentation/use_counter.h",
        ]))
    return node


def make_cooperative_scheduling_safepoint(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    node = TextNode("scheduler::CooperativeSchedulingManager::Instance()"
                    "->Safepoint();")
    node.accumulate(
        CodeGenAccumulator.require_include_headers([
            "third_party/blink/renderer/platform/scheduler/public/cooperative_scheduling_manager.h"
        ]))
    return node


def make_log_activity(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    target = cg_context.member_like or cg_context.property_
    ext_attrs = target.extended_attributes
    if "LogActivity" not in ext_attrs:
        return None
    target = ext_attrs.value_of("LogActivity")
    if target:
        assert target in ("GetterOnly", "SetterOnly")
        if ((target == "GetterOnly" and not cg_context.attribute_get)
                or (target == "SetterOnly" and not cg_context.attribute_set)):
            return None
    if (cg_context.for_world == cg_context.MAIN_WORLD
            and "LogAllWorlds" not in ext_attrs):
        return None

    pattern = "{_1}${per_context_data} && ${per_context_data}->ActivityLogger()"
    _1 = ""
    if (cg_context.attribute and "PerWorldBindings" not in ext_attrs
            and "LogAllWorlds" not in ext_attrs):
        _1 = "${script_state}->World().IsIsolatedWorld() && "
    cond = _format(pattern, _1=_1)

    pattern = "${per_context_data}->ActivityLogger()->{_1}(\"{_2}.{_3}\"{_4});"
    _2 = cg_context.class_like.identifier
    _3 = cg_context.property_.identifier
    if cg_context.attribute_get:
        _1 = "LogGetter"
        _4 = ""
    elif cg_context.attribute_set:
        _1 = "LogSetter"
        _4 = ", ${v8_property_value}"
    elif cg_context.operation_group:
        _1 = "LogMethod"
        _4 = ", ${info}"
    body = _format(pattern, _1=_1, _2=_2, _3=_3, _4=_4)

    pattern = ("// [LogActivity], [LogAllWorlds]\n" "if ({_1}) {{ {_2} }}")
    node = TextNode(_format(pattern, _1=cond, _2=body))
    node.accumulate(
        CodeGenAccumulator.require_include_headers([
            "third_party/blink/renderer/platform/bindings/v8_dom_activity_logger.h",
            "third_party/blink/renderer/platform/bindings/v8_per_context_data.h",
        ]))
    return node


def _make_overload_dispatcher_per_arg_size(cg_context, items):
    """
    https://heycam.github.io/webidl/#dfn-overload-resolution-algorithm

    Args:
        items: Partial list of an "effective overload set" with the same
            type list size.

    Returns:
        A pair of a resulting CodeNode and a boolean flag that is True if there
        exists a case that overload resolution will fail, i.e. a bailout that
        throws a TypeError is necessary.
    """
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(items, (list, tuple))
    assert all(
        isinstance(item, web_idl.OverloadGroup.EffectiveOverloadItem)
        for item in items)

    # Variables shared with nested functions
    if len(items) > 1:
        arg_index = web_idl.OverloadGroup.distinguishing_argument_index(items)
    else:
        arg_index = None
    func_like = None
    dispatcher_nodes = SequenceNode()

    # True if there exists a case that overload resolution will fail.
    can_fail = True

    def find_test(item, test):
        # |test| is a callable that takes (t, u) where:
        #   t = the idl_type (in the original form)
        #   u = the unwrapped version of t
        idl_type = item.type_list[arg_index]
        t = idl_type
        u = idl_type.unwrap()
        return test(t, u) or (u.is_union and any(
            [test(m, m.unwrap()) for m in u.flattened_member_types]))

    def find(test):
        for item in items:
            if find_test(item, test):
                return item.function_like
        return None

    def find_all_interfaces():
        result = []  # [(func_like, idl_type), ...]
        for item in items:
            idl_type = item.type_list[arg_index].unwrap()
            if idl_type.is_interface:
                result.append((item.function_like, idl_type))
            if idl_type.is_union:
                for member_type in idl_type.flattened_member_types:
                    if member_type.unwrap().is_interface:
                        result.append((item.function_like,
                                       member_type.unwrap()))
        return result

    def make_node(pattern):
        value = _format("${info}[{}]", arg_index)
        func_name = callback_function_name(cg_context,
                                           func_like.overload_index)
        return TextNode(_format(pattern, value=value, func_name=func_name))

    def dispatch_if(expr):
        if expr is True:
            pattern = "return {func_name}(${info});"
        else:
            pattern = ("if (" + expr + ") {{\n"
                       "  return {func_name}(${info});\n"
                       "}}")
        node = make_node(pattern)
        conditional = expr_from_exposure(func_like.exposure)
        if not conditional.is_always_true:
            node = CxxUnlikelyIfNode(cond=conditional, body=node)
        dispatcher_nodes.append(node)
        return expr is True and conditional.is_always_true

    if len(items) == 1:
        func_like = items[0].function_like
        can_fail = False
        return make_node("return {func_name}(${info});"), can_fail

    # 12.2. If V is undefined, ...
    func_like = find(lambda t, u: t.is_optional)
    if func_like:
        dispatch_if("{value}->IsUndefined()")

    # 12.3. if V is null or undefined, ...
    func_like = find(lambda t, u: t.does_include_nullable_or_dict)
    if func_like:
        dispatch_if("{value}->IsNullOrUndefined()")

    # 12.4. if V is a platform object, ...
    def inheritance_length(func_and_type):
        return len(func_and_type[1].type_definition_object.
                   inclusive_inherited_interfaces)

    # Attempt to match from most derived to least derived.
    for func_like, idl_type in sorted(
            find_all_interfaces(), key=inheritance_length, reverse=True):
        v8_bridge_name = v8_bridge_class_name(
            idl_type.unwrap().type_definition_object)
        dispatch_if(
            _format("{}::HasInstance(${isolate}, {value})", v8_bridge_name))

    # V8 specific optimization: BufferSource = ArrayBufferView or ArrayBuffer
    is_typedef_name = lambda t, name: t.is_typedef and t.identifier == name
    func_like = find(
        lambda t, u: is_typedef_name(t.unwrap(typedef=False), "BufferSource"))
    if func_like:
        dispatch_if("{value}->IsArrayBufferView() || "
                    "{value}->IsArrayBuffer() || "
                    "{value}->IsSharedArrayBuffer()")
    else:
        # 12.5. if Type(V) is Object, V has an [[ArrayBufferData]] internal
        #   slot, ...
        func_like = find(lambda t, u: u.is_array_buffer)
        if func_like:
            dispatch_if("{value}->IsArrayBuffer() || "
                        "{value}->IsSharedArrayBuffer()")

        # V8 specific optimization: ArrayBufferView
        func_like = find(lambda t, u: u.is_array_buffer_view)
        if func_like:
            dispatch_if("{value}->IsArrayBufferView()")

    # 12.6. if Type(V) is Object, V has a [[DataView]] internal slot, ...
    func_like = find(lambda t, u: u.is_data_view)
    if func_like:
        dispatch_if("{value}->IsDataView()")

    # 12.7. if Type(V) is Object, V has a [[TypedArrayName]] internal slot, ...
    typed_array_types = ("Int8Array", "Int16Array", "Int32Array", "Uint8Array",
                         "Uint16Array", "Uint32Array", "Uint8ClampedArray",
                         "Float32Array", "Float64Array")
    for typed_array_type in typed_array_types:
        func_like = find(lambda t, u: u.keyword_typename == typed_array_type)
        if func_like:
            dispatch_if(_format("{value}->Is{}()", typed_array_type))

    # 12.8. if IsCallable(V) is true, ...
    func_like = find(lambda t, u: u.is_callback_function)
    if func_like:
        dispatch_if("{value}->IsFunction()")

    # 12.9. if Type(V) is Object and ... @@iterator ...
    func_like = find(lambda t, u: u.is_sequence or u.is_frozen_array)
    if func_like:
        dispatch_if("{value}->IsArray() || "  # Excessive optimization
                    "bindings::IsEsIterableObject"
                    "(${isolate}, {value}, ${exception_state})")
        dispatcher_nodes.append(
            TextNode("if (${exception_state}.HadException()) {\n"
                     "  return;\n"
                     "}"))

    # 12.10. if Type(V) is Object and ...
    def is_es_object_type(t, u):
        return (u.is_callback_interface or u.is_dictionary or u.is_record
                or u.is_object)

    func_like = find(is_es_object_type)
    if func_like:
        dispatch_if("{value}->IsObject()")

    # 12.11. if Type(V) is Boolean and ...
    func_like = find(lambda t, u: u.is_boolean)
    if func_like:
        dispatch_if("{value}->IsBoolean()")

    # 12.12. if Type(V) is Number and ...
    func_like = find(lambda t, u: u.is_numeric)
    if func_like:
        dispatch_if("{value}->IsNumber()")

    # 12.13. if there is an entry in S that has ... a string type ...
    # 12.14. if there is an entry in S that has ... a numeric type ...
    # 12.15. if there is an entry in S that has ... boolean ...
    # 12.16. if there is an entry in S that has any ...
    func_likes = [
        find(lambda t, u: u.is_enumeration),
        find(lambda t, u: u.is_string),
        find(lambda t, u: u.is_numeric),
        find(lambda t, u: u.is_boolean),
        find(lambda t, u: u.is_any),
    ]
    for func_like in func_likes:
        if func_like:
            if dispatch_if(True):
                can_fail = False
                break

    return dispatcher_nodes, can_fail


def make_overload_dispatcher(cg_context):
    # https://heycam.github.io/webidl/#dfn-overload-resolution-algorithm

    assert isinstance(cg_context, CodeGenContext)

    T = TextNode
    F = lambda *args, **kwargs: T(_format(*args, **kwargs))

    overload_group = cg_context.property_
    items = overload_group.effective_overload_set()
    args_size = lambda item: len(item.type_list)
    items_grouped_by_arg_size = itertools.groupby(
        sorted(items, key=args_size, reverse=True), key=args_size)

    # TODO(yukishiino): Runtime-enabled features should be taken into account
    # when calculating the max argument size.
    max_arg_size = max(map(args_size, items))
    arg_count_def = F("const int arg_count = std::min(${info}.Length(), {});",
                      max_arg_size)

    branches = SequenceNode()
    did_use_break = False
    for arg_size, items in items_grouped_by_arg_size:
        items = list(items)

        node, can_fail = _make_overload_dispatcher_per_arg_size(
            cg_context, items)

        if arg_size > 0:
            node = CxxLikelyIfNode(
                cond="arg_count == {}".format(arg_size),
                body=[node, T("break;") if can_fail else None])
            did_use_break = did_use_break or can_fail

        conditional = expr_or(
            map(lambda item: expr_from_exposure(item.function_like.exposure),
                items))
        if not conditional.is_always_true:
            node = CxxUnlikelyIfNode(cond=conditional, body=node)

        branches.append(node)

    if did_use_break:
        branches = CxxBreakableBlockNode(branches)
    branches = SequenceNode([
        arg_count_def,
        branches,
    ])

    if not did_use_break and arg_size == 0 and conditional.is_always_true:
        return branches

    return SequenceNode([
        branches,
        EmptyNode(),
        make_check_argument_length(cg_context),
        T("${exception_state}.ThrowTypeError"
          "(\"Overload resolution failed.\");\n"
          "return;"),
    ])


def make_report_coop_access(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    if cg_context.class_like.identifier != "Window":
        return None

    ext_attrs = cg_context.member_like.extended_attributes
    if "CrossOrigin" not in ext_attrs:
        return None

    values = ext_attrs.values_of("CrossOrigin")
    if (cg_context.attribute_get and not (not values or "Getter" in values)):
        return None
    elif (cg_context.attribute_set and not ("Setter" in values)):
        return None

    return TextNode("${blink_receiver}->ReportCoopAccess(${property_name});")


def make_report_deprecate_as(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    target = cg_context.member_like or cg_context.property_
    name = target.extended_attributes.value_of("DeprecateAs")
    if not name:
        return None

    pattern = ("// [DeprecateAs]\n"
               "Deprecation::CountDeprecation("
               "${current_execution_context}, WebFeature::k{_1});")
    _1 = name
    node = TextNode(_format(pattern, _1=_1))
    node.accumulate(
        CodeGenAccumulator.require_include_headers(
            ["third_party/blink/renderer/core/frame/deprecation.h"]))
    return node


def _make_measure_web_feature_constant(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    target = cg_context.member_like or cg_context.property_
    ext_attrs = target.extended_attributes

    suffix = ""
    if cg_context.attribute_get:
        suffix = "_AttributeGetter"
    elif cg_context.attribute_set:
        suffix = "_AttributeSetter"
    elif cg_context.constructor:
        suffix = "_Constructor"
    elif cg_context.exposed_construct:
        suffix = "_ConstructorGetter"
    elif cg_context.operation:
        suffix = "_Method"

    name = ext_attrs.value_of("MeasureAs") or ext_attrs.value_of("Measure")
    if name:
        name = "k{}".format(name)
    elif cg_context.constructor:
        name = "kV8{}{}".format(cg_context.class_like.identifier, suffix)
    else:
        name = "kV8{}_{}{}".format(
            cg_context.class_like.identifier,
            name_style.raw.upper_camel_case(cg_context.property_.identifier),
            suffix)

    return "WebFeature::{}".format(name)


def make_report_high_entropy(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    target = cg_context.member_like or cg_context.property_
    ext_attrs = target.extended_attributes
    if cg_context.attribute_set or "HighEntropy" not in ext_attrs:
        return None
    assert "Measure" in ext_attrs or "MeasureAs" in ext_attrs, "{}: {}".format(
        cg_context.idl_location_and_name,
        "[HighEntropy] must be specified with either [Measure] or "
        "[MeasureAs].")

    if ext_attrs.value_of("HighEntropy") == "Direct":
        text = _format(
            "// [HighEntropy=Direct]\n"
            "Dactyloscoper::RecordDirectSurface("
            "${current_execution_context}, {measure_constant}, "
            "${return_value});",
            measure_constant=_make_measure_web_feature_constant(cg_context))
    else:
        text = _format(
            "// [HighEntropy]\n"
            "Dactyloscoper::Record("
            "${current_execution_context}, {measure_constant});",
            measure_constant=_make_measure_web_feature_constant(cg_context))
    node = TextNode(text)
    node.accumulate(
        CodeGenAccumulator.require_include_headers(
            ["third_party/blink/renderer/core/frame/dactyloscoper.h"]))
    return node


def make_report_measure_as(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    target = cg_context.member_like or cg_context.property_
    ext_attrs = target.extended_attributes
    if not ("Measure" in ext_attrs or "MeasureAs" in ext_attrs):
        return None

    text = _format(
        "// [Measure], [MeasureAs]\n"
        "UseCounter::Count(${current_execution_context}, {measure_constant});",
        measure_constant=_make_measure_web_feature_constant(cg_context))
    node = TextNode(text)
    node.accumulate(
        CodeGenAccumulator.require_include_headers([
            "third_party/blink/renderer/core/frame/web_feature.h",
            "third_party/blink/renderer/platform/instrumentation/use_counter.h",
        ]))
    return node


def make_return_value_cache_return_early(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    pred = cg_context.member_like.extended_attributes.value_of(
        "CachedAttribute")
    if pred:
        return TextNode("""\
// [CachedAttribute]
static const V8PrivateProperty::SymbolKey kPrivatePropertyCachedAttribute;
auto&& v8_private_cached_attribute =
    V8PrivateProperty::GetSymbol(${isolate}, kPrivatePropertyCachedAttribute);
if (!${blink_receiver}->""" + pred + """()) {
  v8::Local<v8::Value> v8_value;
  if (!v8_private_cached_attribute.GetOrUndefined(${v8_receiver})
           .ToLocal(&v8_value)) {
    return;
  }
  if (!v8_value->IsUndefined()) {
    bindings::V8SetReturnValue(${info}, v8_value);
    return;
  }
}""")

    if "SaveSameObject" in cg_context.member_like.extended_attributes:
        return TextNode("""\
// [SaveSameObject]
static const V8PrivateProperty::SymbolKey kPrivatePropertySaveSameObject;
auto&& v8_private_save_same_object =
    V8PrivateProperty::GetSymbol(${isolate}, kPrivatePropertySaveSameObject);
{
  v8::Local<v8::Value> v8_value;
  if (!v8_private_save_same_object.GetOrUndefined(${v8_receiver})
           .ToLocal(&v8_value)) {
    return;
  }
  if (!v8_value->IsUndefined()) {
    bindings::V8SetReturnValue(${info}, v8_value);
    return;
  }
}""")


def make_return_value_cache_update_value(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    if "CachedAttribute" in cg_context.member_like.extended_attributes:
        return TextNode("// [CachedAttribute]\n"
                        "v8_private_cached_attribute.Set"
                        "(${v8_receiver}, ${info}.GetReturnValue().Get());")

    if "SaveSameObject" in cg_context.member_like.extended_attributes:
        return TextNode("// [SaveSameObject]\n"
                        "v8_private_save_same_object.Set"
                        "(${v8_receiver}, ${info}.GetReturnValue().Get());")


def make_runtime_call_timer_scope(cg_context, overriding_name=None):
    assert isinstance(cg_context, CodeGenContext)
    assert _is_none_or_str(overriding_name)

    target = cg_context.member_like or cg_context.property_

    suffix = ""
    if cg_context.attribute_get:
        suffix = "_Getter"
    elif cg_context.attribute_set:
        suffix = "_Setter"
    elif cg_context.exposed_construct:
        suffix = "_ConstructorGetterCallback"

    counter = (target and
               target.extended_attributes.value_of("RuntimeCallStatsCounter"))
    if counter:
        macro_name = "RUNTIME_CALL_TIMER_SCOPE"
        counter_name = "RuntimeCallStats::CounterId::k{}{}".format(
            counter, suffix)
    else:
        macro_name = "RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT"
        counter_name = "\"Blink_{}_{}{}\"".format(
            blink_class_name(cg_context.class_like), overriding_name
            or target.identifier, suffix)

    return TextNode(
        _format(
            "{macro_name}(${info}.GetIsolate(), {counter_name});",
            macro_name=macro_name,
            counter_name=counter_name))


def make_steps_of_ce_reactions(cg_context):
    assert isinstance(cg_context, CodeGenContext)
    assert (cg_context.attribute_set or cg_context.operation
            or cg_context.indexed_property_setter
            or cg_context.named_property_setter
            or cg_context.named_property_deleter)

    T = TextNode

    nodes = []

    ext_attrs = cg_context.member_like.extended_attributes
    if "CustomElementCallbacks" in ext_attrs or "Reflect" in ext_attrs:
        if "CustomElementCallbacks" in ext_attrs:
            nodes.append(T("// [CustomElementCallbacks]"))
        elif "Reflect" in ext_attrs:
            nodes.append(T("// [Reflect]"))
        nodes.append(
            T("V0CustomElementProcessingStack::CallbackDeliveryScope "
              "v0_custom_element_scope;"))
        nodes[-1].accumulate(
            CodeGenAccumulator.require_include_headers([
                "third_party/blink/renderer/core/html/custom/v0_custom_element_processing_stack.h"
            ]))

    if "CEReactions" in ext_attrs:
        nodes.append(T("// [CEReactions]"))
        nodes.append(T("CEReactionsScope ce_reactions_scope;"))
        nodes[-1].accumulate(
            CodeGenAccumulator.require_include_headers([
                "third_party/blink/renderer/core/html/custom/ce_reactions_scope.h"
            ]))

    if not nodes:
        return None

    # CEReactions scope is not tolerant of V8 exception, so it's necessary to
    # invoke custom element reactions before throwing an exception.  Thus, put
    # an ExceptionState before CEReactions scope.
    nodes.insert(0, WeakDependencyNode(dep_syms=["exception_state"]))

    return SequenceNode(nodes)


def make_steps_of_put_forwards(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    T = TextNode

    return SequenceNode([
        T("// [PutForwards]"),
        T("v8::Local<v8::Value> target;"),
        T("if (!${v8_receiver}->Get(${current_context}, "
          "V8AtomicString(${isolate}, ${property_name}))"
          ".ToLocal(&target)) {\n"
          "  return;\n"
          "}"),
        CxxUnlikelyIfNode(
            cond="!target->IsObject()",
            body=[
                T("${exception_state}.ThrowTypeError("
                  "\"The attribute value is not an object\");"),
                T("return;"),
            ]),
        T("bool did_set;"),
        T("if (!target.As<v8::Object>()->Set(${current_context}, "
          "V8AtomicString(${isolate}, "
          "\"${attribute.extended_attributes.value_of(\"PutForwards\")}\""
          "), ${v8_property_value}).To(&did_set)) {{\n"
          "  return;\n"
          "}}"),
    ])


def make_steps_of_replaceable(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    T = TextNode

    return SequenceNode([
        T("// [Replaceable]"),
        T("bool did_create;"),
        T("if (!${v8_receiver}->CreateDataProperty(${current_context}, "
          "V8AtomicString(${isolate}, ${property_name}), "
          "${v8_property_value}).To(&did_create)) {\n"
          "  return;\n"
          "}"),
    ])


def make_v8_set_return_value(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    T = TextNode
    F = lambda *args, **kwargs: T(_format(*args, **kwargs))

    if cg_context.does_override_idl_return_type:
        return T("bindings::V8SetReturnValue(${info}, ${return_value});")

    if not cg_context.return_type or cg_context.return_type.unwrap().is_void:
        # Request a SymbolNode |return_value| to define itself without
        # rendering any text.
        return T("<% return_value.request_symbol_definition() %>")

    operation = cg_context.operation
    if operation and (operation.is_setter or operation.is_deleter):
        # Blink implementation returns in a type different from the IDL type.
        # Namely, IndexedPropertySetterResult, NamedPropertySetterResult, and
        # NamedPropertyDeleterResult are returned ignoring the operation's
        # return type.
        return T("bindings::V8SetReturnValue(${info}, ${return_value});")

    return_type = cg_context.return_type
    if return_type.is_typedef:
        if return_type.identifier in ("EventHandler",
                                      "OnBeforeUnloadEventHandler",
                                      "OnErrorEventHandler"):
            return T("bindings::V8SetReturnValue(${info}, ${return_value}, "
                     "${isolate}, ${blink_receiver});")

    # [CheckSecurity=ReturnValue]
    #
    # The returned object must be wrapped in its own realm instead of the
    # receiver object's relevant realm or the current realm.
    #
    # [CheckSecurity=ReturnValue] is used only for 'contentDocument' attribute
    # and 'getSVGDocument' operation of HTML{IFrame,Frame,Object,Embed}Element
    # interfaces, and Window.frameElement attribute, so far.
    #
    # All the interfaces above except for Window support 'contentWindow'
    # attribute and that's the global object of the creation context of the
    # returned V8 wrapper.  Window.frameElement is implemented with [Custom]
    # for now and there is no need to support it.
    #
    # Note that the global object has its own context and there is no need to
    # pass the creation context to ToV8.
    if (cg_context.member_like.extended_attributes.value_of("CheckSecurity") ==
            "ReturnValue"):
        return T("""\
// [CheckSecurity=ReturnValue]
bindings::V8SetReturnValue(
    ${info},
    ToV8(${return_value},
         ToV8(${blink_receiver}->contentWindow(),
              v8::Local<v8::Object>(),
              ${isolate}).As<v8::Object>(),
         ${isolate}));\
""")

    return_type = return_type.unwrap(typedef=True)
    return_type_body = return_type.unwrap()

    PRIMITIVE_TYPE_TO_CXX_TYPE = {
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
    cxx_type = PRIMITIVE_TYPE_TO_CXX_TYPE.get(
        return_type_body.keyword_typename)
    if cxx_type:
        return F(
            "bindings::V8SetReturnValue(${info}, ${return_value}, "
            "bindings::V8ReturnValue::PrimitiveType<{cxx_type}>());",
            cxx_type=cxx_type)

    # TODO(yukishiino): Remove |return_type_body.is_enumeration| below once
    # the migration from String to V8Enum type is done.
    if return_type_body.is_string or return_type_body.is_enumeration:
        args = ["${info}", "${return_value}", "${isolate}"]
        if return_type.is_nullable:
            args.append("bindings::V8ReturnValue::kNullable")
        else:
            args.append("bindings::V8ReturnValue::kNonNullable")
        return T("bindings::V8SetReturnValue({});".format(", ".join(args)))

    if return_type_body.is_interface:
        args = ["${info}", "${return_value}"]
        if cg_context.for_world == cg_context.MAIN_WORLD:
            args.append("bindings::V8ReturnValue::kMainWorld")
        elif cg_context.constructor or cg_context.member_like.is_static:
            args.append("${creation_context}")
        else:
            args.append("${blink_receiver}")
        return T("bindings::V8SetReturnValue({});".format(", ".join(args)))

    if return_type.is_frozen_array:
        return T(
            "bindings::V8SetReturnValue("
            "${info}, "
            "ToV8(${return_value}, ${creation_context_object}, ${isolate}), "
            "bindings::V8ReturnValue::kFrozen);")

    if return_type.is_promise:
        return T("bindings::V8SetReturnValue"
                 "(${info}, ${return_value}.V8Value());")

    return T("bindings::V8SetReturnValue(${info}, "
             "ToV8(${return_value}, ${creation_context_object}, ${isolate}));")


def _make_empty_callback_def(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    if cg_context.v8_callback_type == CodeGenContext.V8_FUNCTION_CALLBACK:
        arg_decls = ["const v8::FunctionCallbackInfo<v8::Value>& info"]
        arg_names = ["info"]
    elif (cg_context.v8_callback_type == CodeGenContext.
          V8_ACCESSOR_NAME_GETTER_CALLBACK):
        arg_decls = [
            "v8::Local<v8::Name> v8_property_name",
            "const v8::PropertyCallbackInfo<v8::Value>& info",
        ]
        arg_names = ["v8_property_name", "info"]
    elif (cg_context.v8_callback_type == CodeGenContext.
          V8_ACCESSOR_NAME_SETTER_CALLBACK):
        arg_decls = [
            "v8::Local<v8::Name> v8_property_name",
            "v8::Local<v8::Value> v8_property_value",
            "const v8::PropertyCallbackInfo<void>& info",
        ]
        arg_names = ["v8_property_name", "v8_property_value", "info"]
    elif (cg_context.v8_callback_type == CodeGenContext.
          V8_GENERIC_NAMED_PROPERTY_SETTER_CALLBACK):
        arg_decls = [
            "v8::Local<v8::Name> v8_property_name",
            "v8::Local<v8::Value> v8_property_value",
            "const v8::PropertyCallbackInfo<v8::Value>& info",
        ]
        arg_names = ["v8_property_name", "v8_property_value", "info"]

    func_def = CxxFuncDefNode(
        name=function_name, arg_decls=arg_decls, return_type="void")
    func_def.set_base_template_vars(cg_context.template_bindings())
    body = func_def.body

    for arg_name in arg_names:
        body.add_template_var(arg_name, arg_name)

    bind_callback_local_vars(body, cg_context)
    if cg_context.attribute or cg_context.function_like:
        bind_blink_api_arguments(body, cg_context)
        bind_return_value(body, cg_context)

    return func_def


def make_attribute_get_callback_def(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    func_def = _make_empty_callback_def(cg_context, function_name)
    body = func_def.body

    body.extend([
        make_check_receiver(cg_context),
        EmptyNode(),
        make_runtime_call_timer_scope(cg_context),
        make_bindings_trace_event(cg_context),
        make_report_coop_access(cg_context),
        make_report_deprecate_as(cg_context),
        make_report_measure_as(cg_context),
        make_log_activity(cg_context),
        EmptyNode(),
    ])

    if "Getter" in cg_context.property_.extended_attributes.values_of(
            "Custom"):
        text = _format("${class_name}::{}(${info});",
                       custom_function_name(cg_context))
        body.append(TextNode(text))
        return func_def

    body.extend([
        make_return_value_cache_return_early(cg_context),
        EmptyNode(),
        make_check_security_of_return_value(cg_context),
        make_v8_set_return_value(cg_context),
        make_report_high_entropy(cg_context),
        make_return_value_cache_update_value(cg_context),
    ])

    return func_def


def make_attribute_set_callback_def(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    ext_attrs = cg_context.attribute.extended_attributes
    if cg_context.attribute.is_readonly and not any(
            ext_attr in ext_attrs
            for ext_attr in ("LegacyLenientSetter", "PutForwards",
                             "Replaceable")):
        return None

    func_def = _make_empty_callback_def(cg_context, function_name)
    body = func_def.body

    if "LegacyLenientSetter" in ext_attrs:
        body.append(TextNode("// [LegacyLenientSetter]"))
        return func_def

    body.extend([
        make_check_receiver(cg_context),
        EmptyNode(),
        make_runtime_call_timer_scope(cg_context),
        make_bindings_trace_event(cg_context),
        make_report_deprecate_as(cg_context),
        make_report_measure_as(cg_context),
        make_log_activity(cg_context),
        EmptyNode(),
    ])

    if "Setter" in cg_context.property_.extended_attributes.values_of(
            "Custom"):
        text = _format("${class_name}::{}(${v8_property_value}, ${info});",
                       custom_function_name(cg_context))
        body.append(TextNode(text))
        return func_def

    # Binary size reduction hack
    # 1. Drop the check of argument length although this is a violation of
    #   Web IDL.
    # 2. Leverage the nature of [LegacyTreatNonObjectAsNull] (ES to IDL
    #   conversion never fails).
    if (cg_context.attribute.idl_type.is_typedef
            and (cg_context.attribute.idl_type.identifier in (
                "EventHandler", "OnBeforeUnloadEventHandler",
                "OnErrorEventHandler"))):
        body.extend([
            TextNode("""\
EventListener* event_handler = JSEventHandler::CreateOrNull(
    ${v8_property_value},
    JSEventHandler::HandlerType::k${attribute.idl_type.identifier});\
"""),
        ])
        code_generator_info = cg_context.attribute.code_generator_info
        func_name = name_style.api_func("set", cg_context.attribute.identifier)
        if code_generator_info.defined_in_partial:
            class_name = (code_generator_info.receiver_implemented_as
                          or name_style.class_(
                              cg_context.attribute.owner_mixin.identifier))
            text = _format(
                "{class_name}::{func_name}"
                "(*${blink_receiver}, event_handler);",
                class_name=class_name,
                func_name=func_name)
        else:
            text = _format("${blink_receiver}->{func_name}(event_handler);",
                           func_name=func_name)
        body.append(TextNode(text))
        return func_def

    # Binary size reduction hack
    # When the following conditions are met, the implementation is shared.
    # 1. The attribute is annotated with [CEReactions, Reflect] and not
    #   annotated with other extended attributes having side effect.
    # 2. The interface is implementing Element.
    def optimize_element_cereactions_reflect():
        has_cereactions = False
        has_reflect = False
        for key in ext_attrs.keys():
            if key == "CEReactions":
                has_cereactions = True
            elif key == "Reflect":
                has_reflect = True
            elif key in ("Affects", "CustomElementCallbacks", "DeprecateAs",
                         "Exposed", "LogActivity", "LogAllWorlds", "Measure",
                         "MeasureAs", "ReflectEmpty", "ReflectInvalid",
                         "ReflectMissing", "ReflectOnly",
                         "RuntimeCallStatsCounter", "RuntimeEnabled",
                         "SecureContext", "URL", "Unscopable"):
                pass
            else:
                return None
        if not (has_cereactions and has_reflect):
            return None
        if not cg_context.interface.does_implement("Element"):
            return None
        content_attribute = _make_reflect_content_attribute_key(
            body, cg_context)
        idl_type = cg_context.attribute.idl_type.unwrap(typedef=True)
        if idl_type.is_boolean:
            func_name = "PerformAttributeSetCEReactionsReflectTypeBoolean"
        elif idl_type.type_name == "String":
            func_name = "PerformAttributeSetCEReactionsReflectTypeString"
        elif idl_type.type_name == "StringTreatNullAs":
            func_name = ("PerformAttributeSetCEReactionsReflect"
                         "TypeStringLegacyNullToEmptyString")
        elif idl_type.type_name == "StringOrNull":
            func_name = "PerformAttributeSetCEReactionsReflectTypeStringOrNull"
        else:
            return None
        text = _format(
            "bindings::{func_name}"
            "(${info}, {content_attribute}, "
            "${class_like_name}, ${property_name});",
            func_name=func_name,
            content_attribute=content_attribute)
        return TextNode(text)

    node = optimize_element_cereactions_reflect()
    if node:
        body.append(node)
        return func_def

    body.extend([
        make_check_argument_length(cg_context),
        EmptyNode(),
    ])

    if "PutForwards" in ext_attrs:
        body.append(make_steps_of_put_forwards(cg_context))
        return func_def

    if "Replaceable" in ext_attrs:
        body.append(make_steps_of_replaceable(cg_context))
        return func_def

    body.extend([
        make_steps_of_ce_reactions(cg_context),
        EmptyNode(),
        make_v8_set_return_value(cg_context),
    ])

    return func_def


def make_constant_callback_def(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    logging_nodes = SequenceNode([
        make_report_deprecate_as(cg_context),
        make_report_measure_as(cg_context),
        make_log_activity(cg_context),
    ])
    if not logging_nodes:
        return None

    func_def = _make_empty_callback_def(cg_context, function_name)
    body = func_def.body

    v8_set_return_value = _format(
        "bindings::V8SetReturnValue(${info}, ${class_name}::Constant::{});",
        constant_name(cg_context))
    body.extend([
        make_runtime_call_timer_scope(cg_context),
        make_bindings_trace_event(cg_context),
        logging_nodes,
        EmptyNode(),
        TextNode(v8_set_return_value),
        make_report_high_entropy(cg_context),
    ])

    return func_def


def make_constant_constant_def(cg_context, constant_name):
    # IDL constant's C++ constant definition
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(constant_name, str)

    constant_type = blink_type_info(cg_context.constant.idl_type).value_t
    return TextNode("static constexpr {type} {name} = {value};".format(
        type=constant_type,
        name=constant_name,
        value=cg_context.constant.value.literal))


def make_overload_dispatcher_function_def(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    func_def = _make_empty_callback_def(cg_context, function_name)
    body = func_def.body

    if cg_context.operation_group:
        body.append(make_operation_entry(cg_context))
        body.append(EmptyNode())
        body.append(make_cooperative_scheduling_safepoint(cg_context))
        body.append(EmptyNode())

    if cg_context.constructor_group:
        body.append(make_constructor_entry(cg_context))
        body.append(EmptyNode())

    body.append(make_overload_dispatcher(cg_context))

    return func_def


def make_constructor_entry(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    return SequenceNode([
        make_runtime_call_timer_scope(cg_context),
        make_bindings_trace_event(cg_context),
        EmptyNode(),
        make_check_constructor_call(cg_context),
    ])


def make_constructor_function_def(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    T = TextNode

    func_def = _make_empty_callback_def(cg_context, function_name)
    body = func_def.body

    if len(cg_context.constructor_group) == 1:
        body.append(make_constructor_entry(cg_context))
        body.append(EmptyNode())

    body.extend([
        make_report_deprecate_as(cg_context),
        make_report_measure_as(cg_context),
        make_log_activity(cg_context),
        EmptyNode(),
        make_check_argument_length(cg_context),
        EmptyNode(),
    ])

    if "HTMLConstructor" in cg_context.constructor.extended_attributes:
        body.append(T("// [HTMLConstructor]"))
        text = _format(
            "V8HTMLConstructor::HtmlConstructor("
            "${info}, *${class_name}::GetWrapperTypeInfo(), "
            "HTMLElementType::{});",
            name_style.constant(cg_context.class_like.identifier))
        body.append(T(text))
        body.accumulate(
            CodeGenAccumulator.require_include_headers([
                "third_party/blink/renderer/bindings/core/v8/v8_html_constructor.h"
            ]))
    else:
        body.append(
            T("v8::Local<v8::Object> v8_wrapper = "
              "${return_value}->AssociateWithWrapper(${isolate}, "
              "${class_name}::GetWrapperTypeInfo(), ${v8_receiver});"))
        body.append(T("bindings::V8SetReturnValue(${info}, v8_wrapper);"))

    return func_def


def make_constructor_callback_def(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    constructor_group = cg_context.constructor_group

    if len(constructor_group) == 1:
        return make_constructor_function_def(
            cg_context.make_copy(constructor=constructor_group[0]),
            function_name)

    node = SequenceNode()
    for constructor in constructor_group:
        cgc = cg_context.make_copy(constructor=constructor)
        node.extend([
            make_constructor_function_def(
                cgc, callback_function_name(cgc, constructor.overload_index)),
            EmptyNode(),
        ])
    node.append(
        make_overload_dispatcher_function_def(cg_context, function_name))
    return node


def make_exposed_construct_callback_def(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    func_def = _make_empty_callback_def(cg_context, function_name)
    body = func_def.body

    v8_set_return_value = _format(
        "bindings::V8SetReturnValue"
        "(${info}, {}::GetWrapperTypeInfo(), "
        "bindings::V8ReturnValue::kInterfaceObject);",
        v8_bridge_class_name(cg_context.exposed_construct))
    body.extend([
        make_runtime_call_timer_scope(cg_context),
        make_bindings_trace_event(cg_context),
        make_report_deprecate_as(cg_context),
        make_report_measure_as(cg_context),
        make_log_activity(cg_context),
        EmptyNode(),
        TextNode(v8_set_return_value),
    ])

    return func_def


def make_named_constructor_property_callback_def(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    func_def = _make_empty_callback_def(cg_context, function_name)
    body = func_def.body

    body.extend([
        make_runtime_call_timer_scope(cg_context),
        make_bindings_trace_event(cg_context),
        make_report_deprecate_as(cg_context),
        make_report_measure_as(cg_context),
        make_log_activity(cg_context),
        EmptyNode(),
    ])

    constructor_group = cg_context.exposed_construct
    assert isinstance(constructor_group, web_idl.ConstructorGroup)
    assert isinstance(constructor_group.owner, web_idl.Interface)
    named_ctor_v8_bridge = v8_bridge_class_name(constructor_group.owner)
    cgc = CodeGenContext(
        interface=constructor_group.owner,
        constructor_group=constructor_group,
        is_named_constructor=True,
        class_name=named_ctor_v8_bridge)
    named_ctor_name = callback_function_name(cgc)
    named_ctor_def = make_constructor_callback_def(cgc, named_ctor_name)

    return_value_cache_return_early = """\
static const V8PrivateProperty::SymbolKey kPrivatePropertyNamedConstructor;
auto&& v8_private_named_constructor =
    V8PrivateProperty::GetSymbol(${isolate}, kPrivatePropertyNamedConstructor);
v8::Local<v8::Value> v8_named_constructor;
if (!v8_private_named_constructor.GetOrUndefined(${v8_receiver})
         .ToLocal(&v8_named_constructor)) {
  return;
}
if (!v8_named_constructor->IsUndefined()) {
  bindings::V8SetReturnValue(${info}, v8_named_constructor);
  return;
}
"""

    pattern = """\
v8::Local<v8::Value> v8_value;
if (!bindings::CreateNamedConstructorFunction(
         ${script_state},
         {callback},
         "{func_name}",
         {func_length},
         {v8_bridge}::GetWrapperTypeInfo())
     .ToLocal(&v8_value)) {
  return;
}
bindings::V8SetReturnValue(${info}, v8_value);
"""
    create_named_constructor_function = _format(
        pattern,
        callback=named_ctor_name,
        func_name=constructor_group.identifier,
        func_length=constructor_group.min_num_of_required_arguments,
        v8_bridge=named_ctor_v8_bridge)

    return_value_cache_update_value = """\
v8_private_named_constructor.Set(${v8_receiver}, v8_value);
"""

    body.extend([
        TextNode(return_value_cache_return_early),
        TextNode(create_named_constructor_function),
        TextNode(return_value_cache_update_value),
    ])

    return SequenceNode([named_ctor_def, EmptyNode(), func_def])


def make_no_alloc_direct_call_callback_def(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    assert cg_context.operation_group and len(cg_context.operation_group) == 1

    func_like = cg_context.operation_group[0]

    return_type = ("void" if func_like.return_type.is_void else
                   blink_type_info(func_like.return_type).value_t)
    arg_type_and_names = [(blink_type_info(arg.idl_type).value_t,
                           name_style.arg_f("arg{}_{}", index + 1,
                                            arg.identifier))
                          for index, arg in enumerate(func_like.arguments)]
    arg_decls = ["v8::ApiObject arg0_receiver"] + [
        "{} {}".format(arg_type, arg_name)
        for arg_type, arg_name in arg_type_and_names
    ]
    func_def = CxxFuncDefNode(name=function_name,
                              arg_decls=arg_decls,
                              return_type=return_type)
    body = func_def.body

    pattern = """\
ThreadState::NoAllocationScope no_alloc_scope(ThreadState::Current());
v8::Object* v8_receiver = reinterpret_cast<v8::Object*>(&arg0_receiver);
v8::Isolate* isolate = v8_receiver->GetIsolate();
v8::Isolate::DisallowJavascriptExecutionScope no_js_exec_scope(
    isolate,
    v8::Isolate::DisallowJavascriptExecutionScope::CRASH_ON_FAILURE);
{blink_class}* blink_receiver =
    ToScriptWrappable(v8_receiver)->ToImpl<{blink_class}>();
return blink_receiver->{member_func}({blink_arguments});\
"""
    blink_class = blink_class_name(cg_context.interface)
    member_func = backward_compatible_api_func(cg_context)
    blink_arguments = ", ".join(
        [arg_name for arg_type, arg_name in arg_type_and_names])
    body.append(
        TextNode(
            _format(pattern,
                    blink_class=blink_class,
                    member_func=member_func,
                    blink_arguments=blink_arguments)))

    return func_def


def make_operation_entry(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    return SequenceNode([
        make_runtime_call_timer_scope(cg_context),
        make_bindings_trace_event(cg_context),
    ])


def make_operation_function_def(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    func_def = _make_empty_callback_def(cg_context, function_name)
    body = func_def.body

    if not cg_context.operation_group or len(cg_context.operation_group) == 1:
        body.append(make_operation_entry(cg_context))
        body.append(EmptyNode())

    body.extend([
        make_check_receiver(cg_context),
        EmptyNode(),
        make_report_coop_access(cg_context),
        make_report_deprecate_as(cg_context),
        make_report_measure_as(cg_context),
        make_log_activity(cg_context),
        EmptyNode(),
    ])

    if "Custom" in cg_context.property_.extended_attributes:
        text = _format("${class_name}::{}(${info});",
                       custom_function_name(cg_context))
        body.append(TextNode(text))
        return func_def

    body.extend([
        make_check_argument_length(cg_context),
        EmptyNode(),
        make_steps_of_ce_reactions(cg_context),
        EmptyNode(),
        make_check_security_of_return_value(cg_context),
        make_v8_set_return_value(cg_context),
        make_report_high_entropy(cg_context),
    ])

    return func_def


def make_operation_callback_def(cg_context,
                                function_name,
                                no_alloc_direct_callback_name=None):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    operation_group = cg_context.operation_group

    assert (not ("Custom" in operation_group.extended_attributes)
            or len(operation_group) == 1)
    assert (not ("NoAllocDirectCall" in operation_group.extended_attributes)
            or len(operation_group) == 1)

    if "NoAllocDirectCall" in operation_group.extended_attributes:
        return ListNode([
            make_operation_function_def(
                cg_context.make_copy(operation=operation_group[0]),
                function_name),
            EmptyNode(),
            make_no_alloc_direct_call_callback_def(
                cg_context.make_copy(operation=operation_group[0]),
                no_alloc_direct_callback_name),
        ])

    if len(operation_group) == 1:
        return make_operation_function_def(
            cg_context.make_copy(operation=operation_group[0]), function_name)

    node = SequenceNode()
    for operation in operation_group:
        cgc = cg_context.make_copy(operation=operation)
        node.extend([
            make_operation_function_def(
                cgc, callback_function_name(cgc, operation.overload_index)),
            EmptyNode(),
        ])
    node.append(
        make_overload_dispatcher_function_def(cg_context, function_name))
    return node


def make_stringifier_callback_def(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    if cg_context.stringifier.attribute:
        return make_attribute_get_callback_def(
            cg_context.make_copy(
                attribute=cg_context.stringifier.attribute,
                attribute_get=True), function_name)
    elif cg_context.stringifier.operation:
        return make_operation_function_def(
            cg_context.make_copy(operation=cg_context.stringifier.operation),
            function_name)
    assert False


# ----------------------------------------------------------------------------
# Callback functions of indexed and named interceptors
# ----------------------------------------------------------------------------


def _make_interceptor_callback(cg_context, function_name, arg_decls, arg_names,
                               class_name, runtime_call_timer_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)
    assert isinstance(arg_decls, (list, tuple))
    assert all(isinstance(arg_decl, str) for arg_decl in arg_decls)
    assert isinstance(arg_names, (list, tuple))
    assert all(isinstance(arg_name, str) for arg_name in arg_names)
    assert _is_none_or_str(class_name)
    assert isinstance(runtime_call_timer_name, str)

    func_decl = CxxFuncDeclNode(
        name=function_name,
        arg_decls=arg_decls,
        return_type="void",
        static=True)

    func_def = _make_interceptor_callback_def(cg_context, function_name,
                                              arg_decls, arg_names, class_name,
                                              runtime_call_timer_name)

    return func_decl, func_def


def _make_interceptor_callback_def(cg_context, function_name, arg_decls,
                                   arg_names, class_name,
                                   runtime_call_timer_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)
    assert isinstance(arg_decls, (list, tuple))
    assert all(isinstance(arg_decl, str) for arg_decl in arg_decls)
    assert isinstance(arg_names, (list, tuple))
    assert all(isinstance(arg_name, str) for arg_name in arg_names)
    assert _is_none_or_str(class_name)
    assert isinstance(runtime_call_timer_name, str)

    func_def = CxxFuncDefNode(
        name=function_name,
        arg_decls=arg_decls,
        return_type="void",
        class_name=class_name)
    func_def.set_base_template_vars(cg_context.template_bindings())
    body = func_def.body
    for arg_name in arg_names:
        body.add_template_var(arg_name, arg_name)
    bind_callback_local_vars(body, cg_context)

    body.extend([
        make_runtime_call_timer_scope(cg_context, runtime_call_timer_name),
        EmptyNode(),
    ])

    return func_def


def make_indexed_property_getter_callback(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    arg_decls = [
        "uint32_t index",
        "const v8::PropertyCallbackInfo<v8::Value>& info",
    ]
    arg_names = ["index", "info"]

    func_decl, func_def = _make_interceptor_callback(
        cg_context, function_name, arg_decls, arg_names, cg_context.class_name,
        "IndexedPropertyGetter")
    body = func_def.body

    if not cg_context.interface.indexed_and_named_properties.indexed_getter:
        body.append(
            TextNode("""\
v8::Local<v8::String> property_name =
    V8AtomicString(${isolate}, AtomicString::Number(${index}));
${class_name}::NamedPropertyGetterCallback(property_name, ${info});
"""))
        return func_decl, func_def

    bind_return_value(body, cg_context, overriding_args=["${index}"])

    body.extend([
        TextNode("""\
// LegacyPlatformObjectGetOwnProperty
// https://heycam.github.io/webidl/#LegacyPlatformObjectGetOwnProperty
// step 1.2. If index is a supported property index, then:
// step 3. Return OrdinaryGetOwnProperty(O, P).
if (${index} >= ${blink_receiver}->length())
  return;  // Do not intercept.  Fallback to OrdinaryGetOwnProperty.
"""),
        make_v8_set_return_value(cg_context),
    ])

    return func_decl, func_def


def make_indexed_property_setter_callback(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    arg_decls = [
        "uint32_t index",
        "v8::Local<v8::Value> v8_property_value",
        "const v8::PropertyCallbackInfo<v8::Value>& info",
    ]
    arg_names = ["index", "v8_property_value", "info"]

    func_decl, func_def = _make_interceptor_callback(
        cg_context, function_name, arg_decls, arg_names, cg_context.class_name,
        "IndexedPropertySetter")
    body = func_def.body

    if not cg_context.interface.indexed_and_named_properties.indexed_getter:
        body.append(
            TextNode("""\
v8::Local<v8::String> property_name =
    V8AtomicString(${isolate}, AtomicString::Number(${index}));
${class_name}::NamedPropertySetterCallback(
    property_name, ${v8_property_value}, ${info});
"""))
        return func_decl, func_def

    if not cg_context.indexed_property_setter:
        body.append(
            TextNode("""\
// 3.8.2. [[Set]]
// https://heycam.github.io/webidl/#legacy-platform-object-set
// step 1. If O and Receiver are the same object, then:
if (${info}.Holder() == ${info}.This()) {
  // OrdinarySetWithOwnDescriptor will end up calling DefineOwnProperty,
  // which will fail when the receiver object is this legacy platform
  // object.
  bindings::V8SetReturnValue(${info}, nullptr);
  if (${info}.ShouldThrowOnError()) {
    ExceptionState exception_state(${info}.GetIsolate(),
                                   ExceptionState::kIndexedSetterContext,
                                   "${interface.identifier}");
    exception_state.ThrowTypeError(
        "Indexed property setter is not supported.");
  }
  return;
}

// step 2. Let ownDesc be LegacyPlatformObjectGetOwnProperty(O, P, true).
// step 3. Perform ? OrdinarySetWithOwnDescriptor(O, P, V, Receiver,
//   ownDesc).
//
// Do not intercept.  Fallback to OrdinarySetWithOwnDescriptor.
"""))
        return func_decl, func_def

    bind_return_value(
        body,
        cg_context,
        overriding_args=["${index}", "${blink_property_value}"])
    body.register_code_symbol(
        make_v8_to_blink_value(
            "blink_property_value",
            "${v8_property_value}",
            cg_context.indexed_property_setter.arguments[1].idl_type,
            argument_index=2))

    body.extend([
        TextNode("""\
// 3.8.2. [[Set]]
// https://heycam.github.io/webidl/#legacy-platform-object-set
// step 1. If O and Receiver are the same object, then:\
"""),
        CxxLikelyIfNode(
            cond="${info}.Holder() == ${info}.This()",
            body=[
                TextNode("""\
// step 1.1.1. Invoke the indexed property setter with P and V.\
"""),
                make_steps_of_ce_reactions(cg_context),
                EmptyNode(),
                make_v8_set_return_value(cg_context),
                TextNode("""\
bindings::V8SetReturnValue(${info}, nullptr);
return;"""),
            ]),
        EmptyNode(),
        TextNode("""\
// Do not intercept.  Fallback to OrdinarySetWithOwnDescriptor.
"""),
    ])

    return func_decl, func_def


def make_indexed_property_deleter_callback(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    arg_decls = [
        "uint32_t index",
        "const v8::PropertyCallbackInfo<v8::Boolean>& info",
    ]
    arg_names = ["index", "info"]

    func_decl, func_def = _make_interceptor_callback(
        cg_context, function_name, arg_decls, arg_names, cg_context.class_name,
        "IndexedPropertyDeleter")
    body = func_def.body

    if not cg_context.interface.indexed_and_named_properties.indexed_getter:
        body.append(
            TextNode("""\
v8::Local<v8::String> property_name =
    V8AtomicString(${isolate}, AtomicString::Number(${index}));
${class_name}::NamedPropertyDeleterCallback(property_name, ${info});
"""))
        return func_decl, func_def

    body.append(
        TextNode("""\
// 3.8.4. [[Delete]]
// https://heycam.github.io/webidl/#legacy-platform-object-delete
// step 1.2. If index is not a supported property index, then return true.
// step 1.3. Return false.
const bool is_supported = ${index} < ${blink_receiver}->length();
bindings::V8SetReturnValue(${info}, !is_supported);
if (is_supported and ${info}.ShouldThrowOnError()) {
  ExceptionState exception_state(${info}.GetIsolate(),
                                 ExceptionState::kIndexedDeletionContext,
                                 "${interface.identifier}");
  exception_state.ThrowTypeError("Index property deleter is not supported.");
}
"""))

    return func_decl, func_def


def make_indexed_property_definer_callback(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    arg_decls = [
        "uint32_t index",
        "const v8::PropertyDescriptor& v8_property_desc",
        "const v8::PropertyCallbackInfo<v8::Value>& info",
    ]
    arg_names = ["index", "v8_property_desc", "info"]

    func_decl, func_def = _make_interceptor_callback(
        cg_context, function_name, arg_decls, arg_names, cg_context.class_name,
        "IndexedPropertyDefiner")
    body = func_def.body

    if not cg_context.interface.indexed_and_named_properties.indexed_getter:
        body.append(
            TextNode("""\
v8::Local<v8::String> property_name =
    V8AtomicString(${isolate}, AtomicString::Number(${index}));
${class_name}::NamedPropertyDefinerCallback(
    property_name, ${v8_property_desc}, ${info});
"""))
        return func_decl, func_def

    body.append(
        TextNode("""\
// 3.8.3. [[DefineOwnProperty]]
// https://heycam.github.io/webidl/#legacy-platform-object-defineownproperty
// step 1.1. If the result of calling IsDataDescriptor(Desc) is false, then
//   return false.
if (v8_property_desc.has_get() || v8_property_desc.has_set()) {
  bindings::V8SetReturnValue(${info}, nullptr);
  if (${info}.ShouldThrowOnError()) {
    ExceptionState exception_state(${info}.GetIsolate(),
                                   ExceptionState::kIndexedSetterContext,
                                   "${interface.identifier}");
    exception_state.ThrowTypeError("Accessor properties are not allowed.");
  }
  return;
}
"""))

    if not cg_context.interface.indexed_and_named_properties.indexed_setter:
        body.append(
            TextNode("""\
// step 1.2. If O does not implement an interface with an indexed property
//   setter, then return false.
bindings::V8SetReturnValue(${info}, nullptr);
if (${info}.ShouldThrowOnError()) {
  ExceptionState exception_state(${info}.GetIsolate(),
                                 ExceptionState::kIndexedSetterContext,
                                 "${interface.identifier}");
  exception_state.ThrowTypeError("Index property setter is not supported.");
}
"""))
    else:
        body.append(
            TextNode("""\
// step 1.3. Invoke the indexed property setter with P and Desc.[[Value]].
${class_name}::IndexedPropertySetterCallback(
    ${index}, ${v8_property_desc}.value(), ${info});
"""))

    return func_decl, func_def


def make_indexed_property_descriptor_callback(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    arg_decls = [
        "uint32_t index",
        "const v8::PropertyCallbackInfo<v8::Value>& info",
    ]
    arg_names = ["index", "info"]

    func_decl, func_def = _make_interceptor_callback(
        cg_context, function_name, arg_decls, arg_names, cg_context.class_name,
        "IndexedPropertyDescriptor")
    body = func_def.body

    if not cg_context.interface.indexed_and_named_properties.indexed_getter:
        body.append(
            TextNode("""\
v8::Local<v8::String> property_name =
    V8AtomicString(${isolate}, AtomicString::Number(${index}));
${class_name}::NamedPropertyDescriptorCallback(property_name, ${info});
"""))
        return func_decl, func_def

    pattern = """\
// LegacyPlatformObjectGetOwnProperty
// https://heycam.github.io/webidl/#LegacyPlatformObjectGetOwnProperty
// step 1.2.3. If operation was defined without an identifier, then set
//   value to the result of performing the steps listed in the interface
//   description to determine the value of an indexed property with index
//   as the index.
// step 1.2.4. Otherwise, operation was defined with an identifier. Set
//   value to the result of performing the steps listed in the description
//   of operation with index as the only argument value.
${class_name}::IndexedPropertyGetterCallback(${index}, ${info});
v8::Local<v8::Value> v8_value = ${info}.GetReturnValue().Get();
// step 1.2. If index is a supported property index, then:
// step 3. Return OrdinaryGetOwnProperty(O, P).
if (v8_value->IsUndefined())
  return;  // Do not intercept.  Fallback to OrdinaryGetOwnProperty.

// step 1.2.6. Set desc.[[Value]] to the result of converting value to an
//   ECMAScript value.
// step 1.2.7. If O implements an interface with an indexed property setter,
//   then set desc.[[Writable]] to true, otherwise set it to false.
// step 1.2.8. Set desc.[[Enumerable]] and desc.[[Configurable]] to true.
v8::PropertyDescriptor desc(v8_value, /*writable=*/{cxx_writable});
desc.set_enumerable(true);
desc.set_configurable(true);
bindings::V8SetReturnValue(${info}, desc);"""
    writable = bool(
        cg_context.interface.indexed_and_named_properties.indexed_setter)
    cxx_writable = "true" if writable else "false"
    body.append(TextNode(_format(pattern, cxx_writable=cxx_writable)))

    return func_decl, func_def


def make_indexed_property_enumerator_callback(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    if not cg_context.interface.indexed_and_named_properties.indexed_getter:
        return None, None

    arg_decls = ["const v8::PropertyCallbackInfo<v8::Array>& info"]
    arg_names = ["info"]

    func_decl, func_def = _make_interceptor_callback(
        cg_context, function_name, arg_decls, arg_names, cg_context.class_name,
        "IndexedPropertyEnumerator")
    body = func_def.body

    body.append(
        TextNode("""\
// 3.8.6. [[OwnPropertyKeys]]
// https://heycam.github.io/webidl/#legacy-platform-object-ownpropertykeys
// step 2. If O supports indexed properties, then for each index of O's
//   supported property indices, in ascending numerical order, append
//   ! ToString(index) to keys.
uint32_t length = ${blink_receiver}->length();
v8::Local<v8::Array> array =
    bindings::EnumerateIndexedProperties(${isolate}, length);
bindings::V8SetReturnValue(${info}, array);
"""))

    return func_decl, func_def


def make_named_property_getter_callback(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    arg_decls = [
        "v8::Local<v8::Name> v8_property_name",
        "const v8::PropertyCallbackInfo<v8::Value>& info",
    ]
    arg_names = ["v8_property_name", "info"]

    func_decl, func_def = _make_interceptor_callback(
        cg_context, function_name, arg_decls, arg_names, cg_context.class_name,
        "NamedPropertyGetter")
    body = func_def.body

    bind_return_value(
        body, cg_context, overriding_args=["${blink_property_name}"])

    if "Custom" in cg_context.named_property_getter.extended_attributes:
        text = _format("${class_name}::{}(${blink_property_name}, ${info});",
                       custom_function_name(cg_context))
        body.append(TextNode(text))
        return func_decl, func_def

    # The named property getter's implementation of Blink is not designed to
    # represent the property existence, and we have to determine the property
    # existence by heuristics.
    type = cg_context.return_type.unwrap()
    if type.is_any or type.is_object:
        not_found_expr = "${return_value}.IsEmpty()"
    elif type.is_string:
        not_found_expr = "${return_value}.IsNull()"
    elif type.is_interface:
        not_found_expr = "!${return_value}"
    elif type.is_union:
        not_found_expr = "${return_value}.IsNull()"
    else:
        assert False

    body.extend([
        TextNode("""\
// LegacyPlatformObjectGetOwnProperty
// https://heycam.github.io/webidl/#LegacyPlatformObjectGetOwnProperty
// step 2.1. If the result of running the named property visibility
//   algorithm with property name P and object O is true, then:\
"""),
        CxxUnlikelyIfNode(
            cond=not_found_expr,
            body=[
                TextNode("// step 3. Return OrdinaryGetOwnProperty(O, P)."),
                TextNode("return;  // Do not intercept."),
            ]),
        TextNode("""\
// step 2.1.3. If operation was defined without an identifier, then set
//   value to the result of performing the steps listed in the interface
//   description to determine the value of a named property with P as the
//   name.
// step 2.1.4. Otherwise, operation was defined with an identifier. Set
//   value to the result of performing the steps listed in the description
//   of operation with P as the only argument value.\
"""),
        make_v8_set_return_value(cg_context),
    ])

    return func_decl, func_def


def make_named_property_setter_callback(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    arg_decls = [
        "v8::Local<v8::Name> v8_property_name",
        "v8::Local<v8::Value> v8_property_value",
        "const v8::PropertyCallbackInfo<v8::Value>& info",
    ]
    arg_names = ["v8_property_name", "v8_property_value", "info"]

    func_decl, func_def = _make_interceptor_callback(
        cg_context, function_name, arg_decls, arg_names, cg_context.class_name,
        "NamedPropertySetter")
    body = func_def.body

    if not cg_context.named_property_setter:
        body.append(
            TextNode("""\
// 3.8.2. [[Set]]
// https://heycam.github.io/webidl/#legacy-platform-object-set
// step 1. If O and Receiver are the same object, then:
if (${info}.Holder() == ${info}.This()) {
  // OrdinarySetWithOwnDescriptor will end up calling DefineOwnProperty.
  // 3.8.3. [[DefineOwnProperty]]
  // https://heycam.github.io/webidl/#legacy-platform-object-defineownproperty
  // step 2.1. Let creating be true if P is not a supported property name,
  //   and false otherwise.
  // step 2.2.1. If creating is false and O does not implement an interface
  //   with a named property setter, then return false.
  ${class_name}::NamedPropertyGetterCallback(${v8_property_name}, ${info});
  const bool is_creating = ${info}.GetReturnValue().Get()->IsUndefined();
  if (!is_creating) {
    bindings::V8SetReturnValue(${info}, nullptr);
    if (${info}.ShouldThrowOnError()) {
      ExceptionState exception_state(${info}.GetIsolate(),
                                     ExceptionState::kNamedSetterContext,
                                     "${interface.identifier}");
      exception_state.ThrowTypeError(
          "Named property setter is not supported.");
    }
    return;
  }
}

// step 2. Let ownDesc be LegacyPlatformObjectGetOwnProperty(O, P, true).
// step 3. Perform ? OrdinarySetWithOwnDescriptor(O, P, V, Receiver,
//   ownDesc).
//
// Do not intercept.  Fallback to OrdinarySetWithOwnDescriptor.
"""))
        return func_decl, func_def

    bind_return_value(
        body,
        cg_context,
        overriding_args=["${blink_property_name}", "${blink_property_value}"])
    body.register_code_symbol(
        make_v8_to_blink_value(
            "blink_property_value",
            "${v8_property_value}",
            cg_context.named_property_setter.arguments[1].idl_type,
            argument_index=2))

    if "Custom" in cg_context.named_property_setter.extended_attributes:
        text = _format(
            "${class_name}::{}"
            "(${blink_property_name}, ${v8_property_value}, ${info});",
            custom_function_name(cg_context))
        body.append(TextNode(text))
        return func_decl, func_def

    body.extend([
        TextNode("""\
// 3.8.2. [[Set]]
// https://heycam.github.io/webidl/#legacy-platform-object-set
// step 1. If O and Receiver are the same object, then:\
"""),
        CxxLikelyIfNode(
            cond="${info}.Holder() == ${info}.This()",
            body=[
                TextNode("""\
// step 1.2.1. Invoke the named property setter with P and V.\
"""),
                make_steps_of_ce_reactions(cg_context),
                EmptyNode(),
                make_v8_set_return_value(cg_context),
                TextNode("""\
% if interface.identifier == "CSSStyleDeclaration":
// CSSStyleDeclaration is abusing named properties.
// Do not intercept if the property is not found.
% else:
bindings::V8SetReturnValue(${info}, nullptr);
% endif
return;"""),
            ]),
        EmptyNode(),
        TextNode("""\
// Do not intercept.  Fallback to OrdinarySetWithOwnDescriptor.
"""),
    ])

    return func_decl, func_def


def make_named_property_deleter_callback(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    arg_decls = [
        "v8::Local<v8::Name> v8_property_name",
        "const v8::PropertyCallbackInfo<v8::Boolean>& info",
    ]
    arg_names = ["v8_property_name", "info"]

    func_decl, func_def = _make_interceptor_callback(
        cg_context, function_name, arg_decls, arg_names, cg_context.class_name,
        "NamedPropertyDeleter")
    body = func_def.body

    props = cg_context.interface.indexed_and_named_properties
    if (not cg_context.named_property_deleter
            and "NotEnumerable" in props.named_getter.extended_attributes):
        body.append(
            TextNode("""\
// 3.8.4. [[Delete]]
// https://heycam.github.io/webidl/#legacy-platform-object-delete
// step 2. If O supports named properties, O does not implement an interface
//   with the [Global] extended attribute and the result of calling the
//   named property visibility algorithm with property name P and object O
//   is true, then:
//
// There is no easy way to determine whether the named property is visible
// or not.  Just do not intercept and fallback to the default behavior.
"""))
        return func_decl, func_def

    if not cg_context.named_property_deleter:
        body.append(
            TextNode("""\
// 3.8.4. [[Delete]]
// https://heycam.github.io/webidl/#legacy-platform-object-delete
// step 2. If O supports named properties, O does not implement an interface
//   with the [Global] extended attribute and the result of calling the
//   named property visibility algorithm with property name P and object O
//   is true, then:
// step 2.1. If O does not implement an interface with a named property
//   deleter, then return false.
ExceptionState exception_state(${info}.GetIsolate(),
                               ExceptionState::kNamedDeletionContext,
                               "${interface.identifier}");
bool does_exist = ${blink_receiver}->NamedPropertyQuery(
    ${blink_property_name}, exception_state);
if (exception_state.HadException())
  return;
if (does_exist) {
  bindings::V8SetReturnValue(${info}, false);
  if (${info}.ShouldThrowOnError()) {
    exception_state.ThrowTypeError("Named property deleter is not supported.");
  }
  return;
}

// Do not intercept.
"""))
        return func_decl, func_def

    bind_return_value(
        body, cg_context, overriding_args=["${blink_property_name}"])

    if "Custom" in cg_context.named_property_deleter.extended_attributes:
        text = _format("${class_name}::{}(${blink_property_name}, ${info});",
                       custom_function_name(cg_context))
        body.append(TextNode(text))
        return func_decl, func_def

    body.extend([
        TextNode("""\
// 3.8.4. [[Delete]]
// https://heycam.github.io/webidl/#legacy-platform-object-delete\
"""),
        make_steps_of_ce_reactions(cg_context),
        EmptyNode(),
        make_v8_set_return_value(cg_context),
        TextNode("""\
if (${return_value} == NamedPropertyDeleterResult::kDidNotDelete) {
  if (${info}.ShouldThrowOnError()) {
    ExceptionState exception_state(${info}.GetIsolate(),
                                   ExceptionState::kNamedDeletionContext,
                                   "${interface.identifier}");
    exception_state.ThrowTypeError("Failed to delete a property.");
  }
  return;
}"""),
    ])

    return func_decl, func_def


def make_named_property_definer_callback(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    arg_decls = [
        "v8::Local<v8::Name> v8_property_name",
        "const v8::PropertyDescriptor& v8_property_desc",
        "const v8::PropertyCallbackInfo<v8::Value>& info",
    ]
    arg_names = ["v8_property_name", "v8_property_desc", "info"]

    func_decl, func_def = _make_interceptor_callback(
        cg_context, function_name, arg_decls, arg_names, cg_context.class_name,
        "NamedPropertyDefiner")
    body = func_def.body

    if cg_context.interface.identifier == "CSSStyleDeclaration":
        body.append(
            TextNode("""\
// CSSStyleDeclaration is abusing named properties.
// Do not intercept.  Fallback to OrdinaryDefineOwnProperty.
"""))
    elif not cg_context.interface.indexed_and_named_properties.named_setter:
        body.append(
            TextNode("""\
// 3.8.3. [[DefineOwnProperty]]
// https://heycam.github.io/webidl/#legacy-platform-object-defineownproperty
// step 2.1. Let creating be true if P is not a supported property name, and
//   false otherwise.
// step 2.2.1. If creating is false and O does not implement an interface
//   with a named property setter, then return false.
${class_name}::NamedPropertyGetterCallback(${v8_property_name}, ${info});
const bool is_creating = ${info}.GetReturnValue().Get()->IsUndefined();
if (!is_creating) {
  bindings::V8SetReturnValue(${info}, nullptr);
  if (${info}.ShouldThrowOnError()) {
    ExceptionState exception_state(${info}.GetIsolate(),
                                   ExceptionState::kNamedSetterContext,
                                   "${interface.identifier}");
    exception_state.ThrowTypeError("Named property setter is not supported.");
  }
  return;
}

// Do not intercept.  Fallback to OrdinaryDefineOwnProperty.
"""))
    else:
        body.append(
            TextNode("""\
// 3.8.3. [[DefineOwnProperty]]
// https://heycam.github.io/webidl/#legacy-platform-object-defineownproperty
// step 2.2.2. If O implements an interface with a named property setter,
//   then:
// step 2.2.2.1. If the result of calling IsDataDescriptor(Desc) is false,
//   then return false.
if (v8_property_desc.has_get() || v8_property_desc.has_set()) {
  bindings::V8SetReturnValue(${info}, nullptr);
  if (${info}.ShouldThrowOnError()) {
    ExceptionState exception_state(${info}.GetIsolate(),
                                   ExceptionState::kNamedSetterContext,
                                   "${interface.identifier}");
    exception_state.ThrowTypeError("Accessor properties are not allowed.");
  }
  return;
}
// step 2.2.2.2. Invoke the named property setter with P and Desc.[[Value]].
${class_name}::NamedPropertySetterCallback(
    ${v8_property_name}, ${v8_property_desc}.value(), ${info});
bindings::V8SetReturnValue(${info}, nullptr);
"""))

    return func_decl, func_def


def make_named_property_descriptor_callback(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    arg_decls = [
        "v8::Local<v8::Name> v8_property_name",
        "const v8::PropertyCallbackInfo<v8::Value>& info",
    ]
    arg_names = ["v8_property_name", "info"]

    func_decl, func_def = _make_interceptor_callback(
        cg_context, function_name, arg_decls, arg_names, cg_context.class_name,
        "NamedPropertyDescriptor")
    body = func_def.body

    body.append(
        TextNode("""\
// LegacyPlatformObjectGetOwnProperty
// https://heycam.github.io/webidl/#LegacyPlatformObjectGetOwnProperty\
"""))

    if ("LegacyOverrideBuiltins" not in
            cg_context.interface.extended_attributes):
        body.append(
            TextNode("""\
// step 2.1. If the result of running the named property visibility algorithm
//   with property name P and object O is true, then:
if (${v8_receiver}->GetRealNamedPropertyAttributesInPrototypeChain(
        ${current_context}, ${v8_property_name}).IsJust()) {
  return;  // Do not intercept.  Fallback to OrdinaryGetOwnProperty.
}
"""))

    pattern = """\
// step 2.1.3. If operation was defined without an identifier, then set
//   value to the result of performing the steps listed in the interface
//   description to determine the value of a named property with P as the
//   name.
// step 2.1.4. Otherwise, operation was defined with an identifier. Set
//   value to the result of performing the steps listed in the description
//   of operation with P as the only argument value.
${class_name}::NamedPropertyGetterCallback(${v8_property_name}, ${info});
v8::Local<v8::Value> v8_value = ${info}.GetReturnValue().Get();
// step 2.1. If the result of running the named property visibility
//   algorithm with property name P and object O is true, then:
// step 3. Return OrdinaryGetOwnProperty(O, P).
if (v8_value->IsUndefined())
  return;  // Do not intercept.  Fallback to OrdinaryGetOwnProperty.

// step 2.1.6. Set desc.[[Value]] to the result of converting value to an
//   ECMAScript value.
// step 2.1.7. If O implements an interface with a named property setter,
//   then set desc.[[Writable]] to true, otherwise set it to false.
// step 2.1.8. If O implements an interface with the
//   [LegacyUnenumerableNamedProperties] extended attribute, then set
//   desc.[[Enumerable]] to false, otherwise set it to true.
// step 2.1.9. Set desc.[[Configurable]] to true.
v8::PropertyDescriptor desc(v8_value, /*writable=*/{cxx_writable});
desc.set_enumerable({cxx_enumerable});
desc.set_configurable(true);
bindings::V8SetReturnValue(${info}, desc);
"""
    props = cg_context.interface.indexed_and_named_properties
    writable = bool(props.named_setter)
    cxx_writable = "true" if writable else "false"
    enumerable = props.is_named_property_enumerable
    cxx_enumerable = "true" if enumerable else "false"
    body.append(
        TextNode(
            _format(
                pattern,
                cxx_writable=cxx_writable,
                cxx_enumerable=cxx_enumerable)))

    return func_decl, func_def


def make_named_property_query_callback(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    props = cg_context.interface.indexed_and_named_properties
    if "NotEnumerable" in props.named_getter.extended_attributes:
        return None, None

    arg_decls = [
        "v8::Local<v8::Name> v8_property_name",
        "const v8::PropertyCallbackInfo<v8::Integer>& info",
    ]
    arg_names = ["v8_property_name", "info"]

    func_decl, func_def = _make_interceptor_callback(
        cg_context, function_name, arg_decls, arg_names, cg_context.class_name,
        "NamedPropertyQuery")
    body = func_def.body

    flags = []
    if not props.named_setter:
        flags.append("v8::ReadOnly")
    if not props.is_named_property_enumerable:
        flags.append("v8::DontEnum")
    if not flags:
        flags.append("v8::None")
    if len(flags) == 1:
        property_attribute = flags[0]
    else:
        property_attribute = " | ".join(flags)

    body.extend([
        TextNode("""\
ExceptionState exception_state(${isolate},
                               ExceptionState::kNamedGetterContext,
                               "${interface.identifier}");
bool does_exist = ${blink_receiver}->NamedPropertyQuery(
    ${blink_property_name}, exception_state);
if (!does_exist)
  return;  // Do not intercept.
"""),
        TextNode(
            _format(
                "bindings::V8SetReturnValue"
                "(${info}, uint32_t({property_attribute}));",
                property_attribute=property_attribute)),
    ])

    return func_decl, func_def


def make_named_property_enumerator_callback(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    props = cg_context.interface.indexed_and_named_properties
    if "NotEnumerable" in props.named_getter.extended_attributes:
        return None, None

    arg_decls = ["const v8::PropertyCallbackInfo<v8::Array>& info"]
    arg_names = ["info"]

    func_decl, func_def = _make_interceptor_callback(
        cg_context, function_name, arg_decls, arg_names, cg_context.class_name,
        "NamedPropertyEnumerator")
    body = func_def.body

    body.append(
        TextNode("""\
// 3.8.6. [[OwnPropertyKeys]]
// https://heycam.github.io/webidl/#legacy-platform-object-ownpropertykeys
// step 3. If O supports named properties, then for each P of O's supported
//   property names that is visible according to the named property
//   visibility algorithm, append P to keys.
Vector<String> blink_property_names;
ExceptionState exception_state(${info}.GetIsolate(),
                               ExceptionState::kEnumerationContext,
                               "${interface.identifier}");
${blink_receiver}->NamedPropertyEnumerator(
    blink_property_names, exception_state);
if (exception_state.HadException())
  return;
bindings::V8SetReturnValue(
    ${info},
    ToV8(blink_property_names, ${creation_context_object}, ${isolate}));
"""))

    return func_decl, func_def


# ----------------------------------------------------------------------------
# Callback functions of interceptors on named properties object
# ----------------------------------------------------------------------------


def make_named_props_obj_indexed_getter_callback(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    arg_decls = [
        "uint32_t index",
        "const v8::PropertyCallbackInfo<v8::Value>& info",
    ]
    arg_names = ["index", "info"]

    func_def = _make_interceptor_callback_def(
        cg_context, function_name, arg_decls, arg_names, None,
        "NamedPropertiesObject_IndexedPropertyGetter")
    body = func_def.body

    body.append(
        TextNode("""\
v8::Local<v8::String> property_name =
    V8AtomicString(${isolate}, AtomicString::Number(${index}));
NamedPropsObjNamedGetterCallback(property_name, ${info});
"""))

    return func_def


def make_named_props_obj_indexed_setter_callback(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    arg_decls = [
        "uint32_t index",
        "v8::Local<v8::Value> v8_property_value",
        "const v8::PropertyCallbackInfo<v8::Value>& info",
    ]
    arg_names = ["index", "v8_property_value", "info"]

    func_def = _make_interceptor_callback_def(
        cg_context, function_name, arg_decls, arg_names, None,
        "NamedPropertiesObject_IndexedPropertySetter")
    body = func_def.body

    body.append(
        TextNode("""\
v8::Local<v8::String> property_name =
    V8AtomicString(${isolate}, AtomicString::Number(${index}));
NamedPropsObjNamedSetterCallback(property_name, ${v8_property_value}, ${info});
"""))

    return func_def


def make_named_props_obj_indexed_deleter_callback(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    arg_decls = [
        "uint32_t index",
        "const v8::PropertyCallbackInfo<v8::Boolean>& info",
    ]
    arg_names = ["index", "info"]

    func_def = _make_interceptor_callback_def(
        cg_context, function_name, arg_decls, arg_names, None,
        "NamedPropertiesObject_IndexedPropertyDeleter")
    body = func_def.body

    body.append(
        TextNode("""\
bindings::V8SetReturnValue(${info}, false);
if (${info}.ShouldThrowOnError()) {
  ExceptionState exception_state(${info}.GetIsolate(),
                                 ExceptionState::kIndexedDeletionContext,
                                 "${interface.identifier}");
  exception_state.ThrowTypeError("Named property deleter is not supported.");
}
"""))

    return func_def


def make_named_props_obj_indexed_definer_callback(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    arg_decls = [
        "uint32_t index",
        "const v8::PropertyDescriptor& v8_property_desc",
        "const v8::PropertyCallbackInfo<v8::Value>& info",
    ]
    arg_names = ["index", "v8_property_desc", "info"]

    func_def = _make_interceptor_callback_def(
        cg_context, function_name, arg_decls, arg_names, None,
        "NamedPropertiesObject_IndexedPropertyDefiner")
    body = func_def.body

    body.append(
        TextNode("""\
// 3.6.4.2. [[DefineOwnProperty]]
// https://heycam.github.io/webidl/#named-properties-object-defineownproperty
bindings::V8SetReturnValue(${info}, nullptr);
if (${info}.ShouldThrowOnError()) {
  ExceptionState exception_state(${info}.GetIsolate(),
                                 ExceptionState::kIndexedSetterContext,
                                 "${interface.identifier}");
  exception_state.ThrowTypeError("Named property deleter is not supported.");
}
"""))

    return func_def


def make_named_props_obj_indexed_descriptor_callback(cg_context,
                                                     function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    arg_decls = [
        "uint32_t index",
        "const v8::PropertyCallbackInfo<v8::Value>& info",
    ]
    arg_names = ["index", "info"]

    func_def = _make_interceptor_callback_def(
        cg_context, function_name, arg_decls, arg_names, None,
        "NamedPropertiesObject_IndexedPropertyDescriptor")
    body = func_def.body

    body.append(
        TextNode("""\
v8::Local<v8::String> property_name =
    V8AtomicString(${isolate}, AtomicString::Number(${index}));
NamedPropsObjNamedDescriptorCallback(property_name, ${info});
"""))

    return func_def


def make_named_props_obj_named_getter_callback(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    arg_decls = [
        "v8::Local<v8::Name> v8_property_name",
        "const v8::PropertyCallbackInfo<v8::Value>& info",
    ]
    arg_names = ["v8_property_name", "info"]

    func_def = _make_interceptor_callback_def(
        cg_context, function_name, arg_decls, arg_names, None,
        "NamedPropertiesObject_NamedPropertyGetter")
    body = func_def.body

    body.append(
        TextNode("""\
// 3.6.4.1. [[GetOwnProperty]]
// https://heycam.github.io/webidl/#named-properties-object-getownproperty
//
// TODO(yukishiino): Update the following hard-coded call to an appropriate
// one.
V8Window::NamedPropertyGetterCustom(${blink_property_name}, ${info});
"""))

    return func_def


def make_named_props_obj_named_setter_callback(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    arg_decls = [
        "v8::Local<v8::Name> v8_property_name",
        "v8::Local<v8::Value> v8_property_value",
        "const v8::PropertyCallbackInfo<v8::Value>& info",
    ]
    arg_names = ["v8_property_name", "v8_property_value", "info"]

    func_def = _make_interceptor_callback_def(
        cg_context, function_name, arg_decls, arg_names, None,
        "NamedPropertiesObject_NamedPropertySetter")
    body = func_def.body

    body.append(
        TextNode("""\
if (${info}.Holder() == ${info}.This()) {
  // 3.6.4.2. [[DefineOwnProperty]]
  // https://heycam.github.io/webidl/#named-properties-object-defineownproperty
  bindings::V8SetReturnValue(${info}, nullptr);
  if (${info}.ShouldThrowOnError()) {
    ExceptionState exception_state(${info}.GetIsolate(),
                                   ExceptionState::kNamedSetterContext,
                                   "${interface.identifier}");
    exception_state.ThrowTypeError(
        "Named property setter is not supported.");
  }
  return;
}
"""))

    return func_def


def make_named_props_obj_named_deleter_callback(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    arg_decls = [
        "v8::Local<v8::Name> v8_property_name",
        "const v8::PropertyCallbackInfo<v8::Boolean>& info",
    ]
    arg_names = ["v8_property_name", "info"]

    func_def = _make_interceptor_callback_def(
        cg_context, function_name, arg_decls, arg_names, None,
        "NamedPropertiesObject_NamedPropertyDeleter")
    body = func_def.body

    body.append(
        TextNode("""\
// 3.6.4.3. [[Delete]]
// https://heycam.github.io/webidl/#named-properties-object-delete
bindings::V8SetReturnValue(${info}, false);
if (${info}.ShouldThrowOnError()) {
  ExceptionState exception_state(${info}.GetIsolate(),
                                 ExceptionState::kNamedDeletionContext,
                                 "${interface.identifier}");
  exception_state.ThrowTypeError("Named property deleter is not supported.");
}
"""))

    return func_def


def make_named_props_obj_named_definer_callback(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    arg_decls = [
        "v8::Local<v8::Name> v8_property_name",
        "const v8::PropertyDescriptor& v8_property_desc",
        "const v8::PropertyCallbackInfo<v8::Value>& info",
    ]
    arg_names = ["v8_property_name", "v8_property_desc", "info"]

    func_def = _make_interceptor_callback_def(
        cg_context, function_name, arg_decls, arg_names, None,
        "NamedPropertiesObject_NamedPropertyDefiner")
    body = func_def.body

    body.append(
        TextNode("""\
// 3.6.4.2. [[DefineOwnProperty]]
// https://heycam.github.io/webidl/#named-properties-object-defineownproperty
bindings::V8SetReturnValue(${info}, nullptr);
if (${info}.ShouldThrowOnError()) {
  ExceptionState exception_state(${info}.GetIsolate(),
                                 ExceptionState::kNamedSetterContext,
                                 "${interface.identifier}");
  exception_state.ThrowTypeError("Named property setter is not supported.");
}
"""))

    return func_def


def make_named_props_obj_named_descriptor_callback(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    arg_decls = [
        "v8::Local<v8::Name> v8_property_name",
        "const v8::PropertyCallbackInfo<v8::Value>& info",
    ]
    arg_names = ["v8_property_name", "info"]

    func_def = _make_interceptor_callback_def(
        cg_context, function_name, arg_decls, arg_names, None,
        "NamedPropertiesObject_NamedPropertyDescriptor")
    body = func_def.body

    body.append(
        TextNode("""\
// 3.6.4.1. [[GetOwnProperty]]
// https://heycam.github.io/webidl/#named-properties-object-getownproperty
// step 4. If the result of running the named property visibility algorithm
//   with property name P and object object is true, then:
if (${v8_receiver}->GetRealNamedPropertyAttributesInPrototypeChain(
        ${current_context}, ${v8_property_name}).IsJust()) {
  return;  // Do not intercept.  Fallback to OrdinaryGetOwnProperty.
}

// TODO(yukishiino): Update the following hard-coded call to an appropriate
// one.
V8Window::NamedPropertyGetterCustom(${blink_property_name}, ${info});
v8::Local<v8::Value> v8_value = ${info}.GetReturnValue().Get();
if (v8_value->IsUndefined())
  return;  // Do not intercept.  Fallback to OrdinaryGetOwnProperty.

// step 4.7. If A implements an interface with the
//   [LegacyUnenumerableNamedProperties] extended attribute, then set
//   desc.[[Enumerable]] to false, otherwise set it to true.
// step 4.8. Set desc.[[Writable]] to true and desc.[[Configurable]] to
//   true.
v8::PropertyDescriptor desc(v8_value, /*writable=*/true);
desc.set_enumerable(false);
desc.set_configurable(true);
bindings::V8SetReturnValue(${info}, desc);
"""))

    return func_def


# ----------------------------------------------------------------------------
# Callback functions of cross origin interceptors
# ----------------------------------------------------------------------------


def make_cross_origin_access_check_callback(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    func_def = CxxFuncDefNode(
        name=function_name,
        arg_decls=[
            "v8::Local<v8::Context> accessing_context",
            "v8::Local<v8::Object> accessed_object",
            "v8::Local<v8::Value> unused_data",
        ],
        return_type="bool")
    func_def.set_base_template_vars(cg_context.template_bindings())
    body = func_def.body
    body.add_template_var("accessing_context", "accessing_context")
    body.add_template_var("accessed_object", "accessed_object")
    bind_callback_local_vars(body, cg_context)

    if cg_context.interface.identifier == "Window":
        blink_class = "DOMWindow"
    else:
        blink_class = blink_class_name(cg_context.interface)
    body.extend([
        TextNode(
            _format(
                "{blink_class}* blink_accessed_object = "
                "${class_name}::ToWrappableUnsafe(${accessed_object});",
                blink_class=blink_class)),
        TextNode("return BindingSecurity::ShouldAllowAccessTo("
                 "ToLocalDOMWindow(${accessing_context}), "
                 "blink_accessed_object, "
                 "BindingSecurity::ErrorReportOption::kDoNotReport);"),
    ])

    return func_def


def make_cross_origin_indexed_getter_callback(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    arg_decls = [
        "uint32_t index",
        "const v8::PropertyCallbackInfo<v8::Value>& info",
    ]
    arg_names = ["index", "info"]

    func_def = _make_interceptor_callback_def(
        cg_context, function_name, arg_decls, arg_names, None,
        "CrossOriginProperty_IndexedPropertyGetter")
    body = func_def.body

    if cg_context.interface.identifier != "Window":
        body.append(TextNode("${throw_security_error}"))
        return func_def

    bind_return_value(body, cg_context, overriding_args=["${index}"])

    body.extend([
        TextNode("""\
if (${index} >= ${blink_receiver}->length()) {
  ${throw_security_error}
  return;
}
"""),
        make_v8_set_return_value(cg_context),
    ])

    return func_def


def make_cross_origin_indexed_setter_callback(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    arg_decls = [
        "uint32_t index",
        "v8::Local<v8::Value> v8_property_value",
        "const v8::PropertyCallbackInfo<v8::Value>& info",
    ]
    arg_names = ["index", "v8_property_value", "info"]

    func_def = _make_interceptor_callback_def(
        cg_context, function_name, arg_decls, arg_names, None,
        "CrossOriginProperty_IndexedPropertySetter")
    body = func_def.body

    body.append(TextNode("${throw_security_error}"))

    return func_def


def make_cross_origin_indexed_deleter_callback(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    arg_decls = [
        "uint32_t index",
        "const v8::PropertyCallbackInfo<v8::Boolean>& info",
    ]
    arg_names = ["index", "info"]

    func_def = _make_interceptor_callback_def(
        cg_context, function_name, arg_decls, arg_names, None,
        "CrossOriginProperty_IndexedPropertyDeleter")
    body = func_def.body

    body.append(TextNode("${throw_security_error}"))

    return func_def


def make_cross_origin_indexed_definer_callback(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    arg_decls = [
        "uint32_t index",
        "const v8::PropertyDescriptor& v8_property_desc",
        "const v8::PropertyCallbackInfo<v8::Value>& info",
    ]
    arg_names = ["index", "v8_property_desc", "info"]

    func_def = _make_interceptor_callback_def(
        cg_context, function_name, arg_decls, arg_names, None,
        "CrossOriginProperty_IndexedPropertyDefiner")
    body = func_def.body

    body.append(TextNode("${throw_security_error}"))

    return func_def


def make_cross_origin_indexed_descriptor_callback(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    arg_decls = [
        "uint32_t index",
        "const v8::PropertyCallbackInfo<v8::Value>& info",
    ]
    arg_names = ["index", "info"]

    func_def = _make_interceptor_callback_def(
        cg_context, function_name, arg_decls, arg_names, None,
        "CrossOriginProperty_IndexedPropertyDescriptor")
    body = func_def.body

    if cg_context.interface.identifier != "Window":
        body.append(TextNode("${throw_security_error}"))
        return func_def

    body.append(
        TextNode("""\
CrossOriginIndexedGetterCallback(${index}, ${info});
v8::Local<v8::Value> v8_value = ${info}.GetReturnValue().Get();
if (v8_value->IsUndefined()) {
  // Must have already thrown a SecurityError.
  return;
}

v8::PropertyDescriptor desc(v8_value, /*writable=*/false);
desc.set_enumerable(true);
desc.set_configurable(true);
bindings::V8SetReturnValue(${info}, desc);
"""))

    return func_def


def make_cross_origin_indexed_enumerator_callback(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    arg_decls = ["const v8::PropertyCallbackInfo<v8::Array>& info"]
    arg_names = ["info"]

    func_def = _make_interceptor_callback_def(
        cg_context, function_name, arg_decls, arg_names, None,
        "CrossOriginProperty_IndexedPropertyEnumerator")
    body = func_def.body

    if cg_context.interface.identifier != "Window":
        return func_def

    body.append(
        TextNode("""\
uint32_t length = ${blink_receiver}->length();
v8::Local<v8::Array> array =
    bindings::EnumerateIndexedProperties(${isolate}, length);
bindings::V8SetReturnValue(${info}, array);
"""))

    return func_def


def make_cross_origin_named_getter_callback(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    arg_decls = [
        "v8::Local<v8::Name> v8_property_name",
        "const v8::PropertyCallbackInfo<v8::Value>& info",
    ]
    arg_names = ["v8_property_name", "info"]

    func_def = _make_interceptor_callback_def(
        cg_context, function_name, arg_decls, arg_names, None,
        "CrossOriginProperty_NamedPropertyGetter")
    body = func_def.body

    string_case_body = []
    string_case_body.append(
        TextNode("""\
for (const auto& attribute : kCrossOriginAttributeTable) {
  if (${blink_property_name} != attribute.name)
    continue;
  if (UNLIKELY(!attribute.get_value)) {
    ${throw_security_error}
    return;
  }
  attribute.get_value(${v8_property_name}, ${info});
  return;
}
for (const auto& operation : kCrossOriginOperationTable) {
  if (${blink_property_name} != operation.name)
    continue;
  v8::Local<v8::Function> function;
  if (bindings::GetCrossOriginFunction(
          ${info}.GetIsolate(), operation.callback, operation.func_length,
          ${class_name}::GetWrapperTypeInfo())
          .ToLocal(&function)) {
    bindings::V8SetReturnValue(${info}, function);
  }
  return;
}
"""))
    if cg_context.interface.identifier == "Window":
        string_case_body.append(
            TextNode("""\
// Window object's document-tree child browsing context name property set
//
// TODO(yukishiino): Update the following hard-coded call to an appropriate
// one.
V8Window::NamedPropertyGetterCustom(${blink_property_name}, ${info});
if (!${info}.GetReturnValue().Get()->IsUndefined())
  return;
"""))

    body.extend([
        CxxLikelyIfNode(
            cond="${v8_property_name}->IsString()", body=string_case_body),
        EmptyNode(),
        TextNode("""\
// 7.2.3.2 CrossOriginPropertyFallback ( P )
// https://html.spec.whatwg.org/C/#crossoriginpropertyfallback-(-p-)
if (bindings::IsSupportedInCrossOriginPropertyFallback(
        ${info}.GetIsolate(), ${v8_property_name})) {
  return ${info}.GetReturnValue().SetUndefined();
}
${throw_security_error}
"""),
    ])

    return func_def


def make_cross_origin_named_setter_callback(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    arg_decls = [
        "v8::Local<v8::Name> v8_property_name",
        "v8::Local<v8::Value> v8_property_value",
        "const v8::PropertyCallbackInfo<v8::Value>& info",
    ]
    arg_names = ["v8_property_name", "v8_property_value", "info"]

    func_def = _make_interceptor_callback_def(
        cg_context, function_name, arg_decls, arg_names, None,
        "CrossOriginProperty_NamedPropertySetter")
    body = func_def.body

    string_case_body = []
    string_case_body.append(
        TextNode("""\
for (const auto& attribute : kCrossOriginAttributeTable) {
  if (${blink_property_name} == attribute.name && attribute.set_value) {
    attribute.set_value(${v8_property_name}, ${v8_property_value}, ${info});
    return;
  }
}
"""))

    body.extend([
        CxxLikelyIfNode(
            cond="${v8_property_name}->IsString()", body=string_case_body),
        EmptyNode(),
        TextNode("${throw_security_error}"),
    ])

    return func_def


def make_cross_origin_named_deleter_callback(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    arg_decls = [
        "v8::Local<v8::Name> v8_property_name",
        "const v8::PropertyCallbackInfo<v8::Boolean>& info",
    ]
    arg_names = ["v8_property_name", "info"]

    func_def = _make_interceptor_callback_def(
        cg_context, function_name, arg_decls, arg_names, None,
        "CrossOriginProperty_NamedPropertyDeleter")
    body = func_def.body

    body.append(TextNode("${throw_security_error}"))

    return func_def


def make_cross_origin_named_definer_callback(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    arg_decls = [
        "v8::Local<v8::Name> v8_property_name",
        "const v8::PropertyDescriptor& v8_property_desc",
        "const v8::PropertyCallbackInfo<v8::Value>& info",
    ]
    arg_names = ["v8_property_name", "v8_property_desc", "info"]

    func_def = _make_interceptor_callback_def(
        cg_context, function_name, arg_decls, arg_names, None,
        "CrossOriginProperty_NamedPropertyDefiner")
    body = func_def.body

    body.append(TextNode("${throw_security_error}"))

    return func_def


def make_cross_origin_named_descriptor_callback(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    arg_decls = [
        "v8::Local<v8::Name> v8_property_name",
        "const v8::PropertyCallbackInfo<v8::Value>& info",
    ]
    arg_names = ["v8_property_name", "info"]

    func_def = _make_interceptor_callback_def(
        cg_context, function_name, arg_decls, arg_names, None,
        "CrossOriginProperty_NamedPropertyDescriptor")
    body = func_def.body

    string_case_body = []
    string_case_body.append(
        TextNode("""\
// 7.2.3.4 CrossOriginGetOwnPropertyHelper ( O, P )
// https://html.spec.whatwg.org/C/#crossorigingetownpropertyhelper-(-o,-p-)
for (const auto& attribute : kCrossOriginAttributeTable) {
  if (${blink_property_name} != attribute.name)
    continue;
  v8::Local<v8::Value> get;
  v8::Local<v8::Value> set;
  if (!bindings::GetCrossOriginFunctionOrUndefined(
           ${info}.GetIsolate(), attribute.get_callback, 0,
           ${class_name}::GetWrapperTypeInfo())
           .ToLocal(&get) ||
      !bindings::GetCrossOriginFunctionOrUndefined(
           ${info}.GetIsolate(), attribute.set_callback, 1,
           ${class_name}::GetWrapperTypeInfo())
           .ToLocal(&set)) {
    return;
  }
  v8::PropertyDescriptor desc(get, set);
  desc.set_enumerable(false);
  desc.set_configurable(true);
  bindings::V8SetReturnValue(${info}, desc);
  return;
}
for (const auto& operation : kCrossOriginOperationTable) {
  if (${blink_property_name} != operation.name)
    continue;
  v8::Local<v8::Function> function;
  if (!bindings::GetCrossOriginFunction(
           ${info}.GetIsolate(), operation.callback, operation.func_length,
           ${class_name}::GetWrapperTypeInfo())
           .ToLocal(&function)) {
    return;
  }
  v8::PropertyDescriptor desc(function, /*writable=*/false);
  desc.set_enumerable(false);
  desc.set_configurable(true);
  bindings::V8SetReturnValue(${info}, desc);
  return;
}
"""))
    if cg_context.interface.identifier == "Window":
        string_case_body.append(
            TextNode("""\
// Window object's document-tree child browsing context name property set
//
// TODO(yukishiino): Update the following hard-coded call to an appropriate
// one.
V8Window::NamedPropertyGetterCustom(${blink_property_name}, ${info});
if (!${info}.GetReturnValue().Get()->IsUndefined()) {
  v8::PropertyDescriptor desc(${info}.GetReturnValue().Get(),
                              /*writable=*/false);
  desc.set_enumerable(false);
  desc.set_configurable(true);
  bindings::V8SetReturnValue(${info}, desc);
  return;
}
"""))

    body.extend([
        CxxLikelyIfNode(
            cond="${v8_property_name}->IsString()", body=string_case_body),
        EmptyNode(),
        TextNode("""\
// 7.2.3.2 CrossOriginPropertyFallback ( P )
// https://html.spec.whatwg.org/C/#crossoriginpropertyfallback-(-p-)
if (bindings::IsSupportedInCrossOriginPropertyFallback(
        ${info}.GetIsolate(), ${v8_property_name})) {
  v8::PropertyDescriptor desc(v8::Undefined(${info}.GetIsolate()),
                              /*writable=*/false);
  desc.set_enumerable(false);
  desc.set_configurable(true);
  bindings::V8SetReturnValue(${info}, desc);
  return;
}
${throw_security_error}
"""),
    ])

    return func_def


def make_cross_origin_named_query_callback(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    arg_decls = [
        "v8::Local<v8::Name> v8_property_name",
        "const v8::PropertyCallbackInfo<v8::Integer>& info",
    ]
    arg_names = ["v8_property_name", "info"]

    func_def = _make_interceptor_callback_def(
        cg_context, function_name, arg_decls, arg_names, None,
        "CrossOriginProperty_NamedPropertyQuery")
    body = func_def.body

    string_case_body = []
    string_case_body.append(
        TextNode("""\
// 7.2.3.4 CrossOriginGetOwnPropertyHelper ( O, P )
// https://html.spec.whatwg.org/C/#crossorigingetownpropertyhelper-(-o,-p-)
for (const auto& attribute : kCrossOriginAttributeTable) {
  if (${blink_property_name} != attribute.name)
    continue;
  int32_t v8_property_attribute = v8::DontEnum;
  if (!attribute.set_callback)
    v8_property_attribute |= v8::ReadOnly;
  bindings::V8SetReturnValue(${info}, v8_property_attribute);
  return;
}
for (const auto& operation : kCrossOriginOperationTable) {
  if (${blink_property_name} != operation.name)
    continue;
  int32_t v8_property_attribute = v8::DontEnum | v8::ReadOnly;
  bindings::V8SetReturnValue(${info}, v8_property_attribute);
  return;
}
"""))

    body.extend([
        CxxLikelyIfNode(
            cond="${v8_property_name}->IsString()", body=string_case_body),
        EmptyNode(),
        TextNode("""\
// 7.2.3.2 CrossOriginPropertyFallback ( P )
// https://html.spec.whatwg.org/C/#crossoriginpropertyfallback-(-p-)
if (bindings::IsSupportedInCrossOriginPropertyFallback(
        ${info}.GetIsolate(), ${v8_property_name})) {
  int32_t v8_property_attribute = v8::DontEnum | v8::ReadOnly;
  bindings::V8SetReturnValue(${info}, v8_property_attribute);
  return;
}
"""),
    ])

    return func_def


def make_cross_origin_named_enumerator_callback(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    arg_decls = ["const v8::PropertyCallbackInfo<v8::Array>& info"]
    arg_names = ["info"]

    func_def = _make_interceptor_callback_def(
        cg_context, function_name, arg_decls, arg_names, None,
        "CrossOriginProperty_NamedPropertyEnumerator")
    body = func_def.body

    body.append(
        TextNode("""\
bindings::V8SetReturnValue(
    ${info},
    bindings::EnumerateCrossOriginProperties(
        ${isolate},
        kCrossOriginAttributeTable,
        kCrossOriginOperationTable));
"""))

    return func_def


# ----------------------------------------------------------------------------
# Callback functions of same origin interceptors
# ----------------------------------------------------------------------------


def make_same_origin_indexed_getter_callback(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    arg_decls = [
        "uint32_t index",
        "const v8::PropertyCallbackInfo<v8::Value>& info",
    ]
    arg_names = ["index", "info"]

    func_def = _make_interceptor_callback_def(
        cg_context, function_name, arg_decls, arg_names, None,
        "SameOriginProperty_IndexedPropertyGetter")
    body = func_def.body

    bind_return_value(body, cg_context, overriding_args=["${index}"])

    body.extend([
        TextNode("""\
if (${index} >= ${blink_receiver}->length()) {
  return;
}
"""),
        make_v8_set_return_value(cg_context),
    ])

    return func_def


def make_same_origin_indexed_setter_callback(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    arg_decls = [
        "uint32_t index",
        "v8::Local<v8::Value> v8_property_value",
        "const v8::PropertyCallbackInfo<v8::Value>& info",
    ]
    arg_names = ["index", "v8_property_value", "info"]

    func_def = _make_interceptor_callback_def(
        cg_context, function_name, arg_decls, arg_names, None,
        "SameOriginProperty_IndexedPropertySetter")
    body = func_def.body

    body.append(
        TextNode("""\
bindings::V8SetReturnValue(${info}, nullptr);
if (${info}.ShouldThrowOnError()) {
  ExceptionState exception_state(${info}.GetIsolate(),
                                 ExceptionState::kIndexedSetterContext,
                                 "${interface.identifier}");
  exception_state.ThrowTypeError(
      "Indexed property setter is not supported.");
}
"""))

    return func_def


def make_same_origin_indexed_deleter_callback(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    arg_decls = [
        "uint32_t index",
        "const v8::PropertyCallbackInfo<v8::Boolean>& info",
    ]
    arg_names = ["index", "info"]

    func_def = _make_interceptor_callback_def(
        cg_context, function_name, arg_decls, arg_names, None,
        "SameOriginProperty_IndexedPropertyDeleter")
    body = func_def.body

    body.append(
        TextNode("""\
// 7.4.9 [[Delete]] ( P )
// https://html.spec.whatwg.org/C/#windowproxy-delete
const bool is_supported = ${index} < ${blink_receiver}->length();
bindings::V8SetReturnValue(${info}, !is_supported);
if (is_supported and ${info}.ShouldThrowOnError()) {
  ExceptionState exception_state(${info}.GetIsolate(),
                                 ExceptionState::kIndexedDeletionContext,
                                 "${interface.identifier}");
  exception_state.ThrowTypeError("Index property deleter is not supported.");
}
"""))

    return func_def


def make_same_origin_indexed_definer_callback(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    arg_decls = [
        "uint32_t index",
        "const v8::PropertyDescriptor& v8_property_desc",
        "const v8::PropertyCallbackInfo<v8::Value>& info",
    ]
    arg_names = ["index", "v8_property_desc", "info"]

    func_def = _make_interceptor_callback_def(
        cg_context, function_name, arg_decls, arg_names, None,
        "SameOriginProperty_IndexedPropertyDefiner")
    body = func_def.body

    body.append(
        TextNode("""\
// 7.4.6 [[DefineOwnProperty]] ( P, Desc )
// https://html.spec.whatwg.org/C/#windowproxy-defineownproperty
bindings::V8SetReturnValue(${info}, nullptr);
if (${info}.ShouldThrowOnError()) {
  ExceptionState exception_state(${info}.GetIsolate(),
                                 ExceptionState::kIndexedSetterContext,
                                 "${interface.identifier}");
  exception_state.ThrowTypeError("Index property setter is not supported.");
}
"""))

    return func_def


def make_same_origin_indexed_descriptor_callback(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    arg_decls = [
        "uint32_t index",
        "const v8::PropertyCallbackInfo<v8::Value>& info",
    ]
    arg_names = ["index", "info"]

    func_def = _make_interceptor_callback_def(
        cg_context, function_name, arg_decls, arg_names, None,
        "SameOriginProperty_IndexedPropertyDescriptor")
    body = func_def.body

    body.append(
        TextNode("""\
// 7.4.5 [[GetOwnProperty]] ( P )
// https://html.spec.whatwg.org/C/#windowproxy-getownproperty
SameOriginIndexedGetterCallback(${index}, ${info});
v8::Local<v8::Value> v8_value = ${info}.GetReturnValue().Get();
if (v8_value->IsUndefined()) {
  return;  // Do not intercept.
}

v8::PropertyDescriptor desc(v8_value, /*writable=*/false);
desc.set_enumerable(true);
desc.set_configurable(true);
bindings::V8SetReturnValue(${info}, desc);
"""))

    return func_def


def make_same_origin_indexed_enumerator_callback(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    arg_decls = ["const v8::PropertyCallbackInfo<v8::Array>& info"]
    arg_names = ["info"]

    func_def = _make_interceptor_callback_def(
        cg_context, function_name, arg_decls, arg_names, None,
        "SameOriginProperty_IndexedPropertyEnumerator")
    body = func_def.body

    body.append(
        TextNode("""\
uint32_t length = ${blink_receiver}->length();
v8::Local<v8::Array> array =
    bindings::EnumerateIndexedProperties(${isolate}, length);
bindings::V8SetReturnValue(${info}, array);
"""))

    return func_def


# ----------------------------------------------------------------------------
# Installer functions
# ----------------------------------------------------------------------------

# FN = function name
FN_INSTALL_INTERFACE_TEMPLATE = name_style.func("InstallInterfaceTemplate")
FN_INSTALL_UNCONDITIONAL_PROPS = name_style.func(
    "InstallUnconditionalProperties")
FN_INSTALL_CONTEXT_INDEPENDENT_PROPS = name_style.func(
    "InstallContextIndependentProperties")
FN_INSTALL_CONTEXT_DEPENDENT_PROPS = name_style.func(
    "InstallContextDependentProperties")

# TP = trampoline name
TP_INSTALL_INTERFACE_TEMPLATE = name_style.member_var(
    "install_interface_template_func")
TP_INSTALL_UNCONDITIONAL_PROPS = name_style.member_var(
    "install_unconditional_props_func")
TP_INSTALL_CONTEXT_INDEPENDENT_PROPS = name_style.member_var(
    "install_context_independent_props_func")
TP_INSTALL_CONTEXT_DEPENDENT_PROPS = name_style.member_var(
    "install_context_dependent_props_func")


def bind_installer_local_vars(code_node, cg_context):
    assert isinstance(code_node, SymbolScopeNode)
    assert isinstance(cg_context, CodeGenContext)

    S = SymbolNode

    local_vars = []

    local_vars.extend([
        S("instance_template",
          ("v8::Local<v8::ObjectTemplate> ${instance_template} = "
           "${interface_template}->InstanceTemplate();")),
        S("interface_template",
          ("v8::Local<v8::FunctionTemplate> ${interface_template} = "
           "${wrapper_type_info}->DomTemplate(${isolate}, ${world});")),
        S("is_in_secure_context",
          ("const bool ${is_in_secure_context} = "
           "${execution_context}->IsSecureContext();")),
        S("isolate", "v8::Isolate* ${isolate} = ${v8_context}->GetIsolate();"),
        S("prototype_template",
          ("v8::Local<v8::ObjectTemplate> ${prototype_template} = "
           "${interface_template}->PrototypeTemplate();")),
        S("script_state",
          "ScriptState* ${script_state} = ScriptState::From(${v8_context});"),
        S("signature",
          ("v8::Local<v8::Signature> ${signature} = "
           "v8::Signature::New(${isolate}, ${interface_template});")),
        S("wrapper_type_info",
          ("const WrapperTypeInfo* const ${wrapper_type_info} = "
           "${class_name}::GetWrapperTypeInfo();")),
    ])

    # context_feature_settings
    node = S("context_feature_settings",
             ("const ContextFeatureSettings* ${context_feature_settings} = "
              "ContextFeatureSettings::From("
              "${execution_context}, "
              "ContextFeatureSettings::CreationMode::kDontCreateIfNotExists"
              ");"))
    node.accumulate(
        CodeGenAccumulator.require_include_headers([
            "third_party/blink/renderer/core/context_features/context_feature_settings.h"
        ]))
    local_vars.append(node)

    # execution_context
    node = S("execution_context", ("ExecutionContext* ${execution_context} = "
                                   "ExecutionContext::From(${script_state});"))
    node.accumulate(
        CodeGenAccumulator.require_include_headers([
            "third_party/blink/renderer/core/execution_context/execution_context.h"
        ]))
    local_vars.append(node)

    # parent_interface_template
    pattern = (
        "v8::Local<v8::FunctionTemplate> ${parent_interface_template}{_1};")
    interface = cg_context.interface
    if not interface:
        _1 = ""
    elif (interface and "Global" in interface.extended_attributes
          and interface.indexed_and_named_properties
          and interface.indexed_and_named_properties.has_named_properties):
        # https://heycam.github.io/webidl/#named-properties-object
        _1 = " = ${npo_interface_template}"  # npo = named properties object
    elif interface.inherited:
        _1 = (" = ${wrapper_type_info}->parent_class->dom_template_function"
              "(${isolate}, ${world})")
    else:
        _1 = ""
    local_vars.append(S("parent_interface_template", _format(pattern, _1=_1)))

    # npo_interface_template
    # npo = named properties object
    text = """\
// Named properties object
v8::Local<v8::FunctionTemplate> ${npo_interface_template} =
    v8::FunctionTemplate::New(${isolate});
v8::Local<v8::ObjectTemplate> ${npo_prototype_template} =
    ${npo_interface_template}->PrototypeTemplate();
${npo_interface_template}->Inherit(
    ${wrapper_type_info}->parent_class->dom_template_function(
        ${isolate}, ${world}));
${npo_prototype_template}->SetImmutableProto();
V8DOMConfiguration::SetClassString(
    ${isolate}, ${npo_prototype_template},
    "${interface.identifier}Properties");
// Make the named properties object look like the global object.  Note that
// the named properties object is _not_ a prototype object, plus, we'd like
// the named properties object to behave just like the global object (= the
// wrapper object of the global object) from the point of view of named
// properties.
// https://heycam.github.io/webidl/#named-properties-object
${npo_prototype_template}->SetInternalFieldCount(
    kV8DefaultWrapperInternalFieldCount);
"""
    local_vars.append(S("npo_interface_template", text))
    local_vars.append(
        S("npo_prototype_template",
          "<% npo_interface_template.request_symbol_definition() %>"))

    # Arguments have priority over local vars.
    template_vars = code_node.template_vars
    for symbol_node in local_vars:
        if symbol_node.name not in template_vars:
            code_node.register_code_symbol(symbol_node)


def _make_property_entry_cached_accessor(property_):
    value = property_.extended_attributes.value_of("CachedAccessor")
    return "V8PrivateProperty::CachedAccessor::{}".format(value or "kNone")


def _make_property_entry_check_cross_origin_access(property_,
                                                   is_get=False,
                                                   is_set=False):
    constants = {
        True: "V8DOMConfiguration::kDoNotCheckAccess",
        False: "V8DOMConfiguration::kCheckAccess",
    }
    if "CrossOrigin" not in property_.extended_attributes:
        return constants[False]
    values = property_.extended_attributes.values_of("CrossOrigin")
    if is_get:
        return constants[not values or "Getter" in values]
    elif is_set:
        return constants["Setter" in values]
    else:
        return constants[True]


def _make_property_entry_check_receiver(property_):
    if ("LegacyLenientThis" in property_.extended_attributes
            or (isinstance(property_, web_idl.Attribute)
                and property_.idl_type.unwrap().is_promise)
            or (isinstance(property_, web_idl.OverloadGroup)
                and property_[0].return_type.unwrap().is_promise)):
        return "V8DOMConfiguration::kDoNotCheckHolder"
    else:
        return "V8DOMConfiguration::kCheckHolder"


def _make_property_entry_constant_type_and_value_format(property_):
    idl_type = property_.idl_type.unwrap()
    if (idl_type.keyword_typename == "long long"
            or idl_type.keyword_typename == "unsigned long long"):
        assert False, "64-bit constants are not yet supported."
    if idl_type.keyword_typename == "unsigned long":
        return ("V8DOMConfiguration::kConstantTypeUnsignedLong",
                "static_cast<int>({value})")
    if idl_type.is_integer:
        return ("V8DOMConfiguration::kConstantTypeLong",
                "static_cast<int>({value})")
    if idl_type.is_floating_point_numeric:
        return ("V8DOMConfiguration::kConstantTypeDouble",
                "static_cast<double>({value})")
    assert False, "Unsupported type: {}".format(idl_type.syntactic_form)


def _make_property_entry_has_side_effect(property_):
    if property_.extended_attributes.value_of("Affects") == "Nothing":
        return "V8DOMConfiguration::kHasNoSideEffect"
    else:
        return "V8DOMConfiguration::kHasSideEffect"


def _make_property_entry_on_which_object(property_):
    ON_INSTANCE = "V8DOMConfiguration::kOnInstance"
    ON_PROTOTYPE = "V8DOMConfiguration::kOnPrototype"
    ON_INTERFACE = "V8DOMConfiguration::kOnInterface"
    if isinstance(property_, web_idl.Constant):
        return ON_INTERFACE
    if hasattr(property_, "is_static") and property_.is_static:
        return ON_INTERFACE
    if "Global" in property_.owner.extended_attributes:
        return ON_INSTANCE
    if "LegacyUnforgeable" in property_.extended_attributes:
        return ON_INSTANCE
    return ON_PROTOTYPE


def _make_property_entry_v8_c_function(entry):
    if entry.no_alloc_direct_callback_name is None:
        return None
    return "v8::CFunction::Make({})".format(
        entry.no_alloc_direct_callback_name)


def _make_property_entry_v8_property_attribute(property_):
    values = []
    if "NotEnumerable" in property_.extended_attributes:
        values.append("v8::DontEnum")
    if "LegacyUnforgeable" in property_.extended_attributes:
        if not isinstance(property_, web_idl.Attribute):
            values.append("v8::ReadOnly")
        values.append("v8::DontDelete")
    if not values:
        values.append("v8::None")
    if len(values) == 1:
        return values[0]
    else:
        return "static_cast<v8::PropertyAttribute>({})".format(
            " | ".join(values))


def _make_property_entry_world(world):
    if world == CodeGenContext.MAIN_WORLD:
        return "V8DOMConfiguration::kMainWorld"
    if world == CodeGenContext.NON_MAIN_WORLDS:
        return "V8DOMConfiguration::kNonMainWorlds"
    if world == CodeGenContext.ALL_WORLDS:
        return "V8DOMConfiguration::kAllWorlds"
    assert False


def _make_attribute_registration_table(table_name, attribute_entries):
    assert isinstance(table_name, str)
    assert isinstance(attribute_entries, (list, tuple))
    assert all(
        isinstance(entry, _PropEntryAttribute) for entry in attribute_entries)

    T = TextNode

    entry_nodes = []
    for entry in attribute_entries:
        pattern = ("{{"
                   "\"{property_name}\", "
                   "{attribute_get_callback}, "
                   "{attribute_set_callback}, "
                   "static_cast<unsigned>({cached_accessor}), "
                   "{v8_property_attribute}, "
                   "{on_which_object}, "
                   "{check_receiver}, "
                   "{check_cross_origin_get_access}, "
                   "{check_cross_origin_set_access}, "
                   "{has_side_effect}, "
                   "{world}"
                   "}},")
        text = _format(
            pattern,
            property_name=entry.property_.identifier,
            attribute_get_callback=entry.attr_get_callback_name,
            attribute_set_callback=(entry.attr_set_callback_name or "nullptr"),
            cached_accessor=_make_property_entry_cached_accessor(
                entry.property_),
            v8_property_attribute=_make_property_entry_v8_property_attribute(
                entry.property_),
            on_which_object=_make_property_entry_on_which_object(
                entry.property_),
            check_receiver=_make_property_entry_check_receiver(
                entry.property_),
            check_cross_origin_get_access=(
                _make_property_entry_check_cross_origin_access(entry.property_,
                                                               is_get=True)),
            check_cross_origin_set_access=(
                _make_property_entry_check_cross_origin_access(entry.property_,
                                                               is_set=True)),
            has_side_effect=_make_property_entry_has_side_effect(
                entry.property_),
            world=_make_property_entry_world(entry.world))
        entry_nodes.append(T(text))

    return ListNode([
        T("static constexpr const V8DOMConfiguration::AccessorConfiguration " +
          table_name + "[] = {"),
        ListNode(entry_nodes),
        T("};"),
    ])


def _make_constant_callback_registration_table(table_name, constant_entries):
    assert isinstance(table_name, str)
    assert isinstance(constant_entries, (list, tuple))
    assert all(
        isinstance(entry, _PropEntryConstant)
        and isinstance(entry.const_callback_name, str)
        for entry in constant_entries)

    T = TextNode

    entry_nodes = []
    for entry in constant_entries:
        pattern = ("{{" "\"{property_name}\", " "{constant_callback}" "}},")
        text = _format(
            pattern,
            property_name=entry.property_.identifier,
            constant_callback=entry.const_callback_name)
        entry_nodes.append(T(text))

    return ListNode([
        T("static constexpr const "
          "V8DOMConfiguration::ConstantCallbackConfiguration " + table_name +
          "[] = {"),
        ListNode(entry_nodes),
        T("};"),
    ])


def _make_constant_value_registration_table(table_name, constant_entries):
    assert isinstance(table_name, str)
    assert isinstance(constant_entries, (list, tuple))
    assert all(
        isinstance(entry, _PropEntryConstant)
        and entry.const_callback_name is None for entry in constant_entries)

    T = TextNode

    entry_nodes = []
    for entry in constant_entries:
        pattern = ("{{"
                   "\"{property_name}\", "
                   "{constant_type}, "
                   "{constant_value}"
                   "}},")
        constant_type, constant_value_fmt = (
            _make_property_entry_constant_type_and_value_format(
                entry.property_))
        constant_value = _format(
            constant_value_fmt, value=entry.const_constant_name)
        text = _format(
            pattern,
            property_name=entry.property_.identifier,
            constant_type=constant_type,
            constant_value=constant_value)
        entry_nodes.append(T(text))

    return ListNode([
        T("static constexpr const V8DOMConfiguration::ConstantConfiguration " +
          table_name + "[] = {"),
        ListNode(entry_nodes),
        T("};"),
    ])


def _make_exposed_construct_registration_table(table_name,
                                               exposed_construct_entries):
    assert isinstance(table_name, str)
    assert isinstance(exposed_construct_entries, (list, tuple))
    assert all(
        isinstance(entry, _PropEntryExposedConstruct)
        for entry in exposed_construct_entries)

    T = TextNode

    entry_nodes = []
    for entry in exposed_construct_entries:
        pattern = ("{{"
                   "\"{property_name}\", "
                   "{exposed_construct_callback}, "
                   "nullptr, "
                   "static_cast<v8::PropertyAttribute>(v8::DontEnum), "
                   "V8DOMConfiguration::kOnInstance, "
                   "V8DOMConfiguration::kDoNotCheckHolder, "
                   "V8DOMConfiguration::kHasNoSideEffect, "
                   "V8DOMConfiguration::kAlwaysCallGetter, "
                   "{world}"
                   "}}, ")
        text = _format(
            pattern,
            property_name=entry.property_.identifier,
            exposed_construct_callback=entry.prop_callback_name,
            world=_make_property_entry_world(entry.world))
        entry_nodes.append(T(text))

    return ListNode([
        T("static constexpr const V8DOMConfiguration::AttributeConfiguration "
          + table_name + "[] = {"),
        ListNode(entry_nodes),
        T("};"),
    ])


def _make_operation_registration_table(table_name, operation_entries):
    assert isinstance(table_name, str)
    assert isinstance(operation_entries, (list, tuple))
    assert all(
        isinstance(entry, _PropEntryOperationGroup)
        for entry in operation_entries)

    T = TextNode

    no_alloc_direct_call_count = 0
    for entry in operation_entries:
        if entry.no_alloc_direct_callback_name:
            no_alloc_direct_call_count += 1
    assert (no_alloc_direct_call_count == 0
            or no_alloc_direct_call_count == len(operation_entries))
    no_alloc_direct_call_enabled = no_alloc_direct_call_count > 0

    entry_nodes = []
    for entry in operation_entries:
        pattern = ("{{"
                   "\"{property_name}\", "
                   "{operation_callback}, "
                   "{function_length}, "
                   "{v8_property_attribute}, "
                   "{on_which_object}, "
                   "{check_receiver}, "
                   "{check_cross_origin_access}, "
                   "{has_side_effect}, "
                   "{world}"
                   "}}, ")
        if no_alloc_direct_call_enabled:
            pattern = "{{" + pattern + "{v8_c_function}}}, "
        text = _format(
            pattern,
            property_name=entry.property_.identifier,
            operation_callback=entry.op_callback_name,
            function_length=entry.op_func_length,
            v8_property_attribute=_make_property_entry_v8_property_attribute(
                entry.property_),
            on_which_object=_make_property_entry_on_which_object(
                entry.property_),
            check_receiver=_make_property_entry_check_receiver(
                entry.property_),
            check_cross_origin_access=(
                _make_property_entry_check_cross_origin_access(
                    entry.property_)),
            has_side_effect=_make_property_entry_has_side_effect(
                entry.property_),
            world=_make_property_entry_world(entry.world),
            v8_c_function=_make_property_entry_v8_c_function(entry))
        entry_nodes.append(T(text))

    table_decl_before_name = (
        "static constexpr const V8DOMConfiguration::MethodConfiguration")
    if no_alloc_direct_call_enabled:
        table_decl_before_name = ("static const V8DOMConfiguration::"
                                  "NoAllocDirectCallMethodConfiguration")
    return ListNode([
        T(table_decl_before_name + " " + table_name + "[] = {"),
        ListNode(entry_nodes),
        T("};"),
    ])


class _PropEntryBase(object):
    def __init__(self, is_context_dependent, exposure_conditional, world,
                 property_):
        assert isinstance(is_context_dependent, bool)
        assert isinstance(exposure_conditional, CodeGenExpr)

        self.is_context_dependent = is_context_dependent
        self.exposure_conditional = exposure_conditional
        self.world = world
        self.property_ = property_


class _PropEntryAttribute(_PropEntryBase):
    def __init__(self, is_context_dependent, exposure_conditional, world,
                 attribute, attr_get_callback_name, attr_set_callback_name):
        assert isinstance(attr_get_callback_name, str)
        assert _is_none_or_str(attr_set_callback_name)

        _PropEntryBase.__init__(self, is_context_dependent,
                                exposure_conditional, world, attribute)
        self.attr_get_callback_name = attr_get_callback_name
        self.attr_set_callback_name = attr_set_callback_name


class _PropEntryConstant(_PropEntryBase):
    def __init__(self, is_context_dependent, exposure_conditional, world,
                 constant, const_callback_name, const_constant_name):
        assert _is_none_or_str(const_callback_name)
        assert isinstance(const_constant_name, str)

        _PropEntryBase.__init__(self, is_context_dependent,
                                exposure_conditional, world, constant)
        self.const_callback_name = const_callback_name
        self.const_constant_name = const_constant_name


class _PropEntryConstructorGroup(_PropEntryBase):
    def __init__(self, is_context_dependent, exposure_conditional, world,
                 constructor_group, ctor_callback_name, ctor_func_length):
        assert isinstance(ctor_callback_name, str)
        assert isinstance(ctor_func_length, (int, long))

        _PropEntryBase.__init__(self, is_context_dependent,
                                exposure_conditional, world, constructor_group)
        self.ctor_callback_name = ctor_callback_name
        self.ctor_func_length = ctor_func_length


class _PropEntryExposedConstruct(_PropEntryBase):
    def __init__(self, is_context_dependent, exposure_conditional, world,
                 exposed_construct, prop_callback_name):
        assert isinstance(prop_callback_name, str)

        _PropEntryBase.__init__(self, is_context_dependent,
                                exposure_conditional, world, exposed_construct)
        self.prop_callback_name = prop_callback_name


class _PropEntryOperationGroup(_PropEntryBase):
    def __init__(self,
                 is_context_dependent,
                 exposure_conditional,
                 world,
                 operation_group,
                 op_callback_name,
                 op_func_length,
                 no_alloc_direct_callback_name=None):
        assert isinstance(op_callback_name, str)
        assert isinstance(op_func_length, (int, long))

        _PropEntryBase.__init__(self, is_context_dependent,
                                exposure_conditional, world, operation_group)
        self.op_callback_name = op_callback_name
        self.op_func_length = op_func_length
        self.no_alloc_direct_callback_name = no_alloc_direct_callback_name


def _make_property_entries_and_callback_defs(
        cg_context, attribute_entries, constant_entries, constructor_entries,
        exposed_construct_entries, operation_entries):
    """
    Creates intermediate objects to help property installation and also makes
    code nodes of callback functions.

    Args:
        attribute_entries:
        constructor_entries:
        exposed_construct_entries:
        operation_entries:
            Output parameters to store the intermediate objects.
    """
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(attribute_entries, list)
    assert isinstance(constant_entries, list)
    assert isinstance(constructor_entries, list)
    assert isinstance(exposed_construct_entries, list)
    assert isinstance(operation_entries, list)

    interface = cg_context.interface
    global_names = interface.extended_attributes.values_of("Global")

    callback_def_nodes = ListNode()

    def iterate(members, callback):
        for member in members:
            is_context_dependent = member.exposure.is_context_dependent(
                global_names)
            exposure_conditional = expr_from_exposure(
                member.exposure,
                global_names=global_names,
                may_use_feature_selector=True)

            if "PerWorldBindings" in member.extended_attributes:
                worlds = (CodeGenContext.MAIN_WORLD,
                          CodeGenContext.NON_MAIN_WORLDS)
            else:
                worlds = (CodeGenContext.ALL_WORLDS, )

            for world in worlds:
                callback(member, is_context_dependent, exposure_conditional,
                         world)

    def process_attribute(attribute, is_context_dependent,
                          exposure_conditional, world):
        if "CSSProperty" in attribute.extended_attributes:
            return  # [CSSProperty] will be installed in a special manner.

        cgc_attr = cg_context.make_copy(attribute=attribute, for_world=world)
        cgc = cgc_attr.make_copy(attribute_get=True)
        attr_get_callback_name = callback_function_name(cgc)
        attr_get_callback_node = make_attribute_get_callback_def(
            cgc, attr_get_callback_name)
        cgc = cgc_attr.make_copy(attribute_set=True)
        attr_set_callback_name = callback_function_name(cgc)
        attr_set_callback_node = make_attribute_set_callback_def(
            cgc, attr_set_callback_name)
        if attr_set_callback_node is None:
            attr_set_callback_name = None

        callback_def_nodes.extend([
            attr_get_callback_node,
            EmptyNode(),
            attr_set_callback_node,
            EmptyNode(),
        ])

        attribute_entries.append(
            _PropEntryAttribute(
                is_context_dependent=is_context_dependent,
                exposure_conditional=exposure_conditional,
                world=world,
                attribute=attribute,
                attr_get_callback_name=attr_get_callback_name,
                attr_set_callback_name=attr_set_callback_name))

    def process_constant(constant, is_context_dependent, exposure_conditional,
                         world):
        cgc = cg_context.make_copy(
            constant=constant,
            for_world=world,
            v8_callback_type=CodeGenContext.V8_ACCESSOR_NAME_GETTER_CALLBACK)
        const_callback_name = callback_function_name(cgc)
        const_callback_node = make_constant_callback_def(
            cgc, const_callback_name)
        if const_callback_node is None:
            const_callback_name = None
        # IDL constant's C++ constant name
        const_constant_name = _format("${class_name}::Constant::{}",
                                      constant_name(cgc))

        callback_def_nodes.extend([
            const_callback_node,
            EmptyNode(),
        ])

        constant_entries.append(
            _PropEntryConstant(
                is_context_dependent=is_context_dependent,
                exposure_conditional=exposure_conditional,
                world=world,
                constant=constant,
                const_callback_name=const_callback_name,
                const_constant_name=const_constant_name))

    def process_constructor_group(constructor_group, is_context_dependent,
                                  exposure_conditional, world):
        cgc = cg_context.make_copy(
            constructor_group=constructor_group, for_world=world)
        ctor_callback_name = callback_function_name(cgc)
        ctor_callback_node = make_constructor_callback_def(
            cgc, ctor_callback_name)

        callback_def_nodes.extend([
            ctor_callback_node,
            EmptyNode(),
        ])

        constructor_entries.append(
            _PropEntryConstructorGroup(
                is_context_dependent=is_context_dependent,
                exposure_conditional=exposure_conditional,
                world=world,
                constructor_group=constructor_group,
                ctor_callback_name=ctor_callback_name,
                ctor_func_length=(
                    constructor_group.min_num_of_required_arguments)))

    def process_exposed_construct(exposed_construct, is_context_dependent,
                                  exposure_conditional, world):
        if isinstance(exposed_construct, web_idl.LegacyWindowAlias):
            cgc = cg_context.make_copy(
                exposed_construct=exposed_construct.original,
                legacy_window_alias=exposed_construct,
                for_world=world,
                v8_callback_type=CodeGenContext.
                V8_ACCESSOR_NAME_GETTER_CALLBACK)
        elif ("LegacyNoInterfaceObject" in
              exposed_construct.extended_attributes):
            return  # Skip due to [LegacyNoInterfaceObject].
        else:
            cgc = cg_context.make_copy(
                exposed_construct=exposed_construct,
                for_world=world,
                v8_callback_type=CodeGenContext.
                V8_ACCESSOR_NAME_GETTER_CALLBACK)
        prop_callback_name = callback_function_name(cgc)
        prop_callback_node = make_exposed_construct_callback_def(
            cgc, prop_callback_name)

        callback_def_nodes.extend([
            prop_callback_node,
            EmptyNode(),
        ])

        exposed_construct_entries.append(
            _PropEntryExposedConstruct(
                is_context_dependent=is_context_dependent,
                exposure_conditional=exposure_conditional,
                world=world,
                exposed_construct=exposed_construct,
                prop_callback_name=prop_callback_name))

    def process_named_constructor_group(named_constructor_group,
                                        is_context_dependent,
                                        exposure_conditional, world):
        cgc = cg_context.make_copy(
            exposed_construct=named_constructor_group,
            is_named_constructor=True,
            for_world=world,
            v8_callback_type=CodeGenContext.V8_ACCESSOR_NAME_GETTER_CALLBACK)
        prop_callback_name = callback_function_name(cgc)
        prop_callback_node = make_named_constructor_property_callback_def(
            cgc, prop_callback_name)

        callback_def_nodes.extend([
            prop_callback_node,
            EmptyNode(),
        ])

        exposed_construct_entries.append(
            _PropEntryExposedConstruct(
                is_context_dependent=is_context_dependent,
                exposure_conditional=exposure_conditional,
                world=world,
                exposed_construct=named_constructor_group,
                prop_callback_name=prop_callback_name))

    def process_operation_group(operation_group, is_context_dependent,
                                exposure_conditional, world):
        cgc = cg_context.make_copy(
            operation_group=operation_group, for_world=world)
        op_callback_name = callback_function_name(cgc)
        no_alloc_direct_callback_name = (
            callback_function_name(cgc, no_alloc_direct_call=True)
            if "NoAllocDirectCall" in operation_group.extended_attributes else
            None)
        op_callback_node = make_operation_callback_def(
            cgc,
            op_callback_name,
            no_alloc_direct_callback_name=no_alloc_direct_callback_name)

        callback_def_nodes.extend([
            op_callback_node,
            EmptyNode(),
        ])

        operation_entries.append(
            _PropEntryOperationGroup(
                is_context_dependent=is_context_dependent,
                exposure_conditional=exposure_conditional,
                world=world,
                operation_group=operation_group,
                op_callback_name=op_callback_name,
                op_func_length=operation_group.min_num_of_required_arguments,
                no_alloc_direct_callback_name=no_alloc_direct_callback_name))

    def process_stringifier(_, is_context_dependent, exposure_conditional,
                            world):
        cgc = cg_context.make_copy(
            stringifier=interface.stringifier, for_world=world)
        op_callback_name = callback_function_name(cgc)
        op_callback_node = make_stringifier_callback_def(cgc, op_callback_name)

        callback_def_nodes.extend([
            op_callback_node,
            EmptyNode(),
        ])

        operation_entries.append(
            _PropEntryOperationGroup(
                is_context_dependent=is_context_dependent,
                exposure_conditional=exposure_conditional,
                world=world,
                operation_group=cgc.property_,
                op_callback_name=op_callback_name,
                op_func_length=0))

    iterate(interface.attributes, process_attribute)
    iterate(interface.constants, process_constant)
    iterate(interface.constructor_groups, process_constructor_group)
    iterate(interface.exposed_constructs, process_exposed_construct)
    iterate(interface.legacy_window_aliases, process_exposed_construct)
    named_constructor_groups = [
        group for construct in interface.exposed_constructs
        for group in construct.named_constructor_groups
        if construct.named_constructor_groups
    ]
    iterate(named_constructor_groups, process_named_constructor_group)
    iterate(interface.operation_groups, process_operation_group)
    if interface.stringifier:
        iterate([interface.stringifier.operation], process_stringifier)
    collectionlike = (interface.iterable or interface.maplike
                      or interface.setlike)
    if collectionlike:

        def should_define(target):
            if not target[0].is_optionally_defined:
                return True
            return all(target.identifier != member.identifier
                       for member in itertools.chain(
                           interface.attributes, interface.constants,
                           interface.operation_groups))

        iterate(collectionlike.attributes, process_attribute)
        iterate(
            filter(should_define, collectionlike.operation_groups),
            process_operation_group)

    return callback_def_nodes


def _make_install_prototype_object(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    nodes = []

    class_like = cg_context.class_like
    interface = cg_context.interface

    unscopables = []
    is_unscopable = lambda member: "Unscopable" in member.extended_attributes
    unscopables.extend(filter(is_unscopable, class_like.attributes))
    unscopables.extend(filter(is_unscopable, class_like.operations))
    if unscopables:
        nodes.extend([
            TextNode("""\
// [Unscopable]
// 3.7.3. Interface prototype object
// https://heycam.github.io/webidl/#interface-prototype-object
// step 10. If interface has any member declared with the [Unscopable]
//   extended attribute, then:\
"""),
            ListNode([
                TextNode("static constexpr const char* "
                         "kUnscopablePropertyNames[] = {"),
                ListNode([
                    TextNode("\"{}\", ".format(member.identifier))
                    for member in unscopables
                ]),
                TextNode("};"),
            ]),
            TextNode("""\
bindings::InstallUnscopablePropertyNames(
    ${isolate}, ${v8_context}, ${prototype_object}, kUnscopablePropertyNames);
"""),
        ])

    if "LegacyNoInterfaceObject" in class_like.extended_attributes:
        nodes.append(
            TextNode("""\
// [LegacyNoInterfaceObject]
// 3.7.3. Interface prototype object
// https://heycam.github.io/webidl/#interface-prototype-object
// step 13. If the [LegacyNoInterfaceObject] extended attribute was not
//   specified on interface, then:
//
// V8 defines "constructor" property on the prototype object by default.
${prototype_object}->Delete(
    ${v8_context}, V8AtomicString(${isolate}, "constructor")).ToChecked();
"""))

    collectionlike = interface and (interface.iterable or interface.maplike
                                    or interface.setlike)
    if collectionlike:
        property_name = None
        for operation_group in collectionlike.operation_groups:
            if operation_group[0].is_iterator:
                property_name = operation_group.identifier
                break
        if property_name:
            pattern = """\
// @@iterator == "{property_name}"
{{
  v8::Local<v8::Value> v8_value = ${prototype_object}->Get(
      ${v8_context}, V8AtomicString(${isolate}, "{property_name}"))
      .ToLocalChecked();
  ${prototype_object}->DefineOwnProperty(
      ${v8_context}, v8::Symbol::GetIterator(${isolate}), v8_value,
      v8::DontEnum).ToChecked();
}}
"""
            nodes.append(
                TextNode(_format(pattern, property_name=property_name)))

    if class_like.identifier == "FileSystemDirectoryHandle":
        pattern = """\
// Temporary @@asyncIterator support for FileSystemDirectoryHandle
// TODO(https://crbug.com/1087157): Replace with proper bindings support.
// @@asyncIterator == "{property_name}"
{{
  v8::Local<v8::Value> v8_value = ${prototype_object}->Get(
      ${v8_context}, V8AtomicString(${isolate}, "{property_name}"))
      .ToLocalChecked();
  ${prototype_object}->DefineOwnProperty(
      ${v8_context}, v8::Symbol::GetAsyncIterator(${isolate}), v8_value,
      v8::DontEnum).ToChecked();
}}
"""
        nodes.append(TextNode(_format(pattern, property_name="entries")))

    if ("Global" in class_like.extended_attributes
            and class_like.indexed_and_named_properties
            and class_like.indexed_and_named_properties.has_named_properties):
        nodes.append(
            TextNode("""\
// https://heycam.github.io/webidl/#named-properties-object
// V8 defines "constructor" property on the prototype object by default.
// Named properties object is currently implemented as a prototype object
// (implemented with v8::FunctionTemplate::PrototypeTemplate()).
${prototype_object}->GetPrototype().As<v8::Object>()->Delete(
    ${v8_context}, V8AtomicString(${isolate}, "constructor")).ToChecked();
"""))

    return SequenceNode(nodes) if nodes else None


def make_install_interface_template(cg_context, function_name, class_name,
                                    trampoline_var_name, constructor_entries,
                                    supplemental_install_node,
                                    install_unconditional_func_name,
                                    install_context_independent_func_name):
    """
    Returns:
        A triplet of CodeNode of:
        - function declaration
        - function definition
        - trampoline function definition (from the API class to the
          implementation class), which is supposed to be defined inline
    """
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)
    assert _is_none_or_str(class_name)
    assert _is_none_or_str(trampoline_var_name)
    assert isinstance(constructor_entries, (list, tuple))
    assert all(
        isinstance(entry, _PropEntryConstructorGroup)
        for entry in constructor_entries)
    assert isinstance(supplemental_install_node, SequenceNode)
    assert _is_none_or_str(install_unconditional_func_name)
    assert _is_none_or_str(install_context_independent_func_name)

    T = TextNode

    class_like = cg_context.class_like
    interface = cg_context.interface

    arg_decls = [
        "v8::Isolate* isolate",
        "const DOMWrapperWorld& world",
        "v8::Local<v8::FunctionTemplate> interface_template",
    ]
    return_type = "void"

    if trampoline_var_name is None:
        trampoline_def = None
    else:
        trampoline_def = CxxFuncDefNode(
            name=function_name,
            arg_decls=arg_decls,
            return_type=return_type,
            static=True)
        trampoline_def.body.append(
            TextNode(
                _format("return {}(isolate, world, interface_template);",
                        trampoline_var_name)))

    func_decl = CxxFuncDeclNode(
        name=function_name,
        arg_decls=arg_decls,
        return_type=return_type,
        static=True)

    func_def = CxxFuncDefNode(
        name=function_name,
        arg_decls=arg_decls,
        return_type=return_type,
        class_name=class_name)
    func_def.set_base_template_vars(cg_context.template_bindings())

    body = func_def.body
    body.add_template_vars({
        "isolate": "isolate",
        "world": "world",
        "interface_template": "interface_template",
    })
    bind_installer_local_vars(body, cg_context)

    body.extend([
        T("V8DOMConfiguration::InitializeDOMInterfaceTemplate("
          "${isolate}, ${interface_template}, "
          "${wrapper_type_info}->interface_name, ${parent_interface_template}, "
          "kV8DefaultWrapperInternalFieldCount);"),
        EmptyNode(),
    ])

    for entry in constructor_entries:
        set_callback = _format("${interface_template}->SetCallHandler({});",
                               entry.ctor_callback_name)
        set_length = _format("${interface_template}->SetLength({});",
                             entry.ctor_func_length)
        if entry.world == CodeGenContext.MAIN_WORLD:
            body.append(
                CxxLikelyIfNode(
                    cond="${world}.IsMainWorld()",
                    body=[T(set_callback), T(set_length)]))
        elif entry.world == CodeGenContext.NON_MAIN_WORLDS:
            body.append(
                CxxLikelyIfNode(
                    cond="!${world}.IsMainWorld()",
                    body=[T(set_callback), T(set_length)]))
        elif entry.world == CodeGenContext.ALL_WORLDS:
            body.extend([T(set_callback), T(set_length)])
        else:
            assert False
        body.append(EmptyNode())

    body.extend([
        supplemental_install_node,
        EmptyNode(),
    ])

    if class_like.identifier == "CSSStyleDeclaration":
        css_properties = filter(
            lambda attr: "CSSProperty" in attr.extended_attributes,
            class_like.attributes)
        if css_properties:
            prop_name_list = "".join(
                map(lambda attr: "\"{}\", ".format(attr.identifier),
                    css_properties))
            body.append(
                T("""\
// CSSStyleDeclaration-specific settings
// [CSSProperty]
{
  static constexpr const char* kCssProperties[] = {
""" + prop_name_list + """
  };
  bindings::InstallCSSPropertyAttributes(
      ${isolate}, ${world},
      ${instance_template}, ${prototype_template}, ${interface_template},
      ${signature}, kCssProperties);
}
"""))

    if class_like.identifier == "DOMException":
        body.append(
            T("""\
// DOMException-specific settings
// https://heycam.github.io/webidl/#es-DOMException-specialness
{
  v8::Local<v8::FunctionTemplate> intrinsic_error_prototype_interface_template =
      v8::FunctionTemplate::New(${isolate});
  intrinsic_error_prototype_interface_template->RemovePrototype();
  intrinsic_error_prototype_interface_template->SetIntrinsicDataProperty(
      V8AtomicString(${isolate}, "prototype"), v8::kErrorPrototype);
  ${interface_template}->Inherit(intrinsic_error_prototype_interface_template);
}
"""))

    if class_like.identifier == "NativeFileSystemDirectoryIterator":
        body.append(
            T("""\
// Temporary @@asyncIterator support for FileSystemDirectoryHandle
// TODO(https://crbug.com/1087157): Replace with proper bindings support.
{
  v8::Local<v8::FunctionTemplate>
      intrinsic_iterator_prototype_interface_template =
      v8::FunctionTemplate::New(${isolate});
  intrinsic_iterator_prototype_interface_template->RemovePrototype();
  intrinsic_iterator_prototype_interface_template->SetIntrinsicDataProperty(
      V8AtomicString(${isolate}, "prototype"), v8::kAsyncIteratorPrototype);
  ${interface_template}->Inherit(
      intrinsic_iterator_prototype_interface_template);
}
"""))

    if class_like.identifier == "HTMLAllCollection":
        body.append(
            T("""\
// HTMLAllCollection-specific settings
// https://html.spec.whatwg.org/C/#the-htmlallcollection-interface
${instance_template}->SetCallAsFunctionHandler(
    ${class_name}::LegacyCallCustom);
${instance_template}->MarkAsUndetectable();
"""))

    if class_like.identifier == "Iterator":
        body.append(
            T("""\
// Iterator-specific settings
// https://heycam.github.io/webidl/#es-iterator-prototype-object
{
  v8::Local<v8::FunctionTemplate>
      intrinsic_iterator_prototype_interface_template =
      v8::FunctionTemplate::New(${isolate});
  intrinsic_iterator_prototype_interface_template->RemovePrototype();
  intrinsic_iterator_prototype_interface_template->SetIntrinsicDataProperty(
      V8AtomicString(${isolate}, "prototype"), v8::kIteratorPrototype);
  ${interface_template}->Inherit(
      intrinsic_iterator_prototype_interface_template);
}
"""))

    if class_like.identifier == "Location":
        body.append(
            T("""\
// Location-specific settings
// https://html.spec.whatwg.org/C/#the-location-interface
// To create a Location object, run these steps:
// step 3. Let valueOf be location's relevant
//   Realm.[[Intrinsics]].[[%ObjProto_valueOf%]].
// step 3. Perform ! location.[[DefineOwnProperty]]("valueOf",
//   { [[Value]]: valueOf, [[Writable]]: false, [[Enumerable]]: false,
//     [[Configurable]]: false }).
${instance_template}->SetIntrinsicDataProperty(
    V8AtomicString(${isolate}, "valueOf"),
    v8::kObjProto_valueOf,
    static_cast<v8::PropertyAttribute>(
        v8::ReadOnly | v8::DontEnum | v8::DontDelete));
// step 4. Perform ! location.[[DefineOwnProperty]](@@toPrimitive,
//   { [[Value]]: undefined, [[Writable]]: false, [[Enumerable]]: false,
//     [[Configurable]]: false }).
${instance_template}->Set(
    v8::Symbol::GetToPrimitive(${isolate}),
    v8::Undefined(${isolate}),
    static_cast<v8::PropertyAttribute>(
        v8::ReadOnly | v8::DontEnum | v8::DontDelete));
// 7.7.4.2 [[SetPrototypeOf]] ( V )
// https://html.spec.whatwg.org/C/#location-setprototypeof
${instance_template}->SetImmutableProto();
${prototype_template}->SetImmutableProto();
"""))

    if (interface and interface.indexed_and_named_properties
            and interface.indexed_and_named_properties.indexed_getter
            and "Global" not in interface.extended_attributes):
        body.append(
            T("""\
// @@iterator for indexed properties
// https://heycam.github.io/webidl/#define-the-iteration-methods
${prototype_template}->SetIntrinsicDataProperty(
    v8::Symbol::GetIterator(${isolate}), v8::kArrayProto_values, v8::DontEnum);
"""))
    if interface and interface.iterable and not interface.iterable.key_type:
        body.append(
            T("""\
// Value iterator's properties
// https://heycam.github.io/webidl/#define-the-iteration-methods
${prototype_template}->SetIntrinsicDataProperty(
    V8AtomicString(${isolate}, "entries"), v8::kArrayProto_entries, v8::None);
${prototype_template}->SetIntrinsicDataProperty(
    V8AtomicString(${isolate}, "keys"), v8::kArrayProto_keys, v8::None);
${prototype_template}->SetIntrinsicDataProperty(
    V8AtomicString(${isolate}, "values"), v8::kArrayProto_values, v8::None);
${prototype_template}->SetIntrinsicDataProperty(
    V8AtomicString(${isolate}, "forEach"), v8::kArrayProto_forEach, v8::None);
"""))

    if "Global" in class_like.extended_attributes:
        body.append(
            TextNode("""\
// [Global]
// 3.7.1. [[SetPrototypeOf]]
// https://heycam.github.io/webidl/#platform-object-setprototypeof
${instance_template}->SetImmutableProto();
${prototype_template}->SetImmutableProto();
"""))
    elif any("Global" in derived.extended_attributes
             for derived in class_like.deriveds):
        body.append(
            TextNode("""\
// [Global] - prototype object in the prototype chain of global objects
// 3.7.1. [[SetPrototypeOf]]
// https://heycam.github.io/webidl/#platform-object-setprototypeof
${prototype_template}->SetImmutableProto();
"""))

    func_call_pattern = ("{}(${isolate}, ${world}, ${instance_template}, "
                         "${prototype_template}, ${interface_template});")
    if install_unconditional_func_name:
        func_call = _format(func_call_pattern, install_unconditional_func_name)
        body.append(T(func_call))
    if install_context_independent_func_name:
        func_call = _format(func_call_pattern,
                            install_context_independent_func_name)
        body.append(T(func_call))

    return func_decl, func_def, trampoline_def


class PropInstallMode(object):
    class Mode(int):
        pass

    UNCONDITIONAL = Mode(0)
    CONTEXT_INDEPENDENT = Mode(1)
    CONTEXT_DEPENDENT = Mode(2)
    V8_CONTEXT_SNAPSHOT = Mode(3)


def make_install_properties(cg_context, function_name, class_name,
                            prop_install_mode, trampoline_var_name,
                            attribute_entries, constant_entries,
                            exposed_construct_entries, operation_entries):
    """
    Returns:
        A triplet of CodeNode of:
        - function declaration
        - function definition
        - trampoline function definition (from the API class to the
          implementation class), which is supposed to be defined inline
    """
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)
    assert _is_none_or_str(class_name)
    assert isinstance(prop_install_mode, PropInstallMode.Mode)
    assert _is_none_or_str(trampoline_var_name)
    assert isinstance(attribute_entries, (list, tuple))
    assert all(
        isinstance(entry, _PropEntryAttribute) for entry in attribute_entries)
    assert isinstance(constant_entries, (list, tuple))
    assert all(
        isinstance(entry, _PropEntryConstant) for entry in constant_entries)
    assert isinstance(exposed_construct_entries, (list, tuple))
    assert all(
        isinstance(entry, _PropEntryExposedConstruct)
        for entry in exposed_construct_entries)
    assert isinstance(operation_entries, (list, tuple))
    assert all(
        isinstance(entry, _PropEntryOperationGroup)
        for entry in operation_entries)

    if prop_install_mode == PropInstallMode.CONTEXT_DEPENDENT:
        install_prototype_object_node = _make_install_prototype_object(
            cg_context)
    else:
        install_prototype_object_node = None

    if not (attribute_entries or constant_entries or exposed_construct_entries
            or operation_entries or install_prototype_object_node):
        if prop_install_mode != PropInstallMode.V8_CONTEXT_SNAPSHOT:
            return None, None, None

    if prop_install_mode in (PropInstallMode.UNCONDITIONAL,
                             PropInstallMode.CONTEXT_INDEPENDENT):
        arg_decls = [
            "v8::Isolate* isolate",
            "const DOMWrapperWorld& world",
            "v8::Local<v8::ObjectTemplate> instance_template",
            "v8::Local<v8::ObjectTemplate> prototype_template",
            "v8::Local<v8::FunctionTemplate> interface_template",
        ]
        arg_names = [
            "isolate",
            "world",
            "instance_template",
            "prototype_template",
            "interface_template",
        ]
    elif prop_install_mode == PropInstallMode.CONTEXT_DEPENDENT:
        arg_decls = [
            "v8::Local<v8::Context> context",
            "const DOMWrapperWorld& world",
            "v8::Local<v8::Object> instance_object",
            "v8::Local<v8::Object> prototype_object",
            "v8::Local<v8::Function> interface_object",
            "v8::Local<v8::FunctionTemplate> interface_template",
            "FeatureSelector feature_selector",
        ]
        arg_names = [
            "context",
            "world",
            "instance_object",
            "prototype_object",
            "interface_object",
            "interface_template",
            "feature_selector",
        ]
    elif prop_install_mode == PropInstallMode.V8_CONTEXT_SNAPSHOT:
        arg_decls = [
            "v8::Local<v8::Context> context",
            "const DOMWrapperWorld& world",
            "v8::Local<v8::Object> instance_object",
            "v8::Local<v8::Object> prototype_object",
            "v8::Local<v8::Function> interface_object",
            "v8::Local<v8::FunctionTemplate> interface_template",
        ]
        arg_names = [
            "context",
            "world",
            "instance_object",
            "prototype_object",
            "interface_object",
            "interface_template",
        ]
    return_type = "void"

    is_per_context_install = (
        prop_install_mode in (PropInstallMode.CONTEXT_DEPENDENT,
                              PropInstallMode.V8_CONTEXT_SNAPSHOT))

    if trampoline_var_name is None:
        trampoline_def = None
    else:
        trampoline_def = CxxFuncDefNode(
            name=function_name,
            arg_decls=arg_decls,
            return_type=return_type,
            static=True)
        text = _format(
            "return {func}({args});",
            func=trampoline_var_name,
            args=", ".join(arg_names))
        trampoline_def.body.append(TextNode(text))

    func_decl = CxxFuncDeclNode(name=function_name,
                                arg_decls=arg_decls,
                                return_type=return_type,
                                static=bool(class_name))

    func_def = CxxFuncDefNode(
        name=function_name,
        arg_decls=arg_decls,
        return_type=return_type,
        class_name=class_name)
    func_def.set_base_template_vars(cg_context.template_bindings())

    body = func_def.body
    for arg_name in arg_names:
        if arg_name == "context":  # 'context' is reserved by Mako.
            body.add_template_var("v8_context", "context")
        else:
            body.add_template_var(arg_name, arg_name)
    bind_installer_local_vars(body, cg_context)

    if (is_per_context_install
            and "Global" in cg_context.interface.extended_attributes):
        body.extend([
            CxxLikelyIfNode(cond="${instance_object}.IsEmpty()",
                            body=[
                                TextNode("""\
${instance_object} = ${v8_context}->Global()->GetPrototype().As<v8::Object>();\
"""),
                            ]),
            EmptyNode(),
        ])

    if install_prototype_object_node:
        body.extend([
            CxxLikelyIfNode(cond="${feature_selector}.IsAll()",
                            body=[install_prototype_object_node]),
            EmptyNode(),
        ])

    def group_by_condition(entries):
        unconditional_entries = []
        conditional_to_entries = {}
        for entry in entries:
            if entry.exposure_conditional.is_always_true:
                unconditional_entries.append(entry)
            else:
                conditional_to_entries.setdefault(entry.exposure_conditional,
                                                  []).append(entry)
        return unconditional_entries, conditional_to_entries

    def install_properties(table_name, target_entries, make_table_func,
                           installer_call_text):
        unconditional_entries, conditional_to_entries = group_by_condition(
            target_entries)
        if unconditional_entries:
            body.append(
                CxxBlockNode([
                    make_table_func(table_name, unconditional_entries),
                    TextNode(installer_call_text),
                ]))
            body.append(EmptyNode())
        for conditional, entries in conditional_to_entries.items():
            body.append(
                CxxUnlikelyIfNode(
                    cond=conditional,
                    body=[
                        make_table_func(table_name, entries),
                        TextNode(installer_call_text),
                    ]))
        body.append(EmptyNode())

    table_name = "kAttributeTable"
    if is_per_context_install:
        installer_call_text = (
            "V8DOMConfiguration::InstallAccessors(${isolate}, ${world}, "
            "${instance_object}, ${prototype_object}, ${interface_object}, "
            "${signature}, kAttributeTable, base::size(kAttributeTable));")
    else:
        installer_call_text = (
            "V8DOMConfiguration::InstallAccessors(${isolate}, ${world}, "
            "${instance_template}, ${prototype_template}, "
            "${interface_template}, ${signature}, "
            "kAttributeTable, base::size(kAttributeTable));")
    install_properties(table_name, attribute_entries,
                       _make_attribute_registration_table, installer_call_text)

    table_name = "kConstantCallbackTable"
    if is_per_context_install:
        installer_call_text = (
            "V8DOMConfiguration::InstallConstants(${isolate}, "
            "${interface_object}, ${prototype_object}, "
            "kConstantCallbackTable, base::size(kConstantCallbackTable));")
    else:
        installer_call_text = (
            "V8DOMConfiguration::InstallConstants(${isolate}, "
            "${interface_template}, ${prototype_template}, "
            "kConstantCallbackTable, base::size(kConstantCallbackTable));")
    constant_callback_entries = filter(lambda entry: entry.const_callback_name,
                                       constant_entries)
    install_properties(table_name, constant_callback_entries,
                       _make_constant_callback_registration_table,
                       installer_call_text)

    table_name = "kConstantValueTable"
    if is_per_context_install:
        installer_call_text = (
            "V8DOMConfiguration::InstallConstants(${isolate}, "
            "${interface_object}, ${prototype_object}, "
            "kConstantValueTable, base::size(kConstantValueTable));")
    else:
        installer_call_text = (
            "V8DOMConfiguration::InstallConstants(${isolate}, "
            "${interface_template}, ${prototype_template}, "
            "kConstantValueTable, base::size(kConstantValueTable));")
    constant_value_entries = filter(
        lambda entry: not entry.const_callback_name, constant_entries)
    install_properties(table_name, constant_value_entries,
                       _make_constant_value_registration_table,
                       installer_call_text)

    table_name = "kExposedConstructTable"
    if is_per_context_install:
        installer_call_text = (
            "V8DOMConfiguration::InstallAttributes(${isolate}, ${world}, "
            "${instance_object}, ${prototype_object}, "
            "kExposedConstructTable, base::size(kExposedConstructTable));")
    else:
        installer_call_text = (
            "V8DOMConfiguration::InstallAttributes(${isolate}, ${world}, "
            "${instance_template}, ${prototype_template}, "
            "kExposedConstructTable, base::size(kExposedConstructTable));")
    install_properties(table_name, exposed_construct_entries,
                       _make_exposed_construct_registration_table,
                       installer_call_text)

    table_name = "kOperationTable"
    if is_per_context_install:
        installer_call_text = (
            "V8DOMConfiguration::InstallMethods(${isolate}, ${world}, "
            "${instance_object}, ${prototype_object}, ${interface_object}, "
            "${signature}, kOperationTable, base::size(kOperationTable));")
    else:
        installer_call_text = (
            "V8DOMConfiguration::InstallMethods(${isolate}, ${world}, "
            "${instance_template}, ${prototype_template}, "
            "${interface_template}, ${signature}, "
            "kOperationTable, base::size(kOperationTable));")
    entries = filter(lambda entry: not entry.no_alloc_direct_callback_name,
                     operation_entries)
    install_properties(table_name, entries, _make_operation_registration_table,
                       installer_call_text)
    entries = filter(lambda entry: entry.no_alloc_direct_callback_name,
                     operation_entries)
    install_properties(table_name, entries, _make_operation_registration_table,
                       installer_call_text)

    return func_decl, func_def, trampoline_def


def make_indexed_and_named_property_callbacks_and_install_node(cg_context):
    """
    Implements non-ordinary internal methods of legacy platform objects.
    https://heycam.github.io/webidl/#es-legacy-platform-objects

    Also implements the same origin case of indexed access to WindowProxy
    objects just same as indexed properties of legacy platform objects.
    https://html.spec.whatwg.org/C/#the-windowproxy-exotic-object
    """

    assert isinstance(cg_context, CodeGenContext)

    F = lambda *args, **kwargs: TextNode(_format(*args, **kwargs))

    func_decls = ListNode()
    func_defs = ListNode()
    install_node = SequenceNode()

    interface = cg_context.interface
    if not (interface and interface.indexed_and_named_properties
            and "Global" not in interface.extended_attributes):
        return func_decls, func_defs, install_node
    props = interface.indexed_and_named_properties

    def add_callback(func_decl, func_def):
        func_decls.append(func_decl)
        if func_def:
            func_defs.append(func_def)
            func_defs.append(EmptyNode())

    def most_derived_interface(*interfaces):
        key = lambda interface: len(interface.inclusive_inherited_interfaces)
        return sorted(filter(None, interfaces), key=key)[-1]

    cg_context = cg_context.make_copy(
        v8_callback_type=CodeGenContext.V8_OTHER_CALLBACK)

    if props.own_named_getter:
        add_callback(*make_named_property_getter_callback(
            cg_context.make_copy(named_property_getter=props.named_getter),
            "NamedPropertyGetterCallback"))
        add_callback(*make_named_property_setter_callback(
            cg_context.make_copy(named_property_setter=props.named_setter),
            "NamedPropertySetterCallback"))
        add_callback(*make_named_property_deleter_callback(
            cg_context.make_copy(named_property_deleter=props.named_deleter),
            "NamedPropertyDeleterCallback"))
        add_callback(*make_named_property_definer_callback(
            cg_context, "NamedPropertyDefinerCallback"))
        add_callback(*make_named_property_descriptor_callback(
            cg_context, "NamedPropertyDescriptorCallback"))
        add_callback(*make_named_property_query_callback(
            cg_context, "NamedPropertyQueryCallback"))
        add_callback(*make_named_property_enumerator_callback(
            cg_context, "NamedPropertyEnumeratorCallback"))

    if props.named_getter:
        impl_bridge = v8_bridge_class_name(
            most_derived_interface(
                props.named_getter.owner, props.named_setter
                and props.named_setter.owner, props.named_deleter
                and props.named_deleter.owner))
        flags = ["v8::PropertyHandlerFlags::kOnlyInterceptStrings"]
        if "LegacyOverrideBuiltins" not in interface.extended_attributes:
            flags.append("v8::PropertyHandlerFlags::kNonMasking")
        if (props.named_getter.extended_attributes.value_of("Affects") ==
                "Nothing"):
            flags.append("v8::PropertyHandlerFlags::kHasNoSideEffect")
        property_handler_flags = (
            "static_cast<v8::PropertyHandlerFlags>({})".format(" | ".join(
                map(lambda flag: "int32_t({})".format(flag), flags))))
        pattern = """\
// Named interceptors
${instance_template}->SetHandler(
    v8::NamedPropertyHandlerConfiguration(
        {impl_bridge}::NamedPropertyGetterCallback,
        {impl_bridge}::NamedPropertySetterCallback,
% if "NotEnumerable" not in \
interface.indexed_and_named_properties.named_getter.extended_attributes:
        {impl_bridge}::NamedPropertyQueryCallback,
% else:
        nullptr,  // query
% endif
        {impl_bridge}::NamedPropertyDeleterCallback,
% if "NotEnumerable" not in \
interface.indexed_and_named_properties.named_getter.extended_attributes:
        {impl_bridge}::NamedPropertyEnumeratorCallback,
% else:
        nullptr,  // enumerator
% endif
        {impl_bridge}::NamedPropertyDefinerCallback,
        {impl_bridge}::NamedPropertyDescriptorCallback,
        v8::Local<v8::Value>(),
        {property_handler_flags}));"""
        install_node.append(
            F(pattern,
              impl_bridge=impl_bridge,
              property_handler_flags=property_handler_flags))

    if props.own_indexed_getter or props.own_named_getter:
        add_callback(*make_indexed_property_getter_callback(
            cg_context.make_copy(indexed_property_getter=props.indexed_getter),
            "IndexedPropertyGetterCallback"))
        add_callback(*make_indexed_property_setter_callback(
            cg_context.make_copy(indexed_property_setter=props.indexed_setter),
            "IndexedPropertySetterCallback"))
        add_callback(*make_indexed_property_deleter_callback(
            cg_context, "IndexedPropertyDeleterCallback"))
        add_callback(*make_indexed_property_definer_callback(
            cg_context, "IndexedPropertyDefinerCallback"))
        add_callback(*make_indexed_property_descriptor_callback(
            cg_context, "IndexedPropertyDescriptorCallback"))
        add_callback(*make_indexed_property_enumerator_callback(
            cg_context, "IndexedPropertyEnumeratorCallback"))

    if props.indexed_getter or props.named_getter:
        impl_bridge = v8_bridge_class_name(
            most_derived_interface(
                props.indexed_getter and props.indexed_getter.owner,
                props.indexed_setter and props.indexed_setter.owner,
                props.named_getter and props.named_getter.owner,
                props.named_setter and props.named_setter.owner,
                props.named_deleter and props.named_deleter.owner))
        flags = []
        if (props.indexed_getter and props.indexed_getter.extended_attributes.
                value_of("Affects") == "Nothing"):
            flags.append("v8::PropertyHandlerFlags::kHasNoSideEffect")
        else:
            flags.append("v8::PropertyHandlerFlags::kNone")
        property_handler_flags = flags[0]
        pattern = """\
// Indexed interceptors
${instance_template}->SetHandler(
    v8::IndexedPropertyHandlerConfiguration(
        {impl_bridge}::IndexedPropertyGetterCallback,
        {impl_bridge}::IndexedPropertySetterCallback,
        nullptr,  // query
        {impl_bridge}::IndexedPropertyDeleterCallback,
% if interface.indexed_and_named_properties.indexed_getter:
        {impl_bridge}::IndexedPropertyEnumeratorCallback,
% else:
        nullptr,  // enumerator
% endif
        {impl_bridge}::IndexedPropertyDefinerCallback,
        {impl_bridge}::IndexedPropertyDescriptorCallback,
        v8::Local<v8::Value>(),
        {property_handler_flags}));"""
        install_node.append(
            F(pattern,
              impl_bridge=impl_bridge,
              property_handler_flags=property_handler_flags))

    func_defs.accumulate(
        CodeGenAccumulator.require_include_headers([
            "third_party/blink/renderer/bindings/core/v8/v8_set_return_value_for_core.h"
        ]))

    return func_decls, func_defs, install_node


def make_named_properties_object_callbacks_and_install_node(cg_context):
    """
    Implements non-ordinary internal methods of named properties objects.
    https://heycam.github.io/webidl/#named-properties-object
    """

    assert isinstance(cg_context, CodeGenContext)

    callback_defs = []
    install_node = SequenceNode()

    interface = cg_context.interface
    if not (interface and interface.indexed_and_named_properties
            and interface.indexed_and_named_properties.named_getter
            and "Global" in interface.extended_attributes):
        return callback_defs, install_node

    cg_context = cg_context.make_copy(
        v8_callback_type=CodeGenContext.V8_OTHER_CALLBACK)

    func_defs = [
        make_named_props_obj_named_getter_callback(
            cg_context, "NamedPropsObjNamedGetterCallback"),
        make_named_props_obj_named_setter_callback(
            cg_context, "NamedPropsObjNamedSetterCallback"),
        make_named_props_obj_named_deleter_callback(
            cg_context, "NamedPropsObjNamedDeleterCallback"),
        make_named_props_obj_named_definer_callback(
            cg_context, "NamedPropsObjNamedDefinerCallback"),
        make_named_props_obj_named_descriptor_callback(
            cg_context, "NamedPropsObjNamedDescriptorCallback"),
        make_named_props_obj_indexed_getter_callback(
            cg_context, "NamedPropsObjIndexedGetterCallback"),
        make_named_props_obj_indexed_setter_callback(
            cg_context, "NamedPropsObjIndexedSetterCallback"),
        make_named_props_obj_indexed_deleter_callback(
            cg_context, "NamedPropsObjIndexedDeleterCallback"),
        make_named_props_obj_indexed_definer_callback(
            cg_context, "NamedPropsObjIndexedDefinerCallback"),
        make_named_props_obj_indexed_descriptor_callback(
            cg_context, "NamedPropsObjIndexedDescriptorCallback"),
    ]
    for func_def in func_defs:
        callback_defs.append(func_def)
        callback_defs.append(EmptyNode())

    text = """\
// Named interceptors
${npo_prototype_template}->SetHandler(
    v8::NamedPropertyHandlerConfiguration(
        NamedPropsObjNamedGetterCallback,
        NamedPropsObjNamedSetterCallback,
        nullptr,  // query
        NamedPropsObjNamedDeleterCallback,
        nullptr,  // enumerator
        NamedPropsObjNamedDefinerCallback,
        NamedPropsObjNamedDescriptorCallback,
        v8::Local<v8::Value>(),
        static_cast<v8::PropertyHandlerFlags>(
            int32_t(v8::PropertyHandlerFlags::kNonMasking) |
            int32_t(v8::PropertyHandlerFlags::kOnlyInterceptStrings))));
// Indexed interceptors
${npo_prototype_template}->SetHandler(
    v8::IndexedPropertyHandlerConfiguration(
        NamedPropsObjIndexedGetterCallback,
        NamedPropsObjIndexedSetterCallback,
        nullptr,  // query
        NamedPropsObjIndexedDeleterCallback,
        nullptr,  // enumerator
        NamedPropsObjIndexedDefinerCallback,
        NamedPropsObjIndexedDescriptorCallback,
        v8::Local<v8::Value>(),
        v8::PropertyHandlerFlags::kNone));"""
    install_node.append(TextNode(text))

    return callback_defs, install_node


def make_cross_origin_property_callbacks_and_install_node(
        cg_context, attribute_entries, operation_entries):
    """
    Implements non-ordinary internal methods of WindowProxy and Location
    objects.
    https://html.spec.whatwg.org/C/#the-windowproxy-exotic-object
    https://html.spec.whatwg.org/C/#the-location-interface
    """

    assert isinstance(cg_context, CodeGenContext)

    callback_defs = []
    install_node = SequenceNode()

    CROSS_ORIGIN_INTERFACES = ("Window", "Location")
    if cg_context.interface.identifier not in CROSS_ORIGIN_INTERFACES:
        return callback_defs, install_node
    props = cg_context.interface.indexed_and_named_properties

    entry_nodes = []
    for entry in attribute_entries:
        attribute = entry.property_
        if "CrossOrigin" not in attribute.extended_attributes:
            continue
        assert entry.world == CodeGenContext.ALL_WORLDS
        values = attribute.extended_attributes.values_of("CrossOrigin")
        get_func = "nullptr"
        set_func = "nullptr"
        get_value = "nullptr"
        set_value = "nullptr"
        if not values or "Getter" in values:
            get_func = entry.attr_get_callback_name
            cgc = cg_context.make_copy(
                attribute=attribute,
                attribute_get=True,
                v8_callback_type=(
                    CodeGenContext.V8_ACCESSOR_NAME_GETTER_CALLBACK))
            get_value = callback_function_name(cgc, for_cross_origin=True)
            func_def = make_attribute_get_callback_def(cgc, get_value)
            callback_defs.extend([func_def, EmptyNode()])
        if values and "Setter" in values:
            set_func = entry.attr_set_callback_name
            cgc = cg_context.make_copy(
                attribute=attribute,
                attribute_set=True,
                v8_callback_type=(
                    CodeGenContext.V8_GENERIC_NAMED_PROPERTY_SETTER_CALLBACK))
            set_value = callback_function_name(cgc, for_cross_origin=True)
            func_def = make_attribute_set_callback_def(cgc, set_value)
            callback_defs.extend([func_def, EmptyNode()])
        pattern = ("{{\"{property_name}\", "
                   "{get_func}, {set_func}, {get_value}, {set_value}}},")
        entry_nodes.append(
            TextNode(
                _format(
                    pattern,
                    property_name=attribute.identifier,
                    get_func=get_func,
                    set_func=set_func,
                    get_value=get_value,
                    set_value=set_value)))
    callback_defs.append(
        ListNode([
            TextNode("constexpr bindings::CrossOriginAttributeTableEntry "
                     "kCrossOriginAttributeTable[] = {"),
            ListNode(entry_nodes),
            TextNode("};"),
            EmptyNode(),
        ]))

    entry_nodes = []
    for entry in operation_entries:
        operation_group = entry.property_
        if "CrossOrigin" not in operation_group.extended_attributes:
            continue
        assert entry.world == CodeGenContext.ALL_WORLDS
        entry_nodes.append(
            TextNode(
                _format(
                    "{{\"{property_name}\", {op_callback}, {op_func_length}}},",
                    property_name=operation_group.identifier,
                    op_callback=entry.op_callback_name,
                    op_func_length=entry.op_func_length)))
    callback_defs.append(
        ListNode([
            TextNode("constexpr bindings::CrossOriginOperationTableEntry "
                     "kCrossOriginOperationTable[] = {"),
            ListNode(entry_nodes),
            TextNode("};"),
            EmptyNode(),
        ]))

    cg_context = cg_context.make_copy(
        v8_callback_type=CodeGenContext.V8_OTHER_CALLBACK)

    func_defs = [
        make_cross_origin_access_check_callback(
            cg_context, "CrossOriginAccessCheckCallback"),
        make_cross_origin_named_getter_callback(
            cg_context, "CrossOriginNamedGetterCallback"),
        make_cross_origin_named_setter_callback(
            cg_context, "CrossOriginNamedSetterCallback"),
        make_cross_origin_named_deleter_callback(
            cg_context, "CrossOriginNamedDeleterCallback"),
        make_cross_origin_named_definer_callback(
            cg_context, "CrossOriginNamedDefinerCallback"),
        make_cross_origin_named_descriptor_callback(
            cg_context, "CrossOriginNamedDescriptorCallback"),
        make_cross_origin_named_query_callback(
            cg_context, "CrossOriginNamedQueryCallback"),
        make_cross_origin_named_enumerator_callback(
            cg_context, "CrossOriginNamedEnumeratorCallback"),
        make_cross_origin_indexed_getter_callback(
            cg_context.make_copy(
                indexed_property_getter=(props and props.indexed_getter)),
            "CrossOriginIndexedGetterCallback"),
        make_cross_origin_indexed_setter_callback(
            cg_context, "CrossOriginIndexedSetterCallback"),
        make_cross_origin_indexed_deleter_callback(
            cg_context, "CrossOriginIndexedDeleterCallback"),
        make_cross_origin_indexed_definer_callback(
            cg_context, "CrossOriginIndexedDefinerCallback"),
        make_cross_origin_indexed_descriptor_callback(
            cg_context, "CrossOriginIndexedDescriptorCallback"),
        make_cross_origin_indexed_enumerator_callback(
            cg_context, "CrossOriginIndexedEnumeratorCallback"),
    ]
    for func_def in func_defs:
        callback_defs.append(func_def)
        callback_defs.append(EmptyNode())

    text = """\
// Cross origin properties
${instance_template}->SetAccessCheckCallbackAndHandler(
    CrossOriginAccessCheckCallback,
    v8::NamedPropertyHandlerConfiguration(
        CrossOriginNamedGetterCallback,
        CrossOriginNamedSetterCallback,
        CrossOriginNamedQueryCallback,
        CrossOriginNamedDeleterCallback,
        CrossOriginNamedEnumeratorCallback,
        CrossOriginNamedDefinerCallback,
        CrossOriginNamedDescriptorCallback,
        v8::Local<v8::Value>(),
        v8::PropertyHandlerFlags::kNone),
    v8::IndexedPropertyHandlerConfiguration(
        CrossOriginIndexedGetterCallback,
        CrossOriginIndexedSetterCallback,
        nullptr,  // query
        CrossOriginIndexedDeleterCallback,
        CrossOriginIndexedEnumeratorCallback,
        CrossOriginIndexedDefinerCallback,
        CrossOriginIndexedDescriptorCallback,
        v8::Local<v8::Value>(),
        v8::PropertyHandlerFlags::kNone),
    v8::External::New(
        ${isolate},
        const_cast<WrapperTypeInfo*>(${class_name}::GetWrapperTypeInfo())));
"""
    install_node.append(TextNode(text))
    install_node.accumulate(
        CodeGenAccumulator.require_include_headers([
            "third_party/blink/renderer/bindings/core/v8/binding_security.h",
            "third_party/blink/renderer/platform/bindings/v8_cross_origin_property_support.h",
        ]))

    if cg_context.interface.identifier != "Window":
        return callback_defs, install_node

    func_defs = [
        make_same_origin_indexed_getter_callback(
            cg_context.make_copy(
                indexed_property_getter=(props and props.indexed_getter)),
            "SameOriginIndexedGetterCallback"),
        make_same_origin_indexed_setter_callback(
            cg_context, "SameOriginIndexedSetterCallback"),
        make_same_origin_indexed_deleter_callback(
            cg_context, "SameOriginIndexedDeleterCallback"),
        make_same_origin_indexed_definer_callback(
            cg_context, "SameOriginIndexedDefinerCallback"),
        make_same_origin_indexed_descriptor_callback(
            cg_context, "SameOriginIndexedDescriptorCallback"),
        make_same_origin_indexed_enumerator_callback(
            cg_context, "SameOriginIndexedEnumeratorCallback"),
    ]
    for func_def in func_defs:
        callback_defs.append(func_def)
        callback_defs.append(EmptyNode())

    text = """\
// Same origin interceptors
${instance_template}->SetHandler(
    v8::IndexedPropertyHandlerConfiguration(
        SameOriginIndexedGetterCallback,
        SameOriginIndexedSetterCallback,
        nullptr,  // query
        SameOriginIndexedDeleterCallback,
        SameOriginIndexedEnumeratorCallback,
        SameOriginIndexedDefinerCallback,
        SameOriginIndexedDescriptorCallback,
        v8::Local<v8::Value>(),
        v8::PropertyHandlerFlags::kNone));
"""
    install_node.append(TextNode(text))

    return callback_defs, install_node


def make_cross_component_init(
        cg_context, function_name, class_name, has_unconditional_props,
        has_context_independent_props, has_context_dependent_props):
    """
    Returns:
        A triplet of CodeNode of:
        - function declaration
        - function definition
        - trampoline member variable definitions
    """
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)
    assert isinstance(class_name, str)
    assert isinstance(has_unconditional_props, bool)
    assert isinstance(has_context_independent_props, bool)
    assert isinstance(has_context_dependent_props, bool)

    T = TextNode
    F = lambda *args, **kwargs: T(_format(*args, **kwargs))

    def filter_four_trampolines(nodes):
        assert len(nodes) == 4
        flags = (True, has_unconditional_props, has_context_independent_props,
                 has_context_dependent_props)
        return [node for node, flag in zip(nodes, flags) if flag]

    trampoline_var_decls = ListNode(
        filter_four_trampolines([
            F("static InstallInterfaceTemplateFuncType {};",
              TP_INSTALL_INTERFACE_TEMPLATE),
            F("static InstallUnconditionalPropertiesFuncType {};",
              TP_INSTALL_UNCONDITIONAL_PROPS),
            F("static InstallContextIndependentPropertiesFuncType {};",
              TP_INSTALL_CONTEXT_INDEPENDENT_PROPS),
            F("static InstallContextDependentPropertiesFuncType {};",
              TP_INSTALL_CONTEXT_DEPENDENT_PROPS),
        ]))

    trampoline_var_defs = ListNode(
        filter_four_trampolines([
            F(("${class_name}::InstallInterfaceTemplateFuncType "
               "${class_name}::{} = nullptr;"), TP_INSTALL_INTERFACE_TEMPLATE),
            F(("${class_name}::InstallUnconditionalPropertiesFuncType "
               "${class_name}::{} = nullptr;"),
              TP_INSTALL_UNCONDITIONAL_PROPS),
            F(("${class_name}::InstallContextIndependentPropertiesFuncType "
               "${class_name}::{} = nullptr;"),
              TP_INSTALL_CONTEXT_INDEPENDENT_PROPS),
            F(("${class_name}::InstallContextDependentPropertiesFuncType "
               "${class_name}::{} = nullptr;"),
              TP_INSTALL_CONTEXT_DEPENDENT_PROPS),
        ]))
    trampoline_var_defs.set_base_template_vars(cg_context.template_bindings())

    func_decl = CxxFuncDeclNode(
        name=function_name, arg_decls=[], return_type="void", static=True)

    func_def = CxxFuncDefNode(
        name=function_name,
        arg_decls=[],
        return_type="void",
        class_name=class_name)
    func_def.set_base_template_vars(cg_context.template_bindings())

    body = func_def.body
    body.extend(
        filter_four_trampolines([
            F("${class_name}::{} = {};", TP_INSTALL_INTERFACE_TEMPLATE,
              FN_INSTALL_INTERFACE_TEMPLATE),
            F("${class_name}::{} = {};", TP_INSTALL_UNCONDITIONAL_PROPS,
              FN_INSTALL_UNCONDITIONAL_PROPS),
            F("${class_name}::{} = {};", TP_INSTALL_CONTEXT_INDEPENDENT_PROPS,
              FN_INSTALL_CONTEXT_INDEPENDENT_PROPS),
            F("${class_name}::{} = {};", TP_INSTALL_CONTEXT_DEPENDENT_PROPS,
              FN_INSTALL_CONTEXT_DEPENDENT_PROPS),
        ]))

    return func_decl, func_def, trampoline_var_decls, trampoline_var_defs


# ----------------------------------------------------------------------------
# WrapperTypeInfo
# ----------------------------------------------------------------------------


def make_wrapper_type_info(cg_context, function_name,
                           has_context_dependent_props):
    assert isinstance(cg_context, CodeGenContext)
    assert function_name == "GetWrapperTypeInfo"
    assert isinstance(has_context_dependent_props, bool)

    F = lambda *args, **kwargs: TextNode(_format(*args, **kwargs))

    func_def = CxxFuncDefNode(
        name=function_name,
        arg_decls=[],
        return_type="constexpr const WrapperTypeInfo*",
        static=True)
    func_def.set_base_template_vars(cg_context.template_bindings())
    func_def.body.append(TextNode("return &wrapper_type_info_;"))

    member_var_def = TextNode(
        "static const WrapperTypeInfo wrapper_type_info_;")

    wrapper_type_info_def = ListNode()
    wrapper_type_info_def.set_base_template_vars(
        cg_context.template_bindings())

    wrapper_type_info_def.append(
        TextNode("""\
// Migration adapters
v8::Local<v8::FunctionTemplate> ${class_name}::DomTemplate(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world) {
  return V8DOMConfiguration::DomClassTemplate(
      isolate, world,
      const_cast<WrapperTypeInfo*>(${class_name}::GetWrapperTypeInfo()),
      ${class_name}::InstallInterfaceTemplate);
}
"""))
    if has_context_dependent_props:
        pattern = """\
static void InstallContextDependentPropertiesAdapter(
    v8::Local<v8::Context> context,
    const DOMWrapperWorld& world,
    v8::Local<v8::Object> instance_object,
    v8::Local<v8::Object> prototype_object,
    v8::Local<v8::Function> interface_object,
    v8::Local<v8::FunctionTemplate> interface_template) {{
  ${class_name}::{}(
      context, world, instance_object, prototype_object, interface_object,
      interface_template,
      bindings::V8InterfaceBridgeBase::FeatureSelector());
}}
"""
        wrapper_type_info_def.append(
            F(pattern, FN_INSTALL_CONTEXT_DEPENDENT_PROPS))
    pattern = """\
// Construction of WrapperTypeInfo may require non-trivial initialization due
// to cross-component address resolution in order to load the pointer to the
// parent interface's WrapperTypeInfo.  We ignore this issue because the issue
// happens only on component builds and the official release builds
// (statically-linked builds) are never affected by this issue.
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif

const WrapperTypeInfo ${class_name}::wrapper_type_info_{{
    gin::kEmbedderBlink,
    ${class_name}::DomTemplate,
    {install_context_dependent_func},
    "${{class_like.identifier}}",
    {wrapper_type_info_of_inherited},
    {wrapper_type_prototype},
    {wrapper_class_id},
    {active_script_wrappable_inheritance},
}};

#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic pop
#endif
"""
    class_like = cg_context.class_like
    if has_context_dependent_props:
        install_context_dependent_func = (
            "InstallContextDependentPropertiesAdapter")
    else:
        install_context_dependent_func = "nullptr"
    if class_like.inherited:
        wrapper_type_info_of_inherited = "{}::GetWrapperTypeInfo()".format(
            v8_bridge_class_name(class_like.inherited))
    else:
        wrapper_type_info_of_inherited = "nullptr"
    wrapper_type_prototype = ("WrapperTypeInfo::kWrapperTypeObjectPrototype"
                              if isinstance(class_like, web_idl.Interface) else
                              "WrapperTypeInfo::kWrapperTypeNoPrototype")
    wrapper_class_id = ("WrapperTypeInfo::kNodeClassId"
                        if class_like.does_implement("Node") else
                        "WrapperTypeInfo::kObjectClassId")
    active_script_wrappable_inheritance = (
        "WrapperTypeInfo::kInheritFromActiveScriptWrappable"
        if class_like.code_generator_info.is_active_script_wrappable else
        "WrapperTypeInfo::kNotInheritFromActiveScriptWrappable")
    wrapper_type_info_def.append(
        F(pattern,
          install_context_dependent_func=install_context_dependent_func,
          wrapper_type_info_of_inherited=wrapper_type_info_of_inherited,
          wrapper_type_prototype=wrapper_type_prototype,
          wrapper_class_id=wrapper_class_id,
          active_script_wrappable_inheritance=(
              active_script_wrappable_inheritance)))

    blink_class = blink_class_name(class_like)
    pattern = """\
const WrapperTypeInfo& {blink_class}::wrapper_type_info_ =
    ${class_name}::wrapper_type_info_;
"""
    wrapper_type_info_def.append(F(pattern, blink_class=blink_class))

    if class_like.code_generator_info.is_active_script_wrappable:
        pattern = """\
// [ActiveScriptWrappable]
static_assert(
    std::is_base_of<ActiveScriptWrappableBase, {blink_class}>::value,
    "{blink_class} does not inherit from ActiveScriptWrappable<> despite "
    "the IDL has [ActiveScriptWrappable] extended attribute.");
static_assert(
    !std::is_same<decltype(&{blink_class}::HasPendingActivity),
                  decltype(&ScriptWrappable::HasPendingActivity)>::value,
    "{blink_class} is not overriding hasPendingActivity() despite "
    "the IDL has [ActiveScriptWrappable] extended attribute.");"""
    else:
        pattern = """\
// non-[ActiveScriptWrappable]
static_assert(
    !std::is_base_of<ActiveScriptWrappableBase, {blink_class}>::value,
    "{blink_class} inherits from ActiveScriptWrappable<> without "
    "[ActiveScriptWrappable] extended attribute.");
static_assert(
    std::is_same<decltype(&{blink_class}::HasPendingActivity),
                 decltype(&ScriptWrappable::HasPendingActivity)>::value,
    "{blink_class} is overriding hasPendingActivity() without "
    "[ActiveScriptWrappable] extended attribute.");"""
    wrapper_type_info_def.append(F(pattern, blink_class=blink_class))

    return func_def, member_var_def, wrapper_type_info_def


# ----------------------------------------------------------------------------
# V8 Context Snapshot
# ----------------------------------------------------------------------------


def make_v8_context_snapshot_api(cg_context, component, attribute_entries,
                                 constant_entries, constructor_entries,
                                 exposed_construct_entries, operation_entries,
                                 named_properties_object_callback_defs,
                                 cross_origin_property_callback_defs,
                                 install_context_independent_func_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(component, web_idl.Component)

    derived_interfaces = cg_context.interface.deriveds
    derived_names = map(lambda interface: interface.identifier,
                        derived_interfaces)
    derived_names.append(cg_context.interface.identifier)
    if not ("Window" in derived_names or "HTMLDocument" in derived_names):
        return None, None

    header_ns = CxxNamespaceNode(name_style.namespace("v8_context_snapshot"))
    source_ns = CxxNamespaceNode(name_style.namespace("v8_context_snapshot"))

    export_text = component_export(component, False)

    def add_func(func_decl, func_def):
        header_ns.body.extend([
            TextNode(export_text),
            func_decl,
            EmptyNode(),
        ])
        source_ns.body.extend([
            func_def,
            EmptyNode(),
        ])

    add_func(*_make_v8_context_snapshot_get_reference_table_function(
        cg_context, name_style.func("GetRefTableOf",
                                    cg_context.class_name), attribute_entries,
        constant_entries, constructor_entries, exposed_construct_entries,
        operation_entries, named_properties_object_callback_defs,
        cross_origin_property_callback_defs))

    add_func(*_make_v8_context_snapshot_install_props_per_context_function(
        cg_context, name_style.func("InstallPropsOf",
                                    cg_context.class_name), attribute_entries,
        constant_entries, exposed_construct_entries, operation_entries))

    add_func(*_make_v8_context_snapshot_install_props_per_isolate_function(
        cg_context, name_style.func("InstallPropsOf", cg_context.class_name),
        install_context_independent_func_name))

    return header_ns, source_ns


def _make_v8_context_snapshot_get_reference_table_function(
        cg_context, function_name, attribute_entries, constant_entries,
        constructor_entries, exposed_construct_entries, operation_entries,
        named_properties_object_callback_defs,
        cross_origin_property_callback_defs):
    callback_names = ["${class_name}::GetWrapperTypeInfo()"]

    for entry in attribute_entries:
        if entry.exposure_conditional.is_always_true:
            callback_names.append(entry.attr_get_callback_name)
            callback_names.append(entry.attr_set_callback_name)
    for entry in constant_entries:
        if entry.exposure_conditional.is_always_true:
            callback_names.append(entry.const_callback_name)
    for entry in constructor_entries:
        if entry.exposure_conditional.is_always_true:
            callback_names.append(entry.ctor_callback_name)
    for entry in exposed_construct_entries:
        if entry.exposure_conditional.is_always_true:
            callback_names.append(entry.prop_callback_name)
    for entry in operation_entries:
        if entry.exposure_conditional.is_always_true:
            callback_names.append(entry.op_callback_name)

    def collect_callbacks(node):
        if isinstance(node, CxxFuncDefNode):
            callback_names.append(node.function_name)
        elif hasattr(node, "__iter__"):
            for child_node in node:
                collect_callbacks(child_node)

    collect_callbacks(named_properties_object_callback_defs)
    collect_callbacks(cross_origin_property_callback_defs)

    entry_nodes = map(
        lambda name: TextNode("reinterpret_cast<intptr_t>({}),".format(name)),
        filter(None, callback_names))
    table_node = ListNode([
        TextNode("static const intptr_t kReferenceTable[] = {"),
        ListNode(entry_nodes),
        TextNode("};"),
    ])

    func_decl = CxxFuncDeclNode(name=function_name,
                                arg_decls=[],
                                return_type="base::span<const intptr_t>")

    func_def = CxxFuncDefNode(name=function_name,
                              arg_decls=[],
                              return_type="base::span<const intptr_t>")
    func_def.set_base_template_vars(cg_context.template_bindings())
    body = func_def.body
    body.extend([table_node, TextNode("return kReferenceTable;")])

    return func_decl, func_def


def _make_v8_context_snapshot_install_props_per_context_function(
        cg_context, function_name, attribute_entries, constant_entries,
        exposed_construct_entries, operation_entries):
    def selector(entry):
        if entry.exposure_conditional.is_always_true:
            return False
        if entry.is_context_dependent:
            return False
        return True

    func_decl, func_def, _ = make_install_properties(
        cg_context,
        function_name,
        class_name=None,
        prop_install_mode=PropInstallMode.V8_CONTEXT_SNAPSHOT,
        trampoline_var_name=None,
        attribute_entries=filter(selector, attribute_entries),
        constant_entries=filter(selector, constant_entries),
        exposed_construct_entries=filter(selector, exposed_construct_entries),
        operation_entries=filter(selector, operation_entries))

    return func_decl, func_def


def _make_v8_context_snapshot_install_props_per_isolate_function(
        cg_context, function_name, install_context_independent_func_name):
    arg_decls = [
        "v8::Isolate* isolate",
        "const DOMWrapperWorld& world",
        "v8::Local<v8::ObjectTemplate> instance_template",
        "v8::Local<v8::ObjectTemplate> prototype_template",
        "v8::Local<v8::FunctionTemplate> interface_template",
    ]
    arg_names = [
        "isolate",
        "world",
        "instance_template",
        "prototype_template",
        "interface_template",
    ]
    return_type = "void"

    func_decl = CxxFuncDeclNode(name=function_name,
                                arg_decls=arg_decls,
                                return_type=return_type)
    func_def = CxxFuncDefNode(name=function_name,
                              arg_decls=arg_decls,
                              return_type=return_type)

    if not install_context_independent_func_name:
        return func_decl, func_def

    func_def.set_base_template_vars(cg_context.template_bindings())
    body = func_def.body
    for arg_name in arg_names:
        body.add_template_var(arg_name, arg_name)
    pattern = """\
return ${class_name}::{func}(
    ${isolate}, ${world},
    ${instance_template},
    ${prototype_template},
    ${interface_template});\
"""
    body.append(
        TextNode(_format(pattern, func=install_context_independent_func_name)))
    return func_decl, func_def


# ----------------------------------------------------------------------------
# Main functions
# ----------------------------------------------------------------------------


def _collect_include_headers(interface):
    assert isinstance(interface, web_idl.Interface)

    headers = set(interface.code_generator_info.blink_headers)

    def collect_from_idl_type(idl_type):
        idl_type.apply_to_all_composing_elements(add_include_headers)

    def add_include_headers(idl_type):
        # ScriptPromise doesn't require any header for the result type.
        if idl_type.is_promise:
            raise StopIteration(idl_type.syntactic_form)

        type_def_obj = idl_type.type_definition_object
        if type_def_obj is not None:
            if (type_def_obj.identifier in (
                    "OnErrorEventHandlerNonNull",
                    "OnBeforeUnloadEventHandlerNonNull")):
                raise StopIteration(idl_type.syntactic_form)

            headers.add(PathManager(type_def_obj).api_path(ext="h"))
            if isinstance(type_def_obj, web_idl.Interface):
                headers.add(PathManager(type_def_obj).blink_path(ext="h"))
            raise StopIteration(idl_type.syntactic_form)

        union_def_obj = idl_type.union_definition_object
        if union_def_obj is not None:
            headers.add(PathManager(union_def_obj).api_path(ext="h"))

    for attribute in interface.attributes:
        collect_from_idl_type(attribute.idl_type)
    for constructor in interface.constructors:
        for argument in constructor.arguments:
            collect_from_idl_type(argument.idl_type)
    for operation in interface.operations:
        collect_from_idl_type(operation.return_type)
        for argument in operation.arguments:
            collect_from_idl_type(argument.idl_type)

    for exposed_construct in interface.exposed_constructs:
        headers.add(PathManager(exposed_construct).api_path(ext="h"))
    for legacy_window_alias in interface.legacy_window_aliases:
        headers.add(
            PathManager(legacy_window_alias.original).api_path(ext="h"))

    path_manager = PathManager(interface)
    headers.discard(path_manager.api_path(ext="h"))
    headers.discard(path_manager.impl_path(ext="h"))

    # TODO(yukishiino): Window interface should be
    # [ImplementedAs=LocalDOMWindow] instead of [ImplementedAs=DOMWindow], and
    # [CrossOrigin] properties should be implemented specifically with
    # DOMWindow class.  Then, we'll have less hacks.
    if interface.identifier == "Window":
        headers.add("third_party/blink/renderer/core/frame/local_dom_window.h")

    return headers


def generate_interface(interface_identifier):
    assert isinstance(interface_identifier, web_idl.Identifier)

    web_idl_database = package_initializer().web_idl_database()
    interface = web_idl_database.find(interface_identifier)

    path_manager = PathManager(interface)
    api_component = path_manager.api_component
    impl_component = path_manager.impl_component
    is_cross_components = path_manager.is_cross_components
    for_testing = interface.code_generator_info.for_testing

    # Class names
    api_class_name = v8_bridge_class_name(interface)
    if is_cross_components:
        impl_class_name = "{}::Impl".format(api_class_name)
    else:
        impl_class_name = api_class_name

    cg_context = CodeGenContext(interface=interface, class_name=api_class_name)

    # Filepaths
    api_header_path = path_manager.api_path(ext="h")
    api_source_path = path_manager.api_path(ext="cc")
    if is_cross_components:
        impl_header_path = path_manager.impl_path(ext="h")
        impl_source_path = path_manager.impl_path(ext="cc")

    # Root nodes
    api_header_node = ListNode(tail="\n")
    api_header_node.set_accumulator(CodeGenAccumulator())
    api_header_node.set_renderer(MakoRenderer())
    api_source_node = ListNode(tail="\n")
    api_source_node.set_accumulator(CodeGenAccumulator())
    api_source_node.set_renderer(MakoRenderer())
    if is_cross_components:
        impl_header_node = ListNode(tail="\n")
        impl_header_node.set_accumulator(CodeGenAccumulator())
        impl_header_node.set_renderer(MakoRenderer())
        impl_source_node = ListNode(tail="\n")
        impl_source_node.set_accumulator(CodeGenAccumulator())
        impl_source_node.set_renderer(MakoRenderer())
    else:
        impl_header_node = api_header_node
        impl_source_node = api_source_node

    # Namespaces
    api_header_blink_ns = CxxNamespaceNode(name_style.namespace("blink"))
    api_source_blink_ns = CxxNamespaceNode(name_style.namespace("blink"))
    if is_cross_components:
        impl_header_blink_ns = CxxNamespaceNode(name_style.namespace("blink"))
        impl_source_blink_ns = CxxNamespaceNode(name_style.namespace("blink"))
    else:
        impl_header_blink_ns = api_header_blink_ns
        impl_source_blink_ns = api_source_blink_ns

    # Class definitions
    api_class_def = CxxClassDefNode(
        cg_context.class_name,
        base_class_names=[
            _format("bindings::V8InterfaceBridge<${class_name}, {}>",
                    blink_class_name(interface)),
        ],
        final=True,
        export=component_export(api_component, for_testing))
    api_class_def.set_base_template_vars(cg_context.template_bindings())
    api_class_def.bottom_section.append(
        TextNode("friend class {};".format(blink_class_name(interface))))
    if is_cross_components:
        impl_class_def = CxxClassDefNode(impl_class_name,
                                         final=True,
                                         export=component_export(
                                             impl_component, for_testing))
        impl_class_def.set_base_template_vars(cg_context.template_bindings())
        api_class_def.public_section.extend([
            TextNode("// Cross-component implementation class"),
            TextNode("class Impl;"),
            EmptyNode(),
        ])
    else:
        impl_class_def = api_class_def

    # Constants
    constants_def = None
    if interface.constants:
        constants_def = CxxClassDefNode(name="Constant", final=True)
        constants_def.top_section.append(TextNode("STATIC_ONLY(Constant);"))
        for constant in interface.constants:
            cgc = cg_context.make_copy(constant=constant)
            constants_def.public_section.append(
                make_constant_constant_def(cgc, constant_name(cgc)))

    # Custom callback implementations
    custom_callback_impl_decls = ListNode()

    def add_custom_callback_impl_decl(**params):
        arg_decls = params.pop("arg_decls")
        name = params.pop("name", None)
        if name is None:
            name = custom_function_name(cg_context.make_copy(**params))
        custom_callback_impl_decls.append(
            CxxFuncDeclNode(
                name=name,
                arg_decls=arg_decls,
                return_type="void",
                static=True))

    if interface.identifier == "HTMLAllCollection":
        add_custom_callback_impl_decl(
            name=name_style.func("LegacyCallCustom"),
            arg_decls=["const v8::FunctionCallbackInfo<v8::Value>&"])
    for attribute in interface.attributes:
        custom_values = attribute.extended_attributes.values_of("Custom")
        is_cross_origin = "CrossOrigin" in attribute.extended_attributes
        cross_origin_values = attribute.extended_attributes.values_of(
            "CrossOrigin")
        if "Getter" in custom_values:
            add_custom_callback_impl_decl(
                attribute=attribute,
                attribute_get=True,
                arg_decls=["const v8::FunctionCallbackInfo<v8::Value>&"])
            if is_cross_origin and (not cross_origin_values
                                    or "Getter" in cross_origin_values):
                add_custom_callback_impl_decl(
                    attribute=attribute,
                    attribute_get=True,
                    arg_decls=["const v8::PropertyCallbackInfo<v8::Value>&"])
        if "Setter" in custom_values:
            add_custom_callback_impl_decl(
                attribute=attribute,
                attribute_set=True,
                arg_decls=[
                    "v8::Local<v8::Value>",
                    "const v8::FunctionCallbackInfo<v8::Value>&",
                ])
            if is_cross_origin and (not cross_origin_values
                                    or "Setter" in cross_origin_values):
                add_custom_callback_impl_decl(
                    attribute=attribute,
                    attribute_set=True,
                    arg_decls=[
                        "v8::Local<v8::Value>",
                        "const v8::PropertyCallbackInfo<void>&",
                    ])
    for operation_group in interface.operation_groups:
        if "Custom" in operation_group.extended_attributes:
            add_custom_callback_impl_decl(
                operation_group=operation_group,
                arg_decls=["const v8::FunctionCallbackInfo<v8::Value>&"])
    if interface.indexed_and_named_properties:
        props = interface.indexed_and_named_properties
        operation = props.own_named_getter
        if operation and "Custom" in operation.extended_attributes:
            add_custom_callback_impl_decl(
                named_property_getter=operation,
                arg_decls=[
                    "const AtomicString& property_name",
                    "const v8::PropertyCallbackInfo<v8::Value>&",
                ])
        operation = props.own_named_setter
        if operation and "Custom" in operation.extended_attributes:
            add_custom_callback_impl_decl(
                named_property_setter=operation,
                arg_decls=[
                    "const AtomicString& property_name",
                    "v8::Local<v8::Value> v8_property_value",
                    "const v8::PropertyCallbackInfo<v8::Value>&",
                ])
        operation = props.own_named_deleter
        if operation and "Custom" in operation.extended_attributes:
            add_custom_callback_impl_decl(
                named_property_deleter=operation,
                arg_decls=[
                    "const AtomicString& property_name",
                    "const v8::PropertyCallbackInfo<v8::Value>&",
                ])

    # Cross-component trampolines
    if is_cross_components:
        # tp_ = trampoline name
        tp_install_interface_template = TP_INSTALL_INTERFACE_TEMPLATE
        tp_install_unconditional_props = TP_INSTALL_UNCONDITIONAL_PROPS
        tp_install_context_independent_props = (
            TP_INSTALL_CONTEXT_INDEPENDENT_PROPS)
        tp_install_context_dependent_props = TP_INSTALL_CONTEXT_DEPENDENT_PROPS
    else:
        tp_install_interface_template = None
        tp_install_unconditional_props = None
        tp_install_context_independent_props = None
        tp_install_context_dependent_props = None

    # Callback functions
    attribute_entries = []
    constant_entries = []
    constructor_entries = []
    exposed_construct_entries = []
    operation_entries = []
    callback_defs = _make_property_entries_and_callback_defs(
        cg_context,
        attribute_entries=attribute_entries,
        constant_entries=constant_entries,
        constructor_entries=constructor_entries,
        exposed_construct_entries=exposed_construct_entries,
        operation_entries=operation_entries)
    supplemental_install_node = SequenceNode()

    # Indexed and named properties
    # Shorten a function name to mitigate a style check error.
    f = make_indexed_and_named_property_callbacks_and_install_node
    (indexed_and_named_property_decls, indexed_and_named_property_defs,
     indexed_and_named_property_install_node) = f(cg_context)
    supplemental_install_node.append(indexed_and_named_property_install_node)
    supplemental_install_node.append(EmptyNode())

    # Named properties object
    (named_properties_object_callback_defs,
     named_properties_object_install_node) = (
         make_named_properties_object_callbacks_and_install_node(cg_context))
    callback_defs.extend(named_properties_object_callback_defs)
    supplemental_install_node.append(named_properties_object_install_node)
    supplemental_install_node.append(EmptyNode())

    # Cross origin properties
    (cross_origin_property_callback_defs,
     cross_origin_property_install_node) = (
         make_cross_origin_property_callbacks_and_install_node(
             cg_context, attribute_entries, operation_entries))
    callback_defs.extend(cross_origin_property_callback_defs)
    supplemental_install_node.append(cross_origin_property_install_node)
    supplemental_install_node.append(EmptyNode())

    # Installer functions
    is_unconditional = lambda entry: entry.exposure_conditional.is_always_true
    is_context_dependent = lambda entry: entry.is_context_dependent
    is_context_independent = (
        lambda e: not is_context_dependent(e) and not is_unconditional(e))
    (install_unconditional_props_decl, install_unconditional_props_def,
     install_unconditional_props_trampoline) = make_install_properties(
         cg_context,
         FN_INSTALL_UNCONDITIONAL_PROPS,
         class_name=impl_class_name,
         prop_install_mode=PropInstallMode.UNCONDITIONAL,
         trampoline_var_name=tp_install_unconditional_props,
         attribute_entries=filter(is_unconditional, attribute_entries),
         constant_entries=filter(is_unconditional, constant_entries),
         exposed_construct_entries=filter(is_unconditional,
                                          exposed_construct_entries),
         operation_entries=filter(is_unconditional, operation_entries))
    (install_context_independent_props_decl,
     install_context_independent_props_def,
     install_context_independent_props_trampoline) = make_install_properties(
         cg_context,
         FN_INSTALL_CONTEXT_INDEPENDENT_PROPS,
         class_name=impl_class_name,
         prop_install_mode=PropInstallMode.CONTEXT_INDEPENDENT,
         trampoline_var_name=tp_install_context_independent_props,
         attribute_entries=filter(is_context_independent, attribute_entries),
         constant_entries=filter(is_context_independent, constant_entries),
         exposed_construct_entries=filter(is_context_independent,
                                          exposed_construct_entries),
         operation_entries=filter(is_context_independent, operation_entries))
    (install_context_dependent_props_decl, install_context_dependent_props_def,
     install_context_dependent_props_trampoline) = make_install_properties(
         cg_context,
         FN_INSTALL_CONTEXT_DEPENDENT_PROPS,
         class_name=impl_class_name,
         prop_install_mode=PropInstallMode.CONTEXT_DEPENDENT,
         trampoline_var_name=tp_install_context_dependent_props,
         attribute_entries=filter(is_context_dependent, attribute_entries),
         constant_entries=filter(is_context_dependent, constant_entries),
         exposed_construct_entries=filter(is_context_dependent,
                                          exposed_construct_entries),
         operation_entries=filter(is_context_dependent, operation_entries))
    (install_interface_template_decl, install_interface_template_def,
     install_interface_template_trampoline) = make_install_interface_template(
         cg_context,
         FN_INSTALL_INTERFACE_TEMPLATE,
         class_name=impl_class_name,
         trampoline_var_name=tp_install_interface_template,
         constructor_entries=constructor_entries,
         supplemental_install_node=supplemental_install_node,
         install_unconditional_func_name=(install_unconditional_props_def
                                          and FN_INSTALL_UNCONDITIONAL_PROPS),
         install_context_independent_func_name=(
             install_context_independent_props_def
             and FN_INSTALL_CONTEXT_INDEPENDENT_PROPS))
    installer_function_decls = ListNode([
        install_interface_template_decl,
        install_unconditional_props_decl,
        install_context_independent_props_decl,
        install_context_dependent_props_decl,
    ])
    installer_function_defs = ListNode([
        install_interface_template_def,
        EmptyNode(),
        install_unconditional_props_def,
        EmptyNode(),
        install_context_independent_props_def,
        EmptyNode(),
        install_context_dependent_props_def,
    ])
    installer_function_trampolines = ListNode([
        install_interface_template_trampoline,
        install_unconditional_props_trampoline,
        install_context_independent_props_trampoline,
        install_context_dependent_props_trampoline,
    ])

    # WrapperTypeInfo
    (get_wrapper_type_info_def, wrapper_type_info_var_def,
     wrapper_type_info_init) = make_wrapper_type_info(
         cg_context,
         "GetWrapperTypeInfo",
         has_context_dependent_props=bool(
             install_context_dependent_props_decl))

    # Cross-component trampolines
    if is_cross_components:
        (cross_component_init_decl, cross_component_init_def,
         trampoline_var_decls,
         trampoline_var_defs) = make_cross_component_init(
             cg_context,
             "Init",
             class_name=impl_class_name,
             has_unconditional_props=bool(install_unconditional_props_decl),
             has_context_independent_props=bool(
                 install_context_independent_props_decl),
             has_context_dependent_props=bool(
                 install_context_dependent_props_decl))

    # V8 Context Snapshot
    (header_v8_context_snapshot_ns,
     source_v8_context_snapshot_ns) = make_v8_context_snapshot_api(
         cg_context, impl_component, attribute_entries, constant_entries,
         constructor_entries, exposed_construct_entries, operation_entries,
         named_properties_object_callback_defs,
         cross_origin_property_callback_defs,
         (install_context_independent_props_def
          and FN_INSTALL_CONTEXT_INDEPENDENT_PROPS))

    # Header part (copyright, include directives, and forward declarations)
    api_header_node.extend([
        make_copyright_header(),
        EmptyNode(),
        enclose_with_header_guard(
            ListNode([
                make_header_include_directives(api_header_node.accumulator),
                EmptyNode(),
                api_header_blink_ns,
            ]), name_style.header_guard(api_header_path)),
    ])
    api_header_blink_ns.body.extend([
        make_forward_declarations(api_header_node.accumulator),
        EmptyNode(),
    ])
    api_source_node.extend([
        make_copyright_header(),
        EmptyNode(),
        TextNode("#include \"{}\"".format(api_header_path)),
        EmptyNode(),
        make_header_include_directives(api_source_node.accumulator),
        EmptyNode(),
        api_source_blink_ns,
    ])
    api_source_blink_ns.body.extend([
        make_forward_declarations(api_source_node.accumulator),
        EmptyNode(),
    ])
    if is_cross_components:
        impl_header_node.extend([
            make_copyright_header(),
            EmptyNode(),
            enclose_with_header_guard(
                ListNode([
                    make_header_include_directives(
                        impl_header_node.accumulator),
                    EmptyNode(),
                    impl_header_blink_ns,
                ]), name_style.header_guard(impl_header_path)),
        ])
        impl_header_blink_ns.body.extend([
            make_forward_declarations(impl_header_node.accumulator),
            EmptyNode(),
        ])
        impl_source_node.extend([
            make_copyright_header(),
            EmptyNode(),
            TextNode("#include \"{}\"".format(impl_header_path)),
            EmptyNode(),
            make_header_include_directives(impl_source_node.accumulator),
            EmptyNode(),
            impl_source_blink_ns,
        ])
        impl_source_blink_ns.body.extend([
            make_forward_declarations(impl_source_node.accumulator),
            EmptyNode(),
        ])
    api_header_node.accumulator.add_include_headers([
        interface.code_generator_info.blink_headers[0],
        component_export_header(api_component, for_testing),
        "third_party/blink/renderer/platform/bindings/v8_interface_bridge.h",
    ])
    api_source_node.accumulator.add_include_headers([
        "third_party/blink/renderer/bindings/core/v8/v8_dom_configuration.h",
    ])
    if interface.inherited:
        api_source_node.accumulator.add_include_headers(
            [PathManager(interface.inherited).api_path(ext="h")])
    if is_cross_components:
        impl_header_node.accumulator.add_include_headers([
            api_header_path,
            component_export_header(impl_component, for_testing),
        ])
    impl_source_node.accumulator.add_include_headers([
        "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h",
        "third_party/blink/renderer/bindings/core/v8/v8_dom_configuration.h",
        "third_party/blink/renderer/bindings/core/v8/v8_set_return_value_for_core.h",
        "third_party/blink/renderer/platform/bindings/exception_messages.h",
        "third_party/blink/renderer/platform/bindings/runtime_call_stats.h",
        "third_party/blink/renderer/platform/bindings/v8_binding.h",
    ])
    impl_source_node.accumulator.add_include_headers(
        _collect_include_headers(interface))

    # Assemble the parts.
    api_header_blink_ns.body.extend([
        api_class_def,
        EmptyNode(),
    ])
    if is_cross_components:
        impl_header_blink_ns.body.extend([
            impl_class_def,
            EmptyNode(),
        ])

    if constants_def:
        api_class_def.public_section.extend([
            TextNode("// Constants"),
            constants_def,
            EmptyNode(),
        ])

    api_class_def.public_section.append(get_wrapper_type_info_def)
    api_class_def.public_section.append(EmptyNode())
    api_class_def.public_section.extend([
        TextNode("// Migration adapter"),
        TextNode("static v8::Local<v8::FunctionTemplate> DomTemplate("
                 "v8::Isolate* isolate, "
                 "const DOMWrapperWorld& world);"),
        EmptyNode(),
    ])
    api_class_def.private_section.append(wrapper_type_info_var_def)
    api_class_def.private_section.append(EmptyNode())
    api_source_blink_ns.body.extend([
        wrapper_type_info_init,
        EmptyNode(),
    ])

    if is_cross_components:
        api_class_def.public_section.append(installer_function_trampolines)
        api_class_def.public_section.append(EmptyNode())
        api_class_def.private_section.extend([
            TextNode("// Cross-component trampolines"),
            trampoline_var_decls,
            EmptyNode(),
        ])
        api_source_blink_ns.body.extend([
            TextNode("// Cross-component trampolines"),
            trampoline_var_defs,
            EmptyNode(),
        ])
        impl_class_def.public_section.append(cross_component_init_decl)
        impl_class_def.private_section.append(installer_function_decls)
        impl_source_blink_ns.body.extend([
            cross_component_init_def,
            EmptyNode(),
        ])
    else:
        api_class_def.public_section.append(installer_function_decls)
        api_class_def.public_section.append(EmptyNode())

    if custom_callback_impl_decls:
        api_class_def.public_section.extend([
            TextNode("// Custom callback implementations"),
            custom_callback_impl_decls,
            EmptyNode(),
        ])

    if indexed_and_named_property_decls:
        api_class_def.public_section.extend([
            TextNode("// Indexed properties and named properties"),
            indexed_and_named_property_decls,
            EmptyNode(),
        ])
        api_source_blink_ns.body.extend([
            indexed_and_named_property_defs,
            EmptyNode(),
        ])

    impl_source_blink_ns.body.extend([
        CxxNamespaceNode(name="", body=callback_defs),
        EmptyNode(),
        installer_function_defs,
        EmptyNode(),
    ])

    if header_v8_context_snapshot_ns:
        impl_header_blink_ns.body.extend([
            CxxNamespaceNode(name=name_style.namespace("bindings"),
                             body=header_v8_context_snapshot_ns),
            EmptyNode(),
        ])
        impl_source_blink_ns.body.extend([
            CxxNamespaceNode(name=name_style.namespace("bindings"),
                             body=source_v8_context_snapshot_ns),
            EmptyNode(),
        ])

    # Write down to the files.
    write_code_node_to_file(api_header_node,
                            path_manager.gen_path_to(api_header_path))
    write_code_node_to_file(api_source_node,
                            path_manager.gen_path_to(api_source_path))
    if path_manager.is_cross_components:
        write_code_node_to_file(impl_header_node,
                                path_manager.gen_path_to(impl_header_path))
        write_code_node_to_file(impl_source_node,
                                path_manager.gen_path_to(impl_source_path))


def generate_install_properties_per_feature(function_name,
                                            filepath_basename,
                                            for_testing=False):
    assert isinstance(function_name, str)
    assert isinstance(filepath_basename, str)
    assert isinstance(for_testing, bool)

    web_idl_database = package_initializer().web_idl_database()

    # Filepaths
    header_path = PathManager.component_path("modules",
                                             "{}.h".format(filepath_basename))
    source_path = PathManager.component_path("modules",
                                             "{}.cc".format(filepath_basename))

    # Root nodes
    header_node = ListNode(tail="\n")
    header_node.set_accumulator(CodeGenAccumulator())
    header_node.set_renderer(MakoRenderer())
    source_node = ListNode(tail="\n")
    source_node.set_accumulator(CodeGenAccumulator())
    source_node.set_renderer(MakoRenderer())

    # Namespaces
    header_blink_ns = CxxNamespaceNode(name_style.namespace("blink"))
    source_blink_ns = CxxNamespaceNode(name_style.namespace("blink"))
    header_bindings_ns = CxxNamespaceNode(name_style.namespace("bindings"))
    source_bindings_ns = CxxNamespaceNode(name_style.namespace("bindings"))
    header_blink_ns.body.extend([
        make_forward_declarations(header_node.accumulator),
        EmptyNode(),
        header_bindings_ns,
    ])
    source_blink_ns.body.append(source_bindings_ns)

    # Function nodes
    arg_decls = [
        "ScriptState* script_state",
        "OriginTrialFeature feature",
    ]
    func_decl = CxxFuncDeclNode(
        name=function_name, arg_decls=arg_decls, return_type="void")
    func_def = CxxFuncDefNode(
        name=function_name, arg_decls=arg_decls, return_type="void")
    func_def.body.add_template_vars({
        "script_state": "script_state",
        "feature": "feature",
    })
    helper_func_def = CxxFuncDefNode(
        name="InstallPropertiesPerFeatureInternal",
        arg_decls=[
            "ScriptState* script_state",
            "OriginTrialFeature feature",
            "base::span<const std::pair<"
            "const WrapperTypeInfo*, InstallFuncType>> "
            "wrapper_type_info_list",
        ],
        return_type="void")

    # Assemble the parts.
    header_node.accumulator.add_class_decls(["ScriptState"])
    header_node.accumulator.add_include_headers([
        "third_party/blink/renderer/platform/runtime_enabled_features.h",
    ])
    header_node.extend([
        make_copyright_header(),
        EmptyNode(),
        enclose_with_header_guard(
            ListNode([
                make_header_include_directives(header_node.accumulator),
                EmptyNode(),
                header_blink_ns,
            ]), name_style.header_guard(header_path)),
    ])
    source_node.accumulator.add_include_headers([
        "base/containers/span.h",
        "third_party/blink/renderer/platform/bindings/script_state.h",
        "third_party/blink/renderer/platform/bindings/v8_per_context_data.h",
    ])
    source_node.extend([
        make_copyright_header(),
        EmptyNode(),
        TextNode("#include \"{}\"".format(header_path)),
        EmptyNode(),
        TextNode("#include <algorithm>"),
        EmptyNode(),
        make_header_include_directives(source_node.accumulator),
        EmptyNode(),
        source_blink_ns,
    ])
    header_bindings_ns.body.extend([
        TextNode("""\
// Install ES properties associated with the given origin trial feature.\
"""),
        func_decl,
    ])
    source_bindings_ns.body.extend([
        CxxNamespaceNode(
            name="",
            body=[
                TextNode("""\
using InstallFuncType =
    V8InterfaceBridgeBase::InstallContextDependentPropertiesFuncType;\
"""),
                EmptyNode(),
                helper_func_def,
            ]),
        EmptyNode(),
        func_def,
    ])

    # The public function
    feature_to_interfaces = {}
    set_of_interfaces = set()
    for interface in web_idl_database.interfaces:
        if interface.code_generator_info.for_testing != for_testing:
            continue

        for member in itertools.chain(interface.attributes,
                                      interface.constants,
                                      interface.operation_groups,
                                      interface.exposed_constructs):
            features = list(
                member.exposure.context_dependent_runtime_enabled_features)
            for entry in member.exposure.global_names_and_features:
                if entry.feature and entry.feature.is_context_dependent:
                    features.append(entry.feature)
            for feature in features:
                feature_to_interfaces.setdefault(feature, set()).add(interface)
            if features:
                set_of_interfaces.add(interface)

    switch_node = CxxSwitchNode(cond="${feature}")
    switch_node.append(
        case=None,
        body=[
            TextNode("// Ignore unknown, deprecated, and/or unused features."),
            TextNode("return;"),
        ],
        should_add_break=False)
    for feature, interfaces in sorted(feature_to_interfaces.items()):
        entries = [
            TextNode("{{"
                     "{0}::GetWrapperTypeInfo(), "
                     "{0}::InstallContextDependentProperties"
                     "}}, ".format(v8_bridge_class_name(interface)))
            for interface in sorted(interfaces, key=lambda x: x.identifier)
        ]
        table_def = ListNode([
            TextNode("static const std::pair<"
                     "const WrapperTypeInfo*, "
                     "InstallFuncType> wti_list[] = {"),
            ListNode(entries),
            TextNode("};"),
        ])
        switch_node.append(
            case="OriginTrialFeature::k{}".format(feature),
            body=[
                table_def,
                TextNode("selected_wti_list = wti_list;"),
            ])

    func_def.body.extend([
        TextNode("base::span<const std::pair<"
                 "const WrapperTypeInfo*, "
                 "InstallFuncType>> selected_wti_list;"),
        EmptyNode(),
        switch_node,
        EmptyNode(),
        TextNode("InstallPropertiesPerFeatureInternal"
                 "(${script_state}, ${feature}, selected_wti_list);"),
    ])

    for interface in set_of_interfaces:
        path_manager = PathManager(interface)
        source_node.accumulator.add_include_headers(
            [path_manager.api_path(ext="h")])

    # The helper function
    helper_func_def.body.append(
        TextNode("""\
V8PerContextData* per_context_data = script_state->PerContextData();
v8::Isolate* isolate = script_state->GetIsolate();
v8::Local<v8::Context> context = script_state->GetContext();
const DOMWrapperWorld& world = script_state->World();
V8InterfaceBridgeBase::FeatureSelector feature_selector(feature);

for (const auto& pair : wrapper_type_info_list) {
  const WrapperTypeInfo* wrapper_type_info = pair.first;
  InstallFuncType install_func = pair.second;

  v8::Local<v8::Object> instance_object;
  v8::Local<v8::Object> prototype_object;
  v8::Local<v8::Function> interface_object;
  v8::Local<v8::FunctionTemplate> interface_template;

  if (!per_context_data->GetExistingConstructorAndPrototypeForType(
          wrapper_type_info, &prototype_object, &interface_object)) {
    continue;
  }

  interface_template = wrapper_type_info->DomTemplate(isolate, world);
  install_func(context, world, instance_object, prototype_object,
               interface_object, interface_template, feature_selector);
}\
"""))

    # Write down to the files.
    write_code_node_to_file(header_node, path_manager.gen_path_to(header_path))
    write_code_node_to_file(source_node, path_manager.gen_path_to(source_path))


def generate_init_idl_interfaces(function_name,
                                 filepath_basename,
                                 for_testing=False):
    assert isinstance(function_name, str)
    assert isinstance(filepath_basename, str)
    assert isinstance(for_testing, bool)

    web_idl_database = package_initializer().web_idl_database()

    # Filepaths
    header_path = PathManager.component_path("modules",
                                             "{}.h".format(filepath_basename))
    source_path = PathManager.component_path("modules",
                                             "{}.cc".format(filepath_basename))

    # Root nodes
    header_node = ListNode(tail="\n")
    header_node.set_accumulator(CodeGenAccumulator())
    header_node.set_renderer(MakoRenderer())
    source_node = ListNode(tail="\n")
    source_node.set_accumulator(CodeGenAccumulator())
    source_node.set_renderer(MakoRenderer())

    # Namespaces
    header_blink_ns = CxxNamespaceNode(name_style.namespace("blink"))
    source_blink_ns = CxxNamespaceNode(name_style.namespace("blink"))
    header_bindings_ns = CxxNamespaceNode(name_style.namespace("bindings"))
    source_bindings_ns = CxxNamespaceNode(name_style.namespace("bindings"))
    header_blink_ns.body.append(header_bindings_ns)
    source_blink_ns.body.append(source_bindings_ns)

    # Function nodes
    func_decl = CxxFuncDeclNode(
        name=function_name, arg_decls=[], return_type="void")
    func_def = CxxFuncDefNode(
        name=function_name, arg_decls=[], return_type="void")
    header_bindings_ns.body.extend([
        TextNode("""\
// Initializes cross-component trampolines of IDL interface implementations.\
"""),
        func_decl,
    ])
    source_bindings_ns.body.append(func_def)

    # Assemble the parts.
    header_node.extend([
        make_copyright_header(),
        EmptyNode(),
        enclose_with_header_guard(
            ListNode([
                make_header_include_directives(header_node.accumulator),
                EmptyNode(),
                header_blink_ns,
            ]), name_style.header_guard(header_path)),
    ])
    source_node.extend([
        make_copyright_header(),
        EmptyNode(),
        TextNode("#include \"{}\"".format(header_path)),
        EmptyNode(),
        make_header_include_directives(source_node.accumulator),
        EmptyNode(),
        source_blink_ns,
    ])

    init_calls = []
    for interface in web_idl_database.interfaces:
        if interface.code_generator_info.for_testing != for_testing:
            continue

        path_manager = PathManager(interface)
        if path_manager.is_cross_components:
            source_node.accumulator.add_include_headers(
                [path_manager.impl_path(ext="h")])

            class_name = v8_bridge_class_name(interface)
            init_calls.append(_format("{}::Impl::Init();", class_name))
    for init_call in sorted(init_calls):
        func_def.body.append(TextNode(init_call))

    # Write down to the files.
    write_code_node_to_file(header_node, path_manager.gen_path_to(header_path))
    write_code_node_to_file(source_node, path_manager.gen_path_to(source_path))


def generate_interfaces(task_queue):
    assert isinstance(task_queue, TaskQueue)

    web_idl_database = package_initializer().web_idl_database()

    for interface in web_idl_database.interfaces:
        task_queue.post_task(generate_interface, interface.identifier)

    task_queue.post_task(generate_install_properties_per_feature,
                         "InstallPropertiesPerFeature",
                         "properties_per_feature_installer")
    task_queue.post_task(generate_install_properties_per_feature,
                         "InstallPropertiesPerFeatureForTesting",
                         "properties_per_feature_installer_for_testing",
                         for_testing=True)
    task_queue.post_task(generate_init_idl_interfaces, "InitIDLInterfaces",
                         "init_idl_interfaces")
    task_queue.post_task(generate_init_idl_interfaces,
                         "InitIDLInterfacesForTesting",
                         "init_idl_interfaces_for_testing",
                         for_testing=True)
