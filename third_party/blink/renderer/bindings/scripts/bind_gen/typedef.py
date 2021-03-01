# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import web_idl

from . import name_style
from .blink_v8_bridge import blink_class_name
from .code_node import EmptyNode
from .code_node import ListNode
from .code_node import TextNode
from .code_node_cxx import CxxNamespaceNode
from .codegen_accumulator import CodeGenAccumulator
from .codegen_utils import enclose_with_header_guard
from .codegen_utils import make_copyright_header
from .codegen_utils import make_forward_declarations
from .codegen_utils import make_header_include_directives
from .codegen_utils import write_code_node_to_file
from .mako_renderer import MakoRenderer
from .package_initializer import package_initializer
from .path_manager import PathManager
from .task_queue import TaskQueue


def _make_union_typedefs(typedefs):
    assert isinstance(typedefs, (list, tuple))
    assert all(isinstance(typedef, web_idl.Typedef) for typedef in typedefs)

    new_and_old_names = list(
        map(
            lambda typedef: (blink_class_name(typedef),
                             blink_class_name(typedef.idl_type.
                                              new_union_definition_object)),
            filter(lambda typedef: typedef.idl_type.is_union, typedefs)))
    node = ListNode([
        TextNode("using {} = {};".format(new_name, old_name))
        for new_name, old_name in new_and_old_names
    ])
    node.accumulate(
        CodeGenAccumulator.require_class_decls(
            [old_name for _, old_name in new_and_old_names]))
    return node


def make_typedefs(typedefs):
    assert isinstance(typedefs, (list, tuple))
    assert all(isinstance(typedef, web_idl.Typedef) for typedef in typedefs)

    return ListNode([
        TextNode("// Typedefs to IDL unions"),
        _make_union_typedefs(typedefs),
    ])


def generate_typedefs_all(filepath_basename):
    assert isinstance(filepath_basename, str)

    web_idl_database = package_initializer().web_idl_database()

    # Components
    c1 = web_idl.Component("core")
    c2 = web_idl.Component("modules")
    components = (c1, c2)

    # Filepaths
    header_path = {}
    for component in components:
        header_path[component] = PathManager.component_path(
            component, "{}.h".format(filepath_basename))

    # Root nodes
    header_node = {}
    for component in components:
        node = ListNode(tail="\n")
        node.set_accumulator(CodeGenAccumulator())
        node.set_renderer(MakoRenderer())
        header_node[component] = node

    # Namespaces
    header_blink_ns = {}
    for component in components:
        node = CxxNamespaceNode(name_style.namespace("blink"))
        header_blink_ns[component] = node

    # Header part (copyright, include directives, and forward declarations)
    for component in components:
        header_node[component].extend([
            make_copyright_header(),
            EmptyNode(),
            enclose_with_header_guard(
                ListNode([
                    make_header_include_directives(
                        header_node[component].accumulator),
                    EmptyNode(),
                    header_blink_ns[component],
                ]), name_style.header_guard(header_path[component])),
        ])
        header_blink_ns[component].body.extend([
            make_forward_declarations(header_node[component].accumulator),
            EmptyNode(),
        ])

    # Typedefs
    all_typedefs = sorted(web_idl_database.typedefs,
                          key=lambda x: x.identifier)
    typedefs = {
        c1: list(filter(lambda x: c2 not in x.components, all_typedefs)),
        c2: list(filter(lambda x: c2 in x.components, all_typedefs)),
    }
    for component in components:
        header_blink_ns[component].body.extend([
            make_typedefs(typedefs[component]),
            EmptyNode(),
        ])

    # Write down to the files.
    for component in components:
        write_code_node_to_file(
            header_node[component],
            PathManager.gen_path_to(header_path[component]))


def generate_typedefs(task_queue):
    assert isinstance(task_queue, TaskQueue)

    task_queue.post_task(generate_typedefs_all, "v8_typedefs")
