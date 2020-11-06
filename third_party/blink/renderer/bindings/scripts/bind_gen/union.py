# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import web_idl

from . import name_style
from .blink_v8_bridge import blink_class_name
from .blink_v8_bridge import blink_type_info
from .code_node import EmptyNode
from .code_node import ListNode
from .code_node import TextNode
from .code_node_cxx import CxxClassDefNode
from .code_node_cxx import CxxFuncDeclNode
from .code_node_cxx import CxxFuncDefNode
from .code_node_cxx import CxxNamespaceNode
from .codegen_accumulator import CodeGenAccumulator
from .codegen_context import CodeGenContext
from .codegen_format import format_template as _format
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


class _UnionMember(object):
    def __init__(self, base_name):
        assert isinstance(base_name, str)

        self._base_name = base_name
        self._is_null = False
        # Do not apply |name_style| in order to respect the original name
        # (Web spec'ed name) as much as possible.
        self._content_type = "k{}".format(self._base_name)
        self._api_pred = "Is{}".format(self._base_name)
        self._api_get = "GetAs{}".format(self._base_name)
        self._api_set = "Set"
        self._member_var = name_style.member_var("member", self._base_name)
        self._member_type = None
        self._typedef_aliases = ()

    @property
    def is_null(self):
        return self._is_null

    def content_type(self, with_enum_name=True):
        if with_enum_name:
            return "ContentType::{}".format(self._content_type)
        else:
            return self._content_type

    @property
    def api_pred(self):
        return self._api_pred

    @property
    def api_get(self):
        return self._api_get

    @property
    def api_set(self):
        return self._api_set

    @property
    def member_var(self):
        return self._member_var

    @property
    def member_type(self):
        return self._member_type

    @property
    def typedef_aliases(self):
        return self._typedef_aliases


class _UnionMemberImpl(_UnionMember):
    def __init__(self, union, idl_type):
        assert isinstance(union, web_idl.NewUnion)
        assert idl_type is None or isinstance(idl_type, web_idl.IdlType)

        if idl_type is None:
            base_name = "Null"
        else:
            base_name = idl_type.type_name_with_extended_attribute_key_values

        _UnionMember.__init__(self, base_name=base_name)
        self._is_null = idl_type is None
        if not self._is_null:
            self._member_type = blink_type_info(idl_type)
        self._typedef_aliases = tuple([
            _UnionMemberAlias(impl=self, typedef=typedef)
            for typedef in union.typedef_members
            if typedef.idl_type == idl_type
        ])


class _UnionMemberAlias(_UnionMember):
    def __init__(self, impl, typedef):
        assert isinstance(impl, _UnionMemberImpl)
        assert isinstance(typedef, web_idl.Typedef)

        _UnionMember.__init__(self, base_name=typedef.identifier)
        self._member_var = impl.member_var


def make_content_type_enum_class_def(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    entries = []
    for member in cg_context.union_members:
        entries.append(member.content_type(with_enum_name=False))
        for alias in member.typedef_aliases:
            entries.append("{} = {}".format(
                alias.content_type(with_enum_name=False),
                member.content_type(with_enum_name=False)))

    return ListNode([
        TextNode("enum class ContentType {"),
        ListNode(map(TextNode, entries), separator=", "),
        TextNode("};"),
    ])


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

    for member in cg_context.union_members:
        if member.is_null:
            continue
        body.append(
            TextNode("TraceIfNeeded<{}>::Trace(visitor, {});".format(
                member.member_type.member_t, member.member_var)))
    body.append(TextNode("${base_class_name}::Trace(visitor);"))

    return func_decl, func_def


def make_member_vars_def(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    entries = [
        "{} {};".format(member.member_type.member_t, member.member_var)
        for member in cg_context.union_members if not member.is_null
    ]
    return ListNode(map(TextNode, entries))


def generate_union(union_identifier):
    assert isinstance(union_identifier, web_idl.Identifier)

    web_idl_database = package_initializer().web_idl_database()
    union = web_idl_database.find(union_identifier)

    path_manager = PathManager(union)
    assert path_manager.api_component == path_manager.impl_component
    api_component = path_manager.api_component
    for_testing = union.code_generator_info.for_testing

    # Class names
    class_name = blink_class_name(union)

    union_members = map(
        lambda member_type: _UnionMemberImpl(union=union, idl_type=member_type
                                             ), union.flattened_member_types)
    if union.does_include_nullable_type:
        union_members.append(_UnionMemberImpl(union=union, idl_type=None))
    cg_context = CodeGenContext(union=union,
                                union_members=tuple(union_members),
                                class_name=class_name,
                                base_class_name="bindings::UnionBase")

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
                                base_class_names=["bindings::UnionBase"],
                                final=True,
                                export=component_export(
                                    api_component, for_testing))
    class_def.set_base_template_vars(cg_context.template_bindings())

    # Implementation parts
    content_type_enum_class_def = make_content_type_enum_class_def(cg_context)
    trace_decls, trace_defs = make_trace_function(cg_context)
    member_vars_def = make_member_vars_def(cg_context)

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
        "third_party/blink/renderer/platform/bindings/union_base.h",
    ])
    (header_forward_decls, header_include_headers, source_forward_decls,
     source_include_headers) = collect_forward_decls_and_include_headers(
         union.flattened_member_types)
    header_node.accumulator.add_class_decls(header_forward_decls)
    header_node.accumulator.add_include_headers(header_include_headers)
    source_node.accumulator.add_class_decls(source_forward_decls)
    source_node.accumulator.add_include_headers(source_include_headers)

    header_blink_ns.body.append(class_def)
    header_blink_ns.body.append(EmptyNode())

    class_def.public_section.extend([
        content_type_enum_class_def,
        EmptyNode(),
        trace_decls,
        EmptyNode(),
    ])
    source_blink_ns.body.extend([
        trace_defs,
        EmptyNode(),
    ])

    class_def.private_section.append(member_vars_def)
    class_def.private_section.append(EmptyNode())

    # Write down to the files.
    write_code_node_to_file(header_node, path_manager.gen_path_to(header_path))
    write_code_node_to_file(source_node, path_manager.gen_path_to(source_path))


def generate_unions(task_queue):
    assert isinstance(task_queue, TaskQueue)

    web_idl_database = package_initializer().web_idl_database()

    for union in web_idl_database.new_union_types:
        task_queue.post_task(generate_union, union.identifier)
