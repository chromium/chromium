# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import itertools

import web_idl

from . import name_style
from .blink_v8_bridge import blink_class_name
from .blink_v8_bridge import blink_type_info
from .blink_v8_bridge import make_default_value_expr
from .blink_v8_bridge import make_v8_to_blink_value
from .blink_v8_bridge import make_v8_to_blink_value_variadic
from .blink_v8_bridge import native_value_tag
from .blink_v8_bridge import v8_bridge_class_name
from .code_node import EmptyNode
from .code_node import FormatNode
from .code_node import ListNode
from .code_node import SequenceNode
from .code_node import SymbolDefinitionNode
from .code_node import SymbolNode
from .code_node import SymbolScopeNode
from .code_node import SymbolSensitiveSelectionNode
from .code_node import TextNode
from .code_node import WeakDependencyNode
from .code_node_cxx import CxxBlockNode
from .code_node_cxx import CxxBreakableBlockNode
from .code_node_cxx import CxxClassDefNode
from .code_node_cxx import CxxForLoopNode
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
from .codegen_expr import expr_from_exposure
from .codegen_expr import expr_not
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

    name = cg_context.member_like.code_generator_info.property_implemented_as
    if name:
        pass
    elif cg_context.constructor:
        if cg_context.is_legacy_factory_function:
            name = "CreateForJSConstructor"
        else:
            name = "Create"
    else:
        name = (cg_context.member_like.identifier
                or cg_context.property_.identifier)
        if name:
            pass
        elif cg_context.indexed_property_getter:
            name = "AnonymousIndexedGetter"
        elif cg_context.indexed_property_setter:
            name = "AnonymousIndexedSetter"
        elif cg_context.named_property_getter:
            name = "AnonymousNamedGetter"
        elif cg_context.named_property_setter:
            name = "AnonymousNamedSetter"
        elif cg_context.named_property_deleter:
            name = "AnonymousNamedDeleter"

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

    return name


def callback_function_name(cg_context,
                           overload_index=None,
                           argument_count=None,
                           for_cross_origin=False):
    """
    Args:
        cg_context: A CodeGenContext of the target IDL construct.
        overload_index: An overload index if the target is an overloaded
            IDL operation.
        argument_count: When the target is an IDL operation that has optional
            arguments and is annotated with [NoAllocDirectCall], the value is
            the number of arguments that V8 passes in (excluding the fixed
            arguments like the receiver object.)
        for_cross_origin: True if the target is the cross origin accessible
            version.
    """

    assert isinstance(cg_context, CodeGenContext)
    assert overload_index is None or isinstance(overload_index, int)
    assert argument_count is None or isinstance(argument_count, int)
    assert isinstance(for_cross_origin, bool)

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
        if cg_context.is_legacy_factory_function:
            kind = "LegacyFactoryFunction"
        else:
            property_name = ""
            kind = "Constructor"
    elif cg_context.exposed_construct:
        if cg_context.is_legacy_factory_function:
            kind = "LegacyFactoryFunctionProperty"
        elif cg_context.legacy_window_alias:
            kind = "LegacyWindowAlias"
        else:
            kind = "ExposedConstruct"
    elif cg_context.operation_group:
        if cg_context.operation_group.is_static:
            kind = "StaticOperation"
        else:
            kind = "Operation"
    elif cg_context.stringifier:
        kind = "Operation"

    if cg_context.no_alloc_direct_call:
        nadc = "NoAllocDirectCall"
    else:
        nadc = ""

    overload = ""
    if overload_index is not None and (len(cg_context.constructor_group
                                           or cg_context.operation_group) > 1):
        overload += "Overload{}".format(overload_index + 1)
    if argument_count is not None:
        overload += "Arg{}".format(argument_count)

    if for_cross_origin:
        suffix = "CrossOrigin"
    elif nadc or overload:
        suffix = nadc + overload
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


# ----------------------------------------------------------------------------
# Callback functions
# ----------------------------------------------------------------------------


def bind_blink_api_arguments(code_node, cg_context):
    assert isinstance(code_node, SymbolScopeNode)
    assert isinstance(cg_context, CodeGenContext)

    if cg_context.attribute_get:
        return

    if cg_context.is_interceptor_returning_v8intercepted:
        error_exit_return_statement = "return v8::Intercepted::kYes;"
    else:
        error_exit_return_statement = "return;"

    if cg_context.attribute_set:
        real_type = cg_context.attribute.idl_type.unwrap(typedef=True)
        if real_type.is_enumeration:
            pattern = """\
// https://webidl.spec.whatwg.org/#dfn-attribute-setter
// step 4.6.1. Let S be ? ToString(V).
const auto&& arg1_value_string =
    NativeValueTraits<IDLString>::NativeValue(
        ${isolate}, ${v8_property_value}, ${exception_state});
if (${exception_state}.HadException()) [[unlikely]] {{
  return;
}}
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
            make_v8_to_blink_value(
                name,
                v8_value,
                cg_context.attribute.idl_type,
                error_exit_return_statement=error_exit_return_statement))
        return

    for argument in cg_context.function_like.arguments:
        name = name_style.arg_f("arg{}_{}", argument.index + 1,
                                argument.identifier)
        if argument.is_variadic:
            code_node.register_code_symbol(
                make_v8_to_blink_value_variadic(name, "${info}",
                                                argument.index,
                                                argument.idl_type))
        else:
            v8_value = "${{info}}[{}]".format(argument.index)
            code_node.register_code_symbol(
                make_v8_to_blink_value(
                    name,
                    v8_value,
                    argument.idl_type,
                    argument=argument,
                    error_exit_return_statement=error_exit_return_statement,
                    cg_context=cg_context))


def bind_callback_local_vars(code_node, cg_context):
    assert isinstance(code_node, SymbolScopeNode)
    assert isinstance(cg_context, CodeGenContext)

    S = SymbolNode
    T = TextNode
    F = FormatNode

    local_vars = []
    template_vars = {}

    local_vars.extend([
        S("blink_property_name",
          ("const AtomicString& ${blink_property_name} = "
           "ToCoreAtomicString(${isolate}, ${v8_property_name});")),
        S("blink_property_index",
          ("const AtomicString& ${blink_property_index} = "
           "AtomicString::Number(${index});")),
        S("class_like_name", ("const char* const ${class_like_name} = "
                              "\"${class_like.identifier}\";")),
        S("current_context", ("v8::Local<v8::Context> ${current_context} = "
                              "${isolate}->GetCurrentContext();")),
        S("current_script_state",
          ("ScriptState* ${current_script_state} = "
           "ScriptState::From(${isolate}, ${current_context});")),
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
    ])

    is_receiver_context = not (
        (cg_context.member_like and cg_context.member_like.is_static)
        or cg_context.constructor)

    # creation_context
    pattern = "const v8::Local<v8::Context>& ${creation_context} = {_1};"
    _1 = "${receiver_context}" if is_receiver_context else "${current_context}"
    local_vars.append(S("creation_context", _format(pattern, _1=_1)))

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
              "ToExecutionContext(${current_script_state});"))
    node.accumulate(
        CodeGenAccumulator.require_include_headers([
            "third_party/blink/renderer/core/execution_context/execution_context.h"
        ]))
    local_vars.append(node)
    node = S("receiver_execution_context",
             ("ExecutionContext* ${receiver_execution_context} = "
              "ToExecutionContext(${receiver_script_state});"))
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

    # exception_context_type
    pattern = ("const v8::ExceptionContext ${exception_context_type} = "
               "{_1};")
    if cg_context.attribute_get:
        _1 = "v8::ExceptionContext::kAttributeGet"
    elif cg_context.attribute_set:
        _1 = "v8::ExceptionContext::kAttributeSet"
    elif cg_context.constructor_group:
        _1 = "v8::ExceptionContext::kConstructor"
    elif cg_context.indexed_interceptor_kind:
        _1 = "v8::ExceptionContext::kIndexed{}".format(
            cg_context.indexed_interceptor_kind)
    elif cg_context.named_interceptor_kind:
        _1 = "v8::ExceptionContext::kNamed{}".format(
            cg_context.named_interceptor_kind)
    else:
        _1 = "v8::ExceptionContext::kOperation"
    local_vars.append(S("exception_context_type", _format(pattern, _1=_1)))

    # exception_state
    def create_exception_state(symbol_node):
        node = SymbolDefinitionNode(symbol_node)

        pattern = ("{exception_state_type} ${exception_state}({init_args});")
        exception_state_type = "ExceptionState"
        init_args = ["${isolate}", "${exception_context_type}"]
        if cg_context.is_legacy_factory_function:
            init_args.append("\"{}\"".format(cg_context.property_.identifier))
        else:
            init_args.append("${class_like_name}")
        if cg_context.indexed_interceptor_kind:
            init_args.append("${blink_property_index}")
            init_args.append("ExceptionState::kForInterceptor")
        elif (cg_context.named_interceptor_kind
              and cg_context.named_interceptor_kind != "Enumerator"):
            init_args.append("${blink_property_name}")
            init_args.append("ExceptionState::kForInterceptor")
        elif (cg_context.property_ and cg_context.property_.identifier
              and not cg_context.constructor_group):
            init_args.append("${property_name}")
        node.append(
            F(pattern,
              exception_state_type=exception_state_type,
              init_args=", ".join(init_args)))
        return node

    local_vars.append(
        S("exception_state", definition_constructor=create_exception_state))

    # receiver_context
    def create_receiver_context(symbol_node):
        node = SymbolDefinitionNode(symbol_node)
        # tl;dr: This is an optimization to leverage
        # `v8::Object::GetAlignedPointerFromEmbedderDataInCreationContext`.
        # See also the comment in `create_receiver_script_state`.
        #
        # When ${receiver_script_state} is already defined,
        #     ${receiver_script_state}->GetContext()
        # is faster than
        #     ${v8_receiver}->GetCreationContextChecked()
        node.append(
            SymbolSensitiveSelectionNode([
                SymbolSensitiveSelectionNode.Choice(
                    ["receiver_script_state"],
                    T("v8::Local<v8::Context> ${receiver_context} = "
                      "${receiver_script_state}->GetContext();")),
                SymbolSensitiveSelectionNode.Choice(
                    [],
                    T("v8::Local<v8::Context> ${receiver_context} = "
                      "${v8_receiver}->GetCreationContextChecked();")),
            ]))
        return node

    local_vars.append(
        S("receiver_context", definition_constructor=create_receiver_context))

    # receiver_script_state
    def create_receiver_script_state(symbol_node):
        node = SymbolDefinitionNode(symbol_node)
        # tl;dr: This is an optimization to leverage
        # `v8::Object::GetAlignedPointerFromEmbedderDataInCreationContext`.
        #
        # If ${receiver_context} is not used at all, or if
        # ${receiver_script_state} is used before ${receiver_context} is used,
        # then
        #     v8::Object::GetAlignedPointerFromEmbedderDataInCreationContext
        #   + ScriptState::GetContext
        # i.e.
        #     ScriptState::ForRelevantRealm(v8::Local<v8::Object>)
        #   + ScriptState::GetContext
        # is faster than
        #     v8::Object::GetCreationContextChecked
        #   + ScriptState::From(v8::Isolate*, v8::Local<v8::Context>)
        # Depending on already-defined symbols, select the best way to get
        # ${receiver_script_state}.
        node.append(
            SymbolSensitiveSelectionNode([
                SymbolSensitiveSelectionNode.Choice(
                    ["receiver_context"],
                    T("ScriptState* ${receiver_script_state} = "
                      "ScriptState::From(${isolate}, ${receiver_context});")),
                SymbolSensitiveSelectionNode.Choice(
                    [],
                    T("ScriptState* ${receiver_script_state} = "
                      "ScriptState::ForRelevantRealm(${isolate}, "
                      "${v8_receiver});")),
            ]))
        return node

    local_vars.append(
        S("receiver_script_state",
          definition_constructor=create_receiver_script_state))


    # blink_receiver
    if cg_context.class_like.identifier == "Window":
        # TODO(yukishiino): Window interface should be
        # [ImplementedAs=LocalDOMWindow] instead of [ImplementedAs=DOMWindow],
        # and [CrossOrigin] properties should be implemented specifically with
        # DOMWindow class.  Then, we'll have less hacks.
        if (not cg_context.member_like or
                "CrossOrigin" in cg_context.member_like.extended_attributes):
            text = (
                "DOMWindow* ${blink_receiver} = "
                "${class_name}::ToWrappableUnsafe(${isolate},${v8_receiver});")
        else:
            # ToWrappableUnsafe will always return non-null, so we can use
            # UnsafeTo via a reference to avoid the nullptr check as well.
            text = (
                "LocalDOMWindow* ${blink_receiver} = &UnsafeTo<LocalDOMWindow>("
                "*${class_name}::ToWrappableUnsafe(${isolate},${v8_receiver}));"
            )
    else:
        pattern = (
            "{_1}* ${blink_receiver} = "
            "${class_name}::ToWrappableUnsafe(${isolate}, ${v8_receiver});")
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

    # v8_return_value
    def create_v8_return_value(symbol_node):
        return SymbolDefinitionNode(symbol_node, [
            F(
                "v8::Local<v8::Value> ${v8_return_value} = "
                "ToV8Traits<{}>::ToV8"
                "(${script_state}, ${return_value})"
                ";", native_value_tag(cg_context.return_type)),
        ])

    local_vars.append(
        S("v8_return_value", definition_constructor=create_v8_return_value))

    code_node.add_template_vars(template_vars)
    # Allow implementation-specific symbol definitions to have priority.
    for symbol_node in local_vars:
        if symbol_node.name not in code_node.own_template_vars:
            code_node.register_code_symbol(symbol_node)


def _make_throw_security_error():
    # CxxUnlikelyIfNode() is used here as a hint to the bindings generator that
    # we are relatively unlikely to reach a security error, and therefore
    # should prefer to scope variables inside the if(true). We want to lean
    # toward creating the ExceptionState inside the if(true) because it is
    # relatively expensive.
    return SequenceNode([
        TextNode("""\
