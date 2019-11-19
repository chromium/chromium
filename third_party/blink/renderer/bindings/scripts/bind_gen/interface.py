# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os.path

from . import name_style
from .blink_v8_bridge import blink_class_name
from .blink_v8_bridge import blink_type_info
from .blink_v8_bridge import make_v8_to_blink_value
from .code_node import CodeNode
from .code_node import FunctionDefinitionNode
from .code_node import SymbolDefinitionNode
from .code_node import SymbolNode
from .code_node import SymbolScopeNode
from .code_node import TextNode
from .code_node import UnlikelyExitNode
from .codegen_accumulator import CodeGenAccumulator
from .codegen_context import CodeGenContext
from .codegen_utils import collect_include_headers
from .codegen_utils import enclose_with_namespace
from .codegen_utils import make_copyright_header
from .codegen_utils import make_header_include_directives
from .codegen_utils import write_code_node_to_file
from .mako_renderer import MakoRenderer

_format = CodeNode.format_template


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
        name = name_style.arg_f("arg{}_{}", index, argument.identifier)
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
    func_name = (property_implemented_as
                 or name_style.api_func(cg_context.member_like.identifier))
    if cg_context.attribute_set:
        func_name = name_style.api_func("set", func_name)

    if cg_context.member_like.is_static:
        receiver_implemented_as = (
            cg_context.member_like.code_generator_info.receiver_implemented_as)
        class_name = (receiver_implemented_as
                      or name_style.class_(cg_context.class_like.identifier))
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
            name = name_style.arg_f("arg{}_{}", index, argument.identifier)
            arguments.append(_format("${{{}}}", name))

    if cg_context.is_return_by_argument:
        arguments.append("${return_value}")

    if cg_context.may_throw_exception:
        arguments.append("${exception_state}")

    text = _format("{_1}({_2})", _1=func_name, _2=", ".join(arguments))
    code_node.add_template_var("blink_api_call", TextNode(text))


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


def bind_return_value(code_node, cg_context):
    assert isinstance(code_node, SymbolScopeNode)
    assert isinstance(cg_context, CodeGenContext)

    def create_definition(symbol_node):
        if cg_context.return_type.unwrap().is_void:
            text = "${blink_api_call};"
        elif cg_context.is_return_by_argument:
            pattern = "{_1} ${return_value};\n${blink_api_call};"
            _1 = blink_type_info(cg_context.return_type).value_t
            text = _format(pattern, _1=_1)
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


