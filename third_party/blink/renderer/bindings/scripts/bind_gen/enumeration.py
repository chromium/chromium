# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import web_idl

from . import name_style
from .blink_v8_bridge import blink_class_name
from .code_node import EmptyNode
from .code_node import ListNode
from .code_node import TextNode
from .code_node_cxx import CxxClassDefNode
from .code_node_cxx import CxxFuncDeclNode
from .code_node_cxx import CxxFuncDefNode
from .code_node_cxx import CxxNamespaceNode
from .codegen_accumulator import IncludeDefinition
from .codegen_accumulator import CodeGenAccumulator
from .codegen_context import CodeGenContext
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


def make_factory_methods(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    T = TextNode

    decls = ListNode()
    defs = ListNode()

    func_decl = CxxFuncDeclNode(name="Create",
                                arg_decls=[
                                    "v8::Isolate* isolate",
                                    "v8::Local<v8::Value> value",
                                    "ExceptionState& exception_state",
                                ],
                                return_type="${class_name}",
                                static=True)
    func_def = CxxFuncDefNode(name="Create",
                              arg_decls=[
                                  "v8::Isolate* isolate",
                                  "v8::Local<v8::Value> value",
                                  "ExceptionState& exception_state",
                              ],
                              return_type="${class_name}",
                              class_name=cg_context.class_name)
    func_def.set_base_template_vars(cg_context.template_bindings())
    decls.append(func_decl)
    defs.append(func_def)

    func_def.body.extend([
        T("const auto& result = bindings::FindIndexInEnumStringTable("
          "isolate, value, string_table_, \"${enumeration.identifier}\", "
          "exception_state);"),
        T("return result.has_value() ? "
          "${class_name}(static_cast<Enum>(result.value())) : "
          "${class_name}();"),
    ])

    func_decl = CxxFuncDeclNode(name="Create",
                                arg_decls=["const String& value"],
                                return_type="std::optional<${class_name}>",
                                static=True)
    func_def = CxxFuncDefNode(name="Create",
                              arg_decls=["const String& value"],
                              return_type="std::optional<${class_name}>",
                              class_name=cg_context.class_name)
    func_def.set_base_template_vars(cg_context.template_bindings())
    decls.append(func_decl)
    defs.append(EmptyNode())
    defs.append(func_def)

    func_def.body.extend([
        T("const auto& result = bindings::FindIndexInEnumStringTable"
          "(value, string_table_);"),
        T("if (!result)\n"
          "  return std::nullopt;"),
        T("return ${class_name}(static_cast<Enum>(result.value()));"),
    ])

    return decls, defs


def make_default_constructor(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    func_decl = CxxFuncDeclNode(name=cg_context.class_name,
                                arg_decls=[],
                                return_type="",
                                constexpr=True,
                                default=True)

    return func_decl, None


def make_constructors(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    T = TextNode

    class_name = cg_context.class_name

    decls = ListNode([
        CxxFuncDefNode(
            name=class_name,
            arg_decls=["Enum value"],
            return_type="",
            explicit=True,
            constexpr=True,
            member_initializer_list=[
                "${base_class_name}("
                "static_cast<enum_int_t>(value), "
                "string_table_[static_cast<enum_int_t>(value)])"
            ]),
        CxxFuncDeclNode(
            name=class_name,
            arg_decls=["const ${class_name}&"],
            return_type="",
            constexpr=True,
            default=True),
        CxxFuncDeclNode(
            name=class_name,
            arg_decls=["${class_name}&&"],
            return_type="",
            constexpr=True,
            default=True),
        CxxFuncDeclNode(
            name="~${class_name}", arg_decls=[], return_type="", default=True),
    ])

    defs = ListNode([
        T("static_assert("
          "std::is_trivially_copyable<${class_name}>::value, \"\");"),
    ])
    defs.set_base_template_vars(cg_context.template_bindings())

    return decls, defs


def make_assignment_operators(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    decls = ListNode([
        CxxFuncDeclNode(
            name="operator=",
            arg_decls=["const ${class_name}&"],
            return_type="${class_name}&",
            default=True),
        CxxFuncDeclNode(
            name="operator=",
            arg_decls=["${class_name}&&"],
            return_type="${class_name}&",
            default=True),
    ])
    defs = ListNode()

    # Migration adapter
    func_decl = CxxFuncDeclNode(
        name="operator=",
        arg_decls=["const String&"],
        return_type="${class_name}&")
    func_def = CxxFuncDefNode(
        name="operator=",
        arg_decls=["const String& str_value"],
        return_type="${class_name}&",
        class_name=cg_context.class_name)
    decls.append(func_decl)
    defs.append(func_def)

    func_def.set_base_template_vars(cg_context.template_bindings())
    func_def.body.append(
        TextNode("""\
const auto& index =
    bindings::FindIndexInEnumStringTable(str_value, string_table_);
CHECK(index.has_value());
return operator=(${class_name}(static_cast<Enum>(index.value())));
"""))

    return decls, defs


def make_equality_operators(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    func1_def = CxxFuncDefNode(
        name="operator==",
        arg_decls=["const ${class_name}& lhs", "${class_name}::Enum rhs"],
        return_type="bool",
        inline=True)
    func1_def.set_base_template_vars(cg_context.template_bindings())
    func1_def.body.append(TextNode("return lhs.AsEnum() == rhs;"))

    func2_def = CxxFuncDefNode(
        name="operator==",
        arg_decls=["${class_name}::Enum lhs", "const ${class_name}& rhs"],
        return_type="bool",
        inline=True)
    func2_def.set_base_template_vars(cg_context.template_bindings())
    func2_def.body.append(TextNode("return lhs == rhs.AsEnum();"))

    decls = ListNode([func1_def, EmptyNode(), func2_def])

    return decls, None


def make_as_enum_function(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    func_def = CxxFuncDefNode(
        name="AsEnum", arg_decls=[], return_type="Enum", const=True)
    func_def.body.append(TextNode("return static_cast<Enum>(GetEnumValue());"))

    return func_def, None


def make_nested_enum_class_def(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    enum_values = [
        TextNode(name_style.constant(value))
        for value in cg_context.enumeration.values
    ]

    return ListNode([
        TextNode("enum class Enum : enum_int_t {"),
        ListNode(enum_values, separator=", "),
        TextNode("};"),
        TextNode("static constexpr size_t kEnumSize = {};".format(
            len(enum_values))),
    ])


def make_enum_string_table(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    decls = TextNode("static const char* const string_table_[];")

    str_values = [
        TextNode("\"{}\"".format(value))
        for value in cg_context.enumeration.values
    ]

    # Define the string table in *.cc so that there never exists a copy of
    # the table (i.e. the strings in the table are interned strings in the
    # scope of this IDL enumeration).  This trick makes it possible to compare
    # the strings by their address.
    defs = ListNode([
        TextNode("constexpr const char* const "
                 "${class_name}::string_table_[] = {"),
        ListNode(str_values, separator=", "),
        TextNode("};"),
    ])
    defs.set_base_template_vars(cg_context.template_bindings())

    return decls, defs


def generate_enumeration(enumeration_identifier):
    assert isinstance(enumeration_identifier, web_idl.Identifier)

    web_idl_database = package_initializer().web_idl_database()
    enumeration = web_idl_database.find(enumeration_identifier)

    path_manager = PathManager(enumeration)
    assert path_manager.api_component == path_manager.impl_component
    api_component = path_manager.api_component
    for_testing = enumeration.code_generator_info.for_testing

    # Class names
    class_name = blink_class_name(enumeration)

    cg_context = CodeGenContext(
        enumeration=enumeration,
        class_name=class_name,
        base_class_name="bindings::EnumerationBase")

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
                                base_class_names=["bindings::EnumerationBase"],
                                final=True,
                                export=component_export(
                                    api_component, for_testing))
    class_def.set_base_template_vars(cg_context.template_bindings())

    # Implementation parts
    factory_decls, factory_defs = make_factory_methods(cg_context)
    default_ctor_decls, default_ctor_defs = make_default_constructor(
        cg_context)
    ctor_decls, ctor_defs = make_constructors(cg_context)
    assign_decls, assign_defs = make_assignment_operators(cg_context)
    equal_decls, equal_defs = make_equality_operators(cg_context)
    nested_enum_class_def = make_nested_enum_class_def(cg_context)
    table_decls, table_defs = make_enum_string_table(cg_context)
    as_enum_decl, as_enum_def = make_as_enum_function(cg_context)

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
    header_node.accumulator.add_stdcpp_include_headers([
        "optional",
    ])
    header_node.accumulator.add_include_headers([
        component_export_header(api_component, for_testing),
        IncludeDefinition(
            "third_party/blink/renderer/platform/bindings/enumeration_base.h",
            "IWYU pragma: export")
    ])
    source_node.accumulator.add_include_headers([
        "third_party/blink/renderer/bindings/core/v8/generated_code_helper.h",
    ])

    header_blink_ns.body.append(class_def)
    header_blink_ns.body.append(EmptyNode())

    class_def.public_section.append(nested_enum_class_def)
    class_def.public_section.append(EmptyNode())

    class_def.private_section.append(table_decls)
    class_def.private_section.append(EmptyNode())
    source_blink_ns.body.append(table_defs)
    source_blink_ns.body.append(EmptyNode())

    class_def.public_section.append(factory_decls)
    class_def.public_section.append(EmptyNode())
    source_blink_ns.body.append(factory_defs)
    source_blink_ns.body.append(EmptyNode())

    class_def.private_section.append(default_ctor_decls)
    class_def.private_section.append(EmptyNode())
    source_blink_ns.body.append(default_ctor_defs)
    source_blink_ns.body.append(EmptyNode())

    class_def.public_section.append(ctor_decls)
    class_def.public_section.append(EmptyNode())
    source_blink_ns.body.append(ctor_defs)
    source_blink_ns.body.append(EmptyNode())

    class_def.public_section.append(assign_decls)
    class_def.public_section.append(EmptyNode())
    source_blink_ns.body.append(assign_defs)
    source_blink_ns.body.append(EmptyNode())

    class_def.public_section.append(as_enum_decl)
    class_def.public_section.append(EmptyNode())
    source_blink_ns.body.append(as_enum_def)
    source_blink_ns.body.append(EmptyNode())

    header_blink_ns.body.append(equal_decls)
    header_blink_ns.body.append(EmptyNode())
    source_blink_ns.body.append(equal_defs)
    source_blink_ns.body.append(EmptyNode())

    # Write down to the files.
    write_code_node_to_file(header_node, path_manager.gen_path_to(header_path))
    write_code_node_to_file(source_node, path_manager.gen_path_to(source_path))


def generate_enumerations(task_queue):
    assert isinstance(task_queue, TaskQueue)

    web_idl_database = package_initializer().web_idl_database()

    for enumeration in web_idl_database.enumerations:
        task_queue.post_task(generate_enumeration, enumeration.identifier)