// if(true) is used as part of a hint to the bindings generator. See
// _make_throw_security_error for details.\
"""),
        CxxUnlikelyIfNode(cond="true",
                          attribute=None,
                          body=TextNode(
                              "BindingSecurity::FailedAccessCheckFor("
                              "${isolate}, "
                              "${class_name}::GetWrapperTypeInfo(), "
                              "${info}.Holder(), "
                              "${exception_state});"))
    ])


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

    if cg_context.attribute_get:
        return "FastGetAttribute"
    else:
        return "setAttribute"


def _make_reflect_process_keyword_state(cg_context):
    # https://html.spec.whatwg.org/C/#keywords-and-enumerated-attributes

    assert isinstance(cg_context, CodeGenContext)
    assert cg_context.attribute_get or cg_context.attribute_set

    T = TextNode
    F = FormatNode

    if not cg_context.attribute_get:
        return None

    is_nullable = cg_context.return_type.unwrap(nullable=False).is_nullable
    ext_attrs = cg_context.attribute.extended_attributes

    def constant(keyword):
        if keyword is None and is_nullable:
            return "g_null_atom"
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

    if "ReflectMissing" in ext_attrs:
        missing_default = ext_attrs.value_of("ReflectMissing")
        branches.append(
            cond="reflect_value.IsNull()",
            body=F("${return_value} = {};", constant(missing_default)))
    elif is_nullable:
        branches.append(
            cond="reflect_value.IsNull()",
            body=T("// Null string to IDL null."))

    if "ReflectEmpty" in ext_attrs:
        empty_default = ext_attrs.value_of("ReflectEmpty")
        branches.append(cond="reflect_value.empty()",
                        body=F("${return_value} = {};",
                               constant(empty_default)))

    keywords = ext_attrs.values_of("ReflectOnly")
    expr = " || ".join(
        map(lambda keyword: "reflect_value == {}".format(constant(keyword)),
            keywords))
    branches.append(cond=expr, body=T("${return_value} = reflect_value;"))

    if "ReflectInvalid" in ext_attrs:
        invalid_default = ext_attrs.value_of("ReflectInvalid")
        branches.append(
            cond=True,
            body=F("${return_value} = {};", constant(invalid_default)))
    else:
        branches.append(cond=True,
                        body=F("${return_value} = {};", constant(None)))

    return SequenceNode(nodes)


def _make_blink_api_call(code_node,
                         cg_context,
                         num_of_args=None,
                         overriding_args=None):
    """
    Returns an expression of Blink C++ function call.

    This function doesn't create a complete C++ statement.  The returned string
    should be used to create an appropriate symbol binding like
    `bind_return_value` does.  (Actually `bind_return_value` is the only
    expected caller except for [NoAllocDirectCall] hack.)

    Args:
        code_node: A CodeNode which is supposed to contain the returned
            expression.
        cg_context: A CodeGenContext of the target IDL construct.
        num_of_args: The number of arguments to be passed to the function.
            This is used to determine which overload should be called.
        overriding_args: By default, the function is called with the arguments
            which are bound by `bind_blink_api_arguments`.  This argument has
            priority over them, and allows that the function is called with
            the explicitly given arguments.

    Returns:
        C++ expression of a function call, e.g. "func(arg1, arg2, ...)".
    """
    assert isinstance(code_node, SymbolScopeNode)
    assert isinstance(cg_context, CodeGenContext)
    assert num_of_args is None or isinstance(num_of_args, int)
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

    if cg_context.may_throw_exception:
        arguments.append("${exception_state}")

    func_name = backward_compatible_api_func(cg_context)
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

    expr = _format("{_1}({_2})", _1=func_designator, _2=", ".join(arguments))
    return expr


def bind_return_value(code_node, cg_context, overriding_args=None):
    assert isinstance(code_node, SymbolScopeNode)
    assert isinstance(cg_context, CodeGenContext)
    assert (overriding_args is None
            or (isinstance(overriding_args, (list, tuple))
                and all(isinstance(arg, str) for arg in overriding_args)))

    T = TextNode
    F = FormatNode

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
        is_return_type_void = (
            (not cg_context.return_type
             or cg_context.return_type.unwrap().is_undefined)
            and not cg_context.does_override_idl_return_type)
        is_return_type_promise = (
            cg_context.return_type
            and cg_context.return_type.unwrap().is_promise
            and not "IDLTypeImplementedAsV8Promise"
            in cg_context.return_type.unwrap().extended_attributes
            and not "PromiseIDLTypeMismatch"
            in cg_context.member_like.extended_attributes)
        if not (is_return_type_void
                or cg_context.does_override_idl_return_type):
            return_type = blink_type_info(cg_context.return_type).value_t
        if len(api_calls) == 1:
            _, api_call = api_calls[0]
            if is_return_type_void:
                nodes.append(F("{};", api_call))
            elif is_return_type_promise:
                return_type = "ScriptPromise<{}>".format(
                    native_value_tag(
                        cg_context.return_type.unwrap().result_type))
                nodes.append(
                    F("{} ${return_value} = {};", return_type, api_call))
            elif "ReflectOnly" in cg_context.member_like.extended_attributes:
                # [ReflectOnly]
                nodes.append(F("auto ${return_value} = {};", api_call))
            else:
                nodes.append(F("auto&& ${return_value} = {};", api_call))
                if (not cg_context.does_override_idl_return_type
                        and not "PromiseIDLTypeMismatch"
                        in cg_context.member_like.extended_attributes):
                    return_type = native_value_tag(cg_context.return_type)
                    idl_return_type = cg_context.return_type
                    nodes.append(
                        F(
                            "static_assert(bindings::IsReturnTypeCompatible<{}, std::remove_cvref_t<decltype(${return_value})>>, \"{}\");",
                            return_type,
                            "Return type from native call is incompatible to the type specified in IDL"
                        ))
        else:
            branches = SequenceNode()
            for index, api_call in api_calls:
                if is_return_type_void:
                    assignment = "{};".format(api_call)
                else:
                    assignment = _format("${return_value} = {};", api_call)
                if index is not None:
                    branches.append(
                        CxxLikelyIfNode(cond=_format(
                            "${non_undefined_argument_length} <= {}", index),
                                        attribute=None,
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
            if cg_context.is_interceptor_returning_v8intercepted:
                error_exit_return_statement = "return v8::Intercepted::kYes;"
            else:
                error_exit_return_statement = "return;"
            nodes.append(
                CxxUnlikelyIfNode(cond="${exception_state}.HadException()",
                                  attribute="[[unlikely]]",
                                  body=T(error_exit_return_statement)))

        if "ReflectOnly" in cg_context.member_like.extended_attributes:
            # [ReflectOnly]
            node = _make_reflect_process_keyword_state(cg_context)
            if node:
                nodes.append(EmptyNode())
                nodes.append(node)

        return SymbolDefinitionNode(symbol_node, nodes)

    code_node.register_code_symbol(
        SymbolNode("return_value", definition_constructor=create_definition))


def _make_bindings_logging_id(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    logging_id = "{}.{}".format(cg_context.class_like.identifier,
                                cg_context.property_.identifier)
    if cg_context.attribute_get:
        logging_id = "{}.{}".format(logging_id, "get")
    elif cg_context.attribute_set:
        logging_id = "{}.{}".format(logging_id, "set")
    elif (cg_context.constructor_group
          and not cg_context.is_legacy_factory_function):
        logging_id = "{}.{}".format(cg_context.class_like.identifier,
                                    "constructor")
    return logging_id


def make_bindings_trace_event(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    return TextNode("BLINK_BINDINGS_TRACE_EVENT(\"{}\");".format(
        _make_bindings_logging_id(cg_context)))


def make_check_argument_length(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    T = TextNode
    F = FormatNode

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

    return CxxUnlikelyIfNode(cond=_format("${info}.Length() < {}",
                                          num_of_required_args),
                             attribute="[[unlikely]]",
                             body=[
                                 F(("V8ThrowException::ThrowTypeError("
                                    "${isolate}, "
                                    "ExceptionMessages::NotEnoughArguments"
                                    "({}, ${info}.Length()));"),
                                   num_of_required_args),
                                 T("return;"),
                             ])


def make_check_constructor_call(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    T = TextNode

    node = SequenceNode([
        CxxUnlikelyIfNode(
            cond="!${info}.IsConstructCall()",
            attribute=None,
            body=T("V8ThrowException::ThrowTypeError(${isolate}, "
                   "ExceptionMessages::ConstructorCalledAsFunction());\n"
                   "return;")),
    ])
    if not cg_context.is_legacy_factory_function:
        node.append(
            CxxLikelyIfNode(
                cond=("ConstructorMode::Current(${isolate}) == "
                      "ConstructorMode::kWrapExistingObject"),
                attribute=None,
                body=T("bindings::V8SetReturnValue(${info}, ${v8_receiver});\n"
                       "return;")))
    node.accumulate(
        CodeGenAccumulator.require_include_headers([
            "third_party/blink/renderer/platform/bindings/v8_object_constructor.h"
        ]))
    return node


def make_check_proxy_access(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    T = TextNode

    if cg_context.class_like.identifier != "Window":
        return None

    ext_attrs = cg_context.member_like.extended_attributes
    if "CrossOrigin" not in ext_attrs:
        return None

    # COOP: restrict-properties and Partitioned Popins never restrict
    # postMessage() and closed accesses, which should still be possible across
    # browsing context groups.
    if cg_context.property_.identifier in ("postMessage", "closed"):
        return None

    values = ext_attrs.values_of("CrossOrigin")
    if cg_context.attribute_get and not (not values or "Getter" in values):
        return None
    elif cg_context.attribute_set and not ("Setter" in values):
        return None

    if cg_context.is_interceptor_returning_v8intercepted:
        error_exit_return_statement = "return v8::Intercepted::kYes;"
    else:
        error_exit_return_statement = "return;"

    node = CxxUnlikelyIfNode(
        cond=
        ("auto reason = ${blink_receiver}->GetProxyAccessBlockedReason(${isolate})"
         ),
        attribute="[[unlikely]]",
        body=[
            T("V8ThrowDOMException::Throw(${isolate}, "
              "DOMExceptionCode::kSecurityError, "
              "DOMWindow::GetProxyAccessBlockedExceptionMessage(*reason));"),
            T(error_exit_return_statement),
        ])
    node.accumulate(
        CodeGenAccumulator.require_include_headers([
            "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
        ]))
    return node


def make_promise_return_context(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    if not cg_context.is_return_type_promise_type:
        return None

    T = TextNode
    return SequenceNode([
        T("// Promise returning function: "
          "Convert a TypeError to a reject promise."),
        T("ExceptionToRejectPromiseScope reject_promise_scope(${info});"),
    ])


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
                attribute=None,
                body=T("return;")),
        ])

    if cg_context.is_return_type_promise_type:
        return SequenceNode([
            CxxUnlikelyIfNode(
                cond="!${class_name}::HasInstance(${isolate}, ${v8_receiver})",
                attribute=None,
                body=[
                    T("V8ThrowException::ThrowTypeError(${isolate}, "
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
             "ToLocalDOMWindow(${current_context}), ${return_value})")
    body = [
        T(use_counter),
        T("bindings::V8SetReturnValue(${info}, nullptr);\n"
          "return;"),
    ]
    node = SequenceNode([
        T("// [CheckSecurity=ReturnValue]"),
        CxxUnlikelyIfNode(cond=cond, attribute=None, body=body),
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

    node = TextNode("BINDINGS_COOPERATIVE_SCHEDULING_SAFEPOINT();")
    node.accumulate(
        CodeGenAccumulator.require_include_headers([
            "third_party/blink/renderer/platform/bindings/cooperative_scheduling_helpers.h"
        ]))
    return node


def make_log_activity(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    ext_attrs = cg_context.logging_target.extended_attributes
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

    pattern = ("${per_context_data}->ActivityLogger()->{_1}(${script_state}, "
               "\"{_2}.{_3}\"{_4});")
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

    pattern = ("// [LogActivity], [LogAllWorlds]\n"
               "if ({_1}) [[unlikely]] {{ {_2} }}")
    node = TextNode(_format(pattern, _1=cond, _2=body))
    node.accumulate(
        CodeGenAccumulator.require_include_headers([
            "third_party/blink/renderer/platform/bindings/v8_dom_activity_logger.h",
            "third_party/blink/renderer/platform/bindings/v8_per_context_data.h",
        ]))
    return node


def _make_overload_dispatcher_per_arg_size(cg_context, items):
    """
    https://webidl.spec.whatwg.org/#dfn-overload-resolution-algorithm

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
        func_name = callback_function_name(
            cg_context, overload_index=func_like.overload_index)
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
            node = CxxUnlikelyIfNode(cond=conditional,
                                     attribute=None,
                                     body=node)
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
        return (len(func_and_type[1].type_definition_object.
                    inclusive_inherited_interfaces),
                func_and_type[1].type_definition_object.identifier)

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
    typed_array_types = ("Int8Array", "Int16Array", "Int32Array",
                         "BigInt64Array", "Uint8Array", "Uint16Array",
                         "Uint32Array", "BigUint64Array", "Uint8ClampedArray",
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
            CxxUnlikelyIfNode(cond="${exception_state}.HadException()",
                              attribute="[[unlikely]]",
                              body=TextNode("return;")))

    # 12.10. if Type(V) is Object and ...
    func_like = find(lambda t, u: u.is_callback_interface or u.is_dictionary or
                     u.is_record or u.is_object)
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
    # https://webidl.spec.whatwg.org/#dfn-overload-resolution-algorithm

    assert isinstance(cg_context, CodeGenContext)

    T = TextNode
    F = FormatNode

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
                attribute=None,
                body=[node, T("break;") if can_fail else None])
            did_use_break = did_use_break or can_fail

        conditional = expr_or(
            list(
                map(
                    lambda item: expr_from_exposure(item.function_like.exposure
                                                    ), items)))
        if not conditional.is_always_true:
            node = CxxUnlikelyIfNode(cond=conditional,
                                     attribute=None,
                                     body=node)

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
        T("V8ThrowException::ThrowTypeError(${isolate}, "
          "\"Overload resolution failed.\");\n"
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

    name = cg_context.logging_target.extended_attributes.value_of(
        "DeprecateAs")
    if not name:
        return None

    pattern = ("// [DeprecateAs]\n"
               "Deprecation::CountDeprecation("
               "${current_execution_context}, WebFeature::k{_1});")
    _1 = name
    node = TextNode(_format(pattern, _1=_1))
    node.accumulate(
        CodeGenAccumulator.require_include_headers([
            "third_party/blink/renderer/core/frame/deprecation/deprecation.h"
        ]))
    return node


def _make_measure_web_feature_constant(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    ext_attrs = cg_context.logging_target.extended_attributes

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
        assert not name.startswith("WebDXFeature::")

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

    ext_attrs = cg_context.logging_target.extended_attributes
    if "HighEntropy" not in ext_attrs:
        return None

    node = SequenceNode([
        TextNode("// [HighEntropy]"),
        FormatNode(
            "const Dactyloscoper::HighEntropyTracer"
            "  high_entropy_tracer(\"{logging_id}\", ${info});",
            logging_id=_make_bindings_logging_id(cg_context)),
    ])
    node.accumulate(
        CodeGenAccumulator.require_include_headers([
            "third_party/blink/renderer/core/frame/dactyloscoper.h",
        ]))
    return node


def make_report_high_entropy_direct(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    ext_attrs = cg_context.logging_target.extended_attributes
    if not ext_attrs.value_of("HighEntropy") == "Direct":
        return None
    if cg_context.attribute_set:
        return None

    assert "Measure" in ext_attrs or "MeasureAs" in ext_attrs, "{}: {}".format(
        cg_context.idl_location_and_name,
        "[HighEntropy=Direct] must be specified with either [Measure] or "
        "[MeasureAs].")

    assert "MeasureAs" not in ext_attrs or not ext_attrs.value_of(
        "MeasureAs").startswith("WebDXFeature::"), "{}: {}".format(
            cg_context.idl_location_and_name,
            "[HighEntropy=Direct] is not yet supported for a WebDXFeature "
            "use counter.")

    node = SequenceNode([
        TextNode("// [HighEntropy=Direct]"),
        FormatNode(
            "Dactyloscoper::RecordDirectSurface("
            "${current_execution_context}, {measure_constant}, "
            "${return_value});",
            measure_constant=_make_measure_web_feature_constant(cg_context)),
    ])
    node.accumulate(
        CodeGenAccumulator.require_include_headers(
            ["third_party/blink/renderer/core/frame/dactyloscoper.h"]))
    return node


def make_report_measure_as(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    ext_attrs = cg_context.logging_target.extended_attributes
    if not ("Measure" in ext_attrs or "MeasureAs" in ext_attrs):
        return None

    measure_as = ext_attrs.value_of("MeasureAs")

    if measure_as and measure_as.startswith("WebDXFeature::"):
        text = _format(
            "// [Measure], [MeasureAs]\n"
            "bindings::CountWebDXFeature(${isolate}, {measure_constant});",
            measure_constant=measure_as)
    else:
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

    target = cg_context.logging_target

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

    if "CEReactions" not in cg_context.member_like.extended_attributes:
        return None

    nodes = [
        TextNode("// [CEReactions]"),
        TextNode("CEReactionsScope ce_reactions_scope;"),
    ]

    nodes[-1].accumulate(
        CodeGenAccumulator.require_include_headers([
            "third_party/blink/renderer/core/html/custom/ce_reactions_scope.h"
        ]))

    # CEReactions scope is not tolerant of V8 exception, so it's necessary to
    # invoke custom element reactions before throwing an exception.  Thus, put
    # an ExceptionState before CEReactions scope.
    nodes.insert(0, WeakDependencyNode(dep_syms=["exception_state"]))

    return SequenceNode(nodes)


def make_steps_of_put_forwards(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    T = TextNode

    if cg_context.is_interceptor_returning_v8intercepted:
        return_statement = "return v8::Intercepted::kYes;"
        error_exit_return_statement = "return v8::Intercepted::kYes;"
    else:
        return_statement = "return;"
        error_exit_return_statement = "return;"

    return SequenceNode([
        T("// [PutForwards]"),
        T("v8::Local<v8::Value> target;"),
        CxxUnlikelyIfNode(
            cond=("!${v8_receiver}->Get(${current_context}, "
                  "V8AtomicString(${isolate}, ${property_name}))"
                  ".ToLocal(&target)"),
            attribute=None,
            body=T(error_exit_return_statement),
        ),
        CxxUnlikelyIfNode(cond="!target->IsObject()",
                          attribute=None,
                          body=[
                              T("V8ThrowException::ThrowTypeError(${isolate}, "
                                "\"The attribute value is not an object\");"),
                              T(error_exit_return_statement),
                          ]),
        T("bool did_set;"),
        CxxUnlikelyIfNode(cond=(
            "!target.As<v8::Object>()->Set(${current_context}, "
            "V8AtomicString(${isolate}, "
            "\"${attribute.extended_attributes.value_of(\"PutForwards\")}\""
            "), ${v8_property_value})"
            ".To(&did_set)"),
                          attribute=None,
                          body=T(error_exit_return_statement)),
        T(return_statement)
    ])


def make_steps_of_replaceable(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    T = TextNode

    return SequenceNode([
        T("// [Replaceable]"),
        T("bool did_create;"),
        CxxUnlikelyIfNode(
            cond=("!${v8_receiver}->CreateDataProperty(${current_context}, "
                  "V8AtomicString(${isolate}, ${property_name}), "
                  "${v8_property_value}).To(&did_create)"),
            attribute=None,
            body=T("return;")),
    ])


def make_v8_set_return_value(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    T = TextNode
    F = FormatNode

    if cg_context.does_override_idl_return_type:
        return T("bindings::V8SetReturnValue(${info}, ${return_value});")

    if (not cg_context.return_type
            or cg_context.return_type.unwrap().is_undefined):
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
    if return_type.is_event_handler:
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
    # Note that the global object has its own context and there is no need to
    # pass the creation context to ToV8.
    null_context_body = [
        T("""\
// Don't wrap the return value if its frame is in the process of detaching and
// has already invalidated its v8::Context, as it is not safe to
// re-initialize the v8::Context in that state. Return null instead.\
"""),
        T("bindings::V8SetReturnValue(${info}, nullptr);"),
        T("return;")
    ]

    if (cg_context.member_like.extended_attributes.value_of("CheckSecurity") ==
            "ReturnValue"):
        node = CxxBlockNode([
            T("// [CheckSecurity=ReturnValue]"),
            F(
                "Frame* blink_frame = {};",
                "${blink_receiver}->GetFrame()->Parent()"
                if cg_context.member_like.identifier == "frameElement" else
                "${blink_receiver}->contentWindow()->GetFrame()"),
            T("DCHECK(IsA<LocalFrame>(blink_frame));"),
            CxxUnlikelyIfNode(cond=T(
                "!blink_frame->IsAttached() && "
                "To<LocalFrame>(blink_frame)"
                "->WindowProxyMaybeUninitialized("
                "${script_state}->World())->ContextIfInitialized()"
                ".IsEmpty()"),
                              attribute="[[unlikely]]",
                              body=null_context_body),
            F(
                "v8::Local<v8::Value> v8_value = "
                "ToV8Traits<{}>::ToV8("
                "ToScriptState(To<LocalFrame>(blink_frame), "
                "${script_state}->World()),"
                "${return_value});", native_value_tag(return_type)),
            T("bindings::V8SetReturnValue(${info}, v8_value);"),
        ])
        node.accumulate(
            CodeGenAccumulator.require_include_headers([
                "third_party/blink/renderer/bindings/core/v8/local_window_proxy.h",
                "third_party/blink/renderer/core/frame/local_frame.h",
            ]))
        return node
    if "NodeWrapInOwnContext" in cg_context.member_like.extended_attributes:
        assert return_type.unwrap().identifier == "Node"
        return CxxBlockNode([
            T("ExecutionContext* node_execution_context = "
              "${blink_receiver}->root()->GetExecutionContext();"),
            T("ScriptState* node_script_state = ${script_state};"),
            CxxUnlikelyIfNode(
                cond=T("node_execution_context && "
                       "${execution_context} != node_execution_context"),
                attribute="[[unlikely]]",
                body=[
                    T("node_script_state = "
                      "ToScriptState(node_execution_context, "
                      "${script_state}->World());"),
                    CxxUnlikelyIfNode(cond=T("!node_script_state"),
                                      attribute="[[unlikely]]",
                                      body=null_context_body)
                ]),
            T("// [NodeWrapInOwnContext]"),
            F(
                "v8::Local<v8::Value> v8_value = "
                "ToV8Traits<{}>::ToV8(node_script_state, ${return_value});",
                native_value_tag(return_type)),
            T("bindings::V8SetReturnValue(${info}, v8_value);")
        ])

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

    if return_type_body.is_string or return_type_body.is_enumeration:
        args = ["${info}", "${return_value}", "${isolate}"]
        if return_type.is_nullable:
            args.append("bindings::V8ReturnValue::kNullable")
        else:
            args.append("bindings::V8ReturnValue::kNonNullable")
        return T("bindings::V8SetReturnValue({});".format(", ".join(args)))

    if return_type_body.is_interface:
        args = ["${info}", "${return_value}"]
        if (return_type_body.identifier == "Window"
                or return_type_body.identifier == "Location"):
            args.append("${blink_receiver}")
            args.append("bindings::V8ReturnValue::kMaybeCrossOrigin")
        elif cg_context.constructor or cg_context.member_like.is_static:
            args.append("${creation_context}")
        elif cg_context.for_world == cg_context.MAIN_WORLD:
            args.append("bindings::V8ReturnValue::kMainWorld")
        else:
            args.append("${blink_receiver}")
        return T("bindings::V8SetReturnValue({});".format(", ".join(args)))

    if return_type_body.is_observable_array:
        return T("bindings::V8SetReturnValue"
                 "(${info}, ${return_value}->GetExoticObject(), "
                 "${blink_receiver});")

    if return_type_body.is_async_iterator or return_type_body.is_sync_iterator:
        # Async iterator objects and sync iterator objects (default iterator
        # objects, map iterator objects, and set iterator objects) are
        # implemented as ScriptWrappable instances.
        return T("bindings::V8SetReturnValue(${info}, ${return_value}, "
                 "${blink_receiver});")

    if return_type.is_promise:
        return T("bindings::V8SetReturnValue(${info}, ${return_value});")

    if return_type.is_any or return_type_body.is_object:
        return T("bindings::V8SetReturnValue(${info}, ${return_value});")

    return T("bindings::V8SetReturnValue(${info}, ${v8_return_value});")


def _make_empty_callback_def(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    return_type = "void"
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
    elif (cg_context.v8_callback_type ==
          CodeGenContext.V8_NAMED_PROPERTY_GETTER_CALLBACK):
        return_type = "v8::Intercepted"
        arg_decls = [
            "v8::Local<v8::Name> v8_property_name",
            "const v8::PropertyCallbackInfo<v8::Value>& info",
        ]
        arg_names = ["v8_property_name", "info"]
    elif (cg_context.v8_callback_type ==
          CodeGenContext.V8_NAMED_PROPERTY_SETTER_CALLBACK):
        return_type = "v8::Intercepted"
        arg_decls = [
            "v8::Local<v8::Name> v8_property_name",
            "v8::Local<v8::Value> v8_property_value",
            "const v8::PropertyCallbackInfo<void>& info",
        ]
        arg_names = ["v8_property_name", "v8_property_value", "info"]

    func_def = CxxFuncDefNode(name=function_name,
                              arg_decls=arg_decls,
                              return_type=return_type)
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
        make_promise_return_context(cg_context),
        make_check_receiver(cg_context),
        EmptyNode(),
        make_runtime_call_timer_scope(cg_context),
        make_bindings_trace_event(cg_context),
        make_report_coop_access(cg_context),
        make_report_deprecate_as(cg_context),
        make_report_high_entropy(cg_context),
        make_report_measure_as(cg_context),
        make_log_activity(cg_context),
        EmptyNode(),
        make_check_proxy_access(cg_context),
        EmptyNode(),
        make_return_value_cache_return_early(cg_context),
        EmptyNode(),
        make_check_security_of_return_value(cg_context),
        make_v8_set_return_value(cg_context),
        make_report_high_entropy_direct(cg_context),
        make_return_value_cache_update_value(cg_context),
    ])
    if cg_context.is_interceptor_returning_v8intercepted:
        body.append(TextNode("return v8::Intercepted::kYes;"))

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
        make_report_high_entropy(cg_context),
        make_report_measure_as(cg_context),
        make_log_activity(cg_context),
        EmptyNode(),
    ])

    # Binary size reduction hack
    # 1. Drop the check of argument length although this is a violation of
    #   Web IDL.
    # 2. Leverage the nature of [LegacyTreatNonObjectAsNull] (ES to IDL
    #   conversion never fails).
    if cg_context.attribute.idl_type.is_event_handler:
        body.append(
            TextNode("""\
EventListener* event_handler = JSEventHandler::CreateOrNull(
    ${v8_property_value},
    JSEventHandler::HandlerType::k${attribute.idl_type.identifier});\
"""))
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
            elif key in ("Affects", "CrossOriginIsolated", "DeprecateAs",
                         "Exposed", "InjectionMitigated", "IsolatedContext",
                         "LogActivity", "LogAllWorlds", "Measure", "MeasureAs",
                         "ReflectEmpty", "ReflectInvalid", "ReflectMissing",
                         "ReflectOnly", "RuntimeCallStatsCounter",
                         "RuntimeEnabled", "SecureContext", "URL",
                         "Unscopable"):
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
        elif idl_type.type_name == "StringLegacyNullToEmptyString":
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
    ])

    if cg_context.attribute.idl_type.unwrap(typedef=True).is_observable_array:
        # Make an expression of "attribute get" instead of "attribute set" in
        # order to acquire the observable array (backing list) object.
        attribute_get_call = _make_blink_api_call(
            body, cg_context.make_copy(attribute_get=True,
                                       attribute_set=False))
        body.extend([
            FormatNode("auto&& observable_array = {attribute_get_call};",
                       attribute_get_call=attribute_get_call),
            TextNode("observable_array->PerformAttributeSet("
                     "${script_state}, ${v8_property_value}, "
                     "${exception_state});"),
        ])
        return func_def

    body.append(make_v8_set_return_value(cg_context))

    if cg_context.is_interceptor_returning_v8intercepted:
        body.append(TextNode("return v8::Intercepted::kYes;"))

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
        body.append(make_promise_return_context(cg_context))
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
        # The constructor callback is installed with
        # v8::FunctionTemplate::SetCallHandler, where no way to control the
        # installation context-by-context. So, we check the exposure and may
        # throw a TypeError if not exposed. For the case of multiple overloads,
        # the overload resolution is already exposure sensitive.
        body.append(make_constructor_entry(cg_context))
        if cg_context.constructor.exposure.is_context_dependent():
            body.append(
                CxxUnlikelyIfNode(cond=expr_not(
                    expr_from_exposure(cg_context.constructor.exposure)),
                                  attribute=None,
                                  body=[
                                      T("V8ThrowException::ThrowTypeError("
                                        "${isolate}, "
                                        "\"Illegal constructor\");"),
                                      T("return;"),
                                  ]))
        body.append(EmptyNode())

    body.extend([
        make_report_deprecate_as(cg_context),
        make_report_high_entropy(cg_context),
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

    body.extend([
        make_report_high_entropy_direct(cg_context),
    ])

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
                cgc,
                callback_function_name(
                    cgc, overload_index=constructor.overload_index)),
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

    if (cg_context.exposed_construct.is_interface
            or cg_context.exposed_construct.is_callback_interface):
        tag = "bindings::V8ReturnValue::kInterfaceObject"
    elif cg_context.exposed_construct.is_namespace:
        tag = "bindings::V8ReturnValue::kNamespaceObject"
    else:
        assert False
    v8_set_return_value = _format(
        "bindings::V8SetReturnValue"
        "(${info}, {bridge}::GetWrapperTypeInfo(), {tag});",
        bridge=v8_bridge_class_name(cg_context.exposed_construct),
        tag=tag)
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


def make_legacy_factory_function_property_callback_def(cg_context,
                                                       function_name):
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
    cgc = CodeGenContext(interface=constructor_group.owner,
                         constructor_group=constructor_group,
                         is_legacy_factory_function=True,
                         class_name=named_ctor_v8_bridge)
    named_ctor_name = callback_function_name(cgc)
    named_ctor_def = make_constructor_callback_def(cgc, named_ctor_name)

    return_value_cache_return_early = """\
static const V8PrivateProperty::SymbolKey
    kPrivatePropertyLegacyFactoryFunction;
auto&& v8_private_legacy_factory_function =
    V8PrivateProperty::GetSymbol(
        ${isolate},
        kPrivatePropertyLegacyFactoryFunction);
v8::Local<v8::Value> v8_legacy_factory_function;
if (!v8_private_legacy_factory_function.GetOrUndefined(${v8_receiver})
         .ToLocal(&v8_legacy_factory_function)) {
  return;
}
if (!v8_legacy_factory_function->IsUndefined()) {
  bindings::V8SetReturnValue(${info}, v8_legacy_factory_function);
  return;
}
"""

    pattern = """\
v8::Local<v8::Value> v8_value;
if (!bindings::CreateLegacyFactoryFunctionFunction(
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
    create_legacy_factory_function_function = _format(
        pattern,
        callback=named_ctor_name,
        func_name=constructor_group.identifier,
        func_length=constructor_group.min_num_of_required_arguments,
        v8_bridge=named_ctor_v8_bridge)

    return_value_cache_update_value = """\
v8_private_legacy_factory_function.Set(${v8_receiver}, v8_value);
"""

    body.extend([
        TextNode(return_value_cache_return_early),
        TextNode(create_legacy_factory_function_function),
        TextNode(return_value_cache_update_value),
    ])

    return SequenceNode([named_ctor_def, EmptyNode(), func_def])


def list_no_alloc_direct_call_callbacks(cg_context):
    """
    Returns a list of [NoAllocDirectCall] callback functions to be registered
    at V8, including all overloaded operations annotated with
    [NoAllocDirectCall] and their variants of optional arguments.

    Example:
      Given the following Web IDL fragments,
        undefined f(DOMString);                            // (a)
        [NoAllocDirectCall] undefined f(Node node);        // (b)
        [NoAllocDirectCall] undefined f(optional long a,
                                        optional long b);  // (c)
      the following callback functions should be generated,
        void F(v8::Local<v8::Value> node);  // (b)
        void F();                           // (c)
        void F(int32_t a);                  // (c)
        void F(int32_t a, int32_t b);       // (c)
      thus the following entries are returned.
        [
          Entry(operation=(b), argument_count=1),  # overload_index=2
          Entry(operation=(c), argument_count=2),  # overload_index=3
          Entry(operation=(c), argument_count=1),  # overload_index=3
          Entry(operation=(c), argument_count=0),  # overload_index=3
        ]
    """
    assert isinstance(cg_context, CodeGenContext)

    class Entry(object):
        def __init__(self, operation, argument_count):
            self.operation = operation
            self.argument_count = argument_count
            self.callback_name = callback_function_name(
                cg_context,
                overload_index=self.operation.overload_index,
                argument_count=self.argument_count)

    entries = []
    for operation in cg_context.operation_group:
        if "NoAllocDirectCall" not in operation.extended_attributes:
            continue
        for argument in reversed(operation.arguments):
            entries.append(Entry(operation, argument.index + 1))
            if not argument.is_optional:
                break
        else:
            entries.append(Entry(operation, 0))
    return entries


def make_no_alloc_direct_call_callback_def(cg_context, function_name,
                                           argument_count):
    """
    Args:
        cg_context: A CodeGenContext of the target IDL construct.
        function_name: The function name to be produced.
        argument_count: The number of arguments that the produced function
            takes, which may be different from the number of arguments of
            the target cg_context.function_like due to optional arguments.
    """
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)
    assert isinstance(argument_count, int)

    S = SymbolNode
    T = TextNode
    F = FormatNode

    function_like = cg_context.function_like

    class ArgumentInfo(object):
        def __init__(self, v8_type, v8_arg_name, blink_arg_name, symbol_node):
            self.v8_type = v8_type
            self.v8_arg_name = v8_arg_name
            self.blink_arg_name = blink_arg_name
            self.symbol_node = symbol_node

    def v8_type_and_symbol_node(argument, v8_arg_name, blink_arg_name):
        unwrapped_idl_type = argument.idl_type.unwrap()
        if "PassAsSpan" in argument.idl_type.effective_annotations:
            return ("v8::Local<v8::Value>",
                    make_v8_to_blink_value(
                        blink_arg_name,
                        "${{{}}}".format(v8_arg_name),
                        argument.idl_type,
                        argument=argument,
                        error_exit_return_statement="return;",
                        cg_context=cg_context))
        if unwrapped_idl_type.is_interface or unwrapped_idl_type.is_sequence:
            return ("v8::Local<v8::Value>" if unwrapped_idl_type.is_interface
                    else "v8::Local<v8::Array>",
                    make_v8_to_blink_value(
                        blink_arg_name,
                        "${{{}}}".format(v8_arg_name),
                        argument.idl_type,
                        argument=argument,
                        error_exit_return_statement="return;",
                        cg_context=cg_context))
        else:
            return (blink_type_info(argument.idl_type).value_t,
                    S(blink_arg_name,
                      "auto&& {} = {};".format(blink_arg_name, v8_arg_name)))

    arg_list = []
    for argument in function_like.arguments:
        if not (argument.index < argument_count):
            break
        blink_arg_name = name_style.arg_f("arg{}_{}", argument.index + 1,
                                          argument.identifier)
        v8_arg_name = name_style.arg_f("v8_arg{}_{}", argument.index + 1,
                                       argument.identifier)
        v8_type, symbol_node = v8_type_and_symbol_node(argument, v8_arg_name,
                                                       blink_arg_name)

        arg_list.append(
            ArgumentInfo(v8_type, v8_arg_name, blink_arg_name, symbol_node))

    arg_decls = (["v8::Local<v8::Object> v8_arg0_receiver"] + list(
        map(lambda arg: "{} {}".format(arg.v8_type, arg.v8_arg_name),
            arg_list)) +
                 ["v8::FastApiCallbackOptions& v8_arg_callback_options"])
    return_type = ("void" if function_like.return_type.is_undefined else
                   blink_type_info(function_like.return_type).value_t)

    func_def = CxxFuncDefNode(name=function_name,
                              arg_decls=arg_decls,
                              return_type=return_type)
    func_def.set_base_template_vars(cg_context.template_bindings())
    body = func_def.body
    for arg in arg_list:
        body.add_template_var(arg.v8_arg_name, arg.v8_arg_name)
        body.register_code_symbol(arg.symbol_node)
    body.add_template_vars({
        "v8_arg0_receiver": "v8_arg0_receiver",
        "v8_arg_callback_options": "v8_arg_callback_options"
    })
    body.register_code_symbols([
        S("blink_receiver", (_format(
            "{}* ${blink_receiver} = "
            "${class_name}::ToWrappableUnsafe(${isolate}, ${v8_receiver});",
            blink_class_name(cg_context.interface)))),
        S("isolate",
          "v8::Isolate* ${isolate} = ${v8_arg_callback_options}.isolate;"),
        S("v8_receiver", ("v8::Local<v8::Object> ${v8_receiver} = "
                          "${v8_arg0_receiver};")),
    ])
    bind_callback_local_vars(body, cg_context)

    body.extend([
        T("v8::HandleScope handle_scope(${isolate});"),
        EmptyNode(),
    ])

    # If [CallWith=Isolate] is specified, make sure ${isolate} is passed first.
    blink_arguments = list()
    if "Isolate" in cg_context.member_like.extended_attributes.values_of(
            "CallWith"):
        blink_arguments.append("${isolate}")

    # Append the method arguments next.
    blink_arguments += list(
        map(lambda arg: "${{{}}}".format(arg.blink_arg_name), arg_list))

    # If there are trailing optional arguments with default values, append
    # them filled with the default values.
    for argument in function_like.arguments[argument_count:]:
        if not argument.default_value:
            break
        blink_arg_name = name_style.arg_f("arg{}_{}", argument.index + 1,
                                          argument.identifier)
        default_expr = make_default_value_expr(argument.idl_type,
                                               argument.default_value)
        body.register_code_symbol(
            S((blink_arg_name),
              "auto&& {}{{{}}};".format(blink_arg_name,
                                        default_expr.initializer_expr)))
        blink_arguments.append("${{{}}}".format(blink_arg_name))

    # Pass ${exception_state} after the method arguments.
    if cg_context.may_throw_exception:
        blink_arguments.append("${exception_state}")

    is_return_type_void = function_like.return_type.is_undefined

    if is_return_type_void:
        body.append(
            F("${blink_receiver}->{member_func}({blink_arguments});",
              member_func=backward_compatible_api_func(cg_context),
              blink_arguments=", ".join(blink_arguments)))
    else:
        body.append(
            F("auto&& return_value = ${blink_receiver}->{member_func}({blink_arguments});",
              member_func=backward_compatible_api_func(cg_context),
              blink_arguments=", ".join(blink_arguments)))
    if cg_context.may_throw_exception:
        body.append(
            CxxUnlikelyIfNode(cond="${exception_state}.HadException()",
                              attribute="[[unlikely]]",
                              body=T("return;")))

    if not is_return_type_void:
        body.extend([T("return return_value;")])

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
        body.append(make_promise_return_context(cg_context))
        body.append(EmptyNode())

    body.extend([
        make_check_receiver(cg_context),
        EmptyNode(),
        make_report_coop_access(cg_context),
        make_report_deprecate_as(cg_context),
        make_report_high_entropy(cg_context),
        make_report_measure_as(cg_context),
        make_log_activity(cg_context),
        EmptyNode(),
        make_check_proxy_access(cg_context),
        EmptyNode(),
        make_check_argument_length(cg_context),
        EmptyNode(),
        make_steps_of_ce_reactions(cg_context),
        EmptyNode(),
        make_check_security_of_return_value(cg_context),
        make_v8_set_return_value(cg_context),
        make_report_high_entropy_direct(cg_context),
    ])

    return func_def


def make_operation_callback_def(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    operation_group = cg_context.operation_group

    nodes = SequenceNode()
    if "NoAllocDirectCall" in operation_group.extended_attributes:
        for entry in list_no_alloc_direct_call_callbacks(cg_context):
            cgc = cg_context.make_copy(operation=entry.operation,
                                       no_alloc_direct_call=True)
            nodes.extend([
                make_no_alloc_direct_call_callback_def(
                    cgc,
                    callback_function_name(
                        cgc,
                        overload_index=entry.operation.overload_index,
                        argument_count=entry.argument_count),
                    argument_count=entry.argument_count),
                EmptyNode(),
            ])

    if len(operation_group) == 1:
        nodes.append(
            make_operation_function_def(
                cg_context.make_copy(operation=operation_group[0]),
                function_name))
        return nodes

    for operation in operation_group:
        cgc = cg_context.make_copy(operation=operation)
        nodes.extend([
            make_operation_function_def(
                cgc,
                callback_function_name(
                    cgc, overload_index=operation.overload_index)),
            EmptyNode(),
        ])
    nodes.append(
        make_overload_dispatcher_function_def(cg_context, function_name))
    return nodes


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


def _make_interceptor_callback(cg_context, function_name, return_type,
                               arg_decls, arg_names, class_name,
                               runtime_call_timer_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)
    assert isinstance(arg_decls, (list, tuple))
    assert all(isinstance(arg_decl, str) for arg_decl in arg_decls)
    assert isinstance(arg_names, (list, tuple))
    assert all(isinstance(arg_name, str) for arg_name in arg_names)
    assert _is_none_or_str(class_name)
    assert isinstance(runtime_call_timer_name, str)

    func_decl = CxxFuncDeclNode(name=function_name,
                                arg_decls=arg_decls,
                                return_type=return_type,
                                static=True)

    func_def = _make_interceptor_callback_def(cg_context, function_name,
                                              return_type, arg_decls,
                                              arg_names, class_name,
                                              runtime_call_timer_name)

    return func_decl, func_def


def _make_interceptor_callback_def(cg_context, function_name, return_type,
                                   arg_decls, arg_names, class_name,
                                   runtime_call_timer_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)
    assert isinstance(arg_decls, (list, tuple))
    assert all(isinstance(arg_decl, str) for arg_decl in arg_decls)
    assert isinstance(arg_names, (list, tuple))
    assert all(isinstance(arg_name, str) for arg_name in arg_names)
    assert _is_none_or_str(class_name)
    assert isinstance(runtime_call_timer_name, str)

    func_def = CxxFuncDefNode(name=function_name,
                              arg_decls=arg_decls,
                              return_type=return_type,
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


def _make_interceptor_callback_args(cg_context, named_or_indexed,
                                    callback_type):
    return_type = "v8::Intercepted"
    arg_decls = []
    arg_names = []

    # name/index parameter is used for every interceptor except Enumerator.
    if callback_type != "Enumerator":
        if named_or_indexed == "Named":
            arg_decls.append("v8::Local<v8::Name> v8_property_name")
            arg_names.append("v8_property_name")
        elif named_or_indexed == "Indexed":
            arg_decls.append("uint32_t index")
            arg_names.append("index")
        else:
            assert False

    if callback_type == "Getter":
        callback_info_type = "v8::Value"
    elif callback_type == "Setter":
        arg_decls.append("v8::Local<v8::Value> v8_property_value")
        arg_names.append("v8_property_value")
        callback_info_type = "void"
    elif callback_type == "Query":
        callback_info_type = "v8::Integer"
    elif callback_type == "Deleter":
        callback_info_type = "v8::Boolean"
    elif callback_type == "Enumerator":
        return_type = "void"
        callback_info_type = "v8::Array"
    elif callback_type == "Definer":
        arg_decls.append("const v8::PropertyDescriptor& v8_property_desc")
        arg_names.append("v8_property_desc")
        callback_info_type = "void"
    elif callback_type == "Descriptor":
        callback_info_type = "v8::Value"
    else:
        assert False
    arg_decls.append(
        _format("const v8::PropertyCallbackInfo<{}>& info",
                callback_info_type))
    arg_names.append("info")

    return return_type, arg_decls, arg_names


def make_indexed_property_getter_callback(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    return_type, arg_decls, arg_names = _make_interceptor_callback_args(
        cg_context, "Indexed", "Getter")
    func_decl, func_def = _make_interceptor_callback(cg_context, function_name,
                                                     return_type, arg_decls,
                                                     arg_names,
                                                     cg_context.class_name,
                                                     "IndexedPropertyGetter")
    body = func_def.body

    if not cg_context.interface.indexed_and_named_properties.indexed_getter:
        body.append(
            TextNode("""\
return ${class_name}::NamedPropertyGetterCallback(
    V8AtomicString(${isolate}, ${blink_property_index}), ${info});
"""))
        return func_decl, func_def

    bind_return_value(body, cg_context, overriding_args=["${index}"])

    body.extend([
        TextNode("""\
// LegacyPlatformObjectGetOwnProperty
// https://webidl.spec.whatwg.org/#LegacyPlatformObjectGetOwnProperty
// step 1.2. If index is a supported property index, then:\
"""),
        CxxUnlikelyIfNode(cond="${index} >= ${blink_receiver}->length()",
                          attribute=None,
                          body=TextNode("""\
// step 3. Return OrdinaryGetOwnProperty(O, P).
// Do not intercept.  Fallback to OrdinaryGetOwnProperty.
return v8::Intercepted::kNo;\
""")),
        make_v8_set_return_value(cg_context),
        TextNode("return v8::Intercepted::kYes;"),
    ])

    return func_decl, func_def


def make_indexed_property_setter_callback(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    return_type, arg_decls, arg_names = _make_interceptor_callback_args(
        cg_context, "Indexed", "Setter")
    func_decl, func_def = _make_interceptor_callback(cg_context, function_name,
                                                     return_type, arg_decls,
                                                     arg_names,
                                                     cg_context.class_name,
                                                     "IndexedPropertySetter")
    body = func_def.body

    if not cg_context.interface.indexed_and_named_properties.indexed_getter:
        body.append(
            TextNode("""\
return ${class_name}::NamedPropertySetterCallback(
    V8AtomicString(${isolate}, ${blink_property_index}), ${v8_property_value},
    ${info});
"""))
        return func_decl, func_def

    if not cg_context.indexed_property_setter:
        body.extend([
            TextNode("""\
// 3.9.2. [[Set]]
// https://webidl.spec.whatwg.org/#legacy-platform-object-set
// OrdinarySetWithOwnDescriptor will end up calling DefineOwnProperty,
// which will fail when the receiver object is this legacy platform
// object.\
"""),
            CxxLikelyIfNode(
                cond="${info}.ShouldThrowOnError()",
                attribute=None,
                body=TextNode(
                    "V8ThrowException::ThrowTypeError(${isolate}, "
                    "\"Indexed property setter is not supported.\");")),
            TextNode("return v8::Intercepted::kYes;"),
        ])
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
            argument=cg_context.indexed_property_setter.arguments[1],
            error_exit_return_statement="return v8::Intercepted::kYes;"))

    body.extend([
        TextNode("""\
// 3.9.2. [[Set]]
// https://webidl.spec.whatwg.org/#legacy-platform-object-set
// step 1. If O and Receiver are the same object, then:\
"""),
        CxxLikelyIfNode(cond="${info}.Holder() == ${info}.This()",
                        attribute=None,
                        body=[
                            TextNode("""\
// step 1.1.1. Invoke the indexed property setter with P and V.\
"""),
                            make_steps_of_ce_reactions(cg_context),
                            EmptyNode(),
                            make_v8_set_return_value(cg_context),
                            TextNode(
                                "return BlinkInterceptorResultToV8Intercepted("
                                "${return_value});"),
                        ]),
        EmptyNode(),
        TextNode("""\
// Do not intercept.  Fallback to OrdinarySetWithOwnDescriptor.
return v8::Intercepted::kNo;
"""),
    ])

    return func_decl, func_def


def make_indexed_property_deleter_callback(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    return_type, arg_decls, arg_names = _make_interceptor_callback_args(
        cg_context, "Indexed", "Deleter")
    func_decl, func_def = _make_interceptor_callback(cg_context, function_name,
                                                     return_type, arg_decls,
                                                     arg_names,
                                                     cg_context.class_name,
                                                     "IndexedPropertyDeleter")
    body = func_def.body

    if not cg_context.interface.indexed_and_named_properties.indexed_getter:
        body.append(
            TextNode("""\
return ${class_name}::NamedPropertyDeleterCallback(
    V8AtomicString(${isolate}, ${blink_property_index}), ${info});
"""))
        return func_decl, func_def

    body.extend([
        TextNode("""\
// 3.9.4. [[Delete]]
// https://webidl.spec.whatwg.org/#legacy-platform-object-delete
// step 1.2. If index is not a supported property index, then return true.
// step 1.3. Return false.\
"""),
        TextNode("const bool is_supported = "
                 "${index} < ${blink_receiver}->length();"),
        TextNode("bindings::V8SetReturnValue(${info}, !is_supported);"),
        CxxLikelyIfNode(cond="is_supported && ${info}.ShouldThrowOnError()",
                        attribute=None,
                        body=TextNode(
                            "V8ThrowException::ThrowTypeError(${isolate}, "
                            "\"Index property deleter is not supported.\");")),
        TextNode("return v8::Intercepted::kYes;")
    ])

    return func_decl, func_def


def make_indexed_property_definer_callback(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    return_type, arg_decls, arg_names = _make_interceptor_callback_args(
        cg_context, "Indexed", "Definer")
    func_decl, func_def = _make_interceptor_callback(cg_context, function_name,
                                                     return_type, arg_decls,
                                                     arg_names,
                                                     cg_context.class_name,
                                                     "IndexedPropertyDefiner")
    body = func_def.body

    if not cg_context.interface.indexed_and_named_properties.indexed_getter:
        body.append(
            TextNode("""\
return ${class_name}::NamedPropertyDefinerCallback(
    V8AtomicString(${isolate}, ${blink_property_index}), ${v8_property_desc},
    ${info});
"""))
        return func_decl, func_def

    body.extend([
        TextNode("""\
// 3.9.3. [[DefineOwnProperty]]
// https://webidl.spec.whatwg.org/#legacy-platform-object-defineownproperty
// step 1.1. If the result of calling IsDataDescriptor(Desc) is false, then
//   return false.\
"""),
        CxxUnlikelyIfNode(
            cond="v8_property_desc.has_get() || v8_property_desc.has_set()",
            attribute=None,
            body=[
                CxxLikelyIfNode(
                    cond="${info}.ShouldThrowOnError()",
                    attribute=None,
                    body=TextNode(
                        "V8ThrowException::ThrowTypeError(${isolate}, "
                        " \"Accessor properties are not allowed.\");")),
                TextNode("return v8::Intercepted::kYes;")
            ])
    ])

    if not cg_context.interface.indexed_and_named_properties.indexed_setter:
        body.extend([
            TextNode("""\
// step 1.2. If O does not implement an interface with an indexed property
//   setter, then return false.\
"""),
            CxxLikelyIfNode(
                cond="${info}.ShouldThrowOnError()",
                attribute=None,
                body=TextNode(
                    "V8ThrowException::ThrowTypeError(${isolate}, "
                    "\"Index property setter is not supported.\");")),
            TextNode("return v8::Intercepted::kYes;"),
        ])
    else:
        body.append(
            TextNode("""\
// step 1.3. Invoke the indexed property setter with P and Desc.[[Value]].
return ${class_name}::IndexedPropertySetterCallback(
    ${index},
    ${v8_property_desc}.has_value()
        ? ${v8_property_desc}.value()
        : v8::Undefined(${isolate}).As<v8::Value>(),
    ${info});
"""))

    return func_decl, func_def


def make_indexed_property_descriptor_callback(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    return_type, arg_decls, arg_names = _make_interceptor_callback_args(
        cg_context, "Indexed", "Descriptor")
    func_decl, func_def = _make_interceptor_callback(
        cg_context, function_name, return_type, arg_decls, arg_names,
        cg_context.class_name, "IndexedPropertyDescriptor")
    body = func_def.body

    if not cg_context.interface.indexed_and_named_properties.indexed_getter:
        body.append(
            TextNode("""\
return ${class_name}::NamedPropertyDescriptorCallback(
    V8AtomicString(${isolate}, ${blink_property_index}), ${info});
"""))
        return func_decl, func_def

    pattern = """\
// LegacyPlatformObjectGetOwnProperty
// https://webidl.spec.whatwg.org/#LegacyPlatformObjectGetOwnProperty
// step 1.2. If index is a supported property index, then:
// step 1.2.3. If operation was defined without an identifier, then set
//   value to the result of performing the steps listed in the interface
//   description to determine the value of an indexed property with index
//   as the index.
// step 1.2.4. Otherwise, operation was defined with an identifier. Set
//   value to the result of performing the steps listed in the description
//   of operation with index as the only argument value.
auto intercepted =
    ${class_name}::IndexedPropertyGetterCallback(${index}, ${info});
if (intercepted == v8::Intercepted::kNo) {{
  // step 3. Return OrdinaryGetOwnProperty(O, P).
  // Do not intercept.  Fallback to OrdinaryGetOwnProperty.
  return v8::Intercepted::kNo;
}}
// step 1.2.6. Set desc.[[Value]] to the result of converting value to an
//   ECMAScript value.
// step 1.2.7. If O implements an interface with an indexed property setter,
//   then set desc.[[Writable]] to true, otherwise set it to false.
// step 1.2.8. Set desc.[[Enumerable]] and desc.[[Configurable]] to true.
v8::Local<v8::Value> v8_value = ${info}.GetReturnValue().Get();
v8::PropertyDescriptor desc(v8_value, /*writable=*/{cxx_writable});
desc.set_enumerable(true);
desc.set_configurable(true);
bindings::V8SetReturnValue(${info}, desc);
return v8::Intercepted::kYes;
"""
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

    return_type, arg_decls, arg_names = _make_interceptor_callback_args(
        cg_context, "Indexed", "Enumerator")
    func_decl, func_def = _make_interceptor_callback(
        cg_context, function_name, return_type, arg_decls, arg_names,
        cg_context.class_name, "IndexedPropertyEnumerator")
    body = func_def.body

    body.append(
        TextNode("""\
// 3.9.6. [[OwnPropertyKeys]]
// https://webidl.spec.whatwg.org/#legacy-platform-object-ownpropertykeys
// step 2. If O supports indexed properties, then for each index of O's
//   supported property indices, in ascending numerical order, append
//   ! ToString(index) to keys.
uint32_t length = ${blink_receiver}->length();
v8::Local<v8::Array> array =
    bindings::EnumerateIndexedProperties(${isolate}, length);
bindings::V8SetReturnValue(${info}, array);
"""))
    body.accumulate(
        CodeGenAccumulator.require_include_headers([
            "third_party/blink/renderer/bindings/core/v8/generated_code_helper.h"
        ]))

    return func_decl, func_def


def make_named_property_getter_callback(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    return_type, arg_decls, arg_names = _make_interceptor_callback_args(
        cg_context, "Named", "Getter")
    func_decl, func_def = _make_interceptor_callback(cg_context, function_name,
                                                     return_type, arg_decls,
                                                     arg_names,
                                                     cg_context.class_name,
                                                     "NamedPropertyGetter")
    body = func_def.body

    bind_return_value(
        body, cg_context, overriding_args=["${blink_property_name}"])

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
        not_found_expr = "!${return_value}"
    else:
        assert False

    if cg_context.class_like.identifier == "WindowProperties":
        body.append(
            TextNode("""\
// 3.7.4.1. [[GetOwnProperty]]
// https://webidl.spec.whatwg.org/#named-properties-object-getownproperty\
"""))
    else:
        body.append(
            TextNode("""\
// LegacyPlatformObjectGetOwnProperty
// https://webidl.spec.whatwg.org/#LegacyPlatformObjectGetOwnProperty\
"""))

    body.extend([
        TextNode("""\
// "If the result of running the named property visibility
//  algorithm with property name P and object O is true, then:"\
"""),
        CxxUnlikelyIfNode(cond=not_found_expr,
                          attribute=None,
                          body=[
                              TextNode("""\
// "Return OrdinaryGetOwnProperty(O, P)."
return v8::Intercepted::kNo;\
""")
                          ]),
        TextNode("""\
% if interface.identifier == "HTMLFormElement":
// At this point we know that the named property exists.
// We then UseCount whether the original property was shadowed or not.
${blink_receiver}->UseCountPropertyAccess(${v8_property_name}, ${info});
% endif\
"""),
        TextNode("""\
// "If operation was defined without an identifier, then set value to the result
//  of performing the steps listed in the interface description to determine the
//  value of a named property with P as the name."
// "Otherwise, operation was defined with an identifier. Set value to the result
//  of performing the steps listed in the description of operation with P as the
//  only argument value."\
"""),
        make_v8_set_return_value(cg_context),
        TextNode("return v8::Intercepted::kYes;"),
    ])

    return func_decl, func_def


def make_named_property_setter_callback(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    return_type, arg_decls, arg_names = _make_interceptor_callback_args(
        cg_context, "Named", "Setter")
    func_decl, func_def = _make_interceptor_callback(cg_context, function_name,
                                                     return_type, arg_decls,
                                                     arg_names,
                                                     cg_context.class_name,
                                                     "NamedPropertySetter")
    body = func_def.body

    if not cg_context.named_property_setter:
        throw_error_nodes = [
            CxxLikelyIfNode(
                cond="${info}.ShouldThrowOnError()",
                attribute=None,
                body=TextNode(
                    "V8ThrowException::ThrowTypeError(${isolate}, "
                    "\"Named property setter is not supported.\");")),
            TextNode("return v8::Intercepted::kYes;")
        ]

        if cg_context.class_like.identifier == "WindowProperties":
            body.append(
                TextNode("""\
// 3.7.4.2. [[DefineOwnProperty]]
// https://webidl.spec.whatwg.org/#named-properties-object-defineownproperty\
"""))
            body.extend(throw_error_nodes)
            return func_decl, func_def

        body.append(
            TextNode("""\
// 3.9.2. [[Set]]
// https://webidl.spec.whatwg.org/#legacy-platform-object-set
// step 3. Perform ? OrdinarySetWithOwnDescriptor(O, P, V, Receiver, ownDesc).\
"""))
        if ("LegacyOverrideBuiltIns" in
                cg_context.interface.extended_attributes):
            body.append(
                TextNode("""\
// [LegacyOverrideBuiltIns]
if (${info}.Holder()->GetRealNamedPropertyAttributesInPrototypeChain(
        ${current_context}, ${v8_property_name}).IsJust()) {
  // Do not intercept. Fallback to the existing property.
  return v8::Intercepted::kNo;
}
"""))

        body.extend([
            TextNode("bool does_exist = ${blink_receiver}->NamedPropertyQuery("
                     "${blink_property_name}, ${exception_state});"),
            CxxUnlikelyIfNode(cond="${exception_state}.HadException()",
                              attribute="[[unlikely]]",
                              body=TextNode("return v8::Intercepted::kYes;")),
            CxxUnlikelyIfNode(cond="does_exist",
                              attribute=None,
                              body=throw_error_nodes),
            TextNode("""\
// Do not intercept. Fallback and let it define a new own property.
return v8::Intercepted::kNo;
""")
        ])
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
            argument=cg_context.named_property_setter.arguments[1],
            error_exit_return_statement="return v8::Intercepted::kYes;"))

    body.extend([
        TextNode("""\
// 3.9.2. [[Set]]
// https://webidl.spec.whatwg.org/#legacy-platform-object-set
// step 1. If O and Receiver are the same object, then:\
"""),
        CxxLikelyIfNode(cond="${info}.Holder() == ${info}.This()",
                        attribute=None,
                        body=[
                            TextNode("""\
// step 1.2.1. Invoke the named property setter with P and V.\
"""),
                            make_steps_of_ce_reactions(cg_context),
                            EmptyNode(),
                            make_v8_set_return_value(cg_context),
                            TextNode("""\
% if interface.identifier == "CSSStyleDeclaration" or \
     interface.identifier == "HTMLEmbedElement" or \
     interface.identifier == "HTMLObjectElement":
// ${interface.identifier} is abusing named properties.
// Do not intercept if the property is not found.
return BlinkInterceptorResultToV8Intercepted(${return_value});
% else:
// Pretend like the set request was intercepted regardless of the actual
// ${return_value} returned.
return v8::Intercepted::kYes;
% endif\
"""),
                        ]),
        EmptyNode(),
        TextNode("""\
// Do not intercept.  Fallback to OrdinarySetWithOwnDescriptor.
return v8::Intercepted::kNo;\
"""),
    ])

    return func_decl, func_def


def make_named_property_deleter_callback(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    return_type, arg_decls, arg_names = _make_interceptor_callback_args(
        cg_context, "Named", "Deleter")
    func_decl, func_def = _make_interceptor_callback(cg_context, function_name,
                                                     return_type, arg_decls,
                                                     arg_names,
                                                     cg_context.class_name,
                                                     "NamedPropertyDeleter")
    body = func_def.body

    props = cg_context.interface.indexed_and_named_properties

    throw_error_nodes = [
        TextNode("bindings::V8SetReturnValue(${info}, false);"),
        CxxLikelyIfNode(cond="${info}.ShouldThrowOnError()",
                        attribute=None,
                        body=TextNode(
                            "V8ThrowException::ThrowTypeError(${isolate}, "
                            "\"Named property deleter is not supported.\");")),
        TextNode("return v8::Intercepted::kYes;"),
    ]

    if cg_context.class_like.identifier == "WindowProperties":
        body.append(
            TextNode("""\
// 3.7.4.3. [[Delete]]
// https://webidl.spec.whatwg.org/#named-properties-object-delete\
"""))
        body.extend(throw_error_nodes)
        return func_decl, func_def

    if (not cg_context.named_property_deleter
            and "NotEnumerable" in props.named_getter.extended_attributes):
        body.append(
            TextNode("""\
// 3.9.4. [[Delete]]
// https://webidl.spec.whatwg.org/#legacy-platform-object-delete
// step 2. If O supports named properties, O does not implement an interface
//   with the [Global] extended attribute and the result of calling the
//   named property visibility algorithm with property name P and object O
//   is true, then:
//
// There is no easy way to determine whether the named property is visible
// or not.  Just do not intercept and fallback to the default behavior.
return v8::Intercepted::kNo;
"""))
        return func_decl, func_def

    if not cg_context.named_property_deleter:
        body.extend([
            TextNode("""\
// 3.9.4. [[Delete]]
// https://webidl.spec.whatwg.org/#legacy-platform-object-delete
// step 2. If O supports named properties, O does not implement an interface
//   with the [Global] extended attribute and the result of calling the
//   named property visibility algorithm with property name P and object O
//   is true, then:
// step 2.1. If O does not implement an interface with a named property
//   deleter, then return false.\
"""),
            TextNode("bool does_exist = ${blink_receiver}->NamedPropertyQuery("
                     "${blink_property_name}, ${exception_state});"),
            CxxUnlikelyIfNode(cond="${exception_state}.HadException()",
                              attribute="[[unlikely]]",
                              body=TextNode("return v8::Intercepted::kYes;")),
            CxxUnlikelyIfNode(cond="does_exist",
                              attribute=None,
                              body=throw_error_nodes),
            EmptyNode(),
            TextNode("""\
// Do not intercept.
return v8::Intercepted::kNo;\
""")
        ])
        return func_decl, func_def

    bind_return_value(
        body, cg_context, overriding_args=["${blink_property_name}"])

    body.extend([
        TextNode("""\
// 3.9.4. [[Delete]]
// https://webidl.spec.whatwg.org/#legacy-platform-object-delete\
"""),
        make_steps_of_ce_reactions(cg_context),
        EmptyNode(),
        make_v8_set_return_value(cg_context),
        CxxUnlikelyIfNode(
            cond="${return_value} == NamedPropertyDeleterResult::kDidNotDelete",
            attribute=None,
            body=[
                CxxLikelyIfNode(cond="${info}.ShouldThrowOnError()",
                                attribute=None,
                                body=TextNode(
                                    "V8ThrowException::ThrowTypeError("
                                    "${isolate}, "
                                    "\"Failed to delete a property.\");")),
                TextNode("return v8::Intercepted::kYes;"),
            ]),
        TextNode(
            "return BlinkInterceptorResultToV8Intercepted(${return_value});"),
    ])

    return func_decl, func_def


def make_named_property_definer_callback(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    return_type, arg_decls, arg_names = _make_interceptor_callback_args(
        cg_context, "Named", "Definer")
    func_decl, func_def = _make_interceptor_callback(cg_context, function_name,
                                                     return_type, arg_decls,
                                                     arg_names,
                                                     cg_context.class_name,
                                                     "NamedPropertyDefiner")
    body = func_def.body

    throw_error_nodes = [
        CxxLikelyIfNode(cond="${info}.ShouldThrowOnError()",
                        attribute=None,
                        body=TextNode(
                            "V8ThrowException::ThrowTypeError(${isolate}, "
                            "\"Named property setter is not supported.\");")),
        TextNode("return v8::Intercepted::kYes;")
    ]

    if cg_context.interface.identifier == "WindowProperties":
        body.append(
            TextNode("""\
// 3.7.4.2. [[DefineOwnProperty]]
// https://webidl.spec.whatwg.org/#named-properties-object-defineownproperty \
"""))
        body.extend(throw_error_nodes)
        return func_decl, func_def

    if cg_context.interface.identifier in ("CSSStyleDeclaration",
                                           "HTMLEmbedElement",
                                           "HTMLObjectElement"):
        body.append(
            TextNode("""\
// ${interface.identifier} is abusing named properties.
// Do not intercept.  Fallback to OrdinaryDefineOwnProperty.
return v8::Intercepted::kNo;
"""))
        return func_decl, func_def

    if not cg_context.interface.indexed_and_named_properties.named_setter:
        body.extend([
            TextNode("""\
// 3.9.3. [[DefineOwnProperty]]
// https://webidl.spec.whatwg.org/#legacy-platform-object-defineownproperty
// step 2.1. Let creating be true if P is not a supported property name, and
//   false otherwise.
// step 2.2.1. If creating is false and O does not implement an interface
//   with a named property setter, then return false.\
"""),
            TextNode("bool does_exist = ${blink_receiver}->NamedPropertyQuery("
                     "${blink_property_name}, ${exception_state});"),
            CxxUnlikelyIfNode(cond="${exception_state}.HadException()",
                              attribute="[[unlikely]]",
                              body=TextNode("return v8::Intercepted::kYes;")),
            CxxUnlikelyIfNode(cond="does_exist",
                              attribute=None,
                              body=throw_error_nodes),
            EmptyNode(),
            TextNode("""\
// Do not intercept. Fallback to OrdinaryDefineOwnProperty.
return v8::Intercepted::kNo;
""")
        ])
    else:
        body.extend([
            TextNode("""\
// 3.9.3. [[DefineOwnProperty]]
// https://webidl.spec.whatwg.org/#legacy-platform-object-defineownproperty
// step 2.2.2. If O implements an interface with a named property setter,
//   then:
// step 2.2.2.1. If the result of calling IsDataDescriptor(Desc) is false,
//   then return false.
"""),
            CxxUnlikelyIfNode(
                cond="v8_property_desc.has_get() || v8_property_desc.has_set()",
                attribute=None,
                body=[
                    CxxLikelyIfNode(
                        cond="${info}.ShouldThrowOnError()",
                        attribute=None,
                        body=[
                            TextNode(
                                "V8ThrowException::ThrowTypeError(${isolate}, "
                                " \"Accessor properties are not allowed.\");"),
                        ]),
                    TextNode("return v8::Intercepted::kYes;"),
                ]),
            EmptyNode(),
            TextNode("""\
// step 2.2.2.2. Invoke the named property setter with P and Desc.[[Value]].
return ${class_name}::NamedPropertySetterCallback(
    ${v8_property_name},
    ${v8_property_desc}.has_value()
        ? ${v8_property_desc}.value()
        : v8::Undefined(${isolate}).As<v8::Value>(),
    ${info});
""")
        ])

    return func_decl, func_def


def make_named_property_descriptor_callback(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    return_type, arg_decls, arg_names = _make_interceptor_callback_args(
        cg_context, "Named", "Descriptor")
    func_decl, func_def = _make_interceptor_callback(
        cg_context, function_name, return_type, arg_decls, arg_names,
        cg_context.class_name, "NamedPropertyDescriptor")
    body = func_def.body

    if cg_context.class_like.identifier == "WindowProperties":
        body.append(
            TextNode("""\
// 3.7.4.1. [[GetOwnProperty]]
// https://webidl.spec.whatwg.org/#named-properties-object-getownproperty
"""))
    else:
        body.append(
            TextNode("""\
// LegacyPlatformObjectGetOwnProperty
// https://webidl.spec.whatwg.org/#LegacyPlatformObjectGetOwnProperty
"""))

    if ("LegacyOverrideBuiltIns" not in
            cg_context.interface.extended_attributes):
        body.extend([
            TextNode("""\
// "If the result of running the named property visibility algorithm with
//  property name P and object O is true, then:"\
"""),
            TextNode("""\
if (${v8_receiver}->GetRealNamedPropertyAttributesInPrototypeChain(
        ${current_context}, ${v8_property_name}).IsJust()) {
  // Do not intercept.  Fallback to OrdinaryGetOwnProperty.
  return v8::Intercepted::kNo;
}
""")
        ])

    pattern = """\
// "If operation was defined without an identifier, then set value to the result
//  of performing the steps listed in the interface description to determine the
//  value of a named property with P as the name."
// "Otherwise, operation was defined with an identifier. Set value to the result
//  of performing the steps listed in the description of operation with P as the
//  only argument value."
auto intercepted =
    ${class_name}::NamedPropertyGetterCallback(${v8_property_name}, ${info});
// "Return OrdinaryGetOwnProperty(O, P)."
if (intercepted == v8::Intercepted::kNo) {{
  // Do not intercept.  Fallback to OrdinaryGetOwnProperty.
  return v8::Intercepted::kNo;
}}

// "Let desc be a newly created Property Descriptor with no fields."
// "Set desc.[[Value]] to the result of converting value to an ECMAScript
//  value."
// "If O implements an interface with a named property setter, then set
//  desc.[[Writable]] to true, otherwise set it to false."
// "If O implements an interface with the [LegacyUnenumerableNamedProperties]
//  extended attribute, then set desc.[[Enumerable]] to false, otherwise set
//  it to true."
// "Set desc.[[Configurable]] to true."
// "Return desc."
v8::Local<v8::Value> v8_value = ${info}.GetReturnValue().Get();
v8::PropertyDescriptor desc(v8_value, /*writable=*/{cxx_writable});
desc.set_enumerable({cxx_enumerable});
desc.set_configurable(true);
bindings::V8SetReturnValue(${info}, desc);
return v8::Intercepted::kYes;
"""
    props = cg_context.interface.indexed_and_named_properties
    # https://webidl.spec.whatwg.org/#named-properties-object-getownproperty
    # sets [[Writable]] to true for the named properties object, which is called
    # WindowProperties in our implementation.
    writable = (bool(props.named_setter)
                or cg_context.class_like.identifier == "WindowProperties")
    cxx_writable = "true" if writable else "false"
    enumerable = props.is_named_property_enumerable
    cxx_enumerable = "true" if enumerable else "false"
    body.append(
        TextNode(
            _format(pattern,
                    cxx_writable=cxx_writable,
                    cxx_enumerable=cxx_enumerable)))

    return func_decl, func_def


def make_named_property_query_callback(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    props = cg_context.interface.indexed_and_named_properties
    if "NotEnumerable" in props.named_getter.extended_attributes:
        return None, None

    return_type, arg_decls, arg_names = _make_interceptor_callback_args(
        cg_context, "Named", "Query")
    func_decl, func_def = _make_interceptor_callback(cg_context, function_name,
                                                     return_type, arg_decls,
                                                     arg_names,
                                                     cg_context.class_name,
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
        TextNode("bool does_exist = ${blink_receiver}->NamedPropertyQuery("
                 "${blink_property_name}, ${exception_state});"),
        CxxLikelyIfNode(cond="!does_exist",
                        attribute=None,
                        body=TextNode("return v8::Intercepted::kNo;")),
        TextNode(
            _format(
                "bindings::V8SetReturnValue"
                "(${info}, uint32_t({property_attribute}));",
                property_attribute=property_attribute)),
        TextNode("return v8::Intercepted::kYes;"),
    ])

    return func_decl, func_def


def make_named_property_enumerator_callback(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    props = cg_context.interface.indexed_and_named_properties
    if "NotEnumerable" in props.named_getter.extended_attributes:
        return None, None

    return_type, arg_decls, arg_names = _make_interceptor_callback_args(
        cg_context, "Named", "Enumerator")
    func_decl, func_def = _make_interceptor_callback(
        cg_context, function_name, return_type, arg_decls, arg_names,
        cg_context.class_name, "NamedPropertyEnumerator")
    body = func_def.body

    body.extend([
        TextNode("""\
// 3.9.6. [[OwnPropertyKeys]]
// https://webidl.spec.whatwg.org/#legacy-platform-object-ownpropertykeys
// step 3. If O supports named properties, then for each P of O's supported
//   property names that is visible according to the named property
//   visibility algorithm, append P to keys.\
"""),
        TextNode("Vector<String> blink_property_names;"),
        TextNode("${blink_receiver}->NamedPropertyEnumerator("
                 "blink_property_names, ${exception_state});"),
        CxxUnlikelyIfNode(cond="${exception_state}.HadException()",
                          attribute="[[unlikely]]",
                          body=TextNode("return;")),
        TextNode("""\
bindings::V8SetReturnValue(
    ${info},
    ToV8Traits<IDLSequence<IDLString>>::ToV8(${script_state},
                                             blink_property_names)
         .As<v8::Array>());
""")
    ])

    return func_decl, func_def


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

    if cg_context.interface.identifier == "Window":
        blink_class = "DOMWindow"
    else:
        blink_class = blink_class_name(cg_context.interface)

    body.extend([
        TextNode(
            _format(
                "{blink_class}* blink_accessed_object = "
                "${class_name}::ToWrappableUnsafe("
                "accessing_context->GetIsolate(),"
                "${accessed_object});",
                blink_class=blink_class)),
        TextNode("return BindingSecurity::ShouldAllowAccessTo("
                 "ToLocalDOMWindow(${accessing_context}), "
                 "blink_accessed_object);"),
    ])

    return func_def


def make_cross_origin_throwing_callback(cg_context):
    """
    This generates the following functions:
    CrossOriginNamedDeleterCallback
    CrossOriginNamedDefinerCallback
    CrossOriginIndexedSetterCallback
    CrossOriginIndexedDeleterCallback
    CrossOriginIndexedDefinerCallback
    """
    assert isinstance(cg_context, CodeGenContext)
    assert (cg_context.named_interceptor_kind
            or cg_context.indexed_interceptor_kind)
    assert (not cg_context.named_interceptor_kind
            or not cg_context.indexed_interceptor_kind)

    if cg_context.named_interceptor_kind:
        named_or_indexed = "Named"
        callback_type = cg_context.named_interceptor_kind
    else:
        named_or_indexed = "Indexed"
        callback_type = cg_context.indexed_interceptor_kind

    return_type, arg_decls, arg_names = _make_interceptor_callback_args(
        cg_context, named_or_indexed, callback_type)
    func_def = _make_interceptor_callback_def(
        cg_context, "CrossOrigin{}{}Callback".format(named_or_indexed,
                                                     callback_type),
        return_type, arg_decls, arg_names, None,
        "CrossOriginProperty_{}Property{}".format(named_or_indexed,
                                                  callback_type))

    func_def.body.extend([
        _make_throw_security_error(),
        TextNode("return v8::Intercepted::kYes;"),
    ])

    return func_def


def make_cross_origin_indexed_getter_callback(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    return_type, arg_decls, arg_names = _make_interceptor_callback_args(
        cg_context, "Indexed", "Getter")
    func_def = _make_interceptor_callback_def(
        cg_context, function_name, return_type, arg_decls, arg_names, None,
        "CrossOriginProperty_IndexedPropertyGetter")
    body = func_def.body

    if cg_context.interface.identifier != "Window":
        body.extend([
            _make_throw_security_error(),
            TextNode("return v8::Intercepted::kYes;"),
        ])
        return func_def

    bind_return_value(body, cg_context, overriding_args=["${index}"])

    # Do this before the index verification below, because we do not want to
    # reveal any information about the number of frames in this window.
    body.extend([
        make_check_proxy_access(cg_context),
        EmptyNode(),
    ])

    body.extend([
        CxxLikelyIfNode(cond="${index} >= ${blink_receiver}->length()",
                        attribute=None,
                        body=[
                            _make_throw_security_error(),
                            TextNode("return v8::Intercepted::kYes;"),
                        ]),
        make_v8_set_return_value(cg_context),
        TextNode("return v8::Intercepted::kYes;"),
    ])

    return func_def


def make_cross_origin_indexed_descriptor_callback(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    return_type, arg_decls, arg_names = _make_interceptor_callback_args(
        cg_context, "Indexed", "Descriptor")
    func_def = _make_interceptor_callback_def(
        cg_context, function_name, return_type, arg_decls, arg_names, None,
        "CrossOriginProperty_IndexedPropertyDescriptor")
    body = func_def.body

    if cg_context.interface.identifier != "Window":
        body.extend([
            _make_throw_security_error(),
            TextNode("return v8::Intercepted::kYes;"),
        ])
        return func_def

    body.append(
        TextNode("""\
auto intercepted = CrossOriginIndexedGetterCallback(${index}, ${info});
if (intercepted == v8::Intercepted::kNo) {
  return v8::Intercepted::kNo;
}
// TODO(ishell, 328490288): inline CrossOriginIndexedGetterCallback() here
// in order to avoid this non-robust way of detecting whether exception
// was thrown.
v8::Local<v8::Value> v8_value = ${info}.GetReturnValue().Get();
if (v8_value->IsUndefined()) {
  // Must have already thrown a SecurityError.
  return v8::Intercepted::kYes;
}

v8::PropertyDescriptor desc(v8_value, /*writable=*/false);
desc.set_enumerable(true);
desc.set_configurable(true);
bindings::V8SetReturnValue(${info}, desc);
return v8::Intercepted::kYes;
"""))

    return func_def


def make_cross_origin_indexed_enumerator_callback(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    return_type, arg_decls, arg_names = _make_interceptor_callback_args(
        cg_context, "Indexed", "Enumerator")
    func_def = _make_interceptor_callback_def(
        cg_context, function_name, return_type, arg_decls, arg_names, None,
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

    return_type, arg_decls, arg_names = _make_interceptor_callback_args(
        cg_context, "Named", "Getter")
    func_def = _make_interceptor_callback_def(
        cg_context, function_name, return_type, arg_decls, arg_names, None,
        "CrossOriginProperty_NamedPropertyGetter")
    body = func_def.body

    string_case_body = []
    string_case_body.extend([
        CxxForLoopNode(
            cond="const auto& attribute : kCrossOriginAttributeTable",
            body=[
                CxxLikelyIfNode(
                    cond="${blink_property_name} != attribute.name",
                    attribute=None,
                    body=TextNode("continue;")),
                CxxUnlikelyIfNode(
                    cond="!attribute.get_value",
                    attribute="[[unlikely]]",
                    body=[
                        _make_throw_security_error(),
                        TextNode("return v8::Intercepted::kYes;"),
                    ]),
                TextNode(
                    "return attribute.get_value(${v8_property_name}, ${info});"
                ),
            ]),
        CxxForLoopNode(
            cond="const auto& operation : kCrossOriginOperationTable",
            body=[
                CxxLikelyIfNode(
                    cond="${blink_property_name} != operation.name",
                    attribute=None,
                    body=TextNode("continue;")),
                TextNode("v8::Local<v8::Function> function;"),
                CxxLikelyIfNode(
                    cond="bindings::GetCrossOriginFunction("
                    "${isolate}, operation.name, operation.callback, "
                    "operation.func_length,"
                    "${class_name}::GetWrapperTypeInfo(), "
                    "v8::ExceptionContext::kOperation, ${class_like_name})"
                    ".ToLocal(&function)",
                    attribute=None,
                    body=TextNode(
                        "bindings::V8SetReturnValue(${info}, function);")),
                TextNode("return v8::Intercepted::kYes;")
            ])
    ])
    if cg_context.interface.identifier == "Window":
        string_case_body.append(
            TextNode("""\
// Window object's document-tree child browsing context name property set
auto&& return_value = ${blink_receiver}->AnonymousNamedGetter(
    ${blink_property_name});
if (!return_value.IsEmpty()) {
  bindings::V8SetReturnValue(${info}, return_value);
  return v8::Intercepted::kYes;
}
"""))

    body.extend([
        CxxLikelyIfNode(cond="${v8_property_name}->IsString()",
                        attribute=None,
                        body=string_case_body),
        EmptyNode(),
        TextNode("""\
// 7.2.3.2 CrossOriginPropertyFallback ( P )
// https://html.spec.whatwg.org/C/#crossoriginpropertyfallback-(-p-)
if (bindings::IsSupportedInCrossOriginPropertyFallback(
        ${info}.GetIsolate(), ${v8_property_name})) {
  ${info}.GetReturnValue().SetUndefined();
  return v8::Intercepted::kYes;
}
"""),
        _make_throw_security_error(),
        TextNode("return v8::Intercepted::kYes;"),
    ])

    return func_def


def make_cross_origin_named_setter_callback(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    return_type, arg_decls, arg_names = _make_interceptor_callback_args(
        cg_context, "Named", "Setter")
    func_def = _make_interceptor_callback_def(
        cg_context, function_name, return_type, arg_decls, arg_names, None,
        "CrossOriginProperty_NamedPropertySetter")
    body = func_def.body

    string_case_body = []
    string_case_body.append(
        TextNode("""\
for (const auto& attribute : kCrossOriginAttributeTable) {
  if (${blink_property_name} == attribute.name && attribute.set_value) {
    return attribute.set_value(${v8_property_name}, ${v8_property_value},
                               ${info});
  }
}
"""))

    body.extend([
        CxxLikelyIfNode(cond="${v8_property_name}->IsString()",
                        attribute=None,
                        body=string_case_body),
        EmptyNode(),
        _make_throw_security_error(),
        TextNode("return v8::Intercepted::kYes;"),
    ])

    return func_def


def make_cross_origin_named_descriptor_callback(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    return_type, arg_decls, arg_names = _make_interceptor_callback_args(
        cg_context, "Named", "Descriptor")
    func_def = _make_interceptor_callback_def(
        cg_context, function_name, return_type, arg_decls, arg_names, None,
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
  if (!bindings::GetCrossOriginGetterSetter(
           ${info}.GetIsolate(), attribute.name, attribute.get_callback, 0,
           ${class_name}::GetWrapperTypeInfo(),
           v8::ExceptionContext::kAttributeGet, ${class_like_name})
           .ToLocal(&get) ||
      !bindings::GetCrossOriginGetterSetter(
           ${info}.GetIsolate(), attribute.name, attribute.set_callback, 1,
           ${class_name}::GetWrapperTypeInfo(),
           v8::ExceptionContext::kAttributeSet, ${class_like_name})
           .ToLocal(&set)) {
    // Exception was thrown which means that the request was intercepted.
    return v8::Intercepted::kYes;
  }
  v8::PropertyDescriptor desc(get, set);
  desc.set_enumerable(false);
  desc.set_configurable(true);
  bindings::V8SetReturnValue(${info}, desc);
  return v8::Intercepted::kYes;
}
for (const auto& operation : kCrossOriginOperationTable) {
  if (${blink_property_name} != operation.name)
    continue;
  v8::Local<v8::Function> function;
  if (!bindings::GetCrossOriginFunction(
           ${info}.GetIsolate(), operation.name, operation.callback,
           operation.func_length, ${class_name}::GetWrapperTypeInfo(),
           v8::ExceptionContext::kOperation, ${class_like_name})
           .ToLocal(&function)) {
    // Exception was thrown which means that the request was intercepted.
    return v8::Intercepted::kYes;
  }
  v8::PropertyDescriptor desc(function, /*writable=*/false);
  desc.set_enumerable(false);
  desc.set_configurable(true);
  bindings::V8SetReturnValue(${info}, desc);
  return v8::Intercepted::kYes;
}
"""))
    if cg_context.interface.identifier == "Window":
        string_case_body.append(
            TextNode("""\
// Window object's document-tree child browsing context name property set
auto&& return_value = ${blink_receiver}->AnonymousNamedGetter(
    ${blink_property_name});
if (!return_value.IsEmpty()) {
  v8::PropertyDescriptor desc(return_value, /*writable=*/false);
  desc.set_enumerable(false);
  desc.set_configurable(true);
  bindings::V8SetReturnValue(${info}, desc);
  return v8::Intercepted::kYes;
}
"""))

    body.extend([
        CxxLikelyIfNode(cond="${v8_property_name}->IsString()",
                        attribute=None,
                        body=string_case_body),
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
  return v8::Intercepted::kYes;
}
"""),
        _make_throw_security_error(),
        TextNode("return v8::Intercepted::kYes;"),
    ])

    return func_def


def make_cross_origin_named_query_callback(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    return_type, arg_decls, arg_names = _make_interceptor_callback_args(
        cg_context, "Named", "Query")
    func_def = _make_interceptor_callback_def(
        cg_context, function_name, return_type, arg_decls, arg_names, None,
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
  return v8::Intercepted::kYes;
}
for (const auto& operation : kCrossOriginOperationTable) {
  if (${blink_property_name} != operation.name)
    continue;
  int32_t v8_property_attribute = v8::DontEnum | v8::ReadOnly;
  bindings::V8SetReturnValue(${info}, v8_property_attribute);
  return v8::Intercepted::kYes;
}
return v8::Intercepted::kNo;
"""))

    body.extend([
        CxxLikelyIfNode(cond="${v8_property_name}->IsString()",
                        attribute=None,
                        body=string_case_body),
        EmptyNode(),
        TextNode("""\
// 7.2.3.2 CrossOriginPropertyFallback ( P )
// https://html.spec.whatwg.org/C/#crossoriginpropertyfallback-(-p-)
if (bindings::IsSupportedInCrossOriginPropertyFallback(
        ${info}.GetIsolate(), ${v8_property_name})) {
  int32_t v8_property_attribute = v8::DontEnum | v8::ReadOnly;
  bindings::V8SetReturnValue(${info}, v8_property_attribute);
  return v8::Intercepted::kYes;
}
return v8::Intercepted::kNo;
"""),
    ])

    return func_def