def make_check_receiver(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    T = TextNode

    if (cg_context.attribute
            and "LenientThis" in cg_context.attribute.extended_attributes):
        return SymbolScopeNode([
            T("// [LenientThis]"),
            UnlikelyExitNode(
                cond=T("!${v8_class}::HasInstance(${receiver}, ${isolate})"),
                body=SymbolScopeNode([T("return;")])),
        ])

    if cg_context.return_type.unwrap().is_promise:
        return SymbolScopeNode([
            T("// Promise returning function: "
              "Convert a TypeError to a reject promise."),
            UnlikelyExitNode(
                cond=T("!${v8_class}::HasInstance(${receiver}, ${isolate})"),
                body=SymbolScopeNode([
                    T("${exception_state}.ThrowTypeError("
                      "\"Illegal invocation\");"),
                    T("return;")
                ])),
        ])

    return None


def make_log_activity(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    ext_attrs = cg_context.member_like.extended_attributes
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
    if cg_context.attribute_set:
        _1 = "LogSetter"
        _4 = ", ${info}[0]"
    if cg_context.operation_group:
        _1 = "LogMethod"
        _4 = ", ${info}"
    body = _format(pattern, _1=_1, _2=_2, _3=_3, _4=_4)

    pattern = ("// [LogActivity], [LogAllWorlds]\n" "if ({_1}) {{ {_2} }}")
    node = TextNode(_format(pattern, _1=cond, _2=body))
    node.accumulate(
        CodeGenAccumulator.require_include_headers([
            "third_party/blink/renderer/"
            "platform/bindings/v8_dom_activity_logger.h",
        ]))
    return node


def make_report_deprecate_as(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    name = cg_context.member_like.extended_attributes.value_of("DeprecateAs")
    if not name:
        return None

    pattern = ("// [DeprecateAs]\n"
               "Deprecation::CountDeprecation("
               "${execution_context}, WebFeature::k{_1});")
    _1 = name
    node = TextNode(_format(pattern, _1=_1))
    node.accumulate(
        CodeGenAccumulator.require_include_headers([
            "third_party/blink/renderer/core/frame/deprecation.h",
        ]))
    return node


def make_report_measure_as(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    ext_attrs = cg_context.member_like.extended_attributes
    if not ("Measure" in ext_attrs or "MeasureAs" in ext_attrs):
        assert "HighEntropy" not in ext_attrs, "{}: {}".format(
            cg_context.idl_location_and_name,
            "[HighEntropy] must be specified with either [Measure] or "
            "[MeasureAs].")
        return None

    suffix = ""
    if cg_context.attribute_get:
        suffix = "_AttributeGetter"
    elif cg_context.attribute_set:
        suffix = "_AttributeSetter"
    elif cg_context.constructor:
        suffix = "_Constructor"
    elif cg_context.operation:
        suffix = "_Method"
    name = cg_context.member_like.extended_attributes.value_of("MeasureAs")
    if name:
        name = "k{}".format(name)
    elif cg_context.constructor:
        name = "kV8{}{}".format(cg_context.class_like.identifier, suffix)
    else:
        name = "kV8{}_{}{}".format(
            cg_context.class_like.identifier,
            name_style.raw.upper_camel_case(cg_context.member_like.identifier),
            suffix)

    node = SymbolScopeNode()

    pattern = ("// [Measure], [MeasureAs]\n"
               "UseCounter::Count(${execution_context}, WebFeature::{_1});")
    _1 = name
    node.append(TextNode(_format(pattern, _1=_1)))
    node.accumulate(
        CodeGenAccumulator.require_include_headers([
            "third_party/blink/renderer/core/frame/web_feature.h",
            "third_party/blink/renderer/platform/instrumentation/use_counter.h",
        ]))

    if "HighEntropy" not in ext_attrs or cg_context.attribute_set:
        return node

    pattern = (
        "// [HighEntropy]\n"
        "Dactyloscoper::Record(${execution_context}, WebFeature::{_1});")
    _1 = name
    node.append(TextNode(_format(pattern, _1=_1)))
    node.accumulate(
        CodeGenAccumulator.require_include_headers([
            "third_party/blink/renderer/core/frame/dactyloscoper.h",
        ]))

    return node


def make_runtime_call_timer_scope(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    pattern = "RUNTIME_CALL_TIMER_SCOPE{_1}(${isolate}, {_2});"
    _1 = "_DISABLED_BY_DEFAULT"
    suffix = ""
    if cg_context.attribute_get:
        suffix = "_Getter"
    elif cg_context.attribute_set:
        suffix = "_Setter"
    counter = cg_context.member_like.extended_attributes.value_of(
        "RuntimeCallStatsCounter")
    if counter:
        _2 = "k{}{}".format(counter, suffix)
    else:
        _2 = "\"Blink_{}_{}{}\"".format(
            blink_class_name(cg_context.class_like),
            cg_context.member_like.identifier, suffix)
    node = TextNode(_format(pattern, _1=_1, _2=_2))
    node.accumulate(
        CodeGenAccumulator.require_include_headers([
            "third_party/blink/renderer/platform/bindings/runtime_call_stats.h",
        ]))
    return node


_callback_common_binders = (
    bind_blink_api_arguments,
    bind_blink_api_call,
    bind_callback_local_vars,
    bind_return_value,
    bind_v8_set_return_value,
)


def make_attribute_get_def(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    T = TextNode

    cg_context = cg_context.make_copy(attribute_get=True)

    func_name = name_style.func(cg_context.attribute.identifier,
                                "AttributeGetCallback")

    func_def = FunctionDefinitionNode(
        name=T(func_name),
        arg_decls=[T("const v8::FunctionCallbackInfo<v8::Value>& info")],
        return_type=T("void"))

    body = func_def.body
    body.add_template_var("info", "info")
    body.add_template_vars(cg_context.template_bindings())

    for bind in _callback_common_binders:
        bind(body, cg_context)

    body.extend([
        make_runtime_call_timer_scope(cg_context),
        make_report_deprecate_as(cg_context),
        make_report_measure_as(cg_context),
        make_log_activity(cg_context),
        T(""),
        make_check_receiver(cg_context),
        T(""),
        T("${v8_set_return_value}"),
    ])

    return func_def


def make_operation_def(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    T = TextNode

    func_name = name_style.func(cg_context.operation.identifier,
                                "OperationCallback")

    func_def = FunctionDefinitionNode(
        name=T(func_name),
        arg_decls=[T("const v8::FunctionCallbackInfo<v8::Value>& info")],
        return_type=T("void"))

    body = func_def.body
    body.add_template_var("info", "info")
    body.add_template_vars(cg_context.template_bindings())

    for bind in _callback_common_binders:
        bind(body, cg_context)

    body.extend([
        make_runtime_call_timer_scope(cg_context),
        make_report_deprecate_as(cg_context),
        make_report_measure_as(cg_context),
        make_log_activity(cg_context),
        T(""),
        make_check_receiver(cg_context),
        T(""),
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

    T = TextNode

    func_def = FunctionDefinitionNode(
        name=T("InstallInterfaceTemplate"),
        arg_decls=[
            T("v8::Isolate* isolate"),
            T("const DOMWrapperWorld& world"),
            T("v8::Local<v8::FunctionTemplate> interface_template"),
        ],
        return_type=T("void"))

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


def generate_interfaces(web_idl_database, output_dirs):
    filename = "v8_example_interface.cc"
    filepath = os.path.join(output_dirs['core'], filename)

    interface = web_idl_database.find("CSS")

    cg_context = CodeGenContext(interface=interface)

    root_node = SymbolScopeNode(separator_last="\n")
    root_node.set_accumulator(CodeGenAccumulator())
    root_node.set_renderer(MakoRenderer())

    root_node.accumulator.add_include_headers(
        collect_include_headers(interface))

    code_node = SymbolScopeNode()

    for attribute in interface.attributes:
        code_node.append(
            make_attribute_get_def(cg_context.make_copy(attribute=attribute)))

    for operation_group in interface.operation_groups:
        for operation in operation_group:
            code_node.append(
                make_operation_def(
                    cg_context.make_copy(
                        operation_group=operation_group, operation=operation)))

    code_node.append(make_install_interface_template_def(cg_context))

    root_node.extend([
        make_copyright_header(),
        TextNode(""),
        make_header_include_directives(root_node.accumulator),
        TextNode(""),
        enclose_with_namespace(code_node, name_style.namespace("blink")),
    ])

    write_code_node_to_file(root_node, filepath)
