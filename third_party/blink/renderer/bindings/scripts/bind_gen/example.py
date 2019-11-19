# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os.path

from blinkbuild.name_style_converter import NameStyleConverter

from .clang_format import clang_format
from .code_node import CodeNode
from .code_node import FunctionDefinitionNode
from .code_node import LiteralNode
from .code_node import SequenceNode
from .code_node import SymbolDefinitionNode
from .code_node import SymbolNode
from .code_node import SymbolScopeNode
from .code_node import TextNode
from .code_node import UnlikelyExitNode
from .codegen_context import CodeGenContext
from .mako_renderer import MakoRenderer

_format = CodeNode.format_template


def _snake_case(name):
    return NameStyleConverter(name).to_snake_case()


def _upper_camel_case(name):
    return NameStyleConverter(name).to_upper_camel_case()


def bind_callback_local_vars(code_node, cg_context):
    assert isinstance(code_node, SymbolScopeNode)
    assert isinstance(cg_context, CodeGenContext)

    S = SymbolNode
    T = TextNode

    local_vars = []
    template_vars = {}

    local_vars.extend([
        S("class_like_name", ("const char* const ${class_like_name} = "
                              "\"${class_like.identifier}\";")),
        S("current_context", ("v8::Local<v8::Context> ${current_context} = "
                              "${isolate}->GetCurrentContext();")),
        S("current_script_state", ("ScriptState* ${current_script_state} = "
                                   "ScriptState::From(${current_context});")),
        S("execution_context", ("ExecutionContext* ${execution_context} = "
                                "ExecutionContext::From(${script_state});")),
        S("isolate", "v8::Isolate* ${isolate} = ${info}.GetIsolate();"),
        S("per_context_data", ("V8PerContextData* ${per_context_data} = "
                               "${script_state}->PerContextData();")),
        S("per_isolate_data", ("V8PerIsolateData* ${per_isolate_data} = "
                               "V8PerIsolateData::From(${isolate});")),
        S("property_name",
          "const char* const ${property_name} = \"${property.identifier}\";"),
        S("receiver", "v8::Local<v8::Object> ${receiver} = ${info}.This();"),
        S("receiver_context", ("v8::Local<v8::Context> ${receiver_context} = "
                               "${receiver}->CreationContext();")),
        S("receiver_script_state",
          ("ScriptState* ${receiver_script_state} = "
           "ScriptState::From(${receiver_context});")),
    ])

    is_receiver_context = (cg_context.member_like
                           and not cg_context.member_like.is_static)

    # creation_context
    pattern = "const v8::Local<v8::Context>& ${creation_context} = {_1};"
    _1 = "${receiver_context}" if is_receiver_context else "${current_context}"
    local_vars.append(S("creation_context", _format(pattern, _1=_1)))

    # creation_context_object
    text = ("${receiver}"
            if is_receiver_context else "${current_context}->Global()")
    template_vars["creation_context_object"] = T(text)

    # script_state
    pattern = "ScriptState* ${script_state} = {_1};"
    _1 = ("${receiver_script_state}"
          if is_receiver_context else "${current_script_state}")
    local_vars.append(S("script_state", _format(pattern, _1=_1)))

    # exception_state_context_type
    pattern = (
        "const ExceptionState::ContextType ${exception_state_context_type} = "
        "{_1};")
    if cg_context.attribute_get:
        _1 = "ExceptionState::kGetterContext"
    elif cg_context.attribute_set:
        _1 = "ExceptionState::kSetterContext"
    elif cg_context.constructor:
        _1 = "ExceptionState::kConstructionContext"
    else:
        _1 = "ExceptionState::kExecutionContext"
    local_vars.append(
        S("exception_state_context_type", _format(pattern, _1=_1)))

    # exception_state
    pattern = "ExceptionState ${exception_state}({_1});{_2}"
    _1 = [
        "${isolate}", "${exception_state_context_type}", "${class_like_name}",
        "${property_name}"
    ]
    _2 = ""
    if cg_context.return_type and cg_context.return_type.unwrap().is_promise:
        _2 = ("\n"
              "ExceptionToRejectPromiseScope reject_promise_scope"
              "(${info}, ${exception_state});")
    local_vars.append(
        S("exception_state", _format(pattern, _1=", ".join(_1), _2=_2)))

    code_node.register_code_symbols(local_vars)
    code_node.add_template_vars(template_vars)


def make_v8_to_blink_value(blink_var_name, v8_value_expr, idl_type):
    assert isinstance(blink_var_name, str)
    assert isinstance(v8_value_expr, str)

    pattern = "NativeValueTraits<{_1}>::NativeValue({_2})"
    _1 = "IDL{}".format(idl_type.type_name)
    _2 = ["${isolate}", v8_value_expr, "${exception_state}"]

    blink_value = _format(pattern, _1=_1, _2=", ".join(_2))
    idl_type_tag = _1

    pattern = "{_1}& ${{{_2}}} = {_3};"
    _1 = "NativeValueTraits<{}>::ImplType".format(idl_type_tag)
    _2 = blink_var_name
    _3 = blink_value
    text = _format(pattern, _1=_1, _2=_2, _3=_3)

    def create_definition(symbol_node):
        return SymbolDefinitionNode(symbol_node, [
            TextNode(text),
            UnlikelyExitNode(
                cond=TextNode("${exception_state}.HadException()"),
                body=SymbolScopeNode([TextNode("return;")])),
        ])

    return SymbolNode(blink_var_name, definition_constructor=create_definition)