def make_cross_origin_named_enumerator_callback(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    return_type, arg_decls, arg_names = _make_interceptor_callback_args(
        cg_context, "Named", "Enumerator")
    func_def = _make_interceptor_callback_def(
        cg_context, function_name, return_type, arg_decls, arg_names, None,
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
        S("is_cross_origin_isolated",
          ("const bool ${is_cross_origin_isolated} = "
           "${execution_context}"
           "->CrossOriginIsolatedCapabilityOrDisabledWebSecurity();")),
        S("is_in_injection_mitigated_context",
          ("const bool ${is_in_injection_mitigated_context} = "
           "${execution_context}->IsInjectionMitigatedContext();")),
        S("is_in_isolated_context",
          ("const bool ${is_in_isolated_context} = "
           "${execution_context}->IsIsolatedContext();")),
        S("is_in_secure_context",
          ("const bool ${is_in_secure_context} = "
           "${execution_context}->IsSecureContext();")),
        S("isolate", "v8::Isolate* ${isolate} = ${v8_context}->GetIsolate();"),
        S("script_state", ("ScriptState* ${script_state} = "
                           "ScriptState::From(${isolate}, ${v8_context});")),
        S("wrapper_type_info",
          ("const WrapperTypeInfo* const ${wrapper_type_info} = "
           "${class_name}::GetWrapperTypeInfo();")),
    ])

    if (cg_context.interface or cg_context.async_iterator
            or cg_context.sync_iterator):
        local_vars.extend([
            S("interface_function_template",
              ("v8::Local<v8::FunctionTemplate> "
               "${interface_function_template} = "
               "${interface_template}.As<v8::FunctionTemplate>();")),
            S("instance_object_template",
              ("v8::Local<v8::ObjectTemplate> ${instance_object_template} = "
               "${interface_function_template}->InstanceTemplate();")),
            S("instance_template",
              ("v8::Local<v8::Template> ${instance_template} = "
               "${instance_object_template};")),
            S("prototype_object_template",
              ("v8::Local<v8::ObjectTemplate> ${prototype_object_template} = "
               "${interface_function_template}->PrototypeTemplate();")),
            S("prototype_template",
              ("v8::Local<v8::Template> ${prototype_template} = "
               "${prototype_object_template};")),
            S("signature", ("v8::Local<v8::Signature> ${signature} = "
                            "v8::Signature::New(${isolate}, "
                            "${interface_function_template});")),
        ])
    elif cg_context.namespace:
        local_vars.extend([
            S("namespace_object_template",
              ("v8::Local<v8::ObjectTemplate> "
               "${namespace_object_template} = "
               "${interface_template}.As<v8::ObjectTemplate>();")),
            S("instance_template",
              "v8::Local<v8::Template> ${instance_template};"),
            S("prototype_template",
              "v8::Local<v8::Template> ${prototype_template};"),
            S("signature", "v8::Local<v8::Signature> ${signature};"),
        ])
    elif cg_context.callback_interface:
        local_vars.extend([
            S("interface_function_template",
              ("v8::Local<v8::FunctionTemplate> "
               "${interface_function_template} = "
               "${interface_template}.As<v8::FunctionTemplate>();")),
            S("instance_template",
              "v8::Local<v8::Template> ${instance_template};"),
            S("prototype_template",
              "v8::Local<v8::Template> ${prototype_template};"),
            S("signature", "v8::Local<v8::Signature> ${signature};"),
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
                                   "ToExecutionContext(${script_state});"))
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
    elif interface.inherited:
        _1 = (" = ${wrapper_type_info}->parent_class"
              "->GetV8ClassTemplate(${isolate}, ${world})"
              ".As<v8::FunctionTemplate>()")
    else:
        _1 = ""
    local_vars.append(S("parent_interface_template", _format(pattern, _1=_1)))

    # Arguments have priority over local vars.
    for symbol_node in local_vars:
        if symbol_node.name not in code_node.own_template_vars:
            code_node.register_code_symbol(symbol_node)


