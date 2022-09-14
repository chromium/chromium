# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from mojom_parser_test_case import MojomParserTestCase
from mojom.generate import module as mojom


class ConstTest(MojomParserTestCase):
  """Tests constant parsing behavior."""

  def testLiteralInt(self):
    a_mojom = 'a.mojom'
    self.WriteFile(a_mojom, 'const int32 k = 42;')
    self.ParseMojoms([a_mojom])
    a = self.LoadModule(a_mojom)
    self.assertEqual(1, len(a.constants))
    self.assertEqual('k', a.constants[0].mojom_name)
    self.assertEqual('42', a.constants[0].value)

  def testLiteralFloat(self):
    a_mojom = 'a.mojom'
    self.WriteFile(a_mojom, 'const float k = 42.5;')
    self.ParseMojoms([a_mojom])
    a = self.LoadModule(a_mojom)
    self.assertEqual(1, len(a.constants))
    self.assertEqual('k', a.constants[0].mojom_name)
    self.assertEqual('42.5', a.constants[0].value)

  def testLiteralString(self):
    a_mojom = 'a.mojom'
    self.WriteFile(a_mojom, 'const string k = "woot";')
    self.ParseMojoms([a_mojom])
    a = self.LoadModule(a_mojom)
    self.assertEqual(1, len(a.constants))
    self.assertEqual('k', a.constants[0].mojom_name)
    self.assertEqual('"woot"', a.constants[0].value)

  def testEnumConstant(self):
    a_mojom = 'a.mojom'
    self.WriteFile(a_mojom, 'module a; enum E { kA = 41, kB };')
    b_mojom = 'b.mojom'
    self.WriteFile(
        b_mojom, """\
      import "a.mojom";
      const a.E kE1 = a.E.kB;

      // We also allow value names to be unqualified, implying scope from the
      // constant's type.
      const a.E kE2 = kB;
      """)
    self.ParseMojoms([a_mojom, b_mojom])
    a = self.LoadModule(a_mojom)
    b = self.LoadModule(b_mojom)
    self.assertEqual(1, len(a.enums))
    self.assertEqual('E', a.enums[0].mojom_name)
    self.assertEqual(2, len(b.constants))
    self.assertEqual('kE1', b.constants[0].mojom_name)
    self.assertEqual(a.enums[0], b.constants[0].kind)
    self.assertEqual(a.enums[0].fields[1], b.constants[0].value.field)
    self.assertEqual(42, b.constants[0].value.field.numeric_value)
    self.assertEqual('kE2', b.constants[1].mojom_name)
    self.assertEqual(a.enums[0].fields[1], b.constants[1].value.field)
    self.assertEqual(42, b.constants[1].value.field.numeric_value)

  def testConstantReference(self):
    a_mojom = 'a.mojom'
    self.WriteFile(a_mojom, 'const int32 kA = 42; const int32 kB = kA;')
    self.ParseMojoms([a_mojom])
    a = self.LoadModule(a_mojom)
    self.assertEqual(2, len(a.constants))
    self.assertEqual('kA', a.constants[0].mojom_name)
    self.assertEqual('42', a.constants[0].value)
    self.assertEqual('kB', a.constants[1].mojom_name)
    self.assertEqual('42', a.constants[1].value)

  def testImportedConstantReference(self):
    a_mojom = 'a.mojom'
    self.WriteFile(a_mojom, 'const int32 kA = 42;')
    b_mojom = 'b.mojom'
    self.WriteFile(b_mojom, 'import "a.mojom"; const int32 kB = kA;')
    self.ParseMojoms([a_mojom, b_mojom])
    a = self.LoadModule(a_mojom)
    b = self.LoadModule(b_mojom)
    self.assertEqual(1, len(a.constants))
    self.assertEqual(1, len(b.constants))
    self.assertEqual('kA', a.constants[0].mojom_name)
    self.assertEqual('42', a.constants[0].value)
    self.assertEqual('kB', b.constants[0].mojom_name)
    self.assertEqual('42', b.constants[0].value)
