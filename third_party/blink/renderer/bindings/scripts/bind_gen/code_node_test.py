# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from .code_node import ListNode
from .code_node import LiteralNode
from .code_node import SymbolNode
from .code_node import SymbolScopeNode
from .code_node import SymbolSensitiveSelectionNode
from .code_node import TextNode
from .code_node import WeakDependencyNode
from .code_node import render_code_node
from .codegen_accumulator import CodeGenAccumulator
from .mako_renderer import MakoRenderer


class CodeNodeTest(unittest.TestCase):
    def setUp(self):
        super(CodeNodeTest, self).setUp()
        self.addTypeEqualityFunc(str, self.assertMultiLineEqual)

    def assertRenderResult(self, node, expected):
        if node.renderer is None:
            node.set_renderer(MakoRenderer())
        if node.accumulator is None:
            node.set_accumulator(CodeGenAccumulator())

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
        Tests that list operations (insert, append, and extend) of ListNode
        work just same as Python built-in list.
        """
        root = ListNode(separator=",")
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

    def test_list_node_head_and_tail(self):
        self.assertRenderResult(ListNode(), "")
        self.assertRenderResult(ListNode(head="head"), "")
        self.assertRenderResult(ListNode(tail="tail"), "")
        self.assertRenderResult(
            ListNode([TextNode("-content-")], head="head", tail="tail"),
            "head-content-tail")

    def test_nested_sequence(self):
        """Tests nested ListNodes."""
        root = ListNode(separator=",")
        nested = ListNode(separator=",")
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
        root = SymbolScopeNode(tail="\n")

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

    def test_weak_dependency_node(self):
        root = SymbolScopeNode(tail="\n")

        root.register_code_symbols([
            SymbolNode("var1", "int ${var1} = 1;"),
            SymbolNode("var2", "int ${var2} = 2;"),
            SymbolNode("var3", "int ${var3} = 3;"),
        ])

        root.extend([
            WeakDependencyNode(dep_syms=["var1", "var2"]),
            TextNode("f();"),
            TextNode("(void)${var3};"),
            TextNode("(void)${var1};"),
        ])

        self.assertRenderResult(
            root, """\
int var1 = 1;

f();
int var3 = 3;
(void)var3;
(void)var1;
""")

    def test_symbol_sensitive_selection_node(self):
        root = SymbolScopeNode(tail="\n")

        root.register_code_symbols([
            SymbolNode("var1", "int ${var1} = 1;"),
            SymbolNode("var2", "int ${var2} = 2;"),
            SymbolNode("var3", "int ${var3} = 3;"),
        ])

        choice1 = SymbolSensitiveSelectionNode.Choice(
            symbol_names=["var1", "var2"],
            code_node=TextNode("F(${var1}, ${var2});"))
        choice2 = SymbolSensitiveSelectionNode.Choice(
            symbol_names=["var3"], code_node=TextNode("F(${var3});"))
        choice3 = SymbolSensitiveSelectionNode.Choice(
            symbol_names=[], code_node=TextNode("F();"))
        root.append(SymbolSensitiveSelectionNode([choice1, choice2, choice3]))

        self.assertRenderResult(root, """\
F();
""")

        root.insert(0, TextNode("(void)${var3};"))
        self.assertRenderResult(root, """\
int var3 = 3;
(void)var3;
F(var3);
""")

        root.insert(0, TextNode("(void)${var2};"))
        self.assertRenderResult(
            root, """\
int var2 = 2;
(void)var2;
int var3 = 3;
(void)var3;
F(var3);
""")

        root.insert(0, TextNode("(void)${var1};"))
        self.assertRenderResult(
            root, """\
int var1 = 1;
(void)var1;
int var2 = 2;
(void)var2;
int var3 = 3;
(void)var3;
F(var1, var2);
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
            renderer.reset()
            root.render(renderer)

        callers_on_error = list(renderer.callers_on_error)
        self.assertEqual(len(callers_on_error), 3)
        self.assertEqual(callers_on_error[0], root[0][0])
        self.assertEqual(callers_on_error[1], root[0])
        self.assertEqual(callers_on_error[2], root)
        self.assertEqual(renderer.last_caller_on_error, root[0][0])