def _make_property_entry_cross_origin_check(property_,
                                            is_get=False,
                                            is_set=False):
    constants = {
        False: "unsigned(IDLMemberInstaller::FlagCrossOriginCheck::kCheck)",
        True:
        "unsigned(IDLMemberInstaller::FlagCrossOriginCheck::kDoNotCheck)",
    }
    if property_.is_static:
        return constants[True]
    if "CrossOrigin" not in property_.extended_attributes:
        return constants[False]
    values = property_.extended_attributes.values_of("CrossOrigin")
    if is_get:
        return constants[not values or "Getter" in values]
    elif is_set:
        return constants["Setter" in values]
    else:
        return constants[True]


def _make_property_entry_location(property_):
    if hasattr(property_, "is_static") and property_.is_static:
        return "unsigned(IDLMemberInstaller::FlagLocation::kInterface)"
    if "Global" in property_.owner.extended_attributes:
        return "unsigned(IDLMemberInstaller::FlagLocation::kInstance)"
    if "LegacyUnforgeable" in property_.extended_attributes:
        return "unsigned(IDLMemberInstaller::FlagLocation::kInstance)"
    return "unsigned(IDLMemberInstaller::FlagLocation::kPrototype)"


def _make_property_entry_receiver_check(property_):
    if ("LegacyLenientThis" in property_.extended_attributes
            or property_.is_static
            or (isinstance(property_, web_idl.Attribute)
                and property_.idl_type.unwrap().is_promise)
            or (isinstance(property_, web_idl.OverloadGroup)
                and property_[0].return_type.unwrap().is_promise)):
        return "unsigned(IDLMemberInstaller::FlagReceiverCheck::kDoNotCheck)"
    else:
        return "unsigned(IDLMemberInstaller::FlagReceiverCheck::kCheck)"


