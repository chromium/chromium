# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import web_idl

from . import name_style
from .blink_v8_bridge import blink_class_name
from .blink_v8_bridge import blink_type_info
from .blink_v8_bridge import native_value_tag
from .code_node import EmptyNode
from .code_node import FormatNode
from .code_node import ListNode
from .code_node import SymbolNode
from .code_node import SymbolScopeNode
from .code_node import TextNode
from .code_node_cxx import CxxClassDefNode
from .code_node_cxx import CxxFuncDeclNode
from .code_node_cxx import CxxFuncDefNode
from .code_node_cxx import CxxLikelyIfNode
from .code_node_cxx import CxxNamespaceNode
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


def bind_local_vars(code_node, cg_context):
    assert isinstance(code_node, SymbolScopeNode)
    assert isinstance(cg_context, CodeGenContext)

    S = SymbolNode

    local_vars = [
        S("backing_list_instance_object_template",
          ("v8::Local<v8::ObjectTemplate> "
           "${backing_list_instance_object_template} = "
           "${backing_list_interface_function_template}"
           "->InstanceTemplate();")),
        S("backing_list_interface_function_template",
          ("v8::Local<v8::FunctionTemplate> "
           "${backing_list_interface_function_template} = "
           "${backing_list_template}.As<v8::FunctionTemplate>();")),
        S("isolate",
          "v8::Isolate* ${isolate} = ${script_state}->GetIsolate();"),
        S("per_isolate_data", ("V8PerIsolateData* ${per_isolate_data} = "
                               "V8PerIsolateData::From(${isolate});")),
        S("world",
          "const DOMWrapperWorld& ${world} = ${script_state}->World();"),
        S("wrapper_type_info",
          ("const WrapperTypeInfo* const ${wrapper_type_info} = "
           "${class_name}::GetStaticWrapperTypeInfo();")),
    ]

    # Arguments have priority over local vars.
    for symbol_node in local_vars:
        if symbol_node.name not in code_node.own_template_vars:
            code_node.register_code_symbol(symbol_node)


