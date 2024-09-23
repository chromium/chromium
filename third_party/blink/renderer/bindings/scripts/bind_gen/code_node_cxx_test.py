# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from .code_node import SymbolNode
from .code_node import SymbolScopeNode
from .code_node import TextNode
from .code_node import render_code_node
from .code_node_cxx import CxxClassDefNode
from .code_node_cxx import CxxFuncDefNode
from .code_node_cxx import CxxLikelyIfNode
from .code_node_cxx import CxxUnlikelyIfNode
from .codegen_accumulator import CodeGenAccumulator
from .mako_renderer import MakoRenderer


class CodeNodeCxxTest(unittest.TestCase):
    def setUp(self):
        super(CodeNodeCxxTest, self).setUp()
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

    def test_symbol_definition_with_branches(self):
        root = SymbolScopeNode()

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
            CxxUnlikelyIfNode(cond=TextNode("${var2}"),
                              attribute=None,
                              body=[
                                  TextNode("${var3};"),
                                  TextNode("return ${var4};"),
                              ]),
            CxxLikelyIfNode(cond=TextNode("${var5}"),
                            attribute=None,
                            body=[
                                TextNode("return ${var6};"),
                            ]),
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
var3;\
""")

    def test_symbol_definition_with_nested_branches(self):
        root = SymbolScopeNode()

        root.register_code_symbols([
            SymbolNode("var1", "int ${var1} = 1;"),
            SymbolNode("var2", "int ${var2} = 2;"),
            SymbolNode("var3", "int ${var3} = 3;"),
            SymbolNode("var4", "int ${var4} = 4;"),
            SymbolNode("var5", "int ${var5} = 5;"),
            SymbolNode("var6", "int ${var6} = 6;"),
        ])

        root.extend([
            CxxUnlikelyIfNode(cond=TextNode("false"),
                              attribute=None,
                              body=[
                                  CxxUnlikelyIfNode(
                                      cond=TextNode("false"),
                                      attribute=None,
                                      body=[
                                          TextNode("return ${var1};"),
                                      ]),
                                  TextNode("return;"),
                              ]),
            CxxLikelyIfNode(cond=TextNode("true"),
                            attribute=None,
                            body=[
                                CxxLikelyIfNode(
                                    cond=TextNode("true"),
                                    attribute=None,
                                    body=[
                                        TextNode("return ${var2};"),
                                    ]),
                                TextNode("return;"),
                            ]),
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
}\
""")

    def test_function_definition_minimum(self):
        root = CxxFuncDefNode(
            name="blink::bindings::func", arg_decls=[], return_type="void")

        self.assertRenderResult(root, """\
void blink::bindings::func() {

}\
""")

    def test_function_definition_full(self):
        root = CxxFuncDefNode(
            name="blink::bindings::func",
            arg_decls=["int arg1", "int arg2"],
            return_type="void",
            const=True,
            override=True,
            member_initializer_list=[
                "member1(0)",
                "member2(\"str\")",
            ])

        root.body.register_code_symbols([
            SymbolNode("var1", "int ${var1} = 1;"),
            SymbolNode("var2", "int ${var2} = 2;"),
        ])

        root.body.extend([
            CxxUnlikelyIfNode(cond=TextNode("${var1}"),
                              attribute=None,
                              body=[TextNode("return ${var1};")]),
            TextNode("return ${var2};"),
        ])

        self.assertRenderResult(
            root, """\
void blink::bindings::func(int arg1, int arg2) const override\
 : member1(0), member2("str") {
  int var1 = 1;
  if (var1) {
    return var1;
  }
  int var2 = 2;
  return var2;
}\
""")

    def test_class_definition(self):
        root = CxxClassDefNode("X", ["A", "B"], final=True)

        root.public_section.extend([
            TextNode("void m1();"),
            TextNode("void m2();"),
        ])
        root.private_section.extend([
            TextNode("int m1_;"),
            TextNode("int m2_;"),
        ])

        self.assertRenderResult(
            root, """\
class X final : public A, public B {

 public:
  void m1();
  void m2();


 private:
  int m1_;
  int m2_;


};\
""")
