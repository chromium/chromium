# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from mojom.generate import module as mojom
from mojom.generate import translate
from mojom.parse import ast

class TranslateTest(unittest.TestCase):
  """Tests |parser.Parse()|."""

  def testSimpleArray(self):
    """Tests a simple int32[]."""
    # pylint: disable=W0212
    self.assertEquals(translate._MapKind("int32[]"), "a:i32")

  def testAssociativeArray(self):
    """Tests a simple uint8{string}."""
    # pylint: disable=W0212
    self.assertEquals(translate._MapKind("uint8{string}"), "m[s][u8]")

  def testLeftToRightAssociativeArray(self):
    """Makes sure that parsing is done from right to left on the internal kinds
       in the presence of an associative array."""
    # pylint: disable=W0212
    self.assertEquals(translate._MapKind("uint8[]{string}"), "m[s][a:u8]")

  def testTranslateSimpleUnions(self):
    """Makes sure that a simple union is translated correctly."""
    tree = ast.Mojom(None, ast.ImportList(), [
        ast.Union(
            "SomeUnion", None,
            ast.UnionBody([
                ast.UnionField("a", None, None, "int32"),
                ast.UnionField("b", None, None, "string")
            ]))
    ])

    translation = translate.OrderedModule(tree, "mojom_tree", [])
    self.assertEqual(1, len(translation.unions))

    union = translation.unions[0]
    self.assertTrue(isinstance(union, mojom.Union))
    self.assertEqual("SomeUnion", union.mojom_name)
    self.assertEqual(2, len(union.fields))
    self.assertEqual("a", union.fields[0].mojom_name)
    self.assertEqual(mojom.INT32.spec, union.fields[0].kind.spec)
    self.assertEqual("b", union.fields[1].mojom_name)
    self.assertEqual(mojom.STRING.spec, union.fields[1].kind.spec)

  def testMapKindRaisesWithDuplicate(self):
    """Verifies _MapTreeForType() raises when passed two values with the same
       name."""
    methods = [
        ast.Method('dup', None, None, ast.ParameterList(), None),
        ast.Method('dup', None, None, ast.ParameterList(), None)
    ]
    with self.assertRaises(Exception):
      translate._ElemsOfType(methods, ast.Method, 'scope')

  def testAssociatedKinds(self):
    """Tests type spec translation of associated interfaces and requests."""
    # pylint: disable=W0212
    self.assertEquals(
        translate._MapKind("asso<SomeInterface>?"), "?asso:x:SomeInterface")
    self.assertEquals(translate._MapKind("rca<SomeInterface>?"),
                      "?rca:x:SomeInterface")

  def testSelfRecursiveUnions(self):
    """Verifies _UnionField() raises when a union is self-recursive."""
    tree = ast.Mojom(None, ast.ImportList(), [
        ast.Union("SomeUnion", None,
                  ast.UnionBody([ast.UnionField("a", None, None, "SomeUnion")]))
    ])
    with self.assertRaises(Exception):
      translate.OrderedModule(tree, "mojom_tree", [])

    tree = ast.Mojom(None, ast.ImportList(), [
        ast.Union(
            "SomeUnion", None,
            ast.UnionBody([ast.UnionField("a", None, None, "SomeUnion?")]))
    ])
    with self.assertRaises(Exception):
      translate.OrderedModule(tree, "mojom_tree", [])

  def testDuplicateAttributesException(self):
    tree = ast.Mojom(None, ast.ImportList(), [
        ast.Union(
            "FakeUnion",
            ast.AttributeList([
                ast.Attribute("key1", "value"),
                ast.Attribute("key1", "value")
            ]),
            ast.UnionBody([
                ast.UnionField("a", None, None, "int32"),
                ast.UnionField("b", None, None, "string")
            ]))
    ])
    with self.assertRaises(Exception):
      translate.OrderedModule(tree, "mojom_tree", [])

  def testEnumWithReservedValues(self):
    """Verifies that assigning reserved values to enumerators fails."""
    # -128 is reserved for the empty representation in WTF::HashTraits.
    tree = ast.Mojom(None, ast.ImportList(), [
        ast.Enum(
            "MyEnum", None,
            ast.EnumValueList([
                ast.EnumValue('kReserved', None, '-128'),
            ]))
    ])
    with self.assertRaises(Exception) as context:
      translate.OrderedModule(tree, "mojom_tree", [])
    self.assertIn("reserved for WTF::HashTrait", str(context.exception))

    # -127 is reserved for the deleted representation in WTF::HashTraits.
    tree = ast.Mojom(None, ast.ImportList(), [
        ast.Enum(
            "MyEnum", None,
            ast.EnumValueList([
                ast.EnumValue('kReserved', None, '-127'),
            ]))
    ])
    with self.assertRaises(Exception) as context:
      translate.OrderedModule(tree, "mojom_tree", [])
    self.assertIn("reserved for WTF::HashTrait", str(context.exception))

    # Implicitly assigning a reserved value should also fail.
    tree = ast.Mojom(None, ast.ImportList(), [
        ast.Enum(
            "MyEnum", None,
            ast.EnumValueList([
                ast.EnumValue('kNotReserved', None, '-129'),
                ast.EnumValue('kImplicitlyReserved', None, None),
            ]))
    ])
    with self.assertRaises(Exception) as context:
      translate.OrderedModule(tree, "mojom_tree", [])
    self.assertIn("reserved for WTF::HashTrait", str(context.exception))
