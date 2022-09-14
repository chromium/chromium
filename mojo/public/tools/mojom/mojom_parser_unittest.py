# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json

from mojom_parser_test_case import MojomParserTestCase


class MojomParserTest(MojomParserTestCase):
  """Tests covering the behavior defined by the main mojom_parser.py script.
  This includes behavior around input and output path manipulation, dependency
  resolution, and module serialization and deserialization."""

  def testBasicParse(self):
    """Basic test to verify that we can parse a mojom file and get a module."""
    mojom = 'foo/bar.mojom'
    self.WriteFile(
        mojom, """\
        module test;
        enum TestEnum { kFoo };
        """)
    self.ParseMojoms([mojom])

    m = self.LoadModule(mojom)
    self.assertEqual('foo/bar.mojom', m.path)
    self.assertEqual('test', m.mojom_namespace)
    self.assertEqual(1, len(m.enums))

  def testBasicParseWithAbsolutePaths(self):
    """Verifies that we can parse a mojom file given an absolute path input."""
    mojom = 'foo/bar.mojom'
    self.WriteFile(
        mojom, """\
        module test;
        enum TestEnum { kFoo };
        """)
    self.ParseMojoms([self.GetPath(mojom)])

    m = self.LoadModule(mojom)
    self.assertEqual('foo/bar.mojom', m.path)
    self.assertEqual('test', m.mojom_namespace)
    self.assertEqual(1, len(m.enums))

  def testImport(self):
    """Verify imports within the same set of mojom inputs."""
    a = 'a.mojom'
    b = 'b.mojom'
    self.WriteFile(
        a, """\
        module a;
        import "b.mojom";
        struct Foo { b.Bar bar; };""")
    self.WriteFile(b, """\
        module b;
        struct Bar {};""")
    self.ParseMojoms([a, b])

    ma = self.LoadModule(a)
    mb = self.LoadModule(b)
    self.assertEqual('a.mojom', ma.path)
    self.assertEqual('b.mojom', mb.path)
    self.assertEqual(1, len(ma.imports))
    self.assertEqual(mb, ma.imports[0])

  def testPreProcessedImport(self):
    """Verify imports processed by a previous parser execution can be loaded
    properly when parsing a dependent mojom."""
    a = 'a.mojom'
    self.WriteFile(a, """\
        module a;
        struct Bar {};""")
    self.ParseMojoms([a])

    b = 'b.mojom'
    self.WriteFile(
        b, """\
        module b;
        import "a.mojom";
        struct Foo { a.Bar bar; };""")
    self.ParseMojoms([b])

  def testMissingImport(self):
    """Verify that an import fails if the imported mojom does not exist."""
    a = 'a.mojom'
    self.WriteFile(
        a, """\
        module a;
        import "non-existent.mojom";
        struct Bar {};""")
    with self.assertRaisesRegexp(ValueError, "does not exist"):
      self.ParseMojoms([a])

  def testUnparsedImport(self):
    """Verify that an import fails if the imported mojom is not in the set of
    mojoms provided to the parser on this execution AND there is no pre-existing
    parsed output module already on disk for it."""
    a = 'a.mojom'
    b = 'b.mojom'
    self.WriteFile(a, """\
        module a;
        struct Bar {};""")
    self.WriteFile(
        b, """\
        module b;
        import "a.mojom";
        struct Foo { a.Bar bar; };""")

    # a.mojom has not been parsed yet, so its import will fail when processing
    # b.mojom here.
    with self.assertRaisesRegexp(ValueError, "does not exist"):
      self.ParseMojoms([b])

  def testCheckImportsBasic(self):
    """Verify that the parser can handle --check-imports with a valid set of
    inputs, including support for transitive dependency resolution."""
    a = 'a.mojom'
    a_metadata = 'out/a.build_metadata'
    b = 'b.mojom'
    b_metadata = 'out/b.build_metadata'
    c = 'c.mojom'
    c_metadata = 'out/c.build_metadata'
    self.WriteFile(a_metadata,
                   json.dumps({
                       "sources": [self.GetPath(a)],
                       "deps": []
                   }))
    self.WriteFile(
        b_metadata,
        json.dumps({
            "sources": [self.GetPath(b)],
            "deps": [self.GetPath(a_metadata)]
        }))
    self.WriteFile(
        c_metadata,
        json.dumps({
            "sources": [self.GetPath(c)],
            "deps": [self.GetPath(b_metadata)]
        }))
    self.WriteFile(a, """\
        module a;
        struct Bar {};""")
    self.WriteFile(
        b, """\
        module b;
        import "a.mojom";
        struct Foo { a.Bar bar; };""")
    self.WriteFile(
        c, """\
        module c;
        import "a.mojom";
        import "b.mojom";
        struct Baz { b.Foo foo; };""")
    self.ParseMojoms([a], metadata=a_metadata)
    self.ParseMojoms([b], metadata=b_metadata)
    self.ParseMojoms([c], metadata=c_metadata)

  def testCheckImportsMissing(self):
    """Verify that the parser rejects valid input mojoms when imports don't
    agree with build metadata given via --check-imports."""
    a = 'a.mojom'
    a_metadata = 'out/a.build_metadata'
    b = 'b.mojom'
    b_metadata = 'out/b.build_metadata'
    self.WriteFile(a_metadata,
                   json.dumps({
                       "sources": [self.GetPath(a)],
                       "deps": []
                   }))
    self.WriteFile(b_metadata,
                   json.dumps({
                       "sources": [self.GetPath(b)],
                       "deps": []
                   }))
    self.WriteFile(a, """\
        module a;
        struct Bar {};""")
    self.WriteFile(
        b, """\
        module b;
        import "a.mojom";
        struct Foo { a.Bar bar; };""")

    self.ParseMojoms([a], metadata=a_metadata)
    with self.assertRaisesRegexp(ValueError, "not allowed by build"):
      self.ParseMojoms([b], metadata=b_metadata)
