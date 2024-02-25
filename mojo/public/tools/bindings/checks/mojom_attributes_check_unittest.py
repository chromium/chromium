# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import mojom.generate.check as check
from mojom_bindings_generator import LoadChecks, _Generate
from mojom_parser_test_case import MojomParserTestCase


class FakeArgs:
  """Fakes args to _Generate - intention is to do just enough to run checks"""

  def __init__(self, tester, files=None):
    """ `tester` is MojomParserTestCase for paths.
        `files` will have tester path added."""
    self.checks_string = 'attributes'
    self.depth = tester.GetPath('')
    self.filelist = None
    self.filename = [tester.GetPath(x) for x in files]
    self.gen_directories = tester.GetPath('gen')
    self.generators_string = ''
    self.import_directories = []
    self.output_dir = tester.GetPath('out')
    self.scrambled_message_id_salt_paths = None
    self.typemaps = []
    self.variant = 'none'


class MojoBindingsCheckTest(MojomParserTestCase):
  def _ParseAndGenerate(self, mojoms):
    self.ParseMojoms(mojoms)
    args = FakeArgs(self, files=mojoms)
    _Generate(args, {})

  def _testValid(self, filename, content):
    self.WriteFile(filename, content)
    self._ParseAndGenerate([filename])

  def _testThrows(self, filename, content, regexp):
    mojoms = []
    self.WriteFile(filename, content)
    mojoms.append(filename)
    with self.assertRaisesRegexp(check.CheckException, regexp):
      self._ParseAndGenerate(mojoms)

  def testLoads(self):
    """Validate that the check is registered under the expected name."""
    check_modules = LoadChecks('attributes')
    self.assertTrue(check_modules['attributes'])

  def testNoAnnotations(self):
    # Undecorated mojom should be fine.
    self._testValid(
        "a.mojom", """
      module a;
      struct Bar { int32 a; };
      enum Hello { kValue };
      union Thingy { Bar b; Hello hi; };
      interface Foo {
        Foo(int32 a, Hello hi, Thingy t) => (Bar b);
      };
    """)

  def testValidAnnotations(self):
    # Obviously this is meaningless and won't generate, but it should pass
    # the attribute check's validation.
    self._testValid(
        "a.mojom", """
      [JavaConstantsClassName="FakeClass",JavaPackage="org.chromium.Fake"]
      module a;
      [Stable, Extensible]
      enum Hello { [Default] kValue, kValue2, [MinVersion=2] kValue3 };
      [Native]
      enum NativeEnum {};
      [Stable,Extensible]
      union Thingy { Bar b; [Default]int32 c; Hello hi; };

      [Stable,RenamedFrom="module.other.Foo",
       Uuid="4C178401-4B07-4C2E-9255-5401A943D0C7"]
      struct Structure { Hello hi; };

      [ServiceSandbox=Hello.kValue,RequireContext=Hello.kValue,Stable,
       Uuid="2F17D7DD-865A-4B1C-9394-9C94E035E82F"]
      interface Foo {
        [AllowedContext=Hello.kValue]
        Foo@0(int32 a) => (int32 b);
        [MinVersion=2,Sync,UnlimitedSize,NoInterrupt]
        Bar@1(int32 b, [MinVersion=2]Structure? s) => (bool c);
      };

      [RuntimeFeature=test.mojom.FeatureName]
      interface FooFeatureControlled {};

      interface FooMethodFeatureControlled {
        [RuntimeFeature=test.mojom.FeatureName]
        MethodWithFeature() => (bool c);
      };
    """)

  def testWrongModuleStable(self):
    contents = """
      // err: module cannot be Stable
      [Stable]
      module a;
      enum Hello { kValue, kValue2, kValue3 };
      enum NativeEnum {};
      struct Structure { Hello hi; };

      interface Foo {
        Foo(int32 a) => (int32 b);
        Bar(int32 b, Structure? s) => (bool c);
      };
    """
    self._testThrows('b.mojom', contents,
                     'attribute Stable not allowed on module')

  def testWrongEnumDefault(self):
    contents = """
      module a;
      // err: default should go on EnumValue not Enum.
      [Default=kValue]
      enum Hello { kValue, kValue2, kValue3 };
      enum NativeEnum {};
      struct Structure { Hello hi; };

      interface Foo {
        Foo(int32 a) => (int32 b);
        Bar(int32 b, Structure? s) => (bool c);
      };
    """
    self._testThrows('b.mojom', contents,
                     'attribute Default not allowed on enum')

  def testWrongStructMinVersion(self):
    contents = """
      module a;
      enum Hello { kValue, kValue2, kValue3 };
      enum NativeEnum {};
      // err: struct cannot have MinVersion.
      [MinVersion=2]
      struct Structure { Hello hi; };

      interface Foo {
        Foo(int32 a) => (int32 b);
        Bar(int32 b, Structure? s) => (bool c);
      };
    """
    self._testThrows('b.mojom', contents,
                     'attribute MinVersion not allowed on struct')

  def testWrongMethodRequireContext(self):
    contents = """
      module a;
      enum Hello { kValue, kValue2, kValue3 };
      enum NativeEnum {};
      struct Structure { Hello hi; };

      interface Foo {
        // err: RequireContext is for interfaces.
        [RequireContext=Hello.kValue]
        Foo(int32 a) => (int32 b);
        Bar(int32 b, Structure? s) => (bool c);
      };
    """
    self._testThrows('b.mojom', contents,
                     'RequireContext not allowed on method')

  def testWrongMethodRequireContext(self):
    # crbug.com/1230122
    contents = """
      module a;
      interface Foo {
        // err: sync not Sync.
        [sync]
        Foo(int32 a) => (int32 b);
      };
    """
    self._testThrows('b.mojom', contents,
                     'attribute sync not allowed.*Did you mean: Sync')

  def testStableExtensibleEnum(self):
    # crbug.com/1193875
    contents = """
      module a;
      [Stable]
      enum Foo {
        kDefaultVal,
        kOtherVal = 2,
      };
    """
    self._testThrows('a.mojom', contents,
                     'Extensible.*?required.*?Stable.*?enum')
