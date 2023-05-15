# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from blinkpy.common import path_finder

path_finder.bootstrap_wpt_imports()

from wptrunner import manifestexpected
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
.miss { background: #f5e642; }
</style>
</head>
<body><table>%s</table></body>
</html>
"""


def create_top_level_node(node):
    top_level = wptnode.DataNode()
    top_level.append(node)
    return top_level


# TODO(crbug.com/1440565): right now we're on the fly comparing subtests which may be empty and assuming a positive
# result. A potential refactor could be to simply build the trees earlier so they are the same structure,
# that way we can also write an expected_results file for the error.
def wpt_results_diff(expected_node, actual_node, file_path, test_type):
    """
        assumption for .ini files is that first arg is expected text and second is actual text
    """

    expected = None
    if expected_node:
        expected_top_level = create_top_level_node(expected_node)
        expected = static.compile_ast(expected_top_level, {},
                                      manifestexpected.data_cls_getter,
                                      test_path=file_path).get_test(
                                          actual_node.data)

    actual_top_level = create_top_level_node(actual_node)
    actual = static.compile_ast(actual_top_level, {},
                                manifestexpected.data_cls_getter,
                                test_path=file_path).get_test(actual_node.data)

    return _TEMPLATE % WPTResultsDiffGenerator(test_type).generate_tbody(
        expected, actual)


class WPTResultsDiffGenerator:
    def __init__(self, test_type):
        self.tbody = ""
        self.test_type = test_type

    def generate_tbody(self, expected_node, actual_node):

        self.compare_trees(expected_node, actual_node)
        return self.tbody

    def get_spaces(self, space_count):
        return "&nbsp;" * space_count

    def build_tbody_expected(self, spaces, name, result):
        self.tbody += f"<div>{self.get_spaces(spaces)}[{name}]</div>"
        self.tbody += f"<div>{self.get_spaces(spaces + 2)}expected: {result}</div>"
        self.tbody += f"<div>\n</div>"

    def build_tbody_diff(self, spaces, name, expected_result, actual_result):
        self.tbody += f"<div>{self.get_spaces(spaces)}[{name}]</div>"
        self.tbody += f"<div class=add>{self.get_spaces(spaces + 2)}expected: {expected_result}</div>"
        self.tbody += f"<div class=del>{self.get_spaces(spaces + 2)}actual: {actual_result}</div>"
        self.tbody += f"<div>\n</div>"

    def build_tbody_miss(self, spaces, name, expected_result):
        self.tbody += f"<div>{self.get_spaces(spaces)}[{name}]</div>"
        self.tbody += f"<div class=miss>{self.get_spaces(spaces + 2)}expected: {expected_result}</div>"
        self.tbody += f"<div>\n</div>"

    def compare_single_results(self, actual_result, expected_result, spaces,
                               name):
        if actual_result == expected_result:
            self.build_tbody_expected(spaces, name, expected_result)
        else:
            self.build_tbody_diff(spaces, name, expected_result, actual_result)

    def compare_trees(self, expected_node, actual_node):
        # important difference between expected and actual
        # is that expected will omit PASS results

        default_pass_value = "OK" if self.test_type == "testharness" else "PASS"

        if expected_node:
            # Test comparison
            if expected_node.has_key("expected"):
                self.compare_single_results(actual_node.expected,
                                            expected_node.expected, 0,
                                            actual_node.id)
            else:
                self.compare_single_results(actual_node.expected,
                                            default_pass_value, 0,
                                            actual_node.id)

            # Subtest comparison
            for actual_subtest in actual_node.subtests:
                expected_result = "PASS"

                if expected_node.get_subtest(
                        actual_subtest) and expected_node.get_subtest(
                            actual_subtest).has_key("expected"):
                    expected_result = expected_node.get_subtest(
                        actual_subtest).expected

                self.compare_single_results(
                    actual_node.get_subtest(actual_subtest).expected,
                    expected_result, 2, actual_subtest)

            # Case where a subtest is not run, but it exists in expected subtests
            for expected_subtest in expected_node.subtests:
                if actual_node.get_subtest(expected_subtest):
                    continue
                expected_result = "PASS"
                if expected_node.get_subtest(
                        expected_subtest) and expected_node.get_subtest(
                            expected_subtest).has_key("expected"):
                    expected_result = expected_node.get_subtest(
                        expected_subtest).expected
                self.build_tbody_miss(2, expected_subtest, expected_result)
        else:
            # expected node is None, so no .ini file, assume all passes

            # Test comparison
            self.compare_single_results(actual_node.expected,
                                        default_pass_value, 0, actual_node.id)

            # Subtest comparison
            for actual_subtest in actual_node.subtests:
                self.compare_single_results(
                    actual_node.get_subtest(actual_subtest).expected, "PASS",
                    2, actual_subtest)