def _make_property_entry_v8_cached_accessor(property_):
    return "unsigned(V8PrivateProperty::CachedAccessor::{})".format(
        property_.extended_attributes.value_of("CachedAccessor") or "kNone")


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
        return "unsigned({})".format(values[0])
    else:
        return "unsigned({})".format(" | ".join(values))


def _make_property_entry_v8_side_effect(property_):
    value = property_.extended_attributes.value_of("Affects")
    if value:
        if value == "Everything":
            return "unsigned(v8::SideEffectType::kHasSideEffect)"
        elif value == "Nothing":
            return "unsigned(v8::SideEffectType::kHasNoSideEffect)"
        else:
            assert False
    elif isinstance(property_, web_idl.Attribute):
        return "unsigned(v8::SideEffectType::kHasNoSideEffect)"
    elif isinstance(property_, web_idl.Operation):
        assert property_.identifier == "toString"
        # The stringifier should have no side effect.
        return "unsigned(v8::SideEffectType::kHasNoSideEffect)"
    elif isinstance(property_, web_idl.OperationGroup):
        return "unsigned(v8::SideEffectType::kHasSideEffect)"
    assert False


def _make_property_entry_world(world):
    if world == CodeGenContext.MAIN_WORLD:
        return "unsigned(IDLMemberInstaller::FlagWorld::kMainWorld)"
    if world == CodeGenContext.NON_MAIN_WORLDS:
        return "unsigned(IDLMemberInstaller::FlagWorld::kNonMainWorlds)"
    if world == CodeGenContext.ALL_WORLDS:
        return "unsigned(IDLMemberInstaller::FlagWorld::kAllWorlds)"
    assert False


