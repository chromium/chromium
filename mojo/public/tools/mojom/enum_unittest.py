# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from mojom_parser_test_case import MojomParserTestCase


class EnumTest(MojomParserTestCase):
  """Tests enum parsing behavior."""

  def testExplicitValues(self):
    """Verifies basic parsing of assigned integral values."""
    types = self.ExtractTypes('enum E { kFoo=0, kBar=2, kBaz };')
    self.assertEqual('kFoo', types['E'].fields[0].mojom_name)
    self.assertEqual(0, types['E'].fields[0].numeric_value)
    self.assertEqual('kBar', types['E'].fields[1].mojom_name)
    self.assertEqual(2, types['E'].fields[1].numeric_value)
    self.assertEqual('kBaz', types['E'].fields[2].mojom_name)
    self.assertEqual(3, types['E'].fields[2].numeric_value)

  def testImplicitValues(self):
    """Verifies basic automatic assignment of integral values at parse time."""
    types = self.ExtractTypes('enum E { kFoo, kBar, kBaz };')
    self.assertEqual('kFoo', types['E'].fields[0].mojom_name)
    self.assertEqual(0, types['E'].fields[0].numeric_value)
    self.assertEqual('kBar', types['E'].fields[1].mojom_name)
    self.assertEqual(1, types['E'].fields[1].numeric_value)
    self.assertEqual('kBaz', types['E'].fields[2].mojom_name)
    self.assertEqual(2, types['E'].fields[2].numeric_value)

  def testSameEnumReference(self):
    """Verifies that an enum value can be assigned from the value of another
    field within the same enum."""
    types = self.ExtractTypes('enum E { kA, kB, kFirst=kA };')
    self.assertEqual('kA', types['E'].fields[0].mojom_name)
    self.assertEqual(0, types['E'].fields[0].numeric_value)
    self.assertEqual('kB', types['E'].fields[1].mojom_name)
    self.assertEqual(1, types['E'].fields[1].numeric_value)
    self.assertEqual('kFirst', types['E'].fields[2].mojom_name)
    self.assertEqual(0, types['E'].fields[2].numeric_value)

  def testSameModuleOtherEnumReference(self):
    """Verifies that an enum value can be assigned from the value of a field
    in another enum within the same module."""
    types = self.ExtractTypes('enum E { kA, kB }; enum F { kA = E.kB };')
    self.assertEqual(1, types['F'].fields[0].numeric_value)

  def testImportedEnumReference(self):
    """Verifies that an enum value can be assigned from the value of a field
    in another enum within a different module."""
    a_mojom = 'a.mojom'
    self.WriteFile(a_mojom, 'module a; enum E { kFoo=42, kBar };')
    b_mojom = 'b.mojom'
    self.WriteFile(b_mojom,
                   'module b; import "a.mojom"; enum F { kFoo = a.E.kBar };')
    self.ParseMojoms([a_mojom, b_mojom])
    b = self.LoadModule(b_mojom)

    self.assertEqual('F', b.enums[0].mojom_name)
    self.assertEqual('kFoo', b.enums[0].fields[0].mojom_name)
    self.assertEqual(43, b.enums[0].fields[0].numeric_value)

  def testConstantReference(self):
    """Verifies that an enum value can be assigned from the value of an
    integral constant within the same module."""
    types = self.ExtractTypes('const int32 kFoo = 42; enum E { kA = kFoo };')
    self.assertEqual(42, types['E'].fields[0].numeric_value)

  def testInvalidConstantReference(self):
    """Verifies that enum values cannot be assigned from the value of
    non-integral constants."""
    with self.assertRaisesRegexp(ValueError, 'not an integer'):
      self.ExtractTypes('const float kFoo = 1.0; enum E { kA = kFoo };')
    with self.assertRaisesRegexp(ValueError, 'not an integer'):
      self.ExtractTypes('const double kFoo = 1.0; enum E { kA = kFoo };')
    with self.assertRaisesRegexp(ValueError, 'not an integer'):
      self.ExtractTypes('const string kFoo = "lol"; enum E { kA = kFoo };')

  def testImportedConstantReference(self):
    """Verifies that an enum value can be assigned from the value of an integral
    constant within an imported module."""
    a_mojom = 'a.mojom'
    self.WriteFile(a_mojom, 'module a; const int32 kFoo = 37;')
    b_mojom = 'b.mojom'
    self.WriteFile(b_mojom,
                   'module b; import "a.mojom"; enum F { kFoo = a.kFoo };')
    self.ParseMojoms([a_mojom, b_mojom])
    b = self.LoadModule(b_mojom)

    self.assertEqual('F', b.enums[0].mojom_name)
    self.assertEqual('kFoo', b.enums[0].fields[0].mojom_name)
    self.assertEqual(37, b.enums[0].fields[0].numeric_value)

  def testEnumAttributesAreEnums(self):
    """Verifies that enum values in attributes are really enum types."""
    a_mojom = 'a.mojom'
    self.WriteFile(a_mojom, 'module a; enum E { kFoo, kBar };')
    b_mojom = 'b.mojom'
    self.WriteFile(
        b_mojom, 'module b;'
        'import "a.mojom";'
        '[MooCow=a.E.kFoo]'
        'interface Foo { Foo(); };')
    self.ParseMojoms([a_mojom, b_mojom])
    b = self.LoadModule(b_mojom)
    self.assertEqual(b.interfaces[0].attributes['MooCow'].mojom_name, 'kFoo')

  def testConstantAttributes(self):
    """Verifies that constants as attributes are translated to the constant."""
    a_mojom = 'a.mojom'
    self.WriteFile(
        a_mojom, 'module a;'
        'enum E { kFoo, kBar };'
        'const E kB = E.kFoo;'
        '[Attr=kB] interface Hello { Foo(); };')
    self.ParseMojoms([a_mojom])
    a = self.LoadModule(a_mojom)
    self.assertEqual(a.interfaces[0].attributes['Attr'].mojom_name, 'kB')
    self.assertEquals(a.interfaces[0].attributes['Attr'].value.mojom_name,
                      'kFoo')
