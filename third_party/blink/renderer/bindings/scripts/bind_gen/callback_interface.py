# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import web_idl

from . import name_style
from .blink_v8_bridge import blink_class_name
from .code_node import EmptyNode
from .code_node import ListNode
from .code_node import SequenceNode
from .code_node import TextNode
from .code_node_cxx import CxxClassDefNode
from .code_node_cxx import CxxFuncDeclNode
from .code_node_cxx import CxxFuncDefNode
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

# IDL callback interface and IDL callback function share a lot by their nature,
# so this module uses some implementations from IDL callback function.
from .callback_function import make_callback_invocation_function
from .callback_function import make_invoke_and_report_function
from .callback_function import make_is_runnable_or_throw_exception
from .callback_function import make_nameclient_implementation

# IDL legacy callback interface is mostly the same as IDL interface, so this
# module uses some implementations from IDL interface
from .interface import FN_INSTALL_INTERFACE_TEMPLATE
from .interface import FN_INSTALL_UNCONDITIONAL_PROPS
from .interface import PropInstallMode
from .interface import constant_name
from .interface import make_constant_constant_def
from .interface import make_install_interface_template
from .interface import make_install_properties
from .interface import make_property_entries_and_callback_defs
from .interface import make_wrapper_type_info


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
        CxxFuncDefNode(
            name=cg_context.class_name,
            arg_decls=["v8::Local<v8::Object> callback_object"],
            return_type="",
            explicit=True,
            member_initializer_list=[
                "${base_class_name}(callback_object, kSingleOperation)",
            ]),
        CxxFuncDeclNode(name="~${class_name}",
                        arg_decls=[],
                        return_type="",
                        override=True,
                        default=True),
    ])

    return decls, None


