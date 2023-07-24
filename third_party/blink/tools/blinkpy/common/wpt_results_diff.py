# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from typing import List, Optional, Union
from blinkpy.common import path_finder
from blinkpy.w3c.wpt_metadata import fill_implied_expectations

path_finder.bootstrap_wpt_imports()

from wptrunner.manifestexpected import TestNode, data_cls_getter
from wptrunner.wptmanifest import node as wptnode
from wptrunner.wptmanifest.backends import static

_TEMPLATE = """<html>
<head>
<style>
table { white-space: pre-wrap; font-family: monospace; border-collapse: collapse; }
div {white-space: pre-wrap; font-family: monospace;}
th { color: #444; background: #eed; text-align: right; vertical-align: baseline; padding: 1px 4px 1px 4px; }
.del { background: #faa; }
.add { background: #afa; }
.missing { background: #f5e642; }
</style>
</head>
<body><table>%s</table></body>
</html>
"""


def wpt_results_diff(actual: TestNode,
                     expected: Optional[TestNode] = None,
                     test_type: str = 'testharness'):
    if not expected:
        test_ast = wptnode.DataNode()
        test_ast.append(wptnode.DataNode(actual.id))
        expected = static.compile_ast(
            test_ast,
            expr_data={},
            data_cls_getter=data_cls_getter,
            test_path=actual.root.test_path).get_test(actual.id)
    fill_implied_expectations(expected, set(actual.subtests), test_type)
    fill_implied_expectations(actual, set(expected.subtests), test_type)
    return _TEMPLATE % WPTResultsDiffGenerator().generate_tbody(
        expected, actual)


class WPTResultsDiffGenerator:
    def __init__(self, indent_per_level: int = 2):
        self.indent_per_level = indent_per_level

    def generate_tbody(self, expected: TestNode, actual: TestNode) -> str:
        assert set(expected.subtests) == set(actual.subtests)
        # TODO(crbug.com/1440565): A `<table>` should contain semantic `<tr>`
        # row and `<td>` cell elements instead of a bag of `<div>`s.
        html = self.generate_row(expected, actual)
        for subtest in expected.subtests:
            html += self.generate_row(expected.get_subtest(subtest),
                                      actual.get_subtest(subtest),
                                      level=1)
        return html

    def generate_row(self,
                     expected: TestNode,
                     actual: TestNode,
                     level: int = 0) -> str:
        row = f'<div>{self.get_spaces(level)}[{actual.name}]</div>'
        expected_result = _format_statuses(expected.get('expected'))
        actual_result = _format_statuses(actual.get('expected'))
        assert not actual.known_intermittent
        if actual.expected in {
                expected.expected, *expected.known_intermittent
        }:
            row += f'<div>{self.get_spaces(level + 1)}expected: {expected_result}</div>'
        elif actual.expected == 'NOTRUN':
            row += f'<div class=missing>{self.get_spaces(level + 1)}expected: {expected_result}</div>'
        else:
            row += (
                f'<div class=add>{self.get_spaces(level + 1)}expected: {expected_result}</div>'
                f'<div class=del>{self.get_spaces(level + 1)}actual: {actual_result}</div>'
            )
        row += '<div>\n</div>'
        return row

    def get_spaces(self, level: int) -> str:
        return '&nbsp;' * level * self.indent_per_level


def _format_statuses(status: Union[str, List[str]]) -> str:
    return status if isinstance(status, str) else f'[{", ".join(status)}]'