def bind_blink_api_arguments(code_node, cg_context):
    assert isinstance(code_node, SymbolScopeNode)
    assert isinstance(cg_context, CodeGenContext)

    if cg_context.attribute_get:
        return

    if cg_context.attribute_set:
        name = "arg1_value"
        v8_value = "${info}[0]"
        code_node.register_code_symbol(
            make_v8_to_blink_value(name, v8_value,
                                   cg_context.attribute.idl_type))
        return

    for index, argument in enumerate(cg_context.function_like.arguments, 1):
        name = "arg{}_{}".format(index, _snake_case(argument.identifier))
        if argument.is_variadic:
            assert False, "Variadic arguments are not yet supported"
        else:
            v8_value = "${{info}}[{}]".format(argument.index)
            code_node.register_code_symbol(
                make_v8_to_blink_value(name, v8_value, argument.idl_type))


def bind_blink_api_call(code_node, cg_context):
    assert isinstance(code_node, SymbolScopeNode)
    assert isinstance(cg_context, CodeGenContext)

    property_implemented_as = (
        cg_context.member_like.code_generator_info.property_implemented_as)
    func_name = property_implemented_as or cg_context.member_like.identifier
    if cg_context.attribute_set:
        func_name = "set{}".format(_upper_camel_case(func_name))

    if cg_context.member_like.is_static:
        receiver_implemented_as = (
            cg_context.member_like.code_generator_info.receiver_implemented_as)
        class_name = receiver_implemented_as or cg_context.class_like.identifier
        func_name = "{}::{}".format(class_name, func_name)

    arguments = []
    ext_attrs = cg_context.member_like.extended_attributes

    values = ext_attrs.values_of("CallWith") + (
        ext_attrs.values_of("SetterCallWith") if cg_context.attribute_set else
        ())
    if "Isolate" in values:
        arguments.append("${isolate}")
    if "ScriptState" in values:
        arguments.append("${script_state}")
    if "ExecutionContext" in values:
        arguments.append("${execution_context}")

    if cg_context.attribute_get:
        pass
    elif cg_context.attribute_set:
        arguments.append("${arg1_value}")
    else:
        for index, argument in enumerate(cg_context.function_like.arguments,
                                         1):
            arguments.append("${{arg{}_{}}}".format(
                index, _snake_case(argument.identifier)))

    if cg_context.is_return_by_argument:
        arguments.append("${return_value}")

    if cg_context.may_throw_exception:
        arguments.append("${exception_state}")

    text = _format("{_1}({_2})", _1=func_name, _2=", ".join(arguments))
    code_node.add_template_var("blink_api_call", TextNode(text))


def bind_return_value(code_node, cg_context):
    assert isinstance(code_node, SymbolScopeNode)
    assert isinstance(cg_context, CodeGenContext)

    def create_definition(symbol_node):
        if cg_context.return_type.unwrap().is_void:
            text = "${blink_api_call};"
        elif cg_context.is_return_by_argument:
            text = "${blink_return_type} ${return_value};\n${blink_api_call};"
        else:
            text = "const auto& ${return_value} = ${blink_api_call};"
        node = SymbolDefinitionNode(symbol_node, [TextNode(text)])
        if cg_context.may_throw_exception:
            node.append(
                UnlikelyExitNode(
                    cond=TextNode("${exception_state}.HadException()"),
                    body=SymbolScopeNode([TextNode("return;")])))
        return node

    code_node.register_code_symbol(
        SymbolNode("return_value", definition_constructor=create_definition))


def bind_v8_set_return_value(code_node, cg_context):
    assert isinstance(code_node, SymbolScopeNode)
    assert isinstance(cg_context, CodeGenContext)

    pattern = "{_1}({_2});"
    _1 = "V8SetReturnValue"
    _2 = ["${info}", "${return_value}"]

    if cg_context.return_type.unwrap().is_void:
        # Render a SymbolNode |return_value| discarding the content text, and
        # let a symbol definition be added.
        pattern = "<% str(return_value) %>"
    elif (cg_context.for_world == cg_context.MAIN_WORLD
          and cg_context.return_type.unwrap().is_interface):
        _1 = "V8SetReturnValueForMainWorld"
    elif cg_context.return_type.unwrap().is_interface:
        _2.append("${creation_context_object}")

    text = _format(pattern, _1=_1, _2=", ".join(_2))
    code_node.add_template_var("v8_set_return_value", TextNode(text))


