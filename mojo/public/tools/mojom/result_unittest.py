# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from mojom_parser_test_case import MojomParserTestCase
from mojom.generate import module as mojom


class ResultTest(MojomParserTestCase):
  """Tests result<T,E> parsing behavior."""

  def testResultResponse(self):
    a_mojom = 'a.mojom'
    self.WriteFile(
        a_mojom, """
        interface Test {
            Method() => result<bool, string>;
        };
    """)
    self.ParseMojoms([a_mojom])
    a = self.LoadModule(a_mojom)
    self.assertEqual(1, len(a.interfaces))
    self.assertEqual(1, len(a.interfaces[0].methods))
    self.assertEqual('b', a.interfaces[0].methods[0].result.success_type)
    self.assertEqual('s', a.interfaces[0].methods[0].result.failure_type)

  def testResultResponse(self):
    a_mojom = 'a.mojom'
    self.WriteFile(
        a_mojom, """
        struct Success {};
        struct Failure {};

        interface Test {
            Method() => result<Success, Failure>;
        };
    """)
    self.ParseMojoms([a_mojom])
    a = self.LoadModule(a_mojom)
    self.assertEqual(1, len(a.interfaces))
    self.assertEqual(1, len(a.interfaces[0].methods))
    self.assertEqual('x:Success',
                     a.interfaces[0].methods[0].result.success_type)
    self.assertEqual('x:Failure',
                     a.interfaces[0].methods[0].result.failure_type)
