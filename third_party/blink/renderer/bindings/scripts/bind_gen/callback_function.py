# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import web_idl

from . import name_style
from .blink_v8_bridge import blink_class_name
from .blink_v8_bridge import blink_type_info
from .blink_v8_bridge import make_blink_to_v8_value
from .blink_v8_bridge import native_value_tag
from .code_node import EmptyNode
from .code_node import FormatNode
from .code_node import ListNode
from .code_node import SequenceNode
from .code_node import SymbolNode
from .code_node import SymbolScopeNode
from .code_node import TextNode
from .code_node import Likeliness
from .code_node_cxx import CxxClassDefNode
from .code_node_cxx import CxxForLoopNode
from .code_node_cxx import CxxFuncDeclNode
from .code_node_cxx import CxxFuncDefNode
from .code_node_cxx import CxxIfElseNode
from .code_node_cxx import CxxLikelyIfNode
from .code_node_cxx import CxxNamespaceNode
from .code_node_cxx import CxxUnlikelyIfNode
from .codegen_accumulator import CodeGenAccumulator
from .codegen_context import CodeGenContext
from .codegen_utils import collect_forward_decls_and_include_headers
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


def bind_local_vars(code_node, cg_context, is_construct_call=False):
    assert isinstance(code_node, SymbolScopeNode)
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(is_construct_call, bool)

    S = SymbolNode

    local_vars = []

    local_vars.extend([
        S("isolate", "v8::Isolate* ${isolate} = GetIsolate();"),
        S("script_state",
          "ScriptState* ${script_state} = CallbackRelevantScriptState();"),
    ])

    if cg_context.callback_function:
        local_vars.append(
            S("class_like_name", ("const char* const ${class_like_name} = "
                                  "\"${callback_function.identifier}\";")))
        if is_construct_call:
            local_vars.append(
                S("property_name",
                  "const char* const ${property_name} = \"construct\";"))
        else:
            local_vars.append(
                S("property_name",
                  "const char* const ${property_name} = \"invoke\";"))
    elif cg_context.callback_interface:
        local_vars.extend([
            S("class_like_name", ("const char* const ${class_like_name} = "
                                  "\"${callback_interface.identifier}\";")),
            S("property_name", ("const char* const ${property_name} = "
                                "\"${property.identifier}\";")),
        ])

    code_node.register_code_symbols(local_vars)


def _make_arg_type_and_names(func_like):
    assert isinstance(func_like, web_idl.FunctionLike)

    arg_type_and_names = []
    for index, argument in enumerate(func_like.arguments):
        type_info = blink_type_info(argument.idl_type)
        if argument.idl_type.unwrap(variadic=False).is_interface:
            arg_type = type_info.ref_t
        else:
            arg_type = type_info.const_ref_t
        arg_name = name_style.arg_f("arg{}_{}", index + 1, argument.identifier)
        arg_type_and_names.append((arg_type, arg_name))
    return arg_type_and_names


