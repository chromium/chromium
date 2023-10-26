# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import importlib.util
import os
import sys
import unittest

def _GetDirAbove(dirname):
  """Returns the directory "above" this file containing |dirname| (which must
  also be "above" this file)."""
  path = os.path.abspath(__file__)
  while True:
    path, tail = os.path.split(path)
    assert tail
    if tail == dirname:
      return path

try:
  importlib.util.find_spec("mojom")
except ImportError:
  sys.path.append(os.path.join(_GetDirAbove('pylib'), 'pylib'))
import mojom.parse.ast as ast
import mojom.parse.conditional_features as conditional_features
import mojom.parse.parser as parser

ENABLED_FEATURES = frozenset({'red', 'green', 'blue'})

class ConditionalFeaturesTest(unittest.TestCase):
  """Tests |mojom.parse.conditional_features|."""

  def parseAndAssertEqual(self, source, expected_source):
    definition = parser.Parse(source, "my_file.mojom")
    conditional_features.RemoveDisabledDefinitions(definition, ENABLED_FEATURES)
    expected = parser.Parse(expected_source, "my_file.mojom")
    self.assertEquals(definition, expected)

  def testFilterConst(self):
    """Test that Consts are correctly filtered."""
    const_source = """
      [EnableIf=blue]
      const int kMyConst1 = 1;
      [EnableIf=orange]
      const double kMyConst2 = 2;
      const int kMyConst3 = 3;
    """
    expected_source = """
      [EnableIf=blue]
      const int kMyConst1 = 1;
      const int kMyConst3 = 3;
    """
    self.parseAndAssertEqual(const_source, expected_source)

  def testFilterIfNotConst(self):
    """Test that Consts are correctly filtered."""
    const_source = """
      [EnableIfNot=blue]
      const int kMyConst1 = 1;
      [EnableIfNot=orange]
      const double kMyConst2 = 2;
      [EnableIf=blue]
      const int kMyConst3 = 3;
      [EnableIfNot=blue]
      const int kMyConst4 = 4;
      [EnableIfNot=purple]
      const int kMyConst5 = 5;
    """
    expected_source = """
      [EnableIfNot=orange]
      const double kMyConst2 = 2;
      [EnableIf=blue]
      const int kMyConst3 = 3;
      [EnableIfNot=purple]
      const int kMyConst5 = 5;
    """
    self.parseAndAssertEqual(const_source, expected_source)

  def testFilterIfNotMultipleConst(self):
    """Test that Consts are correctly filtered."""
    const_source = """
      [EnableIfNot=blue]
      const int kMyConst1 = 1;
      [EnableIfNot=orange]
      const double kMyConst2 = 2;
      [EnableIfNot=orange]
      const int kMyConst3 = 3;
    """
    expected_source = """
      [EnableIfNot=orange]
      const double kMyConst2 = 2;
      [EnableIfNot=orange]
      const int kMyConst3 = 3;
    """
    self.parseAndAssertEqual(const_source, expected_source)

  def testFilterEnum(self):
    """Test that EnumValues are correctly filtered from an Enum."""
    enum_source = """
      enum MyEnum {
        [EnableIf=purple]
        VALUE1,
        [EnableIf=blue]
        VALUE2,
        VALUE3,
      };
    """
    expected_source = """
      enum MyEnum {
        [EnableIf=blue]
        VALUE2,
        VALUE3
      };
    """
    self.parseAndAssertEqual(enum_source, expected_source)

  def testFilterImport(self):
    """Test that imports are correctly filtered from a Mojom."""
    import_source = """
      [EnableIf=blue]
      import "foo.mojom";
      import "bar.mojom";
      [EnableIf=purple]
      import "baz.mojom";
    """
    expected_source = """
      [EnableIf=blue]
      import "foo.mojom";
      import "bar.mojom";
    """
    self.parseAndAssertEqual(import_source, expected_source)

  def testFilterIfNotImport(self):
    """Test that imports are correctly filtered from a Mojom."""
    import_source = """
      [EnableIf=blue]
      import "foo.mojom";
      [EnableIfNot=purple]
      import "bar.mojom";
      [EnableIfNot=green]
      import "baz.mojom";
    """
    expected_source = """
      [EnableIf=blue]
      import "foo.mojom";
      [EnableIfNot=purple]
      import "bar.mojom";
    """
    self.parseAndAssertEqual(import_source, expected_source)

  def testFilterInterface(self):
    """Test that definitions are correctly filtered from an Interface."""
    interface_source = """
      interface MyInterface {
        [EnableIf=blue]
        enum MyEnum {
          [EnableIf=purple]
          VALUE1,
          VALUE2,
        };
        [EnableIf=blue]
        const int32 kMyConst = 123;
        [EnableIf=purple]
        MyMethod();
      };
    """
    expected_source = """
      interface MyInterface {
        [EnableIf=blue]
        enum MyEnum {
          VALUE2,
        };
        [EnableIf=blue]
        const int32 kMyConst = 123;
      };
    """
    self.parseAndAssertEqual(interface_source, expected_source)

  def testFilterMethod(self):
    """Test that Parameters are correctly filtered from a Method."""
    method_source = """
      interface MyInterface {
        [EnableIf=blue]
        MyMethod([EnableIf=purple] int32 a) => ([EnableIf=red] int32 b);
      };
    """
    expected_source = """
      interface MyInterface {
        [EnableIf=blue]
        MyMethod() => ([EnableIf=red] int32 b);
      };
    """
    self.parseAndAssertEqual(method_source, expected_source)

  def testFilterStruct(self):
    """Test that definitions are correctly filtered from a Struct."""
    struct_source = """
      struct MyStruct {
        [EnableIf=blue]
        enum MyEnum {
          VALUE1,
          [EnableIf=purple]
          VALUE2,
        };
        [EnableIf=yellow]
        const double kMyConst = 1.23;
        [EnableIf=green]
        int32 a;
        double b;
        [EnableIf=purple]
        int32 c;
        [EnableIf=blue]
        double d;
        int32 e;
        [EnableIf=orange]
        double f;
      };
    """
    expected_source = """
      struct MyStruct {
        [EnableIf=blue]
        enum MyEnum {
          VALUE1,
        };
        [EnableIf=green]
        int32 a;
        double b;
        [EnableIf=blue]
        double d;
        int32 e;
      };
    """
    self.parseAndAssertEqual(struct_source, expected_source)

  def testFilterIfNotStruct(self):
    """Test that definitions are correctly filtered from a Struct."""
    struct_source = """
      struct MyStruct {
        [EnableIf=blue]
        enum MyEnum {
          VALUE1,
          [EnableIfNot=red]
          VALUE2,
        };
        [EnableIfNot=yellow]
        const double kMyConst = 1.23;
        [EnableIf=green]
        int32 a;
        double b;
        [EnableIfNot=purple]
        int32 c;
        [EnableIf=blue]
        double d;
        int32 e;
        [EnableIfNot=red]
        double f;
      };
    """
    expected_source = """
      struct MyStruct {
        [EnableIf=blue]
        enum MyEnum {
          VALUE1,
        };
        [EnableIfNot=yellow]
        const double kMyConst = 1.23;
        [EnableIf=green]
        int32 a;
        double b;
        [EnableIfNot=purple]
        int32 c;
        [EnableIf=blue]
        double d;
        int32 e;
      };
    """
    self.parseAndAssertEqual(struct_source, expected_source)

  def testFilterUnion(self):
    """Test that UnionFields are correctly filtered from a Union."""
    union_source = """
      union MyUnion {
        [EnableIf=yellow]
        int32 a;
        [EnableIf=red]
        bool b;
      };
    """
    expected_source = """
      union MyUnion {
        [EnableIf=red]
        bool b;
      };
    """
    self.parseAndAssertEqual(union_source, expected_source)

  def testSameNameFields(self):
    mojom_source = """
      enum Foo {
        [EnableIf=red]
        VALUE1 = 5,
        [EnableIf=yellow]
        VALUE1 = 6,
      };
      [EnableIf=red]
      const double kMyConst = 1.23;
      [EnableIf=yellow]
      const double kMyConst = 4.56;
    """
    expected_source = """
      enum Foo {
        [EnableIf=red]
        VALUE1 = 5,
      };
      [EnableIf=red]
      const double kMyConst = 1.23;
    """
    self.parseAndAssertEqual(mojom_source, expected_source)

  def testFeaturesWithEnableIf(self):
    mojom_source = """
      feature Foo {
        const string name = "FooFeature";
        [EnableIf=red]
        const bool default_state = false;
        [EnableIf=yellow]
        const bool default_state = true;
      };
    """
    expected_source = """
      feature Foo {
        const string name = "FooFeature";
        [EnableIf=red]
        const bool default_state = false;
      };
    """
    self.parseAndAssertEqual(mojom_source, expected_source)

  def testMultipleEnableIfs(self):
    source = """
      enum Foo {
        [EnableIf=red,EnableIf=yellow]
        kBarValue = 5,
      };
    """
    definition = parser.Parse(source, "my_file.mojom")
    self.assertRaises(conditional_features.EnableIfError,
                      conditional_features.RemoveDisabledDefinitions,
                      definition, ENABLED_FEATURES)

  def testMultipleEnableIfs(self):
    source = """
      enum Foo {
        [EnableIf=red,EnableIfNot=yellow]
        kBarValue = 5,
      };
    """
    definition = parser.Parse(source, "my_file.mojom")
    self.assertRaises(conditional_features.EnableIfError,
                      conditional_features.RemoveDisabledDefinitions,
                      definition, ENABLED_FEATURES)

  def testMultipleEnableIfs(self):
    source = """
      enum Foo {
        [EnableIfNot=red,EnableIfNot=yellow]
        kBarValue = 5,
      };
    """
    definition = parser.Parse(source, "my_file.mojom")
    self.assertRaises(conditional_features.EnableIfError,
                      conditional_features.RemoveDisabledDefinitions,
                      definition, ENABLED_FEATURES)

if __name__ == '__main__':
  unittest.main()