def make_attribute_get_def(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    L = LiteralNode
    T = TextNode

    cg_context = cg_context.make_copy(attribute_get=True)

    func_def = FunctionDefinitionNode(
        name=L("AttributeGetCallback"),
        arg_decls=[L("const v8::FunctionCallbackInfo<v8::Value>& info")],
        return_type=L("void"))

    body = func_def.body
    body.add_template_var("info", "info")
    body.add_template_vars(cg_context.template_bindings())

    binders = [
        bind_callback_local_vars,
        bind_blink_api_arguments,
        bind_blink_api_call,
        bind_return_value,
        bind_v8_set_return_value,
    ]
    for bind in binders:
        bind(body, cg_context)

    body.extend([
        T("${v8_set_return_value}"),
    ])

    return func_def


def make_operation_def(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    L = LiteralNode
    T = TextNode

    func_def = FunctionDefinitionNode(
        name=L("OperationCallback"),
        arg_decls=[L("const v8::FunctionCallbackInfo<v8::Value>& info")],
        return_type=L("void"))

    body = func_def.body
    body.add_template_var("info", "info")
    body.add_template_vars(cg_context.template_bindings())

    binders = [
        bind_callback_local_vars,
        bind_blink_api_arguments,
        bind_blink_api_call,
        bind_return_value,
        bind_v8_set_return_value,
    ]
    for bind in binders:
        bind(body, cg_context)

    body.extend([
        T("${v8_set_return_value}"),
    ])

    return func_def


def bind_template_installer_local_vars(code_node, cg_context):
    assert isinstance(code_node, SymbolScopeNode)
    assert isinstance(cg_context, CodeGenContext)

    S = SymbolNode

    local_vars = []

    local_vars.extend([
        S("instance_template",
          ("v8::Local<v8::ObjectTemplate> ${instance_template} = "
           "${interface_template}->InstanceTemplate();")),
        S("prototype_template",
          ("v8::Local<v8::ObjectTemplate> ${prototype_template} = "
           "${interface_template}->PrototypeTemplate();")),
        S("signature",
          ("v8::Local<v8::Signature> ${signature} = "
           "v8::Signature::New(${isolate}, ${interface_template});")),
        S("wrapper_type_info",
          ("const WrapperTypeInfo* const ${wrapper_type_info} = "
           "${v8_class}::GetWrapperTypeInfo();")),
    ])

    pattern = (
        "v8::Local<v8::FunctionTemplate> ${parent_interface_template}{_1};")
    _1 = (" = ${wrapper_type_info}->parent_class->dom_template_function"
          "(${isolate}, ${world})")
    if not cg_context.class_like.inherited:
        _1 = ""
    local_vars.append(S("parent_interface_template", _format(pattern, _1=_1)))

    code_node.register_code_symbols(local_vars)


def make_install_interface_template_def(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    L = LiteralNode
    T = TextNode

    func_def = FunctionDefinitionNode(
        name=L("InstallInterfaceTemplate"),
        arg_decls=[
            L("v8::Isolate* isolate"),
            L("const DOMWrapperWorld& world"),
            L("v8::Local<v8::FunctionTemplate> interface_template"),
        ],
        return_type=L("void"))

    body = func_def.body
    body.add_template_var("isolate", "isolate")
    body.add_template_var("world", "world")
    body.add_template_var("interface_template", "interface_template")
    body.add_template_vars(cg_context.template_bindings())

    binders = [
        bind_template_installer_local_vars,
    ]
    for bind in binders:
        bind(body, cg_context)

    body.extend([
        T("V8DOMConfiguration::InitializeDOMInterfaceTemplate("
          "${isolate}, ${interface_template}, "
          "${wrapper_type_info}->interface_name, ${parent_interface_template}, "
          "kV8DefaultWrapperInternalFieldCount);"),
    ])

    if cg_context.class_like.constructor_groups:
        body.extend([
            T("${interface_template}->SetCallHandler(ConstructorCallback);"),
            T("${interface_template}->SetLength("
              "${class_like.constructor_groups[0]"
              ".min_num_of_required_arguments});"),
        ])

    return func_def


def run_example(web_idl_database, output_dirs):
    filename = 'v8_example.cc'
    filepath = os.path.join(output_dirs['core'], filename)

    namespace = list(web_idl_database.namespaces)[0]

    cg_context = CodeGenContext(namespace=namespace)

    root_node = SymbolScopeNode(separator_last="\n")
    root_node.set_renderer(MakoRenderer())

    for attribute in namespace.attributes:
        root_node.append(
            make_attribute_get_def(cg_context.make_copy(attribute=attribute)))

    for operation_group in namespace.operation_groups:
        for operation in operation_group:
            root_node.append(
                make_operation_def(
                    cg_context.make_copy(
                        operation_group=operation_group, operation=operation)))

    root_node.append(make_install_interface_template_def(cg_context))

    prev = ''
    current = str(root_node)
    while current != prev:
        prev = current
        current = str(root_node)
    rendered_text = current

    format_result = clang_format(rendered_text, filename=filename)
    with open(filepath, 'w') as output_file:
        output_file.write(format_result.contents)
