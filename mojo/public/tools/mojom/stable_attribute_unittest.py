# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from mojom_parser_test_case import MojomParserTestCase

from mojom.generate import module


class StableAttributeTest(MojomParserTestCase):
  """Tests covering usage of the [Stable] attribute."""

  def testStableAttributeTagging(self):
    """Verify that we recognize the [Stable] attribute on relevant definitions
    and the resulting parser outputs are tagged accordingly."""
    mojom = 'test.mojom'
    self.WriteFile(
        mojom, """\
        [Stable] enum TestEnum { kFoo };
        enum UnstableEnum { kBar };
        [Stable] struct TestStruct { TestEnum a; };
        struct UnstableStruct { UnstableEnum a; };
        [Stable] union TestUnion { TestEnum a; TestStruct b; };
        union UnstableUnion { UnstableEnum a; UnstableStruct b; };
        [Stable] interface TestInterface { Foo@0(TestUnion x) => (); };
        interface UnstableInterface { Foo(UnstableUnion x) => (); };
        """)
    self.ParseMojoms([mojom])

    m = self.LoadModule(mojom)
    self.assertEqual(2, len(m.enums))
    self.assertTrue(m.enums[0].stable)
    self.assertFalse(m.enums[1].stable)
    self.assertEqual(2, len(m.structs))
    self.assertTrue(m.structs[0].stable)
    self.assertFalse(m.structs[1].stable)
    self.assertEqual(2, len(m.unions))
    self.assertTrue(m.unions[0].stable)
    self.assertFalse(m.unions[1].stable)
    self.assertEqual(2, len(m.interfaces))
    self.assertTrue(m.interfaces[0].stable)
    self.assertFalse(m.interfaces[1].stable)

  def testStableStruct(self):
    """A [Stable] struct is valid if all its fields are also stable."""
    self.ExtractTypes('[Stable] struct S {};')
    self.ExtractTypes('[Stable] struct S { int32 x; bool b; };')
    self.ExtractTypes('[Stable] enum E { A }; [Stable] struct S { E e; };')
    self.ExtractTypes('[Stable] struct S {}; [Stable] struct T { S s; };')
    self.ExtractTypes(
        '[Stable] struct S {}; [Stable] struct T { array<S> ss; };')
    self.ExtractTypes(
        '[Stable] interface F {}; [Stable] struct T { pending_remote<F> f; };')

    with self.assertRaisesRegexp(Exception, 'because it depends on E'):
      self.ExtractTypes('enum E { A }; [Stable] struct S { E e; };')
    with self.assertRaisesRegexp(Exception, 'because it depends on X'):
      self.ExtractTypes('struct X {}; [Stable] struct S { X x; };')
    with self.assertRaisesRegexp(Exception, 'because it depends on T'):
      self.ExtractTypes('struct T {}; [Stable] struct S { array<T> xs; };')
    with self.assertRaisesRegexp(Exception, 'because it depends on T'):
      self.ExtractTypes('struct T {}; [Stable] struct S { map<int32, T> xs; };')
    with self.assertRaisesRegexp(Exception, 'because it depends on T'):
      self.ExtractTypes('struct T {}; [Stable] struct S { map<T, int32> xs; };')
    with self.assertRaisesRegexp(Exception, 'because it depends on F'):
      self.ExtractTypes(
          'interface F {}; [Stable] struct S { pending_remote<F> f; };')
    with self.assertRaisesRegexp(Exception, 'because it depends on F'):
      self.ExtractTypes(
          'interface F {}; [Stable] struct S { pending_receiver<F> f; };')

  def testStableUnion(self):
    """A [Stable] union is valid if all its fields' types are also stable."""
    self.ExtractTypes('[Stable] union U {};')
    self.ExtractTypes('[Stable] union U { int32 x; bool b; };')
    self.ExtractTypes('[Stable] enum E { A }; [Stable] union U { E e; };')
    self.ExtractTypes('[Stable] struct S {}; [Stable] union U { S s; };')
    self.ExtractTypes(
        '[Stable] struct S {}; [Stable] union U { array<S> ss; };')
    self.ExtractTypes(
        '[Stable] interface F {}; [Stable] union U { pending_remote<F> f; };')

    with self.assertRaisesRegexp(Exception, 'because it depends on E'):
      self.ExtractTypes('enum E { A }; [Stable] union U { E e; };')
    with self.assertRaisesRegexp(Exception, 'because it depends on X'):
      self.ExtractTypes('struct X {}; [Stable] union U { X x; };')
    with self.assertRaisesRegexp(Exception, 'because it depends on T'):
      self.ExtractTypes('struct T {}; [Stable] union U { array<T> xs; };')
    with self.assertRaisesRegexp(Exception, 'because it depends on T'):
      self.ExtractTypes('struct T {}; [Stable] union U { map<int32, T> xs; };')
    with self.assertRaisesRegexp(Exception, 'because it depends on T'):
      self.ExtractTypes('struct T {}; [Stable] union U { map<T, int32> xs; };')
    with self.assertRaisesRegexp(Exception, 'because it depends on F'):
      self.ExtractTypes(
          'interface F {}; [Stable] union U { pending_remote<F> f; };')
    with self.assertRaisesRegexp(Exception, 'because it depends on F'):
      self.ExtractTypes(
          'interface F {}; [Stable] union U { pending_receiver<F> f; };')

  def testStableInterface(self):
    """A [Stable] interface is valid if all its methods' parameter types are
    stable, including response parameters where applicable."""
    self.ExtractTypes('[Stable] interface F {};')
    self.ExtractTypes('[Stable] interface F { A@0(int32 x); };')
    self.ExtractTypes('[Stable] interface F { A@0(int32 x) => (bool b); };')
    self.ExtractTypes("""\
        [Stable] enum E { A, B, C };
        [Stable] struct S {};
        [Stable] interface F { A@0(E e, S s) => (bool b, array<S> s); };
        """)

    with self.assertRaisesRegexp(Exception, 'because it depends on E'):
      self.ExtractTypes(
          'enum E { A, B, C }; [Stable] interface F { A@0(E e); };')
    with self.assertRaisesRegexp(Exception, 'because it depends on E'):
      self.ExtractTypes(
          'enum E { A, B, C }; [Stable] interface F { A@0(int32 x) => (E e); };'
      )
    with self.assertRaisesRegexp(Exception, 'because it depends on S'):
      self.ExtractTypes(
          'struct S {}; [Stable] interface F { A@0(int32 x) => (S s); };')
    with self.assertRaisesRegexp(Exception, 'because it depends on S'):
      self.ExtractTypes(
          'struct S {}; [Stable] interface F { A@0(S s) => (bool b); };')

    with self.assertRaisesRegexp(Exception, 'explicit method ordinals'):
      self.ExtractTypes('[Stable] interface F { A() => (); };')