def make_factory_methods(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    func_def = CxxFuncDefNode(
        name="Create",
        arg_decls=["v8::Local<v8::Object> callback_object"],
        return_type="${class_name}*",
        static=True)
    func_def.set_base_template_vars(cg_context.template_bindings())

    func_def.body.append(
        TextNode("return MakeGarbageCollected<${class_name}>("
                 "callback_object);"))

    return func_def, None


def make_constructors(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    decls = ListNode([
        CxxFuncDefNode(name=cg_context.class_name,
                       arg_decls=["v8::Local<v8::Object> callback_object"],
                       return_type="",
                       explicit=True,
                       member_initializer_list=[
                           "${base_class_name}(callback_object)",
                       ]),
        CxxFuncDeclNode(name="~${class_name}",
                        arg_decls=[],
                        return_type="",
                        override=True,
                        default=True),
    ])

    return decls, None


def make_nameclient_implementation(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    func_decl = CxxFuncDeclNode(name="NameInHeapSnapshot",
                                arg_decls=[],
                                return_type="const char*",
                                const=True,
                                override=True)

    func_def = CxxFuncDefNode(name="NameInHeapSnapshot",
                              arg_decls=[],
                              return_type="const char*",
                              class_name=cg_context.class_name,
                              const=True)
    func_def.set_base_template_vars(cg_context.template_bindings())
    func_def.body.append(TextNode("return \"${class_name}\";"))

    return func_decl, func_def


def make_callback_invocation_function(cg_context,
                                      function_name,
                                      skip_runnability_check=False,
                                      is_construct_call=False):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)
    assert isinstance(skip_runnability_check, bool)
    assert isinstance(is_construct_call, bool)

    T = TextNode
    F = FormatNode

    func_like = cg_context.function_like
    return_type = ("void" if func_like.return_type.unwrap().is_undefined else
                   blink_type_info(func_like.return_type).value_t)
    maybe_return_type = "v8::Maybe<{}>".format(return_type)
    arg_type_and_names = _make_arg_type_and_names(func_like)
    arg_decls = [
        "{} {}".format(arg_type, arg_name)
        for arg_type, arg_name in arg_type_and_names
    ]
    if not is_construct_call:
        arg_decls.insert(
            0, "bindings::V8ValueOrScriptWrappableAdapter arg0_receiver")

    decls = SequenceNode()
    defs = SequenceNode()

    func_decl = CxxFuncDeclNode(name=function_name,
                                arg_decls=arg_decls,
                                return_type=maybe_return_type,
                                nodiscard=True)
    if cg_context.callback_function:
        if is_construct_call:
            comment = T("""\
// Performs "construct".
// https://webidl.spec.whatwg.org/#construct-a-callback-function\
""")
        else:
            comment = T("""\
// Performs "invoke".
// https://webidl.spec.whatwg.org/#invoke-a-callback-function\
""")
    elif cg_context.callback_interface:
        comment = T("""\
// Performs "call a user object's operation".
// https://webidl.spec.whatwg.org/#call-a-user-objects-operation\
""")
    decls.extend([
        comment,
        func_decl,
    ])

    func_def = CxxFuncDefNode(name=function_name,
                              arg_decls=arg_decls,
                              return_type=maybe_return_type,
                              class_name=cg_context.class_name)
    func_def.set_base_template_vars(cg_context.template_bindings())
    defs.append(func_def)
    body = func_def.body
    if is_construct_call:
        body.add_template_var("arg0_receiver", "nullptr")
    else:
        body.add_template_var("arg0_receiver", "arg0_receiver")
    for arg_type, arg_name in arg_type_and_names:
        body.add_template_var(arg_name, arg_name)
    bind_local_vars(body, cg_context, is_construct_call)

    if func_like.return_type.unwrap(typedef=True).is_undefined:
        text = "v8::JustVoid()"
    else:
        text = "helper.Result<{}, {}>()".format(
            native_value_tag(func_like.return_type), return_type)
    body.add_template_var("return_value_on_success", text)
    text = "v8::Nothing<{}>()".format(return_type)
    body.add_template_var("return_value_on_failure", text)

    body.append(
        T("""\
ScriptState* callback_relevant_script_state =
    CallbackRelevantScriptStateOrThrowException(
        ${class_like_name}, ${property_name});
if (!callback_relevant_script_state) {
  return ${return_value_on_failure};
}
"""))

    if not skip_runnability_check:
        body.extend([
            CxxUnlikelyIfNode(
                cond=("!IsCallbackFunctionRunnable("
                      "callback_relevant_script_state, "
                      "IncumbentScriptState())"),
                attribute=None,
                body=[
                    T("v8::HandleScope handle_scope(${isolate});"),
                    T("v8::Context::Scope context_scope("
                      "callback_relevant_script_state->GetContext());"),
                    T("V8ThrowException::ThrowError(${isolate},"
                      "\"The provided callback is no longer runnable.\");"),
                    T("return ${return_value_on_failure};"),
                ]),
            EmptyNode(),
        ])

    if cg_context.callback_function:
        template_params = ["${base_class_name}"]
        if is_construct_call:
            template_params.append(
                "bindings::CallbackInvokeHelperMode::kConstructorCall")
        elif "LegacyTreatNonObjectAsNull" in func_like.extended_attributes:
            template_params.append(
                "bindings::"
                "CallbackInvokeHelperMode::kLegacyTreatNonObjectAsNull")
        else:
            template_params.append(
                "bindings::CallbackInvokeHelperMode::kDefault")
        if func_like.return_type.unwrap(typedef=True).is_promise:
            template_params.append(
                "bindings::CallbackReturnTypeIsPromise::kYes")
        else:
            template_params.append(
                "bindings::CallbackReturnTypeIsPromise::kNo")
    elif cg_context.callback_interface:
        template_params = ["CallbackInterfaceBase"]
    body.extend([
        F("""\
bindings::CallbackInvokeHelper<{template_params}> helper(
    this, ${class_like_name}, ${property_name});\
""",
          template_params=", ".join(template_params)),
        CxxUnlikelyIfNode(cond="!helper.PrepareForCall(${arg0_receiver})",
                          attribute="[[unlikely]]",
                          body=[
                              CxxLikelyIfNode(
                                  cond="helper.V8Result().IsEmpty()",
                                  attribute=None,
                                  body=[
                                      T("return ${return_value_on_failure};"),
                                  ]),
                              T("return ${return_value_on_success};"),
                          ]),
    ])

    # The maximum number of arguments to a variadic function that we're willing
    # to allocate on the stack. If the function takes more, we'll use the heap
    # instead.
    max_stack_array_length = 10
    arguments = func_like.arguments

    is_variadic = arguments and arguments[-1].is_variadic
    if is_variadic:
        _, variadic_arg_name = arg_type_and_names[-1]

    # The next step in the C++ code is to define an array containing all
    # the arguments, and its length argc. Depending on how many args we have,
    # argv will either be a regular array or a LocalVector. To hide the
    # difference, we unify both of them into a single span type
    if not arguments:
        body.append(T("base::span<v8::Local<v8::Value>> argv;"))
    elif not is_variadic:
        body.append(
            T("v8::Local<v8::Value> argv_arr[{}];".format(len(arguments))))
        body.append(T("base::span<v8::Local<v8::Value>> argv(argv_arr);"))
    else:
        # Forward declare both possible representations of argv so they're in
        # scope for the rest of the function. We use a span to hide the
        # difference from the type system.
        body.append(
            T("v8::Local<v8::Value> argv_arr[{}];".format(
                max_stack_array_length))),
        body.append(T("v8::LocalVector<v8::Value> argv_vec(GetIsolate());")),
        body.append(T("base::span<v8::Local<v8::Value>> argv;")),

        body.append(
            T("const size_t argc = {} + {}.size();".format(
                len(arguments) - 1, variadic_arg_name)))
        body.append(
            CxxIfElseNode(
                cond=T("argc <= {}".format(max_stack_array_length)),
                attribute=None,
                # If argc is small, just use argv-arr
                then=SymbolScopeNode(
                    code_nodes=[T("argv = base::make_span(argv_arr, argc);")]),
                then_likeliness=Likeliness.LIKELY,
                # If argc is large, create a vector instead
                else_=SymbolScopeNode(code_nodes=[
                    T("argv_vec.resize(argc);"),
                    T("argv = argv_vec;"),
                ]),
                else_likeliness=Likeliness.UNLIKELY))

    for index, arg_type_and_name in enumerate(arg_type_and_names):
        if arguments[index].is_variadic:
            break
        _, arg_name = arg_type_and_name
        v8_arg_name = name_style.local_var_f("v8_arg{}_{}", index + 1,
                                             arguments[index].identifier)
        body.register_code_symbol(
            make_blink_to_v8_value(v8_arg_name,
                                   arg_name,
                                   arguments[index].idl_type,
                                   argument=arguments[index],
                                   error_exit_return_statement=(
                                       "return ${return_value_on_failure};")))
        body.append(
            F("argv[{index}] = ${{{v8_arg}}};",
              index=index,
              v8_arg=v8_arg_name))
    if is_variadic:
        v8_arg_name = name_style.local_var_f("v8_arg{}_{}", len(arguments),
                                             arguments[-1].identifier)
        body.register_code_symbol(
            make_blink_to_v8_value(
                v8_arg_name,
                "{}[i]".format(variadic_arg_name),
                arguments[-1].idl_type.unwrap(variadic=True),
                argument=arguments[-1],
                error_exit_return_statement=(
                    "return ${return_value_on_failure};")))
        body.append(
            CxxForLoopNode(
                cond=F("wtf_size_t i = 0; i < {var_arg}.size(); ++i",
                       var_arg=variadic_arg_name),
                body=[
                    F("argv[{non_var_arg_size} + i] = ${{{v8_arg}}};",
                      non_var_arg_size=len(arguments) - 1,
                      v8_arg=v8_arg_name),
                ],
                weak_dep_syms=["isolate", "script_state"]))
    body.extend([
        CxxUnlikelyIfNode(
            cond="!helper.Call(static_cast<int>(argv.size()), argv.data())",
            attribute=None,
            body=[
                T("return ${return_value_on_failure};"),
            ]),
        T("return ${return_value_on_success};"),
    ])

    return decls, defs


def make_invoke_and_report_function(cg_context, function_name, api_func_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)
    assert isinstance(api_func_name, str)

    T = TextNode
    F = FormatNode

    func_like = cg_context.function_like
    if not (func_like.return_type.unwrap().is_undefined
            or func_like.identifier == "Function"):
        return None, None

    arg_type_and_names = _make_arg_type_and_names(func_like)
    arg_decls = ["bindings::V8ValueOrScriptWrappableAdapter arg0_receiver"] + [
        "{} {}".format(arg_type, arg_name)
        for arg_type, arg_name in arg_type_and_names
    ]

    decls = SequenceNode()
    defs = SequenceNode()

    func_decl = CxxFuncDeclNode(name=function_name,
                                arg_decls=arg_decls,
                                return_type="void")
    if cg_context.callback_function:
        comment = T("""\
// Performs "invoke" and then reports an exception if any to the global
// error handler such as DevTools console.\
""")
    elif cg_context.callback_interface:
        comment = T("""\
// Performs "call a user object's operation" and then reports an exception
// if any to the global error handler such as DevTools console.\
""")
    decls.extend([
        comment,
        func_decl,
    ])

    func_def = CxxFuncDefNode(name=function_name,
                              arg_decls=arg_decls,
                              return_type="void",
                              class_name=cg_context.class_name)
    func_def.set_base_template_vars(cg_context.template_bindings())
    defs.append(func_def)
    body = func_def.body
    bind_local_vars(body, cg_context)

    arg_names = ["arg0_receiver"
                 ] + [arg_name for arg_type, arg_name in arg_type_and_names]

    body.extend([
        T("v8::TryCatch try_catch(${isolate});"),
        T("try_catch.SetVerbose(true);"),
        EmptyNode(),
        F("std::ignore = {api_func_name}({arg_names});",
          api_func_name=api_func_name,
          arg_names=", ".join(arg_names)),
    ])

    return decls, defs


def make_is_runnable_or_throw_exception(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    T = TextNode

    decls = SequenceNode()
    defs = SequenceNode()

    ignore_pause_def = ListNode([
        T("enum class IgnorePause {"),
        T("  kDontIgnore,"),
        T("  kIgnore,"),
        T("};"),
    ])
    func_decl = CxxFuncDeclNode(name=function_name,
                                arg_decls=["IgnorePause ignore_pause"],
                                return_type="bool")
    decls.extend([
        T("""\
// Returns true if the callback is runnable, otherwise returns false and
// throws an exception.\
"""),
        ignore_pause_def,
        func_decl,
    ])

    func_def = CxxFuncDefNode(name=function_name,
                              arg_decls=["IgnorePause ignore_pause"],
                              return_type="bool",
                              class_name=cg_context.class_name)
    func_def.set_base_template_vars(cg_context.template_bindings())
    defs.append(func_def)
    body = func_def.body
    body.add_template_var("ignore_pause", "ignore_pause")
    bind_local_vars(body, cg_context)

    body.extend([
        T("""\
ScriptState* callback_relevant_script_state = CallbackRelevantScriptState();

const bool is_runnable =
    ignore_pause == IgnorePause::kIgnore
    ? IsCallbackFunctionRunnableIgnoringPause(
          callback_relevant_script_state, IncumbentScriptState())
    : IsCallbackFunctionRunnable(
          callback_relevant_script_state, IncumbentScriptState());
if (is_runnable)
  return true;
"""),
        T("ScriptState::Scope scope(callback_relevant_script_state);"),
        T("""\
V8ThrowException::ThrowError(
    ${isolate}, "The provided callback is no longer runnable.");
return false;\
"""),
    ])

    return decls, defs


def generate_callback_function(callback_function_identifier):
    assert isinstance(callback_function_identifier, web_idl.Identifier)

    web_idl_database = package_initializer().web_idl_database()
    callback_function = web_idl_database.find(callback_function_identifier)

    path_manager = PathManager(callback_function)
    assert path_manager.api_component == path_manager.impl_component
    api_component = path_manager.api_component
    for_testing = callback_function.code_generator_info.for_testing

    # Class names
    class_name = blink_class_name(callback_function)

    if "SupportsTaskAttribution" in callback_function.extended_attributes:
        base_class_name = "CallbackFunctionWithTaskAttributionBase"
    else:
        base_class_name = "CallbackFunctionBase"

    cg_context = CodeGenContext(callback_function=callback_function,
                                class_name=class_name,
                                base_class_name=base_class_name)

    # Filepaths
    header_path = path_manager.api_path(ext="h")
    source_path = path_manager.api_path(ext="cc")

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

    # Class definition
    class_def = CxxClassDefNode(cg_context.class_name,
                                base_class_names=[base_class_name],
                                final=True,
                                export=component_export(
                                    api_component, for_testing))
    class_def.set_base_template_vars(cg_context.template_bindings())

    # Implementation parts
    factory_decls, factory_defs = make_factory_methods(cg_context)
    ctor_decls, ctor_defs = make_constructors(cg_context)
    nameclient_decls, nameclient_defs = make_nameclient_implementation(
        cg_context)

    cgc = cg_context.make_copy(callback_function=callback_function)

    invoke_decls, invoke_defs = make_callback_invocation_function(
        cgc, name_style.func("Invoke"), is_construct_call=False)
    construct_decls, construct_defs = make_callback_invocation_function(
        cgc, name_style.func("Construct"), is_construct_call=True)

    (invoke_and_report_decls,
     invoke_and_report_defs) = make_invoke_and_report_function(
         cgc, name_style.func("InvokeAndReportException"),
         name_style.func("Invoke"))

    event_handler_decls, event_handler_defs = None, None
    if callback_function.identifier == "EventHandlerNonNull":
        event_handler_decls = SequenceNode()
        event_handler_defs = SequenceNode()
        (decls, defs) = make_is_runnable_or_throw_exception(
            cgc, name_style.func("IsRunnableOrThrowException"))
        event_handler_decls.append(decls)
        event_handler_defs.append(defs)
        event_handler_decls.append(EmptyNode())
        event_handler_defs.append(EmptyNode())
        (decls, defs) = make_callback_invocation_function(
            cgc,
            name_style.func("InvokeWithoutRunnabilityCheck"),
            skip_runnability_check=True)
        event_handler_decls.append(decls)
        event_handler_defs.append(defs)

    # Header part (copyright, include directives, and forward declarations)
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
    header_blink_ns.body.extend([
        make_forward_declarations(header_node.accumulator),
        EmptyNode(),
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
    source_blink_ns.body.extend([
        make_forward_declarations(source_node.accumulator),
        EmptyNode(),
    ])

    # Assemble the parts.
    header_node.accumulator.add_include_headers([
        component_export_header(api_component, for_testing),
        "third_party/blink/renderer/platform/bindings/callback_function_base.h",
        "third_party/blink/renderer/platform/bindings/v8_value_or_script_wrappable_adapter.h",
    ])
    source_node.accumulator.add_stdcpp_include_headers([
        "tuple",
    ])
    source_node.accumulator.add_include_headers([
        "third_party/blink/renderer/bindings/core/v8/callback_invoke_helper.h",
        "third_party/blink/renderer/bindings/core/v8/generated_code_helper.h",
        "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h",
        "base/containers/span.h",
    ])
    (
        header_forward_decls,
        header_include_headers,
        header_stdcpp_include_headers,
        source_forward_decls,
        source_include_headers,
    ) = collect_forward_decls_and_include_headers(
        [callback_function.return_type] + list(
            map(lambda argument: argument.idl_type,
                callback_function.arguments)))
    header_node.accumulator.add_class_decls(header_forward_decls)
    header_node.accumulator.add_include_headers(header_include_headers)
    header_node.accumulator.add_stdcpp_include_headers(
        header_stdcpp_include_headers)
    source_node.accumulator.add_class_decls(source_forward_decls)
    source_node.accumulator.add_include_headers(source_include_headers)

    header_blink_ns.body.append(class_def)
    header_blink_ns.body.append(EmptyNode())

    class_def.public_section.append(factory_decls)
    class_def.public_section.append(EmptyNode())
    source_blink_ns.body.append(factory_defs)
    source_blink_ns.body.append(EmptyNode())

    class_def.public_section.append(ctor_decls)
    class_def.public_section.append(EmptyNode())
    source_blink_ns.body.append(ctor_defs)
    source_blink_ns.body.append(EmptyNode())

    class_def.public_section.append(TextNode("// NameClient overrides:"))
    class_def.public_section.append(nameclient_decls)
    class_def.public_section.append(EmptyNode())
    source_blink_ns.body.append(nameclient_defs)
    source_blink_ns.body.append(EmptyNode())

    class_def.public_section.append(invoke_decls)
    class_def.public_section.append(EmptyNode())
    source_blink_ns.body.append(invoke_defs)
    source_blink_ns.body.append(EmptyNode())

    class_def.public_section.append(construct_decls)
    class_def.public_section.append(EmptyNode())
    source_blink_ns.body.append(construct_defs)
    source_blink_ns.body.append(EmptyNode())

    class_def.public_section.append(invoke_and_report_decls)
    class_def.public_section.append(EmptyNode())
    source_blink_ns.body.append(invoke_and_report_defs)
    source_blink_ns.body.append(EmptyNode())

    class_def.public_section.append(event_handler_decls)
    class_def.public_section.append(EmptyNode())
    source_blink_ns.body.append(event_handler_defs)
    source_blink_ns.body.append(EmptyNode())

    # Write down to the files.
    write_code_node_to_file(header_node, path_manager.gen_path_to(header_path))
    write_code_node_to_file(source_node, path_manager.gen_path_to(source_path))


def generate_callback_functions(task_queue):
    assert isinstance(task_queue, TaskQueue)

    web_idl_database = package_initializer().web_idl_database()

    for callback_function in web_idl_database.callback_functions:
        if callback_function.identifier in (
                "OnErrorEventHandlerNonNull",
                "OnBeforeUnloadEventHandlerNonNull"):
            # OnErrorEventHandlerNonNull and OnBeforeUnloadEventHandlerNonNull
            # are unified into EventHandlerNonNull, and they won't be used.
            continue
        task_queue.post_task(generate_callback_function,
                             callback_function.identifier)
