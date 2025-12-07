# Copyright 2025 The Chromium Authors
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
    self.checks_string = 'direct_receiver'
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

  def assertValid(self, filename, content):
    self.WriteFile(filename, content)
    self._ParseAndGenerate([filename])

  def assertThrows(self, filename, content, regexp):
    mojoms = []
    self.WriteFile(filename, content)
    mojoms.append(filename)
    with self.assertRaisesRegex(check.CheckException, regexp):
      self._ParseAndGenerate(mojoms)

  def testLoads(self):
    """Validate that the check is registered under the expected name."""
    check_modules = LoadChecks('direct_receiver')
    self.assertTrue(check_modules['direct_receiver'])

  def testAllowedWhenAnnotated(self):
    self.assertValid(
        "a.mojom", """
          module a;
          struct B {
            pending_associated_receiver<MyInterface3> passed;
            pending_remote<Normal> normal;
          };
          interface Normal {};
          [DirectReceiver]
          interface MyInterface {
            Method(pending_receiver<MyInterface2> passed);
            Method2(B b, pending_associated_remote<Normal> normal);
          };
          [DirectReceiver]
          interface MyInterface2 {
          };
          [DirectReceiver]
          interface MyInterface3 {
          };
    """)

  def testMissingViaMethod(self):
    contents = """
      module a;
      interface A {};

      [DirectReceiver]
      interface B {
        Method(pending_receiver<A> a);
      };
    """
    self.assertThrows('b.mojom', contents,
                      'interface A must be a DirectReceiver')

  def testMissingViaStruct(self):
    contents = """
      module a;
      interface A {};
      struct C {
        pending_receiver<A> a;
      };

      [DirectReceiver]
      interface B {
        Method(C c);
      };
    """
    self.assertThrows('b.mojom', contents,
                      'interface A must be a DirectReceiver')

  def testMissingViaUnion(self):
    contents = """
      module a;
      interface A {};
      union C {
        pending_receiver<A> a;
        bool other;
      };

      [DirectReceiver]
      interface B {
        Method(C c);
      };
    """
    self.assertThrows('b.mojom', contents,
                      'interface A must be a DirectReceiver')

  def testMissingViaArray(self):
    contents = """
      module a;
      interface A {};
      struct C {
        array<pending_receiver<A>> a;
        bool other;
      };

      [DirectReceiver]
      interface B {
        Method(C c);
      };
    """
    self.assertThrows('b.mojom', contents,
                      'interface A must be a DirectReceiver')

  def testMissingViaMap(self):
    contents = """
      module a;
      interface A {};
      struct C {
        map<string, pending_receiver<A>> a;
        bool other;
      };

      [DirectReceiver]
      interface B {
        Method(C c);
      };
    """
    self.assertThrows('b.mojom', contents,
                      'interface A must be a DirectReceiver')
