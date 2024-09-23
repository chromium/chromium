# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from mojom.generate import module
from mojom.generate import compatibility_checker
from mojom_parser_test_case import MojomParserTestCase


class VersionCompatibilityTest(MojomParserTestCase):
  """Tests covering compatibility between two versions of the same mojom type
  definition. This coverage ensures that we can reliably detect unsafe changes
  to definitions that are expected to tolerate version skew in production
  environments."""

  def _GetTypeCompatibilityMap(self, old_mojom, new_mojom):
    """Helper to support the implementation of assertBackwardCompatible and
    assertNotBackwardCompatible."""

    old = self.ExtractTypes(old_mojom)
    new = self.ExtractTypes(new_mojom)
    self.assertEqual(set(old.keys()), set(new.keys()),
                     'Old and new test mojoms should use the same type names.')

    checker = compatibility_checker.BackwardCompatibilityChecker()
    compatibility_map = {}
    errors = []
    for name in old:
      try:
        compatibility_map[name] = checker.IsBackwardCompatible(
            new[name], old[name])
      except compatibility_checker.CompatibilityError as e:
        compatibility_map[name] = False
        errors.append(e)
    return compatibility_map, errors

  def assertBackwardCompatible(self, old_mojom, new_mojom):
    compatibility_map, errors = self._GetTypeCompatibilityMap(
        old_mojom, new_mojom)
    for name, compatible in compatibility_map.items():
      if not compatible:
        raise AssertionError(
            'Given the old mojom:\n\n    %s\n\nand the new mojom:\n\n    %s\n\n'
            'The new definition of %s should pass a backward-compatibiity '
            'check, but it does not. Errors: %s' %
            (old_mojom, new_mojom, name, errors))

  def assertNotBackwardCompatible(self, old_mojom, new_mojom):
    compatibility_map, _ = self._GetTypeCompatibilityMap(old_mojom, new_mojom)
    if all(compatibility_map.values()):
      raise AssertionError(
          'Given the old mojom:\n\n    %s\n\nand the new mojom:\n\n    %s\n\n'
          'The new mojom should fail a backward-compatibility check, but it '
          'does not.' % (old_mojom, new_mojom))

  def testNewNonExtensibleEnumValue(self):
    """Adding a value to a non-extensible enum breaks backward-compatibility."""
    self.assertNotBackwardCompatible('enum E { kFoo, kBar };',
                                     'enum E { kFoo, kBar, kBaz };')

  def testNewNonExtensibleEnumValueWithMinVersion(self):
    """Adding a value to a non-extensible enum breaks backward-compatibility,
    even with a new [MinVersion] specified for the value."""
    self.assertNotBackwardCompatible(
        'enum E { kFoo, kBar };', 'enum E { kFoo, kBar, [MinVersion=1] kBaz };')

  def testNewValueInExistingVersion(self):
    """Adding a value to an existing version is not allowed, even if the old
    enum was marked [Extensible]. Note that it is irrelevant whether or not the
    new enum is marked [Extensible]."""
    self.assertNotBackwardCompatible(
        '[Extensible] enum E { [Default] kFoo, kBar };',
        'enum E { kFoo, kBar, kBaz };')
    self.assertNotBackwardCompatible(
        '[Extensible] enum E { [Default] kFoo, kBar };',
        '[Extensible] enum E { [Default] kFoo, kBar, kBaz };')
    self.assertNotBackwardCompatible(
        '[Extensible] enum E { [Default] kFoo, [MinVersion=1] kBar };',
        'enum E { kFoo, [MinVersion=1] kBar, [MinVersion=1] kBaz };')

  def testEnumValueRemoval(self):
    """Removal of an enum value is never valid even for [Extensible] enums."""
    self.assertNotBackwardCompatible('enum E { kFoo, kBar };',
                                     'enum E { kFoo };')
    self.assertNotBackwardCompatible(
        '[Extensible] enum E { [Default] kFoo, kBar };',
        '[Extensible] enum E { [Default] kFoo };')
    self.assertNotBackwardCompatible(
        '[Extensible] enum E { [Default] kA, [MinVersion=1] kB };',
        '[Extensible] enum E { [Default] kA, };')
    self.assertNotBackwardCompatible(
        """[Extensible] enum E {
          [Default] kA,
          [MinVersion=1] kB,
          [MinVersion=1] kZ };""",
        '[Extensible] enum E { [Default] kA, [MinVersion=1] kB };')

  def testNewExtensibleEnumValueWithMinVersion(self):
    """Adding a new and properly [MinVersion]'d value to an [Extensible] enum
    is a backward-compatible change. Note that it is irrelevant whether or not
    the new enum is marked [Extensible]."""
    self.assertBackwardCompatible('[Extensible] enum E { [Default] kA, kB };',
                                  'enum E { kA, kB, [MinVersion=1] kC };')
    self.assertBackwardCompatible(
        '[Extensible] enum E { [Default] kA, kB };',
        '[Extensible] enum E { [Default] kA, kB, [MinVersion=1] kC };')
    self.assertBackwardCompatible(
        '[Extensible] enum E { [Default] kA, [MinVersion=1] kB };',
        """[Extensible] enum E {
          [Default] kA,
          [MinVersion=1] kB,
          [MinVersion=2] kC };""")

  def testRenameEnumValue(self):
    """Renaming an enum value does not affect backward-compatibility. Only
    numeric value is relevant."""
    self.assertBackwardCompatible('enum E { kA, kB };', 'enum E { kX, kY };')

  def testAddEnumValueAlias(self):
    """Adding new enum fields does not affect backward-compatibility if it does
    not introduce any new numeric values."""
    self.assertBackwardCompatible(
        'enum E { kA, kB };', 'enum E { kA, kB, kC = kA, kD = 1, kE = kD };')

  def testEnumIdentity(self):
    """An unchanged enum is obviously backward-compatible."""
    self.assertBackwardCompatible('enum E { kA, kB, kC };',
                                  'enum E { kA, kB, kC };')

  def testNewStructFieldUnversioned(self):
    """Adding a new field to a struct without a new (i.e. higher than any
    existing version) [MinVersion] tag breaks backward-compatibility."""
    self.assertNotBackwardCompatible('struct S { string a; };',
                                     'struct S { string a; string b; };')

  def testStructFieldRemoval(self):
    """Removing a field from a struct breaks backward-compatibility."""
    self.assertNotBackwardCompatible('struct S { string a; string b; };',
                                     'struct S { string a; };')

  def testStructFieldTypeChange(self):
    """Changing the type of an existing field always breaks
    backward-compatibility."""
    self.assertNotBackwardCompatible('struct S { string a; };',
                                     'struct S { array<int32> a; };')

  def testStructFieldBecomingOptional(self):
    """Changing a field from non-optional to optional breaks
    backward-compatibility."""
    self.assertNotBackwardCompatible('struct S { string a; };',
                                     'struct S { string? a; };')

  def testStructFieldBecomingNonOptional(self):
    """Changing a field from optional to non-optional breaks
    backward-compatibility."""
    self.assertNotBackwardCompatible('struct S { string? a; };',
                                     'struct S { string a; };')

  def testStructFieldOrderChange(self):
    """Changing the order of fields breaks backward-compatibility."""
    self.assertNotBackwardCompatible('struct S { string a; bool b; };',
                                     'struct S { bool b; string a; };')
    self.assertNotBackwardCompatible('struct S { string a@0; bool b@1; };',
                                     'struct S { string a@1; bool b@0; };')

  def testStructFieldMinVersionChange(self):
    """Changing the MinVersion of a field breaks backward-compatibility."""
    self.assertNotBackwardCompatible(
        'struct S { string a; [MinVersion=1] string? b; };',
        'struct S { string a; [MinVersion=2] string? b; };')

  def testStructFieldTypeChange(self):
    """If a struct field's own type definition changes, the containing struct
    is backward-compatible if and only if the field type's change is
    backward-compatible."""
    self.assertBackwardCompatible(
        'struct S {}; struct T { S s; };',
        'struct S { [MinVersion=1] int32 x; }; struct T { S s; };')
    self.assertBackwardCompatible(
        '[Extensible] enum E { [Default] kA }; struct S { E e; };',
        """[Extensible] enum E {
          [Default] kA,
          [MinVersion=1] kB };
          struct S { E e; };""")
    self.assertNotBackwardCompatible(
        'struct S {}; struct T { S s; };',
        'struct S { int32 x; }; struct T { S s; };')
    self.assertNotBackwardCompatible(
        '[Extensible] enum E { [Default] kA }; struct S { E e; };',
        '[Extensible] enum E { [Default] kA, kB }; struct S { E e; };')

  def testNewStructFieldWithInvalidMinVersion(self):
    """Adding a new field using an existing MinVersion breaks backward-
    compatibility."""
    self.assertNotBackwardCompatible(
        """\
        struct S {
          string a;
          [MinVersion=1] string? b;
        };
        """, """\
        struct S {
          string a;
          [MinVersion=1] string? b;
          [MinVersion=1] string? c;
        };""")

  def testNewStructFieldWithValidMinVersion(self):
    """Adding a new field is safe if tagged with a MinVersion greater than any
    previously used MinVersion in the struct."""
    self.assertBackwardCompatible(
        'struct S { int32 a; };',
        'struct S { int32 a; [MinVersion=1] int32 b; };')
    self.assertBackwardCompatible(
        'struct S { int32 a; [MinVersion=1] int32 b; };',
        'struct S { int32 a; [MinVersion=1] int32 b; [MinVersion=2] bool c; };')

  def testNewStructFieldNullableReference(self):
    """Adding a new nullable reference-typed field is fine if versioned
    properly."""
    self.assertBackwardCompatible(
        'struct S { int32 a; };',
        'struct S { int32 a; [MinVersion=1] string? b; };')

  def testStructFieldRename(self):
    """Renaming a field has no effect on backward-compatibility."""
    self.assertBackwardCompatible('struct S { int32 x; bool b; };',
                                  'struct S { int32 a; bool b; };')

  def testStructFieldReorderWithExplicitOrdinals(self):
    """Reordering fields has no effect on backward-compatibility when field
    ordinals are explicitly labeled and remain unchanged."""
    self.assertBackwardCompatible('struct S { bool b@1; int32 a@0; };',
                                  'struct S { int32 a@0; bool b@1; };')

  def testNewUnionFieldUnversioned(self):
    """Adding a new field to a union without a new (i.e. higher than any
    existing version) [MinVersion] tag breaks backward-compatibility."""
    self.assertNotBackwardCompatible('union U { string a; };',
                                     'union U { string a; string b; };')

  def testUnionFieldRemoval(self):
    """Removing a field from a union breaks backward-compatibility."""
    self.assertNotBackwardCompatible('union U { string a; string b; };',
                                     'union U { string a; };')

  def testUnionFieldTypeChange(self):
    """Changing the type of an existing field always breaks
    backward-compatibility."""
    self.assertNotBackwardCompatible('union U { string a; };',
                                     'union U { array<int32> a; };')

  def testUnionFieldBecomingOptional(self):
    """Changing a field from non-optional to optional breaks
    backward-compatibility."""
    self.assertNotBackwardCompatible('union U { string a; };',
                                     'union U { string? a; };')

  def testFieldNestedTypeChanged(self):
    """Changing the definition of a nested type within a field (such as an array
    element or interface endpoint type) should only break backward-compatibility
    if the changes to that type are not backward-compatible."""
    self.assertBackwardCompatible(
        """\
        struct S { string a; };
        struct T { array<S> ss; };
        """, """\
        struct S {
          string a;
          [MinVersion=1] string? b;
        };
        struct T { array<S> ss; };
        """)
    self.assertBackwardCompatible(
        """\
        interface F { Do(); };
        struct S { pending_receiver<F> r; };
        """, """\
        interface F {
          Do();
          [MinVersion=1] Say();
        };
        struct S { pending_receiver<F> r; };
        """)

  def testRecursiveTypeChange(self):
    """Recursive types do not break the compatibility checker."""
    self.assertBackwardCompatible(
        """\
        struct S {
          string a;
          array<S> others;
        };""", """\
        struct S {
          string a;
          array<S> others;
          [MinVersion=1] string? b;
        };""")

  def testUnionFieldBecomingNonOptional(self):
    """Changing a field from optional to non-optional breaks
    backward-compatibility."""
    self.assertNotBackwardCompatible('union U { string? a; };',
                                     'union U { string a; };')

  def testUnionFieldOrderChange(self):
    """Changing the order of fields breaks backward-compatibility."""
    self.assertNotBackwardCompatible('union U { string a; bool b; };',
                                     'union U { bool b; string a; };')
    self.assertNotBackwardCompatible('union U { string a@0; bool b@1; };',
                                     'union U { string a@1; bool b@0; };')

  def testUnionFieldMinVersionChange(self):
    """Changing the MinVersion of a field breaks backward-compatibility."""
    self.assertNotBackwardCompatible(
        'union U { string a; [MinVersion=1] string b; };',
        'union U { string a; [MinVersion=2] string b; };')

  def testUnionFieldTypeChange(self):
    """If a union field's own type definition changes, the containing union
    is backward-compatible if and only if the field type's change is
    backward-compatible."""
    self.assertBackwardCompatible(
        'struct S {}; union U { S s; };',
        'struct S { [MinVersion=1] int32 x; }; union U { S s; };')
    self.assertBackwardCompatible(
        '[Extensible] enum E { [Default] kA }; union U { E e; };',
        """[Extensible] enum E {
          [Default] kA,
          [MinVersion=1] kB };
          union U { E e; };""")
    self.assertNotBackwardCompatible(
        'struct S {}; union U { S s; };',
        'struct S { int32 x; }; union U { S s; };')
    self.assertNotBackwardCompatible(
        '[Extensible] enum E { [Default] kA }; union U { E e; };',
        '[Extensible] enum E { [Default] kA, kB }; union U { E e; };')

  def testNewUnionFieldWithInvalidMinVersion(self):
    """Adding a new field using an existing MinVersion breaks backward-
    compatibility."""
    self.assertNotBackwardCompatible(
        """\
        union U {
          string a;
          [MinVersion=1] string b;
        };
        """, """\
        union U {
          string a;
          [MinVersion=1] string b;
          [MinVersion=1] string c;
        };""")

  def testNewUnionFieldWithValidMinVersion(self):
    """Adding a new field is safe if tagged with a MinVersion greater than any
    previously used MinVersion in the union."""
    self.assertBackwardCompatible(
        'union U { int32 a; };',
        'union U { int32 a; [MinVersion=1] int32 b; };')
    self.assertBackwardCompatible(
        'union U { int32 a; [MinVersion=1] int32 b; };',
        'union U { int32 a; [MinVersion=1] int32 b; [MinVersion=2] bool c; };')

  def testUnionFieldRename(self):
    """Renaming a field has no effect on backward-compatibility."""
    self.assertBackwardCompatible('union U { int32 x; bool b; };',
                                  'union U { int32 a; bool b; };')

  def testUnionFieldReorderWithExplicitOrdinals(self):
    """Reordering fields has no effect on backward-compatibility when field
    ordinals are explicitly labeled and remain unchanged."""
    self.assertBackwardCompatible('union U { bool b@1; int32 a@0; };',
                                  'union U { int32 a@0; bool b@1; };')

  def testNewInterfaceMethodUnversioned(self):
    """Adding a new method to an interface without a new (i.e. higher than any
    existing version) [MinVersion] tag breaks backward-compatibility."""
    self.assertNotBackwardCompatible('interface F { A(); };',
                                     'interface F { A(); B(); };')

  def testInterfaceMethodRemoval(self):
    """Removing a method from an interface breaks backward-compatibility."""
    self.assertNotBackwardCompatible('interface F { A(); B(); };',
                                     'interface F { A(); };')

  def testInterfaceMethodParamsChanged(self):
    """Changes to the parameter list are only backward-compatible if they meet
    backward-compatibility requirements of an equivalent struct definition."""
    self.assertNotBackwardCompatible('interface F { A(); };',
                                     'interface F { A(int32 x); };')
    self.assertNotBackwardCompatible('interface F { A(int32 x); };',
                                     'interface F { A(bool x); };')
    self.assertNotBackwardCompatible(
        'interface F { A(int32 x, [MinVersion=1] string? s); };', """\
        interface F {
          A(int32 x, [MinVersion=1] string? s, [MinVersion=1] int32 y);
        };""")

    self.assertBackwardCompatible('interface F { A(int32 x); };',
                                  'interface F { A(int32 a); };')
    self.assertBackwardCompatible(
        'interface F { A(int32 x); };',
        'interface F { A(int32 x, [MinVersion=1] string? s); };')

    self.assertBackwardCompatible(
        'struct S {}; interface F { A(S s); };',
        'struct S { [MinVersion=1] int32 x; }; interface F { A(S s); };')
    self.assertBackwardCompatible(
        'struct S {}; struct T {}; interface F { A(S s); };',
        'struct S {}; struct T {}; interface F { A(T s); };')
    self.assertNotBackwardCompatible(
        'struct S {}; struct T { int32 x; }; interface F { A(S s); };',
        'struct S {}; struct T { int32 x; }; interface F { A(T t); };')

  def testInterfaceMethodReplyAdded(self):
    """Adding a reply to a message breaks backward-compatibilty."""
    self.assertNotBackwardCompatible('interface F { A(); };',
                                     'interface F { A() => (); };')

  def testInterfaceMethodReplyRemoved(self):
    """Removing a reply from a message breaks backward-compatibility."""
    self.assertNotBackwardCompatible('interface F { A() => (); };',
                                     'interface F { A(); };')

  def testInterfaceMethodReplyParamsChanged(self):
    """Similar to request parameters, a change to reply parameters is considered
    backward-compatible if it meets the same backward-compatibility
    requirements imposed on equivalent struct changes."""
    self.assertNotBackwardCompatible('interface F { A() => (); };',
                                     'interface F { A() => (int32 x); };')
    self.assertNotBackwardCompatible('interface F { A() => (int32 x); };',
                                     'interface F { A() => (); };')
    self.assertNotBackwardCompatible('interface F { A() => (bool x); };',
                                     'interface F { A() => (int32 x); };')

    self.assertBackwardCompatible('interface F { A() => (int32 a); };',
                                  'interface F { A() => (int32 x); };')
    self.assertBackwardCompatible(
        'interface F { A() => (int32 x); };',
        'interface F { A() => (int32 x, [MinVersion] string? s); };')

  def testNewInterfaceMethodWithInvalidMinVersion(self):
    """Adding a new method to an existing version is not backward-compatible."""
    self.assertNotBackwardCompatible(
        """\
        interface F {
          A();
          [MinVersion=1] B();
        };
        """, """\
        interface F {
          A();
          [MinVersion=1] B();
          [MinVersion=1] C();
        };
        """)

  def testNewInterfaceMethodWithValidMinVersion(self):
    """Adding a new method is fine as long as its MinVersion exceeds that of any
    method on the old interface definition."""
    self.assertBackwardCompatible('interface F { A(); };',
                                  'interface F { A(); [MinVersion=1] B(); };')

  def testNullableTypeLayoutCompatibility(self):
    """Nullable value types are backwards (layout) compatible if they follow the
    pattern:
      struct Foo {
        bool has_field;
        int32 field;
      };
    Note that |field|'s ordinal order must immediately follow |has_field|.
    Therefore, the following is also valid:
      struct Foo {
        int32 field@1;
        bool has_field@0;
      };
    This is because |field|'s ordinal order immediately follows |has_field|.

    The following is NOT backwards compatible:
      struct Foo {
        bool has_field@1;
        int32 field@0;
      };
    This is because |field|'s ordinal ordering does not immediately follow
    |has_field|."""
    self.assertBackwardCompatible('struct S { bool has_foo; int32 foo; };',
                                  'struct S { int32? foo; };')
    self.assertNotBackwardCompatible(
        'struct S { bool has_foo@1; int32 foo@0; };',
        'struct S { int32? foo; };')

    self.assertBackwardCompatible(
        'struct S { bool has_foo = true; int32 foo = 2; };',
        'struct S { int32? foo = 2; };')

    self.assertBackwardCompatible(
        'struct S { bool has_foo@0; double gap@2; float foo@1; };',
        'struct S { float? foo; double gap; };')
    self.assertNotBackwardCompatible(
        'struct S { bool has_foo; double gap; float foo; };',
        'struct S { float? foo; double gap; };')

    # Tests layout compat + adding a new field.
    self.assertBackwardCompatible(
        'struct S { bool has_foo@0; double gap@2; float foo@1; };',
        'struct S { float? foo; double gap; [MinVersion=1] int32 foobear;};')
    # No min version specified, not compatible.
    self.assertNotBackwardCompatible(
        'struct S { bool has_foo@0; double gap@2; float foo@1; };',
        'struct S { float? foo; double gap; int32 foobear;};')
