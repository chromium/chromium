# Copyright 2023 The Chromium Authors
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
    self.checks_string = 'features'
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
    with self.assertRaisesRegexp(check.CheckException, regexp):
      self._ParseAndGenerate(mojoms)

  def testLoads(self):
    """Validate that the check is registered under the expected name."""
    check_modules = LoadChecks('features')
    self.assertTrue(check_modules['features'])

  def testNullableOk(self):
    self.assertValid(
        "a.mojom", """
          module a;
          // Scaffolding.
          feature kFeature {
            const string name = "Hello";
            const bool enabled_state = false;
          };
          [RuntimeFeature=kFeature]
          interface Guarded {
          };

          // Unguarded interfaces should be ok everywhere.
          interface NotGuarded { };

          // Optional (nullable) interfaces should be ok everywhere:
          struct Bar {
            pending_remote<Guarded>? remote;
            pending_receiver<Guarded>? receiver;
          };
          union Thingy {
            pending_remote<Guarded>? remote;
            pending_receiver<Guarded>? receiver;
          };
          interface Foo {
            Foo(
              pending_remote<Guarded>? remote,
              pending_receiver<Guarded>? receiver,
              pending_associated_remote<Guarded>? a_remote,
              pending_associated_receiver<Guarded>? a_receiver,
              // Unguarded interfaces do not have to be nullable.
              pending_remote<NotGuarded> remote,
              pending_receiver<NotGuarded> receiver,
              pending_associated_remote<NotGuarded> a_remote,
              pending_associated_receiver<NotGuarded> a_receiver
            ) => (
              pending_remote<Guarded>? remote,
              pending_receiver<Guarded>? receiver
            );
            Bar(array<pending_remote<Guarded>?> remote)
              => (map<string, pending_receiver<Guarded>?> a);
          };
    """)

  def testMethodParamsMustBeNullable(self):
    prelude = """
      module a;
      // Scaffolding.
      feature kFeature {
        const string name = "Hello";
        const bool enabled_state = false;
      };
      [RuntimeFeature=kFeature]
      interface Guarded { };
    """
    self.assertThrows(
        'a.mojom', prelude + """
          interface Trial {
            Method(pending_remote<Guarded> a) => ();
          };
                     """, 'interface Guarded has a RuntimeFeature')
    self.assertThrows(
        'a.mojom', prelude + """
          interface Trial {
            Method(bool foo) => (pending_receiver<Guarded> a);
          };
                     """, 'interface Guarded has a RuntimeFeature')
    self.assertThrows(
        'a.mojom', prelude + """
          interface Trial {
            Method(pending_receiver<Guarded> a) => ();
          };
                     """, 'interface Guarded has a RuntimeFeature')
    self.assertThrows(
        'a.mojom', prelude + """
          interface Trial {
            Method(pending_associated_remote<Guarded> a) => ();
          };
                     """, 'interface Guarded has a RuntimeFeature')
    self.assertThrows(
        'a.mojom', prelude + """
          interface Trial {
            Method(pending_associated_receiver<Guarded> a) => ();
          };
                     """, 'interface Guarded has a RuntimeFeature')
    self.assertThrows(
        'a.mojom', prelude + """
          interface Trial {
            Method(array<pending_associated_receiver<Guarded>> a) => ();
          };
                     """, 'interface Guarded has a RuntimeFeature')
    self.assertThrows(
        'a.mojom', prelude + """
          interface Trial {
            Method(map<string, pending_associated_receiver<Guarded>> a) => ();
          };
                     """, 'interface Guarded has a RuntimeFeature')

  def testStructUnionMembersMustBeNullable(self):
    prelude = """
      module a;
      // Scaffolding.
      feature kFeature {
        const string name = "Hello";
        const bool enabled_state = false;
      };
      [RuntimeFeature=kFeature]
      interface Guarded { };
    """
    self.assertThrows(
        'a.mojom', prelude + """
          struct Trial {
            pending_remote<Guarded> a;
          };
                     """, 'interface Guarded has a RuntimeFeature')
    self.assertThrows(
        'a.mojom', prelude + """
          union Trial {
            pending_remote<Guarded> a;
          };
                     """, 'interface Guarded has a RuntimeFeature')