def generate_callback_interface(callback_interface_identifier):
    assert isinstance(callback_interface_identifier, web_idl.Identifier)

    web_idl_database = package_initializer().web_idl_database()
    callback_interface = web_idl_database.find(callback_interface_identifier)

    path_manager = PathManager(callback_interface)
    assert path_manager.api_component == path_manager.impl_component
    api_component = path_manager.api_component
    for_testing = callback_interface.code_generator_info.for_testing

    # Class names
    class_name = blink_class_name(callback_interface)

    cg_context = CodeGenContext(callback_interface=callback_interface,
                                class_name=class_name,
                                base_class_name="CallbackInterfaceBase")

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
                                base_class_names=["CallbackInterfaceBase"],
                                final=True,
                                export=component_export(
                                    api_component, for_testing))
    class_def.set_base_template_vars(cg_context.template_bindings())

    # Constants
    constants_def = None
    if callback_interface.constants:
        constants_def = CxxClassDefNode(name="Constant", final=True)
        constants_def.top_section.append(TextNode("STATIC_ONLY(Constant);"))
        for constant in callback_interface.constants:
            cgc = cg_context.make_copy(constant=constant)
            constants_def.public_section.append(
                make_constant_constant_def(cgc, constant_name(cgc)))
        backward_compatible_constants_def = ListNode()
        backward_compatible_constants_def.append(
            TextNode("// Migration adapters"))
        for constant in callback_interface.constants:
            cgc = cg_context.make_copy(constant=constant)
            backward_compatible_constants_def.append(
                make_constant_constant_def(
                    cgc, name_style.macro(constant.identifier)))

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
    assert not attribute_entries
    assert not constructor_entries
    assert not exposed_construct_entries
    assert not operation_entries

    # Installer functions
    is_unconditional = lambda entry: entry.exposure_conditional.is_always_true
    assert all(is_unconditional(entry) for entry in constant_entries)
    (install_unconditional_props_decl, install_unconditional_props_def,
     install_unconditional_props_trampoline) = make_install_properties(
         cg_context,
         FN_INSTALL_UNCONDITIONAL_PROPS,
         class_name=class_name,
         prop_install_mode=PropInstallMode.UNCONDITIONAL,
         trampoline_var_name=None,
         attribute_entries=[],
         constant_entries=list(filter(is_unconditional, constant_entries)),
         exposed_construct_entries=[],
         operation_entries=[])
    (install_interface_template_decl, install_interface_template_def,
     install_interface_template_trampoline) = make_install_interface_template(
         cg_context,
         FN_INSTALL_INTERFACE_TEMPLATE,
         class_name=class_name,
         trampoline_var_name=None,
         constructor_entries=[],
         supplemental_install_node=SequenceNode(),
         install_unconditional_func_name=(install_unconditional_props_def
                                          and FN_INSTALL_UNCONDITIONAL_PROPS),
         install_context_independent_func_name=None)
    installer_function_decls = ListNode([
        install_interface_template_decl,
        install_unconditional_props_decl,
    ])
    installer_function_defs = ListNode([
        install_interface_template_def,
        EmptyNode(),
        install_unconditional_props_def,
    ])
    installer_function_defs.accumulate(
        CodeGenAccumulator.require_include_headers([
            "third_party/blink/renderer/platform/bindings/idl_member_installer.h",
        ]))

    # WrapperTypeInfo
    (get_wrapper_type_info_def, wrapper_type_info_var_def,
     wrapper_type_info_init) = make_wrapper_type_info(
         cg_context, "GetWrapperTypeInfo", has_context_dependent_props=False)

    # Implementation parts
    factory_decls, factory_defs = make_factory_methods(cg_context)
    ctor_decls, ctor_defs = make_constructors(cg_context)
    nameclient_decls, nameclient_defs = make_nameclient_implementation(
        cg_context)

    assert len(callback_interface.operation_groups) == 1
    operation_group = callback_interface.operation_groups[0]
    assert len(operation_group) == 1
    operation = operation_group[0]
    cgc = cg_context.make_copy(operation_group=operation_group,
                               operation=operation)

    operation_decls, operation_defs = make_callback_invocation_function(
        cgc, name_style.api_func(operation.identifier))

    (invoke_and_report_decls,
     invoke_and_report_defs) = make_invoke_and_report_function(
         cgc, name_style.func("InvokeAndReportException"),
         name_style.api_func(operation.identifier))

    event_listener_decls, event_listener_defs = None, None
    if callback_interface.identifier == "EventListener":
        event_listener_decls = SequenceNode()
        event_listener_defs = SequenceNode()
        (decls, defs) = make_is_runnable_or_throw_exception(
            cgc, name_style.func("IsRunnableOrThrowException"))
        event_listener_decls.append(decls)
        event_listener_defs.append(defs)
        event_listener_decls.append(EmptyNode())
        event_listener_defs.append(EmptyNode())
        (decls, defs) = make_callback_invocation_function(
            cgc,
            name_style.func("InvokeWithoutRunnabilityCheck"),
            skip_runnability_check=True)
        event_listener_decls.append(decls)
        event_listener_defs.append(defs)

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
        "third_party/blink/renderer/platform/bindings/callback_interface_base.h",
        "third_party/blink/renderer/platform/bindings/v8_value_or_script_wrappable_adapter.h",
    ])
    source_node.accumulator.add_stdcpp_include_headers([
        "tuple",
    ])
    source_node.accumulator.add_include_headers([
        "third_party/blink/renderer/bindings/core/v8/callback_invoke_helper.h",
        "third_party/blink/renderer/bindings/core/v8/generated_code_helper.h",
        "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h",
    ])
    (
        header_forward_decls,
        header_include_headers,
        header_stdcpp_include_headers,
        source_forward_decls,
        source_include_headers,
    ) = collect_forward_decls_and_include_headers(
        [callback_interface.operation_groups[0][0].return_type] + list(
            map(lambda argument: argument.idl_type,
                callback_interface.operation_groups[0][0].arguments)))
    header_node.accumulator.add_class_decls(header_forward_decls)
    header_node.accumulator.add_include_headers(header_include_headers)
    header_node.accumulator.add_stdcpp_include_headers(
        header_stdcpp_include_headers)
    source_node.accumulator.add_class_decls(source_forward_decls)
    source_node.accumulator.add_include_headers(source_include_headers)

    header_blink_ns.body.append(class_def)
    header_blink_ns.body.append(EmptyNode())

    if constants_def:
        # Legacy callback interface
        class_def.public_section.extend([
            TextNode("// Constants"),
            constants_def,
            EmptyNode(),
            backward_compatible_constants_def,
            EmptyNode(),
        ])

        class_def.public_section.append(get_wrapper_type_info_def)
        class_def.public_section.append(EmptyNode())
        class_def.private_section.append(wrapper_type_info_var_def)
        class_def.private_section.append(EmptyNode())
        source_blink_ns.body.extend([
            wrapper_type_info_init,
            EmptyNode(),
        ])

        class_def.public_section.append(installer_function_decls)
        class_def.public_section.append(EmptyNode())
        if callback_defs:
            source_blink_ns.body.extend([
                CxxNamespaceNode(name="", body=callback_defs),
                EmptyNode(),
            ])
        source_blink_ns.body.extend([
            installer_function_defs,
            EmptyNode(),
        ])

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

    class_def.public_section.append(operation_decls)
    class_def.public_section.append(EmptyNode())
    source_blink_ns.body.append(operation_defs)
    source_blink_ns.body.append(EmptyNode())

    class_def.public_section.append(invoke_and_report_decls)
    class_def.public_section.append(EmptyNode())
    source_blink_ns.body.append(invoke_and_report_defs)
    source_blink_ns.body.append(EmptyNode())

    class_def.public_section.append(event_listener_decls)
    class_def.public_section.append(EmptyNode())
    source_blink_ns.body.append(event_listener_defs)
    source_blink_ns.body.append(EmptyNode())

    # Write down to the files.
    write_code_node_to_file(header_node, path_manager.gen_path_to(header_path))
    write_code_node_to_file(source_node, path_manager.gen_path_to(source_path))


def generate_callback_interfaces(task_queue):
    assert isinstance(task_queue, TaskQueue)

    web_idl_database = package_initializer().web_idl_database()

    for callback_interface in web_idl_database.callback_interfaces:
        task_queue.post_task(generate_callback_interface,
                             callback_interface.identifier)
