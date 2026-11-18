# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from mojom_parser_test_case import MojomParserTestCase


class UnverifiedAttributeTest(MojomParserTestCase):
  """Tests Mojom IR properties for the [Unverified] parameter attribute."""

  def testUnverifiedAttributeTagging(self):
    mojom = 'test.mojom'
    self.WriteFile(
      mojom,
      """\
        module test.mojom;

        struct S {
          int32 x;
        };

        interface WithUnverified {
          Mixed(string safe, [Unverified] string risky) => (uint64 out);
          Multiple([Unverified] int32 a, [MinVersion=1, Unverified] S? b);
          Plain(int32 req) => (bool resp);
        };

        interface WithoutUnverified {
          NoUnverified(string a) => (uint64 b);
          Empty();
        };
        """,
    )
    self.ParseMojoms([mojom])

    m = self.LoadModule(mojom)
    self.assertEqual(2, len(m.interfaces))

    with_unverified = m.interfaces[0]
    self.assertTrue(with_unverified.has_unverified_parameters)
    self.assertEqual(3, len(with_unverified.methods))

    mixed = with_unverified.methods[0]
    self.assertFalse(mixed.parameters[0].unverified)
    self.assertTrue(mixed.parameters[1].unverified)
    self.assertFalse(mixed.response_parameters[0].unverified)
    self.assertTrue(mixed.has_unverified_parameters)

    multiple = with_unverified.methods[1]
    self.assertTrue(multiple.parameters[0].unverified)
    self.assertTrue(multiple.parameters[1].unverified)
    self.assertEqual(1, multiple.parameters[1].min_version)
    self.assertTrue(multiple.has_unverified_parameters)

    plain = with_unverified.methods[2]
    self.assertFalse(plain.parameters[0].unverified)
    self.assertFalse(plain.response_parameters[0].unverified)
    self.assertFalse(plain.has_unverified_parameters)

    without_unverified = m.interfaces[1]
    self.assertFalse(without_unverified.has_unverified_parameters)
    self.assertFalse(without_unverified.methods[0].has_unverified_parameters)
    self.assertFalse(without_unverified.methods[1].has_unverified_parameters)