def make_wrapper_type_info(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    member_var_def = TextNode(
        "static const WrapperTypeInfo wrapper_type_info_body_;")

    wrapper_type_info_def = TextNode("""\
// static
const WrapperTypeInfo ${class_name}::wrapper_type_info_body_{
    gin::kEmbedderBlink,
    ${class_name}::InstallObservableArrayBackingListTemplate,
    nullptr,
    "${class_name}",
    nullptr,  // parent_class
    kDOMWrappersTag,
    kDOMWrappersTag,
    WrapperTypeInfo::kWrapperTypeNoPrototype,
    WrapperTypeInfo::kObjectClassId,
    WrapperTypeInfo::kNotInheritFromActiveScriptWrappable,
    WrapperTypeInfo::kIdlObservableArray,
};

// static
const WrapperTypeInfo& ${class_name}::wrapper_type_info_ =
    ${class_name}::wrapper_type_info_body_;
""")
    wrapper_type_info_def.set_base_template_vars(
        cg_context.template_bindings())

    return member_var_def, wrapper_type_info_def


def make_handler_class(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    base_class_name = ("bindings::ObservableArrayExoticObjectHandler<"
                       "{backing_list_wrappable}, {element_idl_type}>".format(
                           backing_list_wrappable=cg_context.class_name,
                           element_idl_type=native_value_tag(
                               cg_context.observable_array.element_type)))

    decls = ListNode([
        TextNode("class Handler;"),
        TextNode("friend class {};".format(base_class_name)),
    ])

    defs = ListNode([
        TextNode("template class {};".format(base_class_name)),
        EmptyNode(),
        CxxClassDefNode("{}::Handler".format(cg_context.class_name),
                        base_class_names=[base_class_name],
                        final=True),
    ])

    return decls, defs


def make_constructors(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    func_decl = CxxFuncDeclNode(
        name=cg_context.class_name,
        arg_decls=[
            "GarbageCollectedMixin* platform_object",
            "SetAlgorithmCallback set_algorithm_callback",
            "DeleteAlgorithmCallback delete_algorithm_callback",
        ],
        return_type="",
        explicit=True)
    func_def = CxxFuncDefNode(
        name=cg_context.class_name,
        arg_decls=[
            "GarbageCollectedMixin* platform_object",
            "SetAlgorithmCallback set_algorithm_callback",
            "DeleteAlgorithmCallback delete_algorithm_callback",
        ],
        return_type="",
        class_name=cg_context.class_name,
        member_initializer_list=[
            "BaseClass(platform_object)",
            "set_algorithm_callback_(set_algorithm_callback)",
            "delete_algorithm_callback_(delete_algorithm_callback)",
        ])
    func_def.set_base_template_vars(cg_context.template_bindings())

    return func_decl, func_def


def make_attribute_set_function(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    func_decl = CxxFuncDeclNode(name="PerformAttributeSet",
                                arg_decls=[
                                    "ScriptState* script_state",
                                    "v8::Local<v8::Value> v8_value",
                                    "ExceptionState& exception_state",
                                ],
                                return_type="void")

    func_def = CxxFuncDefNode(name="PerformAttributeSet",
                              arg_decls=[
                                  "ScriptState* script_state",
                                  "v8::Local<v8::Value> v8_value",
                                  "ExceptionState& exception_state",
                              ],
                              return_type="void",
                              class_name=cg_context.class_name)
    func_def.set_base_template_vars(cg_context.template_bindings())

    body = func_def.body
    body.add_template_vars({
        "script_state": "script_state",
        "v8_value": "v8_value",
        "exception_state": "exception_state",
    })
    bind_local_vars(body, cg_context)

    body.append(
        TextNode("Handler::PerformAttributeSet("
                 "${script_state}, *this, ${v8_value}, ${exception_state});"))

    return func_decl, func_def


def make_handler_template_function(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    T = TextNode

    func_decl = CxxFuncDeclNode(name="GetProxyHandlerFunctionTemplate",
                                arg_decls=[
                                    "ScriptState* script_state",
                                ],
                                return_type="v8::Local<v8::FunctionTemplate>",
                                override=True)

    func_def = CxxFuncDefNode(name="GetProxyHandlerFunctionTemplate",
                              arg_decls=[
                                  "ScriptState* script_state",
                              ],
                              return_type="v8::Local<v8::FunctionTemplate>",
                              class_name=cg_context.class_name)
    func_def.set_base_template_vars(cg_context.template_bindings())

    body = func_def.body
    body.add_template_vars({
        "script_state": "script_state",
    })
    bind_local_vars(body, cg_context)

    body.extend([
        T("// Make `template_key` unique for `FindV8Template`."),
        T("static const char kTemplateKeyTag = 0;"),
        T("const void* const template_key = &kTemplateKeyTag;"),
        EmptyNode(),
        T("v8::Local<v8::Template> v8_template = "
          "${per_isolate_data}->FindV8Template(${world}, template_key);"),
        CxxLikelyIfNode(
            cond="!v8_template.IsEmpty()",
            attribute=None,
            body=T("return v8_template.As<v8::FunctionTemplate>();")),
        EmptyNode(),
        T("v8::Local<v8::FunctionTemplate> constructor_template = "
          "v8::FunctionTemplate::New(${isolate});"),
        T("v8::Local<v8::ObjectTemplate> instance_object_template = "
          "constructor_template->InstanceTemplate();"),
    ])

    traps = [
        "defineProperty",
        "deleteProperty",
        "get",
        "getOwnPropertyDescriptor",
        "has",
        "ownKeys",
        "preventExtensions",
        "set",
    ]
    for trap in traps:
        body.append(
            FormatNode(
                "instance_object_template->Set("
                "V8AtomicString(${isolate}, \"{trap_name}\"), "
                "v8::FunctionTemplate::New("
                "${isolate}, Handler::{trap_func}));",
                trap_name=trap,
                trap_func=name_style.func("trap", trap)))

    body.extend([
        EmptyNode(),
        T("${per_isolate_data}->AddV8Template("
          "${world}, template_key, constructor_template);"),
        T("return constructor_template;"),
    ])

    return func_decl, func_def


def make_trace_function(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    func_decl = CxxFuncDeclNode(name="Trace",
                                arg_decls=["Visitor* visitor"],
                                return_type="void",
                                const=True,
                                override=True)

    func_def = CxxFuncDefNode(name="Trace",
                              arg_decls=["Visitor* visitor"],
                              return_type="void",
                              class_name=cg_context.class_name,
                              const=True)
    func_def.set_base_template_vars(cg_context.template_bindings())
    body = func_def.body

    body.append(TextNode("BaseClass::Trace(visitor);"))

    return func_decl, func_def


def make_install_backing_list_template_function(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    func_decl = CxxFuncDeclNode(
        name="InstallObservableArrayBackingListTemplate",
        arg_decls=[
            "v8::Isolate* isolate",
            "const DOMWrapperWorld& world",
            "v8::Local<v8::Template> backing_list_template",
        ],
        return_type="void",
        static=True)

    func_def = CxxFuncDefNode(
        name="InstallObservableArrayBackingListTemplate",
        arg_decls=[
            "v8::Isolate* isolate",
            "const DOMWrapperWorld& world",
            "v8::Local<v8::Template> backing_list_template",
        ],
        return_type="void",
        class_name=cg_context.class_name)
    func_def.set_base_template_vars(cg_context.template_bindings())

    body = func_def.body
    body.add_template_vars({
        "isolate": "isolate",
        "world": "world",
        "backing_list_template": "backing_list_template",
    })
    bind_local_vars(body, cg_context)

    body.append(
        TextNode("bindings::SetupIDLObservableArrayBackingListTemplate("
                 "${isolate}, ${wrapper_type_info}, "
                 "${backing_list_instance_object_template}, "
                 "${backing_list_interface_function_template});"))

    return func_decl, func_def


def make_name_function(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    func_def = CxxFuncDefNode(name="ObservableArrayNameInIDL",
                              arg_decls=[],
                              return_type="const char*",
                              static=True,
                              constexpr=True)
    func_def.set_base_template_vars(cg_context.template_bindings())
    body = func_def.body

    body.append(
        TextNode("return \"{}\";".format(
            cg_context.observable_array.idl_type.syntactic_form)))

    return func_def, None


def generate_observable_array(observable_array_identifier):
    assert isinstance(observable_array_identifier, web_idl.Identifier)

    web_idl_database = package_initializer().web_idl_database()
    observable_array = web_idl_database.find(observable_array_identifier)

    path_manager = PathManager(observable_array)
    assert path_manager.api_component == path_manager.impl_component
    api_component = path_manager.api_component
    for_testing = observable_array.code_generator_info.for_testing

    # Class names
    class_name = blink_class_name(observable_array)
    base_class_name = "bindings::ObservableArrayImplHelper<{}>".format(
        blink_type_info(observable_array.element_type).member_t)

    cg_context = CodeGenContext(observable_array=observable_array,
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
                                base_class_names=[cg_context.base_class_name],
                                final=True,
                                export=component_export(
                                    api_component, for_testing))
    class_def.set_base_template_vars(cg_context.template_bindings())

    # Implementation parts
    wrapper_type_info_var_def, wrapper_type_info_init = make_wrapper_type_info(
        cg_context)
    handler_class_decls, handler_class_defs = make_handler_class(cg_context)
    ctor_decls, ctor_defs = make_constructors(cg_context)
    attr_set_decls, attr_set_defs = make_attribute_set_function(cg_context)
    handler_func_decls, handler_func_defs = make_handler_template_function(
        cg_context)
    trace_func_decls, trace_func_defs = make_trace_function(cg_context)
    install_backing_list_decls, install_backing_list_defs = (
        make_install_backing_list_template_function(cg_context))
    name_func_decls, name_func_defs = make_name_function(cg_context)

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
    header_node.accumulator.add_class_decls([
        "ExceptionState",
    ])
    header_node.accumulator.add_include_headers([
        component_export_header(api_component, for_testing),
        "third_party/blink/renderer/bindings/core/v8/idl_types.h",
        "third_party/blink/renderer/platform/bindings/observable_array.h",
    ])
    source_node.accumulator.add_include_headers([
        "third_party/blink/renderer/bindings/core/v8/generated_code_helper.h",
        "third_party/blink/renderer/bindings/core/v8/observable_array_exotic_object_handler.h",
        "third_party/blink/renderer/platform/bindings/v8_binding.h",
    ])
    (
        header_forward_decls,
        header_include_headers,
        header_stdcpp_include_headers,
        source_forward_decls,
        source_include_headers,
    ) = collect_forward_decls_and_include_headers(
        [observable_array.element_type])
    header_node.accumulator.add_class_decls(header_forward_decls)
    header_node.accumulator.add_include_headers(header_include_headers)
    header_node.accumulator.add_stdcpp_include_headers(
        header_stdcpp_include_headers)
    source_node.accumulator.add_class_decls(source_forward_decls)
    source_node.accumulator.add_include_headers(source_include_headers)

    handler_fwd_decls = CxxNamespaceNode(name_style.namespace("bindings"))
    handler_fwd_decls.body.append(
        TextNode(
            "template <typename BackingListWrappable, typename ElementIdlType> "
            "class ObservableArrayExoticObjectHandler;"))
    header_blink_ns.body.append(handler_fwd_decls)
    header_blink_ns.body.append(EmptyNode())

    header_blink_ns.body.append(class_def)
    header_blink_ns.body.append(EmptyNode())

    class_def.top_section.append(TextNode("DEFINE_WRAPPERTYPEINFO();"))
    class_def.top_section.append(
        TextNode("using BaseClass = {};".format(base_class_name)))

    class_def.public_section.append(
        TextNode("using SetAlgorithmCallback = "
                 "void (*)("
                 "GarbageCollectedMixin* platform_object, "
                 "ScriptState* script_state, "
                 "{}& observable_array, "
                 "size_type index, "
                 "value_type& value, "
                 "ExceptionState& exception_state);".format(
                     cg_context.class_name)))
    class_def.public_section.append(
        TextNode("using DeleteAlgorithmCallback = "
                 "void (*)("
                 "GarbageCollectedMixin* platform_object, "
                 "ScriptState* script_state, "
                 "{}& observable_array, "
                 "size_type index, "
                 "ExceptionState& exception_state);".format(
                     cg_context.class_name)))
    class_def.public_section.append(EmptyNode())

    class_def.private_section.append(wrapper_type_info_var_def)
    class_def.private_section.append(EmptyNode())
    source_blink_ns.body.append(wrapper_type_info_init)
    source_blink_ns.body.append(EmptyNode())

    class_def.private_section.append(handler_class_decls)
    class_def.private_section.append(EmptyNode())
    source_blink_ns.body.append(handler_class_defs)
    source_blink_ns.body.append(EmptyNode())

    class_def.public_section.append(ctor_decls)
    class_def.public_section.append(EmptyNode())
    source_blink_ns.body.append(ctor_defs)
    source_blink_ns.body.append(EmptyNode())

    class_def.public_section.append(attr_set_decls)
    class_def.public_section.append(EmptyNode())
    source_blink_ns.body.append(attr_set_defs)
    source_blink_ns.body.append(EmptyNode())

    class_def.public_section.append(handler_func_decls)
    class_def.public_section.append(EmptyNode())
    source_blink_ns.body.append(handler_func_defs)
    source_blink_ns.body.append(EmptyNode())

    class_def.public_section.append(trace_func_decls)
    class_def.public_section.append(EmptyNode())
    source_blink_ns.body.append(trace_func_defs)
    source_blink_ns.body.append(EmptyNode())

    class_def.public_section.append(install_backing_list_decls)
    class_def.public_section.append(EmptyNode())
    source_blink_ns.body.append(install_backing_list_defs)
    source_blink_ns.body.append(EmptyNode())

    class_def.private_section.append(name_func_decls)
    class_def.private_section.append(EmptyNode())
    source_blink_ns.body.append(name_func_defs)
    source_blink_ns.body.append(EmptyNode())

    class_def.private_section.append(TextNode("// [[SetAlgorithm]]"))
    class_def.private_section.append(
        TextNode("SetAlgorithmCallback set_algorithm_callback_ = nullptr;"))
    class_def.private_section.append(TextNode("// [[DeleteAlgorithm]]"))
    class_def.private_section.append(
        TextNode(
            "DeleteAlgorithmCallback delete_algorithm_callback_ = nullptr;"))

    # Write down to the files.
    write_code_node_to_file(header_node, path_manager.gen_path_to(header_path))
    write_code_node_to_file(source_node, path_manager.gen_path_to(source_path))


def generate_observable_arrays(task_queue):
    assert isinstance(task_queue, TaskQueue)

    web_idl_database = package_initializer().web_idl_database()

    for observable_array in web_idl_database.observable_arrays:
        task_queue.post_task(generate_observable_array,
                             observable_array.identifier)
