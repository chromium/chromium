#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import os.path
import shutil
import tempfile
import unittest

import check_stable_mojom_compatibility

from mojom.generate import module


class Change:
  """Helper to clearly define a mojom file delta to be analyzed."""

  def __init__(self, filename, old=None, new=None):
    """If old is None, this is a file addition. If new is None, this is a file
    deletion. Otherwise it's a file change."""
    self.filename = filename
    self.old = old
    self.new = new


class UnchangedFile(Change):
  def __init__(self, filename, contents):
    super().__init__(filename, old=contents, new=contents)


class CheckStableMojomCompatibilityTest(unittest.TestCase):
  """Tests covering the behavior of the compatibility checking tool. Note that
  details of different compatibility checks and relevant failure modes are NOT
  covered by these tests. Those are instead covered by unittests in
  version_compatibility_unittest.py. Additionally, the tests which ensure a
  given set of [Stable] mojom definitions are indeed plausibly stable (i.e. they
  have no unstable dependencies) are covered by stable_attribute_unittest.py.

  These tests cover higher-level concerns of the compatibility checking tool,
  like file or symbol, renames, changes spread over multiple files, etc."""

  def verifyBackwardCompatibility(self, changes):
    """Helper for implementing assertBackwardCompatible and
    assertNotBackwardCompatible"""

    temp_dir = tempfile.mkdtemp()
    for change in changes:
      if change.old:
        # Populate the old file on disk in our temporary fake source root
        file_path = os.path.join(temp_dir, change.filename)
        dir_path = os.path.dirname(file_path)
        if not os.path.exists(dir_path):
          os.makedirs(dir_path)
        with open(file_path, 'w') as f:
          f.write(change.old)

    delta = []
    for change in changes:
      if change.old != change.new:
        delta.append({
            'filename': change.filename,
            'old': change.old,
            'new': change.new
        })

    try:
      check_stable_mojom_compatibility.Run(['--src-root', temp_dir],
                                           delta=delta)
    finally:
      shutil.rmtree(temp_dir)

  def assertBackwardCompatible(self, changes):
    self.verifyBackwardCompatibility(changes)

  def assertNotBackwardCompatible(self, changes):
    try:
      self.verifyBackwardCompatibility(changes)
    except Exception:
      return

    raise Exception('Change unexpectedly passed a backward-compatibility check')

  def testBasicCompatibility(self):
    """Minimal smoke test to verify acceptance of a simple valid change."""
    self.assertBackwardCompatible([
        Change('foo/foo.mojom',
               old='[Stable] struct S {};',
               new='[Stable] struct S { [MinVersion=1] int32 x; };')
    ])

  def testBasicIncompatibility(self):
    """Minimal smoke test to verify rejection of a simple invalid change."""
    self.assertNotBackwardCompatible([
        Change('foo/foo.mojom',
               old='[Stable] struct S {};',
               new='[Stable] struct S { int32 x; };')
    ])

  def testIgnoreIfNotStable(self):
    """We don't care about types not marked [Stable]"""
    self.assertBackwardCompatible([
        Change('foo/foo.mojom',
               old='struct S {};',
               new='struct S { int32 x; };')
    ])

  def testRename(self):
    """We can do checks for renamed types."""
    self.assertBackwardCompatible([
        Change('foo/foo.mojom',
               old='[Stable] struct S {};',
               new='[Stable, RenamedFrom="S"] struct T {};')
    ])
    self.assertNotBackwardCompatible([
        Change('foo/foo.mojom',
               old='[Stable] struct S {};',
               new='[Stable, RenamedFrom="S"] struct T { int32 x; };')
    ])
    self.assertBackwardCompatible([
        Change('foo/foo.mojom',
               old='[Stable] struct S {};',
               new="""\
               [Stable, RenamedFrom="S"]
               struct T { [MinVersion=1] int32 x; };
               """)
    ])

  def testNewlyStable(self):
    """We don't care about types newly marked as [Stable]."""
    self.assertBackwardCompatible([
        Change('foo/foo.mojom',
               old='struct S {};',
               new='[Stable] struct S { int32 x; };')
    ])

  def testFileRename(self):
    """Make sure we can still do compatibility checks after a file rename."""
    self.assertBackwardCompatible([
        Change('foo/foo.mojom', old='[Stable] struct S {};', new=None),
        Change('bar/bar.mojom',
               old=None,
               new='[Stable] struct S { [MinVersion=1] int32 x; };')
    ])
    self.assertNotBackwardCompatible([
        Change('foo/foo.mojom', old='[Stable] struct S {};', new=None),
        Change('bar/bar.mojom', old=None, new='[Stable] struct S { int32 x; };')
    ])

  def testWithImport(self):
    """Ensure that cross-module dependencies do not break the compatibility
    checking tool."""
    self.assertBackwardCompatible([
        Change('foo/foo.mojom',
               old="""\
               module foo;
               [Stable] struct S {};
               """,
               new="""\
               module foo;
               [Stable] struct S { [MinVersion=2] int32 x; };
               """),
        Change('bar/bar.mojom',
               old="""\
               module bar;
               import "foo/foo.mojom";
               [Stable] struct T { foo.S s; };
               """,
               new="""\
               module bar;
               import "foo/foo.mojom";
               [Stable] struct T { foo.S s; [MinVersion=1] int32 y; };
               """)
    ])

  def testWithMovedDefinition(self):
    """If a definition moves from one file to another, we should still be able
    to check compatibility accurately."""
    self.assertBackwardCompatible([
        Change('foo/foo.mojom',
               old="""\
               module foo;
               [Stable] struct S {};
               """,
               new="""\
               module foo;
               """),
        Change('bar/bar.mojom',
               old="""\
               module bar;
               import "foo/foo.mojom";
               [Stable] struct T { foo.S s; };
               """,
               new="""\
               module bar;
               import "foo/foo.mojom";
               [Stable, RenamedFrom="foo.S"] struct S {
                 [MinVersion=2] int32 x;
               };
               [Stable] struct T { S s; [MinVersion=1] int32 y; };
               """)
    ])

    self.assertNotBackwardCompatible([
        Change('foo/foo.mojom',
               old="""\
               module foo;
               [Stable] struct S {};
               """,
               new="""\
               module foo;
               """),
        Change('bar/bar.mojom',
               old="""\
               module bar;
               import "foo/foo.mojom";
               [Stable] struct T { foo.S s; };
               """,
               new="""\
               module bar;
               import "foo/foo.mojom";
               [Stable, RenamedFrom="foo.S"] struct S { int32 x; };
               [Stable] struct T { S s; [MinVersion=1] int32 y; };
               """)
    ])

  def testWithUnmodifiedImport(self):
    """Unchanged files in the filesystem are still parsed by the compatibility
    checking tool if they're imported by a changed file."""
    self.assertBackwardCompatible([
        UnchangedFile('foo/foo.mojom', 'module foo; [Stable] struct S {};'),
        Change('bar/bar.mojom',
               old="""\
               module bar;
               import "foo/foo.mojom";
               [Stable] struct T { foo.S s; };
               """,
               new="""\
               module bar;
               import "foo/foo.mojom";
               [Stable] struct T { foo.S s; [MinVersion=1] int32 x; };
               """)
    ])

    self.assertNotBackwardCompatible([
        UnchangedFile('foo/foo.mojom', 'module foo; [Stable] struct S {};'),
        Change('bar/bar.mojom',
               old="""\
               module bar;
               import "foo/foo.mojom";
               [Stable] struct T { foo.S s; };
               """,
               new="""\
               module bar;
               import "foo/foo.mojom";
               [Stable] struct T { foo.S s; int32 x; };
               """)
    ])

  def testWithPartialImport(self):
    """The compatibility checking tool correctly parses imports with partial
    paths."""
    self.assertBackwardCompatible([
        UnchangedFile('foo/foo.mojom', 'module foo; [Stable] struct S {};'),
        Change('foo/bar.mojom',
               old="""\
               module bar;
               import "foo/foo.mojom";
               [Stable] struct T { foo.S s; };
               """,
               new="""\
               module bar;
               import "foo.mojom";
               [Stable] struct T { foo.S s; };
               """)
    ])

    self.assertBackwardCompatible([
        UnchangedFile('foo/foo.mojom', 'module foo; [Stable] struct S {};'),
        Change('foo/bar.mojom',
               old="""\
               module bar;
               import "foo.mojom";
               [Stable] struct T { foo.S s; };
               """,
               new="""\
               module bar;
               import "foo/foo.mojom";
               [Stable] struct T { foo.S s; };
               """)
    ])

    self.assertNotBackwardCompatible([
        UnchangedFile('foo/foo.mojom', 'module foo; [Stable] struct S {};'),
        Change('bar/bar.mojom',
               old="""\
               module bar;
               import "foo/foo.mojom";
               [Stable] struct T { foo.S s; };
               """,
               new="""\
               module bar;
               import "foo.mojom";
               [Stable] struct T { foo.S s; };
               """)
    ])

    self.assertNotBackwardCompatible([
        UnchangedFile('foo/foo.mojom', 'module foo; [Stable] struct S {};'),
        Change('bar/bar.mojom',
               old="""\
               module bar;
               import "foo.mojom";
               [Stable] struct T { foo.S s; };
               """,
               new="""\
               module bar;
               import "foo/foo.mojom";
               [Stable] struct T { foo.S s; };
               """)
    ])

  def testNewEnumDefault(self):
    # Should be backwards compatible since it does not affect the wire format.
    # This specific case also checks that the backwards compatibility checker
    # does not throw an error due to the older version of the enum not
    # specifying [Default].
    self.assertBackwardCompatible([
        Change('foo/foo.mojom',
               old='[Extensible] enum E { One };',
               new='[Extensible] enum E { [Default] One };')
    ])
    self.assertBackwardCompatible([
        Change('foo/foo.mojom',
               old='[Extensible] enum E { [Default] One, Two, };',
               new='[Extensible] enum E { One, [Default] Two, };')
    ])
