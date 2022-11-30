# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import mojom.generate.check as check
from mojom_bindings_generator import LoadChecks, _Generate
from mojom_parser_test_case import MojomParserTestCase

# Mojoms that we will use in multiple tests.
basic_mojoms = {
    'level.mojom':
    """
  module level;
  enum Level {
    kHighest,
    kMiddle,
    kLowest,
  };
  """,
    'interfaces.mojom':
    """
  module interfaces;
  import "level.mojom";
  struct Foo {int32 bar;};
  [RequireContext=level.Level.kHighest]
  interface High {
    DoFoo(Foo foo);
  };
  [RequireContext=level.Level.kMiddle]
  interface Mid {
    DoFoo(Foo foo);
  };
  [RequireContext=level.Level.kLowest]
  interface Low {
    DoFoo(Foo foo);
  };
  """
}


class FakeArgs:
  """Fakes args to _Generate - intention is to do just enough to run checks"""

  def __init__(self, tester, files=None):
    """ `tester` is MojomParserTestCase for paths.
        `files` will have tester path added."""
    self.checks_string = 'restrictions'
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
  def _WriteBasicMojoms(self):
    for filename, contents in basic_mojoms.items():
      self.WriteFile(filename, contents)
    return list(basic_mojoms.keys())

  def _ParseAndGenerate(self, mojoms):
    self.ParseMojoms(mojoms)
    args = FakeArgs(self, files=mojoms)
    _Generate(args, {})

  def testLoads(self):
    """Validate that the check is registered under the expected name."""
    check_modules = LoadChecks('restrictions')
    self.assertTrue(check_modules['restrictions'])

  def testValidAnnotations(self):
    mojoms = self._WriteBasicMojoms()

    a = 'a.mojom'
    self.WriteFile(
        a, """
      module a;
      import "level.mojom";
      import "interfaces.mojom";

      interface PassesHigh {
        [AllowedContext=level.Level.kHighest]
        DoHigh(pending_receiver<interfaces.High> hi);
      };
      interface PassesMedium {
        [AllowedContext=level.Level.kMiddle]
        DoMedium(pending_receiver<interfaces.Mid> hi);
        [AllowedContext=level.Level.kMiddle]
        DoMediumRem(pending_remote<interfaces.Mid> hi);
        [AllowedContext=level.Level.kMiddle]
        DoMediumAssoc(pending_associated_receiver<interfaces.Mid> hi);
        [AllowedContext=level.Level.kMiddle]
        DoMediumAssocRem(pending_associated_remote<interfaces.Mid> hi);
      };
      interface PassesLow {
        [AllowedContext=level.Level.kLowest]
        DoLow(pending_receiver<interfaces.Low> hi);
      };

      struct One { pending_receiver<interfaces.High> hi; };
      struct Two { One one; };
      interface PassesNestedHigh {
        [AllowedContext=level.Level.kHighest]
        DoNestedHigh(Two two);
      };

      // Allowed as PassesHigh is not itself restricted.
      interface PassesPassesHigh {
        DoPass(pending_receiver<PassesHigh> hiho);
      };
    """)
    mojoms.append(a)
    self._ParseAndGenerate(mojoms)

  def _testThrows(self, filename, content, regexp):
    mojoms = self._WriteBasicMojoms()
    self.WriteFile(filename, content)
    mojoms.append(filename)
    with self.assertRaisesRegexp(check.CheckException, regexp):
      self._ParseAndGenerate(mojoms)

  def testMissingAnnotation(self):
    contents = """
      module b;
      import "level.mojom";
      import "interfaces.mojom";

      interface PassesHigh {
        // err: missing annotation.
        DoHigh(pending_receiver<interfaces.High> hi);
      };
    """
    self._testThrows('b.mojom', contents, 'require.*?AllowedContext')

  def testAllowTooLow(self):
    contents = """
      module b;
      import "level.mojom";
      import "interfaces.mojom";

      interface PassesHigh {
        // err: level is worse than required.
        [AllowedContext=level.Level.kMiddle]
        DoHigh(pending_receiver<interfaces.High> hi);
      };
    """
    self._testThrows('b.mojom', contents,
                     'RequireContext=.*?kHighest > AllowedContext=.*?kMiddle')

  def testWrongEnumInAllow(self):
    contents = """
      module b;
      import "level.mojom";
      import "interfaces.mojom";
      enum Blah {
        kZero,
      };
      interface PassesHigh {
        // err: different enums.
        [AllowedContext=Blah.kZero]
        DoHigh(pending_receiver<interfaces.High> hi);
      };
    """
    self._testThrows('b.mojom', contents, 'but one of kind')

  def testNotAnEnumInAllow(self):
    contents = """
      module b;
      import "level.mojom";
      import "interfaces.mojom";
      interface PassesHigh {
        // err: not an enum.
        [AllowedContext=doopdedoo.mojom.kWhatever]
        DoHigh(pending_receiver<interfaces.High> hi);
      };
    """
    self._testThrows('b.mojom', contents, 'not a valid enum value')

  def testMissingAllowedForNestedStructs(self):
    contents = """
      module b;
      import "level.mojom";
      import "interfaces.mojom";
      struct One { pending_receiver<interfaces.High> hi; };
      struct Two { One one; };
      interface PassesNestedHigh {
        // err: missing annotation.
        DoNestedHigh(Two two);
      };
    """
    self._testThrows('b.mojom', contents, 'require.*?AllowedContext')

  def testMissingAllowedForNestedUnions(self):
    contents = """
      module b;
      import "level.mojom";
      import "interfaces.mojom";
      struct One { pending_receiver<interfaces.High> hi; };
      struct Two { One one; };
      union Three {One one; Two two; };
      interface PassesNestedHigh {
        // err: missing annotation.
        DoNestedHigh(Three three);
      };
    """
    self._testThrows('b.mojom', contents, 'require.*?AllowedContext')

  def testMultipleInterfacesThrows(self):
    contents = """
      module b;
      import "level.mojom";
      import "interfaces.mojom";
      struct One { pending_receiver<interfaces.High> hi; };
      interface PassesMultipleInterfaces {
        [AllowedContext=level.Level.kMiddle]
        DoMultiple(
          pending_remote<interfaces.Mid> mid,
          pending_receiver<interfaces.High> hi,
          One one
        );
      };
    """
    self._testThrows('b.mojom', contents,
                     'RequireContext=.*?kHighest > AllowedContext=.*?kMiddle')

  def testMultipleInterfacesAllowed(self):
    """Multiple interfaces can be passed, all satisfy the level."""
    mojoms = self._WriteBasicMojoms()

    b = "b.mojom"
    self.WriteFile(
        b, """
      module b;
      import "level.mojom";
      import "interfaces.mojom";
      struct One { pending_receiver<interfaces.High> hi; };
      interface PassesMultipleInterfaces {
        [AllowedContext=level.Level.kHighest]
        DoMultiple(
          pending_receiver<interfaces.High> hi,
          pending_remote<interfaces.Mid> mid,
          One one
        );
      };
    """)
    mojoms.append(b)
    self._ParseAndGenerate(mojoms)
