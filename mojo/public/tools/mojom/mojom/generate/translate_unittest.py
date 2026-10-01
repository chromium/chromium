# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re
import unittest

from mojom.error import Error
from mojom.generate import module as mojom
from mojom.generate import translate
from mojom.parse import ast
from mojom.parse import parser


class TranslateTest(unittest.TestCase):
  """Tests |parser.Parse()|."""

  def testSimpleArray(self):
    """Tests a simple int32[]."""
    self.assertEqual(
      translate._MapKind(ast.Array(ast.Typename(ast.Identifier('int32')))),
      "a:i32",
    )

  def testAssociativeArray(self):
    """Tests a simple uint8{string}."""
    self.assertEqual(
      translate._MapKind(
        ast.Map(ast.Identifier('string'), ast.Typename(ast.Identifier('uint8')))
      ),
      "m[s][u8]",
    )

  def testHashMap(self):
    """Tests a simple hash_map<string, uint8>."""
    self.assertEqual(
      translate._MapKind(
        ast.HashMap(
          ast.Identifier('string'),
          ast.Typename(ast.Identifier('uint8')),
        )
      ),
      "hm[s][u8]",
    )

  def testLeftToRightAssociativeArray(self):
    """Makes sure that parsing is done from right to left on the internal kinds
    in the presence of an associative array."""
    self.assertEqual(
      translate._MapKind(
        ast.Map(
          ast.Identifier('string'),
          ast.Typename(ast.Array(ast.Typename(ast.Identifier('uint8')))),
        )
      ),
      "m[s][a:u8]",
    )

  def testLeftToRightHashMap(self):
    """Makes sure that parsing is done from right to left on the internal kinds
    in the presence of a hash map."""
    self.assertEqual(
      translate._MapKind(
        ast.HashMap(
          ast.Identifier('string'),
          ast.Typename(ast.Array(ast.Typename(ast.Identifier('uint8')))),
        )
      ),
      "hm[s][a:u8]",
    )

  def testTranslateSimpleUnions(self):
    """Makes sure that a simple union is translated correctly."""
    tree = ast.Mojom(
      None,
      ast.ImportList(),
      [
        ast.Union(
          ast.Name("SomeUnion"),
          None,
          ast.UnionBody(
            [
              ast.UnionField(
                ast.Name("a"), None, None, ast.Typename(ast.Identifier("int32"))
              ),
              ast.UnionField(
                ast.Name("b"),
                None,
                None,
                ast.Typename(ast.Identifier("string")),
              ),
            ]
          ),
        )
      ],
    )

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
      ast.Method(ast.Name('dup'), None, None, ast.ParameterList(), None),
      ast.Method(ast.Name('dup'), None, None, ast.ParameterList(), None),
    ]
    with self.assertRaises(Exception):
      translate._ElemsOfType(methods, ast.Method, 'scope')

  def testAssociatedKinds(self):
    """Tests type spec translation of associated interfaces and requests."""
    self.assertEqual(
      translate._MapKind(
        ast.Typename(
          ast.Receiver(ast.Identifier('SomeInterface'), associated=True),
          nullable=True,
        )
      ),
      "?rca:x:SomeInterface",
    )
    self.assertEqual(
      translate._MapKind(
        ast.Typename(
          ast.Remote(ast.Identifier('SomeInterface'), associated=True),
          nullable=True,
        )
      ),
      "?rma:x:SomeInterface",
    )

  def testSelfRecursiveUnions(self):
    """Verifies _UnionField() raises when a union is self-recursive."""
    tree = ast.Mojom(
      None,
      ast.ImportList(),
      [
        ast.Union(
          ast.Name("SomeUnion"),
          None,
          ast.UnionBody(
            [
              ast.UnionField(
                ast.Name("a"),
                None,
                None,
                ast.Typename(ast.Identifier("SomeUnion")),
              )
            ]
          ),
        )
      ],
    )
    with self.assertRaises(Exception):
      translate.OrderedModule(tree, "mojom_tree", [])

    tree = ast.Mojom(
      None,
      ast.ImportList(),
      [
        ast.Union(
          ast.Name("SomeUnion"),
          None,
          ast.UnionBody(
            [
              ast.UnionField(
                ast.Name("a"),
                None,
                None,
                ast.Typename(ast.Identifier("SomeUnion"), nullable=True),
              )
            ]
          ),
        )
      ],
    )
    with self.assertRaises(Exception):
      translate.OrderedModule(tree, "mojom_tree", [])

  def testDuplicateAttributesException(self):
    tree = ast.Mojom(
      None,
      ast.ImportList(),
      [
        ast.Union(
          ast.Name("FakeUnion"),
          ast.AttributeList(
            [
              ast.Attribute(ast.Name("key1"), ast.Name("value")),
              ast.Attribute(ast.Name("key1"), ast.Name("value")),
            ]
          ),
          ast.UnionBody(
            [
              ast.UnionField(
                ast.Name("a"), None, None, ast.Typename(ast.Identifier("int32"))
              ),
              ast.UnionField(
                ast.Name("b"),
                None,
                None,
                ast.Typename(ast.Identifier("string")),
              ),
            ]
          ),
        )
      ],
    )
    with self.assertRaises(Exception):
      translate.OrderedModule(tree, "mojom_tree", [])

  def testExtensibleEnumWithNoDefault(self):
    tree = ast.Mojom(
      None,
      ast.ImportList(),
      [
        ast.Enum(
          ast.Name('TestEnum'),
          ast.AttributeList(
            [
              ast.Attribute(ast.Name('Extensible'), True),
            ]
          ),
          ast.EnumValueList([ast.EnumValue(ast.Name('kValue'), None, None)]),
        )
      ],
    )

    with self.assertRaises(Exception) as context:
      translate.OrderedModule(tree, 'mojom_tree', [])
    self.assertIn('must specify a [Default] enumerator', str(context.exception))

    # Not allowlisted in ChromeOS so this should still warn.
    with self.assertRaises(Exception) as context:
      translate.OrderedModule(
        tree,
        'mojom_tree',
        [],
        extensible_enum_mode=translate.ExtensibleEnumMode.RELAXED_FOR_CHROMEOS,
      )
    self.assertIn('must specify a [Default] enumerator', str(context.exception))

    # However, backwards compatibility checks always suppress this warning.
    translate.OrderedModule(
      tree,
      'mojom_tree',
      [],
      extensible_enum_mode=translate.ExtensibleEnumMode.RELAXED_FOR_BACKWARDS_COMPAT_CHECK,
    )

    # Test that temporary suppressions for non-CrOS do not throw an exception.
    temporarily_suppressed_tree = ast.Mojom(
      ast.Module(ast.Identifier('test.mojom'), None),
      ast.ImportList(),
      [
        ast.Enum(
          ast.Name('ExtensibleEnumForUnitTests'),
          ast.AttributeList(
            [
              ast.Attribute(ast.Name('Extensible'), True),
            ]
          ),
          ast.EnumValueList([ast.EnumValue(ast.Name('kValue'), None, None)]),
        )
      ],
    )

    translate.OrderedModule(temporarily_suppressed_tree, 'mojom_tree', [])
    translate.OrderedModule(
      temporarily_suppressed_tree,
      'mojom_tree',
      [],
      extensible_enum_mode=translate.ExtensibleEnumMode.RELAXED_FOR_CHROMEOS,
    )

    # Test that permanent (for now) suppressions for CrOS do not throw an
    # exception–but are still treated as errors in non-ChromeOS mode.
    suppressed_for_chromeos_tree = ast.Mojom(
      ast.Module(ast.Identifier('test.mojom'), None),
      ast.ImportList(),
      [
        ast.Enum(
          ast.Name('ExtensibleEnumForUnitTestsCrOS'),
          ast.AttributeList(
            [
              ast.Attribute(ast.Name('Extensible'), True),
            ]
          ),
          ast.EnumValueList([ast.EnumValue(ast.Name('kValue'), None, None)]),
        )
      ],
    )
    with self.assertRaises(Exception) as context:
      translate.OrderedModule(suppressed_for_chromeos_tree, 'mojom_tree', [])
    self.assertIn('must specify a [Default] enumerator', str(context.exception))
    translate.OrderedModule(
      suppressed_for_chromeos_tree,
      'mojom_tree',
      [],
      extensible_enum_mode=translate.ExtensibleEnumMode.RELAXED_FOR_CHROMEOS,
    )

  def testEnumWithReservedValues(self):
    """Verifies that assigning reserved values to enumerators fails."""
    # -128 is reserved for the empty representation in blink::HashTraits.
    tree = ast.Mojom(
      None,
      ast.ImportList(),
      [
        ast.Enum(
          ast.Name("MyEnum"),
          None,
          ast.EnumValueList(
            [
              ast.EnumValue(
                ast.Name('kReserved'), None, ast.Literal('int', '-128')
              ),
            ]
          ),
        )
      ],
    )
    with self.assertRaises(Exception) as context:
      translate.OrderedModule(tree, "mojom_tree", [])
    self.assertIn("reserved for blink::HashTrait", str(context.exception))

    # -127 is reserved for the deleted representation in blink::HashTraits.
    tree = ast.Mojom(
      None,
      ast.ImportList(),
      [
        ast.Enum(
          ast.Name("MyEnum"),
          None,
          ast.EnumValueList(
            [
              ast.EnumValue(
                ast.Name('kReserved'), None, ast.Literal('int', '-127')
              ),
            ]
          ),
        )
      ],
    )
    with self.assertRaises(Exception) as context:
      translate.OrderedModule(tree, "mojom_tree", [])
    self.assertIn("reserved for blink::HashTrait", str(context.exception))

    # Implicitly assigning a reserved value should also fail.
    tree = ast.Mojom(
      None,
      ast.ImportList(),
      [
        ast.Enum(
          ast.Name("MyEnum"),
          None,
          ast.EnumValueList(
            [
              ast.EnumValue(
                ast.Name('kNotReserved'), None, ast.Literal('int', '-129')
              ),
              ast.EnumValue(ast.Name('kImplicitlyReserved'), None, None),
            ]
          ),
        )
      ],
    )
    with self.assertRaises(Exception) as context:
      translate.OrderedModule(tree, "mojom_tree", [])
    self.assertIn("reserved for blink::HashTrait", str(context.exception))

  def testDisallowedUntypedHandle(self):
    """Tests that untyped handle is disallowed in non-allowlisted kinds."""
    cases = [
      ('struct S {\n  handle h;\n};', 2, 'S'),
      ('struct S {\n  int32 a;\n  handle? h;\n};', 3, 'S'),
      ('struct S {\n  array<handle> h;\n};', 2, 'S'),
      ('struct S {\n  map<string, handle> h;\n};', 2, 'S'),
      ('union U {\n  handle h;\n};', 2, 'U'),
      ('interface I {\n  Do(handle h);\n};', 2, 'I'),
      ('interface I {\n  Do() => (handle? h);\n};', 2, 'I'),
      ('interface I {\n  Do() => result<handle, string>;\n};', 2, 'I'),
      ('struct S {\n  const handle kH = 0;\n};', 2, None),
    ]
    for source, line, enclosing in cases:
      location = rf' \(in {enclosing}\)' if enclosing else ''
      # The allowlist is type-based, so the path should not matter.
      for path in (
        'my_file.mojom',
        'mojo/public/interfaces/bindings/native_struct.mojom',
      ):
        tree = parser.Parse(source, path)
        for mode in translate.ExtensibleEnumMode:
          with self.subTest(source=source, path=path, mode=mode):
            with self.assertRaisesRegex(
              Error,
              rf"^{re.escape(path)}:{line}: Error: "
              rf"Untyped 'handle' is disallowed{location}; please specify a "
              r"subtype such as 'handle<platform>' or 'handle<message_pipe>'$",
            ):
              translate.OrderedModule(tree, path, {}, extensible_enum_mode=mode)

  def testAllowlistedUntypedHandle(self):
    """Tests that untyped handle is allowed in allowlisted kinds."""
    source = """\
        module test.mojom;
        struct UntypedHandleStructForUnitTests {
          handle a;
          handle? b;
          array<handle> c;
          map<string, array<handle?>> d;
        };
        union UntypedHandleUnionForUnitTests {
          handle a;
        };
        interface UntypedHandleInterfaceForUnitTests {
          A(handle a) => (handle? b);
          B() => result<handle, string>;
        };
        """
    tree = parser.Parse(source, 'my_file.mojom')
    for mode in translate.ExtensibleEnumMode:
      with self.subTest(mode=mode):
        translate.OrderedModule(
          tree, 'my_file.mojom', {}, extensible_enum_mode=mode
        )

  def testAllowlistIsPerKind(self):
    """Tests that allowlisting a kind does not allowlist other kinds in the same
    module."""
    source = """\
        module test.mojom;
        struct UntypedHandleStructForUnitTests {
          handle a;
        };
        struct Other {
          handle b;
        };
        """
    with self.assertRaisesRegex(
      Error,
      r"^my_file\.mojom:6: Error: "
      r"Untyped 'handle' is disallowed \(in test\.mojom\.Other\); ",
    ):
      translate.OrderedModule(
        parser.Parse(source, 'my_file.mojom'), 'my_file.mojom', {}
      )
