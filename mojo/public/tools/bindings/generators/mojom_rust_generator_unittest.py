# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
import sys
import os


# Mocking the mojom objects for testing _GetQualifiedName
class MockModule:
  def __init__(self, path):
    self.path = path

  def GetNamespacePrefix(self):
    return ""


class MockKind:
  def __init__(self, name, module, parent_kind=None):
    self.name = name
    self.mojom_name = name
    self.module = module
    self.parent_kind = parent_kind

  @property
  def qualified_name(self):
    if hasattr(self, 'parent_kind') and self.parent_kind:
      return self.parent_kind.qualified_name + "." + self.mojom_name
    return self.mojom_name


# Import the functions to test
from generators import mojom_rust_generator


class TestGetQualifiedName(unittest.TestCase):
  def setUp(self):
    self.source_to_target_map = {
      'a.mojom': '//a:a',
      'b.mojom': '//b:b',
    }
    self.mod_a = MockModule('a.mojom')
    self.mod_b = MockModule('b.mojom')

  def test_get_local_name_top_level(self):
    ty = MockKind('MyStruct', self.mod_a)
    self.assertEqual(mojom_rust_generator._GetLocalName(ty), 'MyStruct')

  def test_get_local_name_nested(self):
    parent = MockKind('MyStruct', self.mod_a)
    ty = MockKind('MyEnum', self.mod_a, parent_kind=parent)
    self.assertEqual(mojom_rust_generator._GetLocalName(ty), 'MyStruct_MyEnum')

  def test_get_local_name_double_nested(self):
    gp = MockKind('GP', self.mod_a)
    p = MockKind('P', self.mod_a, parent_kind=gp)
    ty = MockKind('C', self.mod_a, parent_kind=p)
    self.assertEqual(mojom_rust_generator._GetLocalName(ty), 'GP_P_C')

  def test_top_level_same_file(self):
    ty = MockKind('MyStruct', self.mod_a)
    name = mojom_rust_generator._GetQualifiedName(
      ty, self.mod_a, self.source_to_target_map
    )
    self.assertEqual(name, 'MyStruct')

  def test_nested_same_file(self):
    parent = MockKind('MyStruct', self.mod_a)
    ty = MockKind('MyEnum', self.mod_a, parent_kind=parent)
    name = mojom_rust_generator._GetQualifiedName(
      ty, self.mod_a, self.source_to_target_map
    )
    self.assertEqual(name, 'MyStruct_MyEnum')

  def test_top_level_different_file_same_target(self):
    source_to_target_map = {
      'a.mojom': '//a:target',
      'b.mojom': '//a:target',
    }
    ty = MockKind('MyStruct', self.mod_b)
    name = mojom_rust_generator._GetQualifiedName(
      ty, self.mod_a, source_to_target_map
    )
    self.assertEqual(name, 'crate::b::MyStruct')

  def test_nested_different_file_same_target(self):
    source_to_target_map = {
      'a.mojom': '//a:target',
      'b.mojom': '//a:target',
    }
    parent = MockKind('MyStruct', self.mod_b)
    ty = MockKind('MyEnum', self.mod_b, parent_kind=parent)
    name = mojom_rust_generator._GetQualifiedName(
      ty, self.mod_a, source_to_target_map
    )
    self.assertEqual(name, 'crate::b::MyStruct_MyEnum')

  def test_top_level_different_target(self):
    ty = MockKind('MyStruct', self.mod_b)
    name = mojom_rust_generator._GetQualifiedName(
      ty, self.mod_a, self.source_to_target_map
    )
    self.assertEqual(name, 'b_b::b::MyStruct')

  def test_nested_different_target(self):
    parent = MockKind('MyStruct', self.mod_b)
    ty = MockKind('MyEnum', self.mod_b, parent_kind=parent)
    name = mojom_rust_generator._GetQualifiedName(
      ty, self.mod_a, self.source_to_target_map
    )
    self.assertEqual(name, 'b_b::b::MyStruct_MyEnum')


class TestMojomTypeToRustTypeUniversalBoxing(unittest.TestCase):
  def setUp(self):
    self.source_to_target_map = {
      'a.mojom': '//a:a',
    }
    self.mojom = mojom_rust_generator.mojom
    self.mod_a = self.mojom.Module('a.mojom', 'a')

  def test_nullable_struct_boxed(self):
    s = self.mojom.Struct('Foo', module=self.mod_a)
    s.name = 'Foo'
    rust_ty = mojom_rust_generator._MojomTypeToRustType(
      s.MakeNullableKind(), self.mod_a, self.source_to_target_map, {}
    )
    self.assertEqual(rust_ty, 'Option<Box<Foo>>')

  def test_non_nullable_struct_not_boxed(self):
    s = self.mojom.Struct('Foo', module=self.mod_a)
    s.name = 'Foo'
    rust_ty = mojom_rust_generator._MojomTypeToRustType(
      s, self.mod_a, self.source_to_target_map, {}
    )
    self.assertEqual(rust_ty, 'Foo')

  def test_nullable_union_boxed(self):
    u = self.mojom.Union('MyUnion', module=self.mod_a)
    u.name = 'MyUnion'
    rust_ty = mojom_rust_generator._MojomTypeToRustType(
      u.MakeNullableKind(), self.mod_a, self.source_to_target_map, {}
    )
    self.assertEqual(rust_ty, 'Option<Box<MyUnion>>')

  def test_nullable_primitive_not_boxed(self):
    rust_ty = mojom_rust_generator._MojomTypeToRustType(
      self.mojom.INT32.MakeNullableKind(),
      self.mod_a,
      self.source_to_target_map,
      {},
    )
    self.assertEqual(rust_ty, 'Option<i32>')

  def test_nullable_array_not_boxed(self):
    arr = self.mojom.Array(self.mojom.INT32)
    rust_ty = mojom_rust_generator._MojomTypeToRustType(
      arr.MakeNullableKind(), self.mod_a, self.source_to_target_map, {}
    )
    self.assertEqual(rust_ty, 'Option<Vec<i32>>')

  def test_array_of_nullable_struct_elements_boxed(self):
    s = self.mojom.Struct('Foo', module=self.mod_a)
    s.name = 'Foo'
    arr = self.mojom.Array(s.MakeNullableKind())
    rust_ty = mojom_rust_generator._MojomTypeToRustType(
      arr, self.mod_a, self.source_to_target_map, {}
    )
    self.assertEqual(rust_ty, 'Vec<Option<Box<Foo>>>')


if __name__ == '__main__':
  unittest.main()