def _make_attribute_registration_table(table_name, attribute_entries):
    assert isinstance(table_name, str)
    assert isinstance(attribute_entries, (list, tuple))
    assert all(
        isinstance(entry, _PropEntryAttribute) for entry in attribute_entries)

    T = TextNode

    entry_nodes = []
    pattern = ("{{"
               "\"{property_name}\", "
               "\"${{class_like.identifier}}\", "
               "{attribute_get_callback}, "
               "{attribute_set_callback}, "
               "{v8_property_attribute}, "
               "{location}, "
               "{world}, "
               "{receiver_check}, "
               "{cross_origin_check_for_get}, "
               "{cross_origin_check_for_set}, "
               "{v8_side_effect}, "
               "{v8_cached_accessor}"
               "}},")
    for entry in attribute_entries:
        text = _format(
            pattern,
            property_name=entry.property_.identifier,
            attribute_get_callback=entry.attr_get_callback_name,
            attribute_set_callback=(entry.attr_set_callback_name or "nullptr"),
            v8_property_attribute=_make_property_entry_v8_property_attribute(
                entry.property_),
            location=_make_property_entry_location(entry.property_),
            world=_make_property_entry_world(entry.world),
            receiver_check=_make_property_entry_receiver_check(
                entry.property_),
            cross_origin_check_for_get=(
                _make_property_entry_cross_origin_check(entry.property_,
                                                        is_get=True)),
            cross_origin_check_for_set=(
                _make_property_entry_cross_origin_check(entry.property_,
                                                        is_set=True)),
            v8_side_effect=_make_property_entry_v8_side_effect(
                entry.property_),
            v8_cached_accessor=_make_property_entry_v8_cached_accessor(
                entry.property_))
        entry_nodes.append(T(text))

    return ListNode([
        T("static const IDLMemberInstaller::AttributeConfig " + table_name +
          "[] = {"),
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
    pattern = (
        "{{"  #
        "\"{property_name}\", "
        "{constant_callback}"
        "}},")
    for entry in constant_entries:
        text = _format(
            pattern,
            property_name=entry.property_.identifier,
            constant_callback=entry.const_callback_name)
        entry_nodes.append(T(text))

    return ListNode([
        T("static const IDLMemberInstaller::ConstantCallbackConfig " +
          table_name + "[] = {"),
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
    pattern = (
        "{{"  #
        "\"{property_name}\", "
        "{constant_value}"
        "}},")
    for entry in constant_entries:
        text = _format(pattern,
                       property_name=entry.property_.identifier,
                       constant_value=entry.const_constant_name)
        entry_nodes.append(T(text))

    return ListNode([
        T("static const IDLMemberInstaller::ConstantValueConfig " +
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
                   "{exposed_construct_callback}"
                   "}}, ")
        text = _format(pattern,
                       property_name=entry.property_.identifier,
                       exposed_construct_callback=entry.prop_callback_name)
        entry_nodes.append(T(text))

    return ListNode([
        T("static const IDLMemberInstaller::ExposedConstructConfig " +
          table_name + "[] = {"),
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
    F = FormatNode

    no_alloc_direct_call_count = len(
        list(
            filter(lambda entry: entry.no_alloc_direct_call_callbacks,
                   operation_entries)))
    assert (no_alloc_direct_call_count == 0
            or no_alloc_direct_call_count == len(operation_entries))
    no_alloc_direct_call_enabled = bool(no_alloc_direct_call_count)

    entry_nodes = []
    entry_nodes_wo_nadc = []
    nadc_overload_nodes = ListNode()
    pattern = ("{{"
               "\"{property_name}\", "
               "\"${{class_like.identifier}}\", "
               "{operation_callback}, "
               "{function_length}, "
               "{v8_property_attribute}, "
               "{location}, "
               "{world}, "
               "{receiver_check}, "
               "{cross_origin_check}, "
               "{v8_side_effect}"
               "}}, ")
    pattern_wo_nadc = pattern
    if no_alloc_direct_call_enabled:
        pattern = ("{{" + pattern + "{v8_cfunction_table}, "
                   "std::size({v8_cfunction_table})}}, ")
    has_no_alloc_direct_call_with_enforce_range = False
    for entry in operation_entries:
        if no_alloc_direct_call_enabled:
            nadc_overload_table_name = name_style.constant(
                "no_alloc_direct_call_overloads_of_",
                entry.property_.identifier)
            nadc_overloads = []
            for nadc_entry in entry.no_alloc_direct_call_callbacks:
                nadc_arg_flags = ""
                for arg in nadc_entry.operation.arguments:
                    if arg.index >= nadc_entry.argument_count:
                        break
                    nadc_v8_type_info_flags = []
                    if "Clamp" in arg.idl_type.effective_annotations:
                        nadc_v8_type_info_flags.append(
                            "v8::CTypeInfo::Flags::kClampBit")
                    if "EnforceRange" in arg.idl_type.effective_annotations:
                        nadc_v8_type_info_flags.append(
                            "v8::CTypeInfo::Flags::kEnforceRangeBit")
                        has_no_alloc_direct_call_with_enforce_range = True
                    arg_type = arg.idl_type.unwrap()
                    if arg_type.is_floating_point_numeric and (
                            not arg_type.keyword_typename.startswith(
                                "unrestricted ")):
                        nadc_v8_type_info_flags.append(
                            "v8::CTypeInfo::Flags::kIsRestrictedBit")
                    if nadc_v8_type_info_flags:
                        nadc_arg_flags += ".Arg<{index}, {flags}>()".format(
                            index=arg.index + 1,
                            flags=", ".join(nadc_v8_type_info_flags))
                nadc_overloads.append(
                    F("v8::CFunctionBuilder().Fn({}){}.Build(),",
                      nadc_entry.callback_name, nadc_arg_flags))
            nadc_overload_nodes.append(
                ListNode([
                    T("static const v8::CFunction " +
                      nadc_overload_table_name + "[] = {"),
                    ListNode(nadc_overloads),
                    T("};"),
                ]))
        else:
            nadc_overload_table_name = None

        def get_formatted_text(input_pattern):
            return _format(
                input_pattern,
                property_name=entry.property_.identifier,
                operation_callback=entry.op_callback_name,
                function_length=entry.op_func_length,
                v8_property_attribute=
                _make_property_entry_v8_property_attribute(entry.property_),
                location=_make_property_entry_location(entry.property_),
                world=_make_property_entry_world(entry.world),
                receiver_check=_make_property_entry_receiver_check(
                    entry.property_),
                cross_origin_check=_make_property_entry_cross_origin_check(
                    entry.property_),
                v8_side_effect=_make_property_entry_v8_side_effect(
                    entry.property_),
                v8_cfunction_table=nadc_overload_table_name)

        entry_nodes.append(T(get_formatted_text(pattern)))
        entry_nodes_wo_nadc.append(T(get_formatted_text(pattern_wo_nadc)))

    table_decl_before_name = (
        "static const IDLMemberInstaller::OperationConfig")
    table_decl_before_name_wo_nadc = table_decl_before_name
    if no_alloc_direct_call_enabled:
        table_decl_before_name = (
            "static const "
            "IDLMemberInstaller::NoAllocDirectCallOperationConfig")
    node = ListNode()
    if nadc_overload_nodes:
        node.extend([
            nadc_overload_nodes,
            EmptyNode(),
        ])
    node.extend([
        T(table_decl_before_name + " " + table_name + "[] = {"),
        ListNode(entry_nodes),
        T("};"),
    ])

    # Disable [NoAllocDirectCall] on x86 due to https://crbug.com/1433212
    if has_no_alloc_direct_call_with_enforce_range:
        node = ListNode([
            T("// Disable [NoAllocDirectCall] on x86 due to "
              "https://crbug.com/1433212"),
            T("#if defined(ARCH_CPU_X86)"),
            T(table_decl_before_name_wo_nadc + " " + table_name + "[] = {"),
            ListNode(entry_nodes_wo_nadc),
            T("};"),
            T("// Disable compiler warnings for unused functions."),
            ListNode([
                F("std::ignore = {};", nadc_entry.callback_name)
                for entry in operation_entries
                for nadc_entry in entry.no_alloc_direct_call_callbacks
            ]),
            T("#else   // defined(ARCH_CPU_X86)"),
            node,
            T("#endif  // defined(ARCH_CPU_X86)"),
        ])
    return node


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
        assert isinstance(ctor_func_length, int)

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
                 no_alloc_direct_call_callbacks=None):
        assert isinstance(op_callback_name, str)
        assert isinstance(op_func_length, int)

        _PropEntryBase.__init__(self, is_context_dependent,
                                exposure_conditional, world, operation_group)
        self.op_callback_name = op_callback_name
        self.op_func_length = op_func_length
        self.no_alloc_direct_call_callbacks = no_alloc_direct_call_callbacks


def make_property_entries_and_callback_defs(cg_context, attribute_entries,
                                            constant_entries,
                                            constructor_entries,
                                            exposed_construct_entries,
                                            operation_entries):
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

    class_like = cg_context.class_like
    interface = cg_context.interface
    # This function produces the property installation code and we'd like to
    # expose IDL constructs with [Exposed] not only on [Global] but also on
    # [TargetOfExposed].
    global_names = (
        class_like.extended_attributes.values_of("Global") +
        class_like.extended_attributes.values_of("TargetOfExposed"))

    callback_def_nodes = ListNode()

    def iterate(members, callback):
        for member in members:
            is_context_dependent = member.exposure.is_context_dependent(
                global_names)
            if isinstance(member, web_idl.OverloadGroup):
                exposure_conditional = expr_or([
                    expr_from_exposure(overload.exposure,
                                       global_names=global_names,
                                       may_use_feature_selector=True)
                    for overload in member
                ])
            else:
                exposure_conditional = expr_from_exposure(
                    member.exposure,
                    global_names=global_names,
                    may_use_feature_selector=True)

            if "PerWorldBindings" in member.extended_attributes:
                assert not isinstance(
                    member, web_idl.ConstructorGroup
                ), "[PerWorldBindings] is not supported for constructors"
                worlds = (CodeGenContext.MAIN_WORLD,
                          CodeGenContext.NON_MAIN_WORLDS)
            else:
                worlds = (CodeGenContext.ALL_WORLDS, )

            for world in worlds:
                callback(member, is_context_dependent, exposure_conditional,
                         world)

    def process_attribute(attribute, is_context_dependent,
                          exposure_conditional, world):
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

    def process_legacy_factory_function_group(legacy_factory_function_group,
                                              is_context_dependent,
                                              exposure_conditional, world):
        cgc = cg_context.make_copy(
            exposed_construct=legacy_factory_function_group,
            is_legacy_factory_function=True,
            for_world=world,
            v8_callback_type=CodeGenContext.V8_ACCESSOR_NAME_GETTER_CALLBACK)
        prop_callback_name = callback_function_name(cgc)
        prop_callback_node = (
            make_legacy_factory_function_property_callback_def(
                cgc, prop_callback_name))

        callback_def_nodes.extend([
            prop_callback_node,
            EmptyNode(),
        ])

        exposed_construct_entries.append(
            _PropEntryExposedConstruct(
                is_context_dependent=is_context_dependent,
                exposure_conditional=exposure_conditional,
                world=world,
                exposed_construct=legacy_factory_function_group,
                prop_callback_name=prop_callback_name))

    def process_operation_group(operation_group, is_context_dependent,
                                exposure_conditional, world):
        cgc = cg_context.make_copy(
            operation_group=operation_group, for_world=world)
        op_callback_name = callback_function_name(cgc)
        op_callback_node = make_operation_callback_def(cgc, op_callback_name)
        no_alloc_direct_call_callbacks = (
            list_no_alloc_direct_call_callbacks(
                cgc.make_copy(no_alloc_direct_call=True))
            if "NoAllocDirectCall" in operation_group.extended_attributes else
            None)

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
                no_alloc_direct_call_callbacks=no_alloc_direct_call_callbacks))

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

    iterate(class_like.attributes, process_attribute)
    iterate(class_like.constants, process_constant)
    if interface:
        iterate(interface.constructor_groups, process_constructor_group)
        iterate(interface.exposed_constructs, process_exposed_construct)
        iterate(interface.legacy_window_aliases, process_exposed_construct)
        legacy_factory_function_groups = [
            group for construct in interface.exposed_constructs
            for group in construct.legacy_factory_function_groups
            if construct.legacy_factory_function_groups
        ]
        iterate(legacy_factory_function_groups,
                process_legacy_factory_function_group)
    if not class_like.is_callback_interface:
        iterate(class_like.operation_groups, process_operation_group)
    if interface and interface.stringifier:
        iterate([interface.stringifier.operation], process_stringifier)
    collectionlike = (interface
                      and (interface.async_iterable or interface.iterable
                           or interface.maplike or interface.setlike))
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
    if interface:
        # Iff the interface has an unscopable member, then collect all
        # unscopable members including ones in inherited interfaces.
        # Otherwise, do not create an @@unscopables object.
        is_unscopable = lambda member: "Unscopable" in member.extended_attributes
        unscopables.extend(filter(is_unscopable, interface.attributes))
        unscopables.extend(filter(is_unscopable, interface.operations))
        if unscopables:
            for i in interface.inclusive_inherited_interfaces:
                if i == interface:
                    continue
                unscopables.extend(filter(is_unscopable, i.attributes))
                unscopables.extend(filter(is_unscopable, i.operations))
    if unscopables:
        nodes.extend([
            TextNode("""\
// [Unscopable]
// 3.7.3. Interface prototype object
// https://webidl.spec.whatwg.org/#interface-prototype-object
// step 10. If interface has any member declared with the [Unscopable]
//   extended attribute, then:\
"""),
            ListNode([
                TextNode("static constexpr const char* "
                         "kUnscopablePropertyNames[] = {"),
                ListNode([
                    TextNode("\"{}\", ".format(name)) for name in sorted(
                        map(lambda member: member.identifier, unscopables))
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
// https://webidl.spec.whatwg.org/#interface-prototype-object
// step 13. If the [LegacyNoInterfaceObject] extended attribute was not
//   specified on interface, then:
//
// V8 defines "constructor" property on the prototype object by default.
${prototype_object}->Delete(
    ${v8_context}, V8AtomicString(${isolate}, "constructor")).ToChecked();
"""))

    if class_like.is_async_iterator or class_like.is_sync_iterator:
        nodes.append(
            TextNode("""\
// V8 defines "constructor" property on the prototype object by default.
${prototype_object}->Delete(
    ${v8_context}, V8AtomicString(${isolate}, "constructor")).ToChecked();
"""))

    if interface and interface.iterable and not interface.iterable.key_type:
        conditional = expr_from_exposure(interface.iterable.exposure)
        if not conditional.is_always_true:
            body = [
                TextNode("""\
// The value-iterator-returning properties of the intrinsic values can be
// installed only per v8::Template (via
// v8::Template::SetIntrinsicDataProperty). So the property installation
// logic is reversed in this case like below.
//   1. Unconditionally install the properties on prototype_object_template
//      per V8 isolate in ${class_name}::InstallInterfaceTemplate.
//   2. Conditionally remove the properties from prototype_object per V8
//      context if they're not enabled.
// https://webidl.spec.whatwg.org/#define-the-iteration-methods\
""")
            ]
            body.extend([
                FormatNode(
                    "${prototype_object}->Delete("
                    "${v8_context}, "
                    "V8AtomicString(${isolate}, \"{property}\"))"
                    ".ToChecked();",
                    property=property)
                for property in ("entries", "keys", "values", "forEach")
            ])
            nodes.append(
                CxxUnlikelyIfNode(cond=expr_not(conditional),
                                  attribute=None,
                                  body=body))

    # Install @@asyncIterator property.
    if interface and interface.async_iterable:
        for operation_group in interface.async_iterable.operation_groups:
            if operation_group[0].is_async_iterator:
                property_name = operation_group.identifier
                break
        else:
            assert False
        pattern = """\
// @@asyncIterator == "{property_name}"
{{
  v8::Local<v8::Value> v8_value = ${prototype_object}->Get(
      ${v8_context}, V8AtomicString(${isolate}, "{property_name}"))
      .ToLocalChecked();
  // "{property_name}" may be hidden in this context.
  if (!v8_value->IsUndefined()) {{
    ${prototype_object}->DefineOwnProperty(
        ${v8_context}, v8::Symbol::GetAsyncIterator(${isolate}), v8_value,
        v8::DontEnum).ToChecked();
  }}
}}
"""
        nodes.append(FormatNode(pattern, property_name=property_name))

    # Install @@iterator property.
    if (interface and ((interface.iterable and interface.iterable.key_type)
                       or interface.maplike or interface.setlike)):
        collectionlike = (interface.iterable or interface.maplike
                          or interface.setlike)
        for operation_group in collectionlike.operation_groups:
            if operation_group[0].is_iterator:
                property_name = operation_group.identifier
                break
        else:
            assert False
        pattern = """\
// @@iterator == "{property_name}"
{{
  v8::Local<v8::Value> v8_value = ${prototype_object}->Get(
      ${v8_context}, V8AtomicString(${isolate}, "{property_name}"))
      .ToLocalChecked();
  // "{property_name}" may be hidden in this context.
  if (!v8_value->IsUndefined()) {{
    ${prototype_object}->DefineOwnProperty(
        ${v8_context}, v8::Symbol::GetIterator(${isolate}), v8_value,
        v8::DontEnum).ToChecked();
  }}
}}
"""
        nodes.append(FormatNode(pattern, property_name=property_name))

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
        "v8::Local<v8::Template> interface_template",
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

    if cg_context.interface:
        body.extend([
            T("bindings::SetupIDLInterfaceTemplate("
              "${isolate}, ${wrapper_type_info}, "
              "${instance_object_template}, "
              "${prototype_object_template}, "
              "${interface_function_template}, "
              "${parent_interface_template});"),
            EmptyNode(),
        ])
    elif cg_context.namespace:
        body.extend([
            T("bindings::SetupIDLNamespaceTemplate("
              "${isolate}, ${wrapper_type_info}, "
              "${namespace_object_template});"),
            EmptyNode(),
        ])
    elif cg_context.callback_interface:
        body.extend([
            T("bindings::SetupIDLCallbackInterfaceTemplate("
              "${isolate}, ${wrapper_type_info}, "
              "${interface_function_template});"),
            EmptyNode(),
        ])
    elif cg_context.async_iterator or cg_context.sync_iterator:
        iterator_interface = (cg_context.async_iterator
                              or cg_context.sync_iterator).interface
        if iterator_interface.async_iterable:
            parent_intrinsic_prototype = (
                "v8::Intrinsic::kAsyncIteratorPrototype")
        elif iterator_interface.iterable:
            parent_intrinsic_prototype = "v8::Intrinsic::kIteratorPrototype"
        elif iterator_interface.maplike:
            parent_intrinsic_prototype = "v8::Intrinsic::kMapIteratorPrototype"
        elif iterator_interface.setlike:
            parent_intrinsic_prototype = "v8::Intrinsic::kSetIteratorPrototype"
        else:
            assert False
        if iterator_interface.async_iterable:
            class_string = "{} AsyncIterator".format(
                iterator_interface.identifier)
        else:
            class_string = "{} Iterator".format(iterator_interface.identifier)
        body.extend([
            FormatNode(
                "bindings::SetupIDLIteratorTemplate("
                "${isolate}, ${wrapper_type_info}, "
                "${instance_object_template}, "
                "${prototype_object_template}, "
                "${interface_function_template}, "
                "{parent_intrinsic_prototype}, "
                "\"{class_string}\");",
                parent_intrinsic_prototype=parent_intrinsic_prototype,
                class_string=class_string),
            EmptyNode(),
        ])
    else:
        assert False

    for entry in constructor_entries:
        nodes = [
            FormatNode("${interface_function_template}->SetCallHandler({});",
                       entry.ctor_callback_name),
            FormatNode("${interface_function_template}->SetLength({});",
                       entry.ctor_func_length),
            FormatNode(
                "${interface_function_template}->SetInterfaceName("
                "V8String(${isolate}, \"{}\"));",
                cg_context.class_like.identifier),
            T("${interface_function_template}->SetExceptionContext("
              "v8::ExceptionContext::kConstructor);"),
        ]
        if not (entry.exposure_conditional.is_always_true
                or entry.is_context_dependent):
            nodes = [
                CxxUnlikelyIfNode(cond=entry.exposure_conditional,
                                  attribute=None,
                                  body=nodes),
            ]
        assert entry.world == CodeGenContext.ALL_WORLDS
        body.extend(nodes)
        body.append(EmptyNode())

    body.extend([
        supplemental_install_node,
        EmptyNode(),
    ])

    if class_like.identifier == "DOMException":
        body.append(
            T("""\
// DOMException-specific settings
// https://webidl.spec.whatwg.org/#es-DOMException-specialness
{
  v8::Local<v8::FunctionTemplate> intrinsic_error_prototype_interface_template =
      v8::FunctionTemplate::New(${isolate}, nullptr, v8::Local<v8::Value>(),
                                v8::Local<v8::Signature>(), 0,
                                v8::ConstructorBehavior::kThrow);
  intrinsic_error_prototype_interface_template->SetIntrinsicDataProperty(
      V8AtomicString(${isolate}, "prototype"), v8::kErrorPrototype);
  ${interface_function_template}->Inherit(
      intrinsic_error_prototype_interface_template);
}
"""))

    if class_like.identifier == "HTMLAllCollection":
        body.append(
            T("""\
// HTMLAllCollection-specific settings
// https://html.spec.whatwg.org/C/#the-htmlallcollection-interface
${instance_object_template}->SetCallAsFunctionHandler(ItemOperationCallback);
${instance_object_template}->MarkAsUndetectable();
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
${instance_object_template}->SetImmutableProto();
${prototype_object_template}->SetImmutableProto();
"""))

    if (interface and interface.indexed_and_named_properties
            and interface.indexed_and_named_properties.indexed_getter
            and "Global" not in interface.extended_attributes):
        body.append(
            T("""\
// @@iterator for indexed properties
// https://webidl.spec.whatwg.org/#define-the-iteration-methods
${prototype_template}->SetIntrinsicDataProperty(
    v8::Symbol::GetIterator(${isolate}), v8::kArrayProto_values, v8::DontEnum);
"""))
    if interface and interface.iterable and not interface.iterable.key_type:
        body.append(
            T("""\
// Value iterator's properties
// https://webidl.spec.whatwg.org/#define-the-iteration-methods
${prototype_template}->SetIntrinsicDataProperty(
    V8AtomicString(${isolate}, "entries"), v8::kArrayProto_entries, v8::None);
${prototype_template}->SetIntrinsicDataProperty(
    V8AtomicString(${isolate}, "keys"), v8::kArrayProto_keys, v8::None);
${prototype_template}->SetIntrinsicDataProperty(
    V8AtomicString(${isolate}, "values"), v8::kArrayProto_values, v8::None);
${prototype_template}->SetIntrinsicDataProperty(
    V8AtomicString(${isolate}, "forEach"), v8::kArrayProto_forEach, v8::None);
"""))

    if interface and "IsCodeLike" in interface.extended_attributes:
        body.append(
            CxxUnlikelyIfNode(
                cond="RuntimeEnabledFeatures::TrustedTypesUseCodeLikeEnabled()",
                attribute=None,
                body=[
                    TextNode("// [IsCodeLike]"),
                    TextNode("${instance_object_template}->SetCodeLike();"),
                ]))

    if "Global" in class_like.extended_attributes:
        body.append(
            TextNode("""\
// [Global]
// 3.7.1. [[SetPrototypeOf]]
// https://webidl.spec.whatwg.org/#platform-object-setprototypeof
${instance_object_template}->SetImmutableProto();
${prototype_object_template}->SetImmutableProto();
"""))
    elif interface and any("Global" in derived.extended_attributes
                           for derived in interface.subclasses):
        body.append(
            TextNode("""\
// [Global] - prototype object in the prototype chain of global objects
// 3.7.1. [[SetPrototypeOf]]
// https://webidl.spec.whatwg.org/#platform-object-setprototypeof
${prototype_object_template}->SetImmutableProto();
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
            "v8::Local<v8::Template> instance_template",
            "v8::Local<v8::Template> prototype_template",
            "v8::Local<v8::Template> interface_template",
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
            "v8::Local<v8::Object> interface_object",
            "v8::Local<v8::Template> interface_template",
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
            "v8::Local<v8::Object> interface_object",
            "v8::Local<v8::Template> interface_template",
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

    body.extend([
        TextNode("using bindings::IDLMemberInstaller;"),
        EmptyNode(),
    ])

    if (is_per_context_install
            and "Global" in cg_context.class_like.extended_attributes):
        body.extend([
            CxxLikelyIfNode(cond="${instance_object}.IsEmpty()",
                            attribute=None,
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
                            attribute=None,
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
                CxxUnlikelyIfNode(cond=conditional,
                                  attribute=None,
                                  body=[
                                      make_table_func(table_name, entries),
                                      TextNode(installer_call_text),
                                  ]))
        body.append(EmptyNode())

    if is_per_context_install:
        pattern = ("{install_func}("
                   "${isolate}, ${world}, "
                   "${instance_object}, "
                   "${prototype_object}, "
                   "${interface_object}, "
                   "${signature}, {table_name});")
    else:
        pattern = ("{install_func}("
                   "${isolate}, ${world}, "
                   "${instance_template}, "
                   "${prototype_template}, "
                   "${interface_template}, "
                   "${signature}, {table_name});")

    table_name = "kAttributeTable"
    installer_call_text = _format(
        pattern,
        install_func="IDLMemberInstaller::InstallAttributes",
        table_name=table_name)
    install_properties(table_name, attribute_entries,
                       _make_attribute_registration_table, installer_call_text)

    table_name = "kConstantCallbackTable"
    installer_call_text = _format(
        pattern,
        install_func="IDLMemberInstaller::InstallConstants",
        table_name=table_name)
    constant_callback_entries = list(
        filter(lambda entry: entry.const_callback_name, constant_entries))
    install_properties(table_name, constant_callback_entries,
                       _make_constant_callback_registration_table,
                       installer_call_text)

    table_name = "kConstantValueTable"
    installer_call_text = _format(
        pattern,
        install_func="IDLMemberInstaller::InstallConstants",
        table_name=table_name)
    constant_value_entries = list(
        filter(lambda entry: not entry.const_callback_name, constant_entries))
    install_properties(table_name, constant_value_entries,
                       _make_constant_value_registration_table,
                       installer_call_text)

    table_name = "kExposedConstructTable"
    installer_call_text = _format(
        pattern,
        install_func="IDLMemberInstaller::InstallExposedConstructs",
        table_name=table_name)
    install_properties(table_name, exposed_construct_entries,
                       _make_exposed_construct_registration_table,
                       installer_call_text)

    table_name = "kOperationTable"
    installer_call_text = _format(
        pattern,
        install_func="IDLMemberInstaller::InstallOperations",
        table_name=table_name)
    entries = list(
        filter(lambda entry: not entry.no_alloc_direct_call_callbacks,
               operation_entries))
    install_properties(table_name, entries, _make_operation_registration_table,
                       installer_call_text)
    entries = list(
        filter(lambda entry: entry.no_alloc_direct_call_callbacks,
               operation_entries))
    install_properties(table_name, entries, _make_operation_registration_table,
                       installer_call_text)

    return func_decl, func_def, trampoline_def


def make_indexed_and_named_property_callbacks_and_install_node(cg_context):
    """
    Implements non-ordinary internal methods of legacy platform objects.
    https://webidl.spec.whatwg.org/#es-legacy-platform-objects

    Also implements the same origin case of indexed access to WindowProxy
    objects just same as indexed properties of legacy platform objects.
    https://html.spec.whatwg.org/C/#the-windowproxy-exotic-object
    """

    assert isinstance(cg_context, CodeGenContext)

    F = FormatNode

    func_decls = ListNode()
    func_defs = ListNode()
    install_node = SequenceNode()

    interface = cg_context.interface
    if not (interface and interface.indexed_and_named_properties):
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

    if props.own_named_getter and "Global" not in interface.extended_attributes:
        add_callback(*make_named_property_getter_callback(
            cg_context.make_copy(named_property_getter=props.named_getter,
                                 named_interceptor_kind="Getter"),
            "NamedPropertyGetterCallback"))
        add_callback(*make_named_property_setter_callback(
            cg_context.make_copy(named_property_setter=props.named_setter,
                                 named_interceptor_kind="Setter"),
            "NamedPropertySetterCallback"))
        add_callback(*make_named_property_deleter_callback(
            cg_context.make_copy(named_property_deleter=props.named_deleter,
                                 named_interceptor_kind="Deleter"),
            "NamedPropertyDeleterCallback"))
        add_callback(*make_named_property_definer_callback(
            cg_context.make_copy(named_interceptor_kind="Definer"),
            "NamedPropertyDefinerCallback"))
        add_callback(*make_named_property_descriptor_callback(
            cg_context.make_copy(named_interceptor_kind="Descriptor"),
            "NamedPropertyDescriptorCallback"))
        add_callback(*make_named_property_query_callback(
            cg_context.make_copy(
                named_interceptor_kind="Query"), "NamedPropertyQueryCallback"))
        add_callback(*make_named_property_enumerator_callback(
            cg_context.make_copy(named_interceptor_kind="Enumerator"),
            "NamedPropertyEnumeratorCallback"))

    if cg_context.class_like.identifier == "WindowProperties":
        interceptor_template = "${prototype_object_template}"
    else:
        interceptor_template = "${instance_object_template}"

    if props.named_getter and "Global" not in interface.extended_attributes:
        impl_bridge = v8_bridge_class_name(
            most_derived_interface(
                props.named_getter.owner, props.named_setter
                and props.named_setter.owner, props.named_deleter
                and props.named_deleter.owner))
        flags = ["v8::PropertyHandlerFlags::kOnlyInterceptStrings"]
        if "LegacyOverrideBuiltIns" not in interface.extended_attributes:
            flags.append("v8::PropertyHandlerFlags::kNonMasking")
        if (props.named_getter.extended_attributes.value_of("Affects") !=
                "Everything"):
            flags.append("v8::PropertyHandlerFlags::kHasNoSideEffect")
        property_handler_flags = (
            "static_cast<v8::PropertyHandlerFlags>({})".format(" | ".join(
                map(lambda flag: "int32_t({})".format(flag), flags))))
        pattern = """\
// Named interceptors
{interceptor_template}->SetHandler(
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
              interceptor_template=interceptor_template,
              impl_bridge=impl_bridge,
              property_handler_flags=property_handler_flags))

    if props.own_indexed_getter or props.own_named_getter:
        add_callback(*make_indexed_property_getter_callback(
            cg_context.make_copy(indexed_property_getter=props.indexed_getter,
                                 indexed_interceptor_kind="Getter"),
            "IndexedPropertyGetterCallback"))
        add_callback(*make_indexed_property_setter_callback(
            cg_context.make_copy(indexed_property_setter=props.indexed_setter,
                                 indexed_interceptor_kind="Setter"),
            "IndexedPropertySetterCallback"))
        add_callback(*make_indexed_property_deleter_callback(
            cg_context.make_copy(indexed_interceptor_kind="Deleter"),
            "IndexedPropertyDeleterCallback"))
        add_callback(*make_indexed_property_definer_callback(
            cg_context.make_copy(indexed_interceptor_kind="Definer"),
            "IndexedPropertyDefinerCallback"))
        add_callback(*make_indexed_property_descriptor_callback(
            cg_context.make_copy(indexed_interceptor_kind="Descriptor"),
            "IndexedPropertyDescriptorCallback"))
        add_callback(*make_indexed_property_enumerator_callback(
            cg_context.make_copy(indexed_interceptor_kind="Enumerator"),
            "IndexedPropertyEnumeratorCallback"))

    if props.indexed_getter or props.named_getter:
        impl_bridge = v8_bridge_class_name(
            most_derived_interface(
                props.indexed_getter and props.indexed_getter.owner,
                props.indexed_setter and props.indexed_setter.owner,
                props.named_getter and props.named_getter.owner,
                props.named_setter and props.named_setter.owner,
                props.named_deleter and props.named_deleter.owner))
        flags = []
        if (props.indexed_getter and
                props.indexed_getter.extended_attributes.value_of("Affects") !=
                "Everything"):
            flags.append("v8::PropertyHandlerFlags::kHasNoSideEffect")
        else:
            flags.append("v8::PropertyHandlerFlags::kNone")
        property_handler_flags = flags[0]
        pattern = """\
// Indexed interceptors
{interceptor_template}->SetHandler(
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
              interceptor_template=interceptor_template,
              impl_bridge=impl_bridge,
              property_handler_flags=property_handler_flags))

    func_defs.accumulate(
        CodeGenAccumulator.require_include_headers([
            "third_party/blink/renderer/bindings/core/v8/v8_set_return_value_for_core.h"
        ]))

    return func_decls, func_defs, install_node


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
    if cg_context.class_like.identifier not in CROSS_ORIGIN_INTERFACES:
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
                    CodeGenContext.V8_NAMED_PROPERTY_GETTER_CALLBACK))
            get_value = callback_function_name(cgc, for_cross_origin=True)
            func_def = make_attribute_get_callback_def(cgc, get_value)
            callback_defs.extend([func_def, EmptyNode()])
        if values and "Setter" in values:
            set_func = entry.attr_set_callback_name
            cgc = cg_context.make_copy(
                attribute=attribute,
                attribute_set=True,
                v8_callback_type=(
                    CodeGenContext.V8_NAMED_PROPERTY_SETTER_CALLBACK))
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
            cg_context.make_copy(named_interceptor_kind="Getter"),
            "CrossOriginNamedGetterCallback"),
        make_cross_origin_named_setter_callback(
            cg_context.make_copy(named_interceptor_kind="Setter"),
            "CrossOriginNamedSetterCallback"),
        make_cross_origin_throwing_callback(
            cg_context.make_copy(named_interceptor_kind="Deleter")),
        make_cross_origin_throwing_callback(
            cg_context.make_copy(named_interceptor_kind="Definer")),
        make_cross_origin_named_descriptor_callback(
            cg_context.make_copy(named_interceptor_kind="Descriptor"),
            "CrossOriginNamedDescriptorCallback"),
        make_cross_origin_named_query_callback(
            cg_context.make_copy(named_interceptor_kind="Query"),
            "CrossOriginNamedQueryCallback"),
        make_cross_origin_named_enumerator_callback(
            cg_context.make_copy(named_interceptor_kind="Enumerator"),
            "CrossOriginNamedEnumeratorCallback"),
        make_cross_origin_indexed_getter_callback(
            cg_context.make_copy(
                indexed_property_getter=(props and props.indexed_getter),
                indexed_interceptor_kind="Getter"),
            "CrossOriginIndexedGetterCallback"),
        make_cross_origin_throwing_callback(
            cg_context.make_copy(indexed_interceptor_kind="Setter")),
        make_cross_origin_throwing_callback(
            cg_context.make_copy(indexed_interceptor_kind="Deleter")),
        make_cross_origin_throwing_callback(
            cg_context.make_copy(indexed_interceptor_kind="Definer")),
        make_cross_origin_indexed_descriptor_callback(
            cg_context.make_copy(indexed_interceptor_kind="Descriptor"),
            "CrossOriginIndexedDescriptorCallback"),
        make_cross_origin_indexed_enumerator_callback(
            cg_context.make_copy(indexed_interceptor_kind="Enumerator"),
            "CrossOriginIndexedEnumeratorCallback"),
    ]
    for func_def in func_defs:
        callback_defs.append(func_def)
        callback_defs.append(EmptyNode())

    text = """\
// Cross origin properties
${instance_object_template}->SetAccessCheckCallbackAndHandler(
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

    F = FormatNode

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
# IsExposed
# ----------------------------------------------------------------------------


def make_is_exposed(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert function_name == "IsExposed"
    class_like = cg_context.class_like

    is_exposed_decl = CxxFuncDeclNode(
        name=function_name,
        arg_decls=["ExecutionContext* execution_context"],
        return_type="bool",
        static=True)
    is_exposed_decl.accumulate(
        CodeGenAccumulator.require_class_decls(["ExecutionContext"]))

    is_exposed_def = CxxFuncDefNode(
        name=function_name,
        arg_decls=["ExecutionContext* execution_context"],
        return_type="bool",
        class_name=cg_context.class_name)

    def define_execution_context(symbol_node):
        # execution_context doesn't really need a definition because it's a
        # function argument, but needs to require ".../execution_context.h".
        node = SymbolDefinitionNode(symbol_node)
        node.accumulate(
            CodeGenAccumulator.require_include_headers([
                "third_party/blink/renderer/core/execution_context/execution_context.h"
            ]))
        return node

    is_exposed_def.body.register_code_symbol(
        SymbolNode("execution_context",
                   definition_constructor=define_execution_context))
    bind_installer_local_vars(is_exposed_def.body, cg_context)
    # If [Exposed] exists at all, then this exposure condition should be valid.
    # Otherwise, it is not an exposed interface at all.
    if class_like.exposure.global_names_and_features:
        is_exposed_def.body.append(
            FormatNode("return {};",
                       expr_from_exposure(class_like.exposure).to_text()))
    else:
        is_exposed_def.body.append(TextNode("return false;"))
    return (is_exposed_decl, is_exposed_def)


# ----------------------------------------------------------------------------
# WrapperTypeInfo
# ----------------------------------------------------------------------------


def make_wrapper_type_info(cg_context, function_name,
                           has_context_dependent_props):
    assert isinstance(cg_context, CodeGenContext)
    assert function_name == "GetWrapperTypeInfo"
    assert isinstance(has_context_dependent_props, bool)

    F = FormatNode
    class_like = cg_context.class_like

    func_def = CxxFuncDefNode(name=function_name,
                              arg_decls=[],
                              return_type="constexpr const WrapperTypeInfo*",
                              static=True)
    func_def.set_base_template_vars(cg_context.template_bindings())
    func_def.body.append(TextNode("return &wrapper_type_info_;"))

    public_defs = SequenceNode()
    public_defs.append(func_def)

    public_defs.append(
        TextNode("""\
  static constexpr v8::CppHeapPointerTag kThisTag =
      static_cast<v8::CppHeapPointerTag>({this_tag});
  static constexpr v8::CppHeapPointerTag kMaxSubclassTag =
      static_cast<v8::CppHeapPointerTag>({max_subclass_tag});
  static constexpr v8::CppHeapPointerTagRange kTagRange =
      v8::CppHeapPointerTagRange(kThisTag, kMaxSubclassTag);
""".format(this_tag=class_like.tag,
           max_subclass_tag=class_like.max_subclass_tag)))

    member_var_def = TextNode(
        "static const WrapperTypeInfo wrapper_type_info_;")
    member_var_def.accumulate(
        CodeGenAccumulator.require_struct_decls(["WrapperTypeInfo"]))

    wrapper_type_info_def = ListNode()
    wrapper_type_info_def.set_base_template_vars(
        cg_context.template_bindings())
    wrapper_type_info_def.accumulate(
        CodeGenAccumulator.require_include_headers([
            "third_party/blink/renderer/platform/bindings/wrapper_type_info.h"
        ]))

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
    ${class_name}::{install_interface_template_func},
    {install_context_dependent_func},
    "${{class_like.identifier}}",
    {wrapper_type_info_of_inherited},
    ${class_name}::kThisTag,
    ${class_name}::kMaxSubclassTag,
    {wrapper_type_prototype},
    {wrapper_class_id},
    {active_script_wrappable_inheritance},
    {idl_definition_kind},
    {is_skipped_in_interface_object_prototype_chain},
}};

#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic pop
#endif
"""
    if has_context_dependent_props:
        install_context_dependent_func = _format(
            "${class_name}::{}", FN_INSTALL_CONTEXT_DEPENDENT_PROPS)
    else:
        install_context_dependent_func = "nullptr"
    if class_like.is_interface and class_like.inherited:
        wrapper_type_info_of_inherited = "{}::GetWrapperTypeInfo()".format(
            v8_bridge_class_name(class_like.inherited))
    else:
        wrapper_type_info_of_inherited = "nullptr"
    if (class_like.is_interface or class_like.is_async_iterator
            or class_like.is_sync_iterator):
        wrapper_type_prototype = "WrapperTypeInfo::kWrapperTypeObjectPrototype"
    else:
        wrapper_type_prototype = "WrapperTypeInfo::kWrapperTypeNoPrototype"
    if class_like.is_interface and class_like.does_implement("Node"):
        wrapper_class_id = "WrapperTypeInfo::kNodeClassId"
    else:
        wrapper_class_id = "WrapperTypeInfo::kObjectClassId"
    if class_like.code_generator_info.is_active_script_wrappable:
        active_script_wrappable_inheritance = (
            "WrapperTypeInfo::kInheritFromActiveScriptWrappable")
    else:
        active_script_wrappable_inheritance = (
            "WrapperTypeInfo::kNotInheritFromActiveScriptWrappable")
    if class_like.is_interface:
        idl_definition_kind = "WrapperTypeInfo::kIdlInterface"
    elif class_like.is_namespace:
        idl_definition_kind = "WrapperTypeInfo::kIdlNamespace"
    elif class_like.is_callback_interface:
        idl_definition_kind = "WrapperTypeInfo::kIdlCallbackInterface"
    elif class_like.is_async_iterator or class_like.is_sync_iterator:
        idl_definition_kind = "WrapperTypeInfo::kIdlAsyncOrSyncIterator"
    else:
        assert False
    is_skipped_in_interface_object_prototype_chain = (
        "true" if class_like.identifier == "WindowProperties" else "false")
    wrapper_type_info_def.append(
        F(pattern,
          install_interface_template_func=FN_INSTALL_INTERFACE_TEMPLATE,
          install_context_dependent_func=install_context_dependent_func,
          wrapper_type_info_of_inherited=wrapper_type_info_of_inherited,
          wrapper_type_prototype=wrapper_type_prototype,
          wrapper_class_id=wrapper_class_id,
          active_script_wrappable_inheritance=(
              active_script_wrappable_inheritance),
          idl_definition_kind=idl_definition_kind,
          is_skipped_in_interface_object_prototype_chain=(
              is_skipped_in_interface_object_prototype_chain)))

    if (class_like.is_interface or class_like.is_async_iterator
            or class_like.is_sync_iterator):
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
    "the IDL has [ActiveScriptWrappable] extended attribute.");"""
    else:
        pattern = """\
// non-[ActiveScriptWrappable]
static_assert(
    !std::is_base_of<ActiveScriptWrappableBase, {blink_class}>::value,
    "{blink_class} inherits from ActiveScriptWrappable<> without "
    "[ActiveScriptWrappable] extended attribute.");"""
    if class_like.is_interface:
        wrapper_type_info_def.append(F(pattern, blink_class=blink_class))

    return public_defs, member_var_def, wrapper_type_info_def


# ----------------------------------------------------------------------------
# V8 Context Snapshot
# ----------------------------------------------------------------------------


def make_v8_context_snapshot_api(cg_context, component, attribute_entries,
                                 constant_entries, constructor_entries,
                                 exposed_construct_entries, operation_entries,
                                 indexed_and_named_property_defs,
                                 cross_origin_property_callback_defs,
                                 install_context_independent_func_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(component, web_idl.Component)

    if not cg_context.interface:
        return None, None

    subclass_interfaces = cg_context.interface.subclasses
    subclass_names = list(
        map(lambda interface: interface.identifier, subclass_interfaces))
    subclass_names.append(cg_context.interface.identifier)
    if not ("Window" in subclass_names or "HTMLDocument" in subclass_names):
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
        cg_context, name_style.func("GetRefTableOf", cg_context.class_name),
        attribute_entries, constant_entries, constructor_entries,
        exposed_construct_entries, operation_entries,
        indexed_and_named_property_defs, cross_origin_property_callback_defs))

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
        indexed_and_named_property_defs, cross_origin_property_callback_defs):
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

    collect_callbacks(cross_origin_property_callback_defs)

    for node in indexed_and_named_property_defs:
        if isinstance(node, CxxFuncDefNode):
            callback_names.append(
                _format("${class_name}::{}", node.function_name))

    entry_nodes = list(
        map(
            lambda name: TextNode("reinterpret_cast<intptr_t>({}),".format(name
                                                                           )),
            filter(None, callback_names)))
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
        attribute_entries=list(filter(selector, attribute_entries)),
        constant_entries=list(filter(selector, constant_entries)),
        exposed_construct_entries=list(
            filter(selector, exposed_construct_entries)),
        operation_entries=list(filter(selector, operation_entries)))

    return func_decl, func_def


def _make_v8_context_snapshot_install_props_per_isolate_function(
        cg_context, function_name, install_context_independent_func_name):
    arg_decls = [
        "v8::Isolate* isolate",
        "const DOMWrapperWorld& world",
        "v8::Local<v8::Template> instance_template",
        "v8::Local<v8::Template> prototype_template",
        "v8::Local<v8::Template> interface_template",
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


def _collect_include_headers(class_like):
    assert isinstance(class_like,
                      (web_idl.Interface, web_idl.Namespace,
                       web_idl.AsyncIterator, web_idl.SyncIterator))

    headers = set(class_like.code_generator_info.blink_headers or [])

    def collect_from_idl_type(idl_type):
        idl_type.apply_to_all_composing_elements(add_include_headers)

    def add_include_headers(idl_type):
        type_def_obj = idl_type.type_definition_object
        if type_def_obj is not None:
            if (type_def_obj.identifier in (
                    "OnErrorEventHandlerNonNull",
                    "OnBeforeUnloadEventHandlerNonNull")):
                raise StopIteration(idl_type.syntactic_form)

            headers.add(PathManager(type_def_obj).api_path(ext="h"))
            if type_def_obj.is_interface or type_def_obj.is_namespace:
                headers.add(PathManager(type_def_obj).blink_path(ext="h"))
            raise StopIteration(idl_type.syntactic_form)

        union_def_obj = idl_type.union_definition_object
        if union_def_obj is not None:
            headers.add(PathManager(union_def_obj).api_path(ext="h"))
            return

        if idl_type.is_frozen_array:
            headers.add(
                "third_party/blink/renderer/bindings/core/v8/frozen_array.h")

    for attribute in class_like.attributes:
        collect_from_idl_type(attribute.idl_type)

    operations = []
    operations.extend(class_like.constructors)
    operations.extend(class_like.operations)
    if class_like.is_interface:
        for x in (class_like.async_iterable, class_like.iterable,
                  class_like.maplike, class_like.setlike):
            if x:
                operations.extend(x.operations)
        for exposed_construct in class_like.exposed_constructs:
            operations.extend(exposed_construct.legacy_factory_functions)
    for operation in operations:
        collect_from_idl_type(operation.return_type)
        for argument in operation.arguments:
            collect_from_idl_type(argument.idl_type)

    if class_like.is_interface:
        for exposed_construct in class_like.exposed_constructs:
            headers.add(PathManager(exposed_construct).api_path(ext="h"))
        for legacy_window_alias in class_like.legacy_window_aliases:
            headers.add(
                PathManager(legacy_window_alias.original).api_path(ext="h"))

    path_manager = PathManager(class_like)
    headers.discard(path_manager.api_path(ext="h"))
    headers.discard(path_manager.impl_path(ext="h"))

    # TODO(yukishiino): Window interface should be
    # [ImplementedAs=LocalDOMWindow] instead of [ImplementedAs=DOMWindow], and
    # [CrossOrigin] properties should be implemented specifically with
    # DOMWindow class.  Then, we'll have less hacks.
    if class_like.identifier == "Window":
        headers.add("third_party/blink/renderer/core/frame/local_dom_window.h")

    return headers


def generate_class_like(class_like,
                        generate_iterator_blink_impl_class_callback=None):
    assert isinstance(class_like,
                      (web_idl.Interface, web_idl.Namespace,
                       web_idl.AsyncIterator, web_idl.SyncIterator))

    path_manager = PathManager(class_like)
    api_component = path_manager.api_component
    impl_component = path_manager.impl_component
    is_cross_components = path_manager.is_cross_components
    for_testing = class_like.code_generator_info.for_testing

    # Class names
    api_class_name = v8_bridge_class_name(class_like)
    if is_cross_components:
        impl_class_name = "{}::Impl".format(api_class_name)
    else:
        impl_class_name = api_class_name

    interface = None
    namespace = None
    if class_like.is_interface:
        interface = class_like
        cg_context = CodeGenContext(interface=interface,
                                    class_name=api_class_name)
    elif class_like.is_namespace:
        namespace = class_like
        cg_context = CodeGenContext(namespace=namespace,
                                    class_name=api_class_name)
    elif class_like.is_async_iterator:
        cg_context = CodeGenContext(async_iterator=class_like,
                                    class_name=api_class_name)
    elif class_like.is_sync_iterator:
        cg_context = CodeGenContext(sync_iterator=class_like,
                                    class_name=api_class_name)
    else:
        assert False

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
                    blink_class_name(class_like)),
        ],
        final=True,
        export=component_export(api_component, for_testing))
    api_class_def.set_base_template_vars(cg_context.template_bindings())
    api_class_def.bottom_section.append(
        TextNode("friend class {};".format(blink_class_name(class_like))))
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
    if class_like.constants:
        constants_def = CxxClassDefNode(name="Constant", final=True)
        constants_def.top_section.append(TextNode("STATIC_ONLY(Constant);"))
        for constant in class_like.constants:
            cgc = cg_context.make_copy(constant=constant)
            constants_def.public_section.append(
                make_constant_constant_def(cgc, constant_name(cgc)))

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
    callback_defs = make_property_entries_and_callback_defs(
        cg_context,
        attribute_entries=attribute_entries,
        constant_entries=constant_entries,
        constructor_entries=constructor_entries,
        exposed_construct_entries=exposed_construct_entries,
        operation_entries=operation_entries)
    supplemental_install_node = SequenceNode()

    # Cross origin properties
    (cross_origin_property_callback_defs,
     cross_origin_property_install_node) = (
         make_cross_origin_property_callbacks_and_install_node(
             cg_context, attribute_entries, operation_entries))
    callback_defs.extend(cross_origin_property_callback_defs)
    supplemental_install_node.append(cross_origin_property_install_node)
    supplemental_install_node.append(EmptyNode())

    # Indexed and named properties
    # Shorten a function name to mitigate a style check error.
    f = make_indexed_and_named_property_callbacks_and_install_node
    (indexed_and_named_property_decls, indexed_and_named_property_defs,
     indexed_and_named_property_install_node) = f(cg_context)
    supplemental_install_node.append(indexed_and_named_property_install_node)
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
         attribute_entries=list(filter(is_unconditional, attribute_entries)),
         constant_entries=list(filter(is_unconditional, constant_entries)),
         exposed_construct_entries=list(
             filter(is_unconditional, exposed_construct_entries)),
         operation_entries=list(filter(is_unconditional, operation_entries)))
    (install_context_independent_props_decl,
     install_context_independent_props_def,
     install_context_independent_props_trampoline) = make_install_properties(
         cg_context,
         FN_INSTALL_CONTEXT_INDEPENDENT_PROPS,
         class_name=impl_class_name,
         prop_install_mode=PropInstallMode.CONTEXT_INDEPENDENT,
         trampoline_var_name=tp_install_context_independent_props,
         attribute_entries=list(
             filter(is_context_independent, attribute_entries)),
         constant_entries=list(filter(is_context_independent,
                                      constant_entries)),
         exposed_construct_entries=list(
             filter(is_context_independent, exposed_construct_entries)),
         operation_entries=list(
             filter(is_context_independent, operation_entries)))
    (install_context_dependent_props_decl, install_context_dependent_props_def,
     install_context_dependent_props_trampoline) = make_install_properties(
         cg_context,
         FN_INSTALL_CONTEXT_DEPENDENT_PROPS,
         class_name=impl_class_name,
         prop_install_mode=PropInstallMode.CONTEXT_DEPENDENT,
         trampoline_var_name=tp_install_context_dependent_props,
         attribute_entries=list(filter(is_context_dependent,
                                       attribute_entries)),
         constant_entries=list(filter(is_context_dependent, constant_entries)),
         exposed_construct_entries=list(
             filter(is_context_dependent, exposed_construct_entries)),
         operation_entries=list(filter(is_context_dependent,
                                       operation_entries)))
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

    # Exposure
    (is_exposed_decl,
     is_exposed_def) = make_is_exposed(cg_context, "IsExposed")

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
         indexed_and_named_property_defs, cross_origin_property_callback_defs,
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
    if class_like.is_async_iterator or class_like.is_sync_iterator:
        api_header_node.accumulator.add_class_decls(
            [blink_class_name(class_like.interface)])
    else:
        api_header_node.accumulator.add_class_decls(
            [blink_class_name(class_like)])
    api_header_node.accumulator.add_include_headers([
        component_export_header(api_component, for_testing),
        "third_party/blink/renderer/platform/bindings/v8_interface_bridge.h",
    ])
    api_source_node.accumulator.add_include_headers([
        # Blink implementation class' header (e.g. node.h for Node)
        (class_like.code_generator_info.blink_headers
         and class_like.code_generator_info.blink_headers[0]),
        "third_party/blink/public/mojom/origin_trial_feature/origin_trial_feature.mojom-shared.h",
        "third_party/blink/renderer/bindings/core/v8/is_return_type_compatible.h",
    ])
    if interface and interface.inherited:
        api_source_node.accumulator.add_include_headers(
            [PathManager(interface.inherited).api_path(ext="h")])
    if is_cross_components:
        impl_header_node.accumulator.add_include_headers([
            api_header_path,
            component_export_header(impl_component, for_testing),
        ])
    impl_source_node.accumulator.add_include_headers([
        # Blink implementation class' header (e.g. node.h for Node)
        (class_like.code_generator_info.blink_headers
         and class_like.code_generator_info.blink_headers[0]),
        "third_party/blink/renderer/bindings/core/v8/generated_code_helper.h",
        "third_party/blink/renderer/bindings/core/v8/is_return_type_compatible.h",
        "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h",
        "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h",
        "third_party/blink/renderer/bindings/core/v8/v8_set_return_value_for_core.h",
        "third_party/blink/renderer/platform/bindings/exception_messages.h",
        "third_party/blink/renderer/platform/bindings/idl_member_installer.h",
        "third_party/blink/renderer/platform/bindings/runtime_call_stats.h",
        "third_party/blink/renderer/platform/bindings/v8_binding.h",
        "third_party/blink/public/mojom/origin_trial_feature/origin_trial_feature.mojom-shared.h",
    ])
    impl_source_node.accumulator.add_include_headers(
        _collect_include_headers(class_like))

    # Assemble the parts.
    if generate_iterator_blink_impl_class_callback:
        assert isinstance(class_like,
                          (web_idl.AsyncIterator, web_idl.SyncIterator))
        generate_iterator_blink_impl_class_callback(
            iterator_class_like=class_like,
            api_component=api_component,
            for_testing=for_testing,
            header_blink_ns=api_header_blink_ns,
            source_blink_ns=api_source_blink_ns)

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

    api_class_def.public_section.extend([
        is_exposed_decl,
        EmptyNode(),
    ])
    api_source_blink_ns.body.extend([
        is_exposed_def,
        EmptyNode(),
    ])

    api_class_def.public_section.append(get_wrapper_type_info_def)
    api_class_def.public_section.append(EmptyNode())
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

    debugging_namespace_name = name_style.namespace("v8",
                                                    class_like.identifier)
    impl_source_blink_ns.body.extend([
        CxxNamespaceNode(
            name="",
            body=[
                # Enclose the implementations with a namespace just in order to
                # include the class_like name in a stacktrace, such as
                #
                #   blink::(anonymous namespace)::v8_class_like::XxxCallback
                #
                # Note that XxxCallback doesn't include the class_like name.
                CxxNamespaceNode(name=debugging_namespace_name,
                                 body=callback_defs),
                EmptyNode(),
                TextNode(
                    "using namespace {};".format(debugging_namespace_name)),
            ]),
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


def generate_interface(interface_identifier):
    assert isinstance(interface_identifier, web_idl.Identifier)

    web_idl_database = package_initializer().web_idl_database()
    interface = web_idl_database.find(interface_identifier)

    generate_class_like(interface)


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
        "mojom::blink::OriginTrialFeature feature",
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
            "mojom::blink::OriginTrialFeature feature",
            "base::span<const WrapperTypeInfo* const> wrapper_type_info_list",
        ],
        return_type="void")

    # Assemble the parts.
    header_node.accumulator.add_class_decls(["ScriptState"])
    header_node.accumulator.add_include_headers([
        "third_party/blink/renderer/platform/feature_context.h",
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
        "third_party/blink/public/mojom/origin_trial_feature/origin_trial_feature.mojom-shared.h",
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
    header_bindings_ns.body.extend([
        TextNode("""\
// Install ES properties associated with the given origin trial feature.\
"""),
        func_decl,
    ])
    source_bindings_ns.body.extend([
        CxxNamespaceNode(name="", body=helper_func_def),
        EmptyNode(),
        func_def,
    ])

    # The public function
    feature_to_class_likes = {}
    set_of_class_likes = set()
    for class_like in itertools.chain(web_idl_database.interfaces,
                                      web_idl_database.namespaces):
        if class_like.code_generator_info.for_testing != for_testing:
            continue

        for member in itertools.chain(class_like.attributes,
                                      class_like.constants,
                                      class_like.operation_groups,
                                      class_like.exposed_constructs):
            features = list(member.exposure.origin_trial_features)
            for entry in member.exposure.global_names_and_features:
                if entry.feature and entry.feature.is_origin_trial:
                    features.append(entry.feature)
            for feature in (member.exposure.
                            only_in_coi_contexts_or_runtime_enabled_features):
                if feature.is_origin_trial:
                    features.append(feature)
            for feature in features:
                feature_to_class_likes.setdefault(feature,
                                                  set()).add(class_like)
            if features:
                set_of_class_likes.add(class_like)

    switch_node = CxxSwitchNode(cond="${feature}")
    switch_node.append(
        case=None,
        body=[
            TextNode("// Ignore unknown, deprecated, and unused features."),
            TextNode("return;"),
        ],
        should_add_break=False)
    for feature, class_likes in sorted(feature_to_class_likes.items()):
        entries = [
            TextNode("{}::GetWrapperTypeInfo(), ".format(
                v8_bridge_class_name(class_like)))
            for class_like in sorted(class_likes, key=lambda x: x.identifier)
        ]
        table_def = ListNode([
            TextNode("static const WrapperTypeInfo* const wti_list[] = {"),
            ListNode(entries),
            TextNode("};"),
        ])
        switch_node.append(
            case="mojom::blink::OriginTrialFeature::k{}".format(feature),
            body=[
                table_def,
                TextNode("selected_wti_list = wti_list;"),
            ])

    func_def.body.extend([
        TextNode(
            "base::span<const WrapperTypeInfo* const> selected_wti_list;"),
        EmptyNode(),
        switch_node,
        EmptyNode(),
        TextNode("InstallPropertiesPerFeatureInternal"
                 "(${script_state}, ${feature}, selected_wti_list);"),
    ])

    for class_like in set_of_class_likes:
        path_manager = PathManager(class_like)
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

for (const auto* wrapper_type_info : wrapper_type_info_list) {
  v8::Local<v8::Object> instance_object;
  v8::Local<v8::Object> prototype_object;
  v8::Local<v8::Function> interface_object;
  v8::Local<v8::Template> interface_template =
      wrapper_type_info->GetV8ClassTemplate(isolate, world);

  switch (wrapper_type_info->idl_definition_kind) {
    case WrapperTypeInfo::kIdlInterface:
      if (!per_context_data->GetExistingConstructorAndPrototypeForType(
              wrapper_type_info, &prototype_object, &interface_object)) {
        continue;
      }
      break;
    case WrapperTypeInfo::kIdlNamespace:
      NOTIMPLEMENTED();
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }

  wrapper_type_info->install_context_dependent_props_func(
      context, world, instance_object, prototype_object,  interface_object,
      interface_template, feature_selector);
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
// Initializes cross-component trampolines of IDL interface / namespace.\
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
    for class_like in itertools.chain(web_idl_database.interfaces,
                                      web_idl_database.namespaces):
        if class_like.code_generator_info.for_testing != for_testing:
            continue

        path_manager = PathManager(class_like)
        if path_manager.is_cross_components:
            source_node.accumulator.add_include_headers(
                [path_manager.impl_path(ext="h")])

            class_name = v8_bridge_class_name(class_like)
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
        # Use the number of attributes + constants + operations as a very rough
        # heuristic for workload. This is by no means close-to-accurate, but is
        # better than nothing.
        task_queue.post_task_with_workload(
            len(interface.attributes) + len(interface.constants) +
            len(interface.operations), generate_interface,
            interface.identifier)

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
