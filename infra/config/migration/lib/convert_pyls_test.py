#! /usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import dataclasses
import datetime
import pathlib
import sys
import textwrap
import typing
import unittest

sys.path.append(str(pathlib.Path(__file__).parent.parent))
from lib import convert_pyls
from lib import pyl


# return typing.Any to prevent type checkers from complaining about the general
# typing.Value type being passed where a more specific type is expected without
# having to specify a type or cast for each call
def _to_pyl_value(value: str) -> typing.Any:
  nodes = list(pyl.parse('test', value))
  assert (all(isinstance(n, pyl.Comment) for n in nodes[:-1])
          and isinstance(nodes[-1], pyl.Value)
          ), f'{value!r} does not parse to a single pyl.Value, got {nodes}'
  node = nodes[-1]
  return dataclasses.replace(node, comments=tuple(nodes[:-1]))


class ConvertPylsTest(unittest.TestCase):

  def test_convert_gn_isolate_map_pyl_success(self):
    gn_isolate_map = textwrap.dedent("""\
        {
          # comment on isolate
          "script_test": {
            # comment on field
            "label": "//abc/def:script_test",
            "type": "script",
            "script": "//abc/def/script_test.py",
          },
          "compile_target": {
            "label": "//ghi/jkl:compile_target",
            "type": "additional_compile_target",
          },
          "script_test2": {
            "label": "//mno/pqr:script_test2",
            "type": "script",
            "script": "//mno/pqr/script_test2.py",
            "args": [
              "--foo",
              "--bar",
            ],
          },
          "non_script_test": {
            "label": "//stu/vwx:non_script_test",
            "type": "console_test_launcher",
          },
        }
        """)

    files = convert_pyls.convert_gn_isolate_map_pyl(
        _to_pyl_value(gn_isolate_map))

    self.maxDiff = None
    self.assertCountEqual(
        files.keys(), ['targets/binaries.star', 'targets/compile_targets.star'])

    year = datetime.datetime.now().year
    binaries_star = files['targets/binaries.star']
    expected_binaries_star = textwrap.dedent(f'''\
        # Copyright {year} The Chromium Authors
        # Use of this source code is governed by a BSD-style license that can be
        # found in the LICENSE file.

        """Binary declarations

        Binaries can be referenced by tests and define the label of the compile target
        to be built as well as various aspects that the infrastructure needs to know in
        order to run the binary.
        """

        load("@chromium-luci//targets.star", "targets")

        # comment on isolate
        targets.binaries.script(
          name = "script_test",
          # comment on field
          label = "//abc/def:script_test",
          script = "//abc/def/script_test.py",
        )

        targets.binaries.script(
          name = "script_test2",
          label = "//mno/pqr:script_test2",
          script = "//mno/pqr/script_test2.py",
          args = [
            "--foo",
            "--bar",
          ],
        )

        targets.binaries.console_test_launcher(
          name = "non_script_test",
          label = "//stu/vwx:non_script_test",
        )
        ''')[:-1]
    self.assertEqual(binaries_star, expected_binaries_star)

    compile_targets_star = files['targets/compile_targets.star']
    expected_compile_targets_star = textwrap.dedent(f'''\
        # Copyright {year} The Chromium Authors
        # Use of this source code is governed by a BSD-style license that can be
        # found in the LICENSE file.

        """Compile target declarations

        Compile targets can be referenced in additional_compile_targets for a builder in
        waterfalls.pyl or as additional_compile_targets in a bundle declaration.
        """

        load("@chromium-luci//targets.star", "targets")

        targets.compile_target(
          name = "compile_target",
          label = "//ghi/jkl:compile_target",
        )
        ''')[:-1]
    self.assertEqual(compile_targets_star, expected_compile_targets_star)

  def test_isolate_missing_type(self):
    gn_isolate_map = _to_pyl_value(
        repr({
            'test_isolate': {
                'label': '//:test_label',
            },
        }))
    with self.assertRaises(Exception) as caught:
      convert_pyls.convert_gn_isolate_map_pyl(gn_isolate_map)
    self.assertEqual(str(caught.exception),
                     'test:1:1: isolate test_isolate missing type')

  def test_args_for_compile_target(self):
    gn_isolate_map = _to_pyl_value(
        repr({
            'test_isolate': {
                'type': 'additional_compile_target',
                'args': [],
            },
        }))
    with self.assertRaises(Exception) as caught:
      convert_pyls.convert_gn_isolate_map_pyl(gn_isolate_map)
    self.assertEqual(
        str(caught.exception),
        ('test:1:55: args specified for isolate "test_isolate"'
         ' with type "additional_compile_target"'),
    )

  def test_script_for_non_script_type(self):
    gn_isolate_map = _to_pyl_value(
        repr({
            'test_isolate': {
                'type': 'executable',
                'script': 'run.py'
            },
        }))
    with self.assertRaises(Exception) as caught:
      convert_pyls.convert_gn_isolate_map_pyl(gn_isolate_map)
    self.assertEqual(
        str(caught.exception),
        ('test:1:40: script specified for isolate "test_isolate"'
         ' with non-"script" type "executable"'),
    )

  def test_unhandled_key_in_isolate(self):
    gn_isolate_map = _to_pyl_value(
        repr({
            'test_isolate': {
                'type': 'executable',
                'unknown_key': 'value'
            },
        }))
    with self.assertRaises(Exception) as caught:
      convert_pyls.convert_gn_isolate_map_pyl(gn_isolate_map)
    self.assertEqual(str(caught.exception),
                     'test:1:40: unhandled key in isolate: "unknown_key"')


if __name__ == '__main__':
  unittest.main()  # pragma: no cover
