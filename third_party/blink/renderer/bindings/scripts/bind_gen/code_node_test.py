# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from .code_node import FunctionDefinitionNode
from .code_node import LikelyExitNode
from .code_node import LiteralNode
from .code_node import SequenceNode
from .code_node import SymbolNode
from .code_node import SymbolScopeNode
from .code_node import TextNode
from .code_node import UnlikelyExitNode
from .codegen_utils import render_code_node
from .mako_renderer import MakoRenderer


class CodeNodeTest(unittest.TestCase):
    def setUp(self):
        super(CodeNodeTest, self).setUp()
        self.addTypeEqualityFunc(str, self.assertMultiLineEqual)

    def assertRenderResult(self, node, expected):
        if node.renderer is None:
            node.set_renderer(MakoRenderer())

        def simplify(text):
            return "\n".join(
                [" ".join(line.split()) for line in text.split("\n")])

        actual = simplify(render_code_node(node))
        expected = simplify(expected)

        self.assertEqual(actual, expected)

    def test_literal_node(self):
        """
        Tests that, in LiteralNode, the special characters of template (%, ${},
        etc) are not processed.
        """
        root = LiteralNode("<% x = 42 %>${x}")
        self.assertRenderResult(root, "<% x = 42 %>${x}")

    def test_empty_literal_node(self):
        root = LiteralNode("")
        self.assertRenderResult(root, "")

    def test_text_node(self):
        """Tests that the template language works in TextNode."""
        root = TextNode("<% x = 42 %>${x}")
        self.assertRenderResult(root, "42")

    def test_empty_text_node(self):
        root = TextNode("")
        self.assertRenderResult(root, "")

    def test_list_operations_of_sequence_node(self):
        """
        Tests that list operations (insert, append, and extend) of SequenceNode
        work just same as Python built-in list.
        """
        root = SequenceNode(separator=",")
        root.extend([
            LiteralNode("2"),
            LiteralNode("4"),
        ])
        root.insert(1, LiteralNode("3"))
        root.insert(0, LiteralNode("1"))
        root.insert(100, LiteralNode("5"))
        root.append(LiteralNode("6"))
        self.assertRenderResult(root, "1,2,3,4,5,6")
        root.remove(root[0])
        root.remove(root[2])
        root.remove(root[-1])
        self.assertRenderResult(root, "2,3,5")

    def test_nested_sequence(self):
        """Tests nested SequenceNodes."""
        root = SequenceNode(separator=",")
        nested = SequenceNode(separator=",")
        nested.extend([
            LiteralNode("2"),
            LiteralNode("3"),
            LiteralNode("4"),
        ])
        root.extend([
            LiteralNode("1"),
            nested,
            LiteralNode("5"),
        ])
        self.assertRenderResult(root, "1,2,3,4,5")

    def test_symbol_definition_chains(self):
        """
        Tests that use of SymbolNode inserts necessary SymbolDefinitionNode
        appropriately.
        """
        root = SymbolScopeNode(separator_last="\n")

        root.register_code_symbols([
            SymbolNode("var1", "int ${var1} = ${var2} + ${var3};"),
            SymbolNode("var2", "int ${var2} = ${var5};"),
            SymbolNode("var3", "int ${var3} = ${var4};"),
            SymbolNode("var4", "int ${var4} = 1;"),
            SymbolNode("var5", "int ${var5} = 2;"),
        ])

        root.append(TextNode("(void)${var1};"))

        self.assertRenderResult(
            root, """\
int var5 = 2;
int var2 = var5;
int var4 = 1;
int var3 = var4;
int var1 = var2 + var3;
(void)var1;
""")

    def test_symbol_definition_with_exit_branches(self):
        root = SymbolScopeNode(separator_last="\n")

        root.register_code_symbols([
            SymbolNode("var1", "int ${var1} = 1;"),
            SymbolNode("var2", "int ${var2} = 2;"),
            SymbolNode("var3", "int ${var3} = 3;"),
            SymbolNode("var4", "int ${var4} = 4;"),
            SymbolNode("var5", "int ${var5} = 5;"),
            SymbolNode("var6", "int ${var6} = 6;"),
        ])

        root.extend([
            TextNode("${var1};"),
            UnlikelyExitNode(
                cond=TextNode("${var2}"),
                body=SymbolScopeNode([
                    TextNode("${var3};"),
                    TextNode("return ${var4};"),
                ])),
            LikelyExitNode(
                cond=TextNode("${var5}"),
                body=SymbolScopeNode([
                    TextNode("return ${var6};"),
                ])),
            TextNode("${var3};"),
        ])

        self.assertRenderResult(
            root, """\
int var1 = 1;
var1;
int var2 = 2;
int var3 = 3;
if (var2) {
  var3;
  int var4 = 4;
  return var4;
}
int var5 = 5;
if (var5) {
  int var6 = 6;
  return var6;
}
var3;
""")

    def test_symbol_definition_with_nested_exit_branches(self):
        root = SymbolScopeNode(separator_last="\n")

        root.register_code_symbols([
            SymbolNode("var1", "int ${var1} = 1;"),
            SymbolNode("var2", "int ${var2} = 2;"),
            SymbolNode("var3", "int ${var3} = 3;"),
            SymbolNode("var4", "int ${var4} = 4;"),
            SymbolNode("var5", "int ${var5} = 5;"),
            SymbolNode("var6", "int ${var6} = 6;"),
        ])

        root.extend([
            UnlikelyExitNode(
                cond=LiteralNode("false"),
                body=SymbolScopeNode([
                    UnlikelyExitNode(
                        cond=LiteralNode("false"),
                        body=SymbolScopeNode([
                            TextNode("return ${var1};"),
                        ])),
                    LiteralNode("return;"),
                ])),
            LikelyExitNode(
                cond=LiteralNode("true"),
                body=SymbolScopeNode([
                    LikelyExitNode(
                        cond=LiteralNode("true"),
                        body=SymbolScopeNode([
                            TextNode("return ${var2};"),
                        ])),
                    LiteralNode("return;"),
                ])),
        ])

        self.assertRenderResult(
            root, """\
if (false) {
  if (false) {
    int var1 = 1;
    return var1;
  }
  return;
}
if (true) {
  if (true) {
    int var2 = 2;
    return var2;
  }
  return;
}
""")

    def test_function_definition_minimum(self):
        root = SymbolScopeNode(separator_last="\n")
        root.append(
            FunctionDefinitionNode(
                name=LiteralNode("blink::bindings::func"),
                arg_decls=[],
                return_type=LiteralNode("void")))

        self.assertRenderResult(root, """\

void blink::bindings::func() {

}
""")

    def test_function_definition_full(self):
        root = SymbolScopeNode(separator_last="\n")

        local_vars = [
            SymbolNode("var1", "int ${var1} = 1;"),
            SymbolNode("var2", "int ${var2} = 2;"),
        ]

        func_body = SymbolScopeNode([
            UnlikelyExitNode(
                cond=TextNode("${var1}"),
                body=SymbolScopeNode([TextNode("return ${var1};")])),
            TextNode("return ${var2};"),
        ])

        root.append(
            FunctionDefinitionNode(
                name=LiteralNode("blink::bindings::func"),
                arg_decls=[LiteralNode("int arg1"),
                           LiteralNode("int arg2")],
                return_type=LiteralNode("void"),
                member_initializer_list=[
                    LiteralNode("member1(0)"),
                    LiteralNode("member2(\"str\")")
                ],
                local_vars=local_vars,
                body=func_body,
                comment=LiteralNode("// comment1\n// comment2")))

        self.assertRenderResult(
            root, """\
// comment1
// comment2
void blink::bindings::func(int arg1, int arg2) : member1(0), member2("str") {
  int var1 = 1;
  if (var1) {
    return var1;
  }
  int var2 = 2;
  return var2;
}
""")

    def test_template_error_handling(self):
        renderer = MakoRenderer()
        root = SymbolScopeNode()
        root.set_renderer(renderer)

        root.append(
            SymbolScopeNode([
                # Have Mako raise a NameError.
                TextNode("${unbound_symbol}"),
            ]))

        with self.assertRaises(NameError):
            root.render()

        callers_on_error = list(renderer.callers_on_error)
        self.assertEqual(len(callers_on_error), 3)
        self.assertEqual(callers_on_error[0], root[0][0])
        self.assertEqual(callers_on_error[1], root[0])
        self.assertEqual(callers_on_error[2], root)
        self.assertEqual(renderer.last_caller_on_error, root[0][0])
