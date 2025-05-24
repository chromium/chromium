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
    self.assertEqual(mojom.BOOL,
                     a.interfaces[0].methods[0].result_response.success_kind)
    self.assertEqual(mojom.STRING,
                     a.interfaces[0].methods[0].result_response.failure_kind)

  def testResultResponseStructs(self):
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

    result_response = a.interfaces[0].methods[0].result_response
    self.assertIsNotNone(result_response.success_kind)
    self.assertIsNotNone(result_response.failure_kind)

    name_to_kind = {}
    for kind in a.structs:
      name_to_kind[kind.mojom_name] = kind

    self.assertEqual(name_to_kind['Success'], result_response.success_kind)
    self.assertEqual(name_to_kind['Failure'], result_response.failure_kind)

  def testResultResponseGeneratedParam(self):
    a_mojom = 'a.mojom'
    self.WriteFile(
        a_mojom, """
        interface Test {
            Method() => result<bool, bool>;
        };
    """)
    self.ParseMojoms([a_mojom])
    a = self.LoadModule(a_mojom)
    self.assertEqual(1, len(a.interfaces))
    self.assertEqual(1, len(a.interfaces[0].methods))

    result_response = a.interfaces[0].methods[0].result_response
    param = result_response.ToResponseParam(a)

    self.assertEqual(param.mojom_name, 'result')
    self.assertTrue(isinstance(param.kind, mojom.Union))
    self.assertEqual(param.kind.fields[0].mojom_name, 'success')
    self.assertEqual(param.kind.fields[0].ordinal, 0)
    self.assertEqual(param.kind.fields[1].mojom_name, 'failure')
    self.assertEqual(param.kind.fields[1].ordinal, 1)
