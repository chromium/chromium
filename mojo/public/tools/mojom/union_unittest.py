# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from mojom_parser_test_case import MojomParserTestCase


class UnionTest(MojomParserTestCase):
  """Tests union parsing behavior."""

  def testExtensibleMustHaveDefault(self):
    """Verifies that extensible unions must have a default field."""
    mojom = 'foo.mojom'
    self.WriteFile(mojom, 'module foo; [Extensible] union U { bool x; };')
    with self.assertRaisesRegexp(Exception, 'must specify a \[Default\]'):
      self.ParseMojoms([mojom])

  def testExtensibleSingleDefault(self):
    """Verifies that extensible unions must not have multiple default fields."""
    mojom = 'foo.mojom'
    self.WriteFile(
        mojom, """\
               module foo;
               [Extensible] union U {
                 [Default] bool x;
                 [Default] bool y;
               };
               """)
    with self.assertRaisesRegexp(Exception, 'Multiple \[Default\] fields'):
      self.ParseMojoms([mojom])

  def testExtensibleDefaultTypeValid(self):
    """Verifies that an extensible union's default field must be nullable or
    integral type."""
    mojom = 'foo.mojom'
    self.WriteFile(
        mojom, """\
               module foo;
               [Extensible] union U {
                 [Default] handle<message_pipe> p;
               };
               """)
    with self.assertRaisesRegexp(Exception, 'must be nullable or integral'):
      self.ParseMojoms([mojom])
