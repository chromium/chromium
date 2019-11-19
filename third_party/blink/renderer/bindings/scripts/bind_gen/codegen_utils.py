# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import web_idl

from .clang_format import clang_format
from .code_node import CodeNode
from .code_node import LiteralNode
from .code_node import SymbolScopeNode
from .codegen_accumulator import CodeGenAccumulator
from .path_manager import PathManager


def make_copyright_header():
    return LiteralNode("""\
// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.\
""")


def make_header_include_directives(accumulator):
    assert isinstance(accumulator, CodeGenAccumulator)

    class HeaderIncludeDirectives(object):
        def __init__(self, accumulator):
            self._accumulator = accumulator

        def __str__(self):
            return "\n".join([
                "#include \"{}\"".format(header)
                for header in sorted(self._accumulator.include_headers)
            ])

    return LiteralNode(HeaderIncludeDirectives(accumulator))


def enclose_with_header_guard(code_node, header_guard):
    assert isinstance(code_node, CodeNode)
    assert isinstance(header_guard, str)

    return SymbolScopeNode([
        LiteralNode("#ifndef {}".format(header_guard)),
        LiteralNode("#define {}".format(header_guard)),
        LiteralNode(""),
        code_node,
        LiteralNode(""),
        LiteralNode("#endif  // {}".format(header_guard)),
    ])


def enclose_with_namespace(code_node, namespace):
    assert isinstance(code_node, CodeNode)
    assert isinstance(namespace, str)

    return SymbolScopeNode([
        LiteralNode("namespace {} {{".format(namespace)),
        LiteralNode(""),
        code_node,
        LiteralNode(""),
        LiteralNode("}}  // namespace {}".format(namespace)),
    ])


def traverse_idl_types(idl_definition, callback):
    """
    Traverses in the given |idl_definition| to find all the web_idl.IdlType used
    in the IDL definition.  Invokes |callback| with each web_idl.IdlType.
    """
    assert callable(callback)

    def get(obj, attr):
        try:
            return getattr(obj, attr)
        except:
            return ()

    xs = (get(idl_definition, "attributes") + get(idl_definition, "constants")
          + get(idl_definition, "own_members") + (idl_definition, ))
    for x in xs:
        idl_type = get(x, "idl_type")
        if idl_type:
            callback(idl_type)

    xs = (get(idl_definition, "constructors") + get(idl_definition,
                                                    "operations"))
    for x in xs:
        for argument in x.arguments:
            callback(argument.idl_type)
        if x.return_type is not None:
            callback(x.return_type)

    xs = get(idl_definition, "flattened_member_types")
    for x in xs:
        callback(x)


def collect_include_headers(idl_definition):
    """
    Returns a list of include headers that are required by generated bindings of
    |idl_definition|.
    """
    type_def_objs = set()

    def collect_type_def_obj(idl_type):
        type_def_obj = idl_type.unwrap().type_definition_object
        if type_def_obj is not None:
            type_def_objs.add(type_def_obj)

    traverse_idl_types(idl_definition, collect_type_def_obj)

    header_paths = set()
    for type_def_obj in type_def_objs:
        if isinstance(type_def_obj, web_idl.Enumeration):
            continue
        header_paths.add(PathManager(type_def_obj).blink_path(ext="h"))

    return header_paths


def render_code_node(code_node):
    """
    Renders |code_node| and turns it into text letting |code_node| apply all
    necessary changes (side effects).  Returns the resulting text.
    """
    prev = "_"
    current = ""
    while current != prev:
        prev = current
        current = str(code_node)
    return current


def write_code_node_to_file(code_node, filepath):
    """Renders |code_node| and then write the result to |filepath|."""
    assert isinstance(code_node, CodeNode)
    assert isinstance(filepath, str)

    rendered_text = render_code_node(code_node)

    format_result = clang_format(rendered_text, filename=filepath)

    with open(filepath, "w") as output_file:
        output_file.write(format_result.contents)
