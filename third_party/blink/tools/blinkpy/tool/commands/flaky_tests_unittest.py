# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import optparse

from blinkpy.tool.commands import flaky_tests
from blinkpy.tool.commands.command_test import CommandsTest
from blinkpy.tool.mock_tool import MockBlinkTool
from blinkpy.web_tests.builder_list import BuilderList
from blinkpy.web_tests.layout_package import bot_test_expectations


class FakeBotTestExpectations(object):
    def expectation_lines(self):
        return []


class FakeBotTestExpectationsFactory(object):
    FAILURE_MAP = {
        'C': 'CRASH',
        'F': 'FAIL',
        'N': 'NO DATA',
        'P': 'PASS',
        'T': 'TIMEOUT',
        'Y': 'NOTRUN',
        'X': 'SKIP',
        'K': 'LEAK'
    }

    def __init__(self, builders):
        self.builders = builders

    def _expectations_from_test_data(self, builder, test_data):
        test_data[bot_test_expectations.ResultsJSON.
                  FAILURE_MAP_KEY] = self.FAILURE_MAP
        json_dict = {
            builder: test_data,
        }
        results = bot_test_expectations.ResultsJSON(builder, json_dict)
        return bot_test_expectations.BotTestExpectations(
            results, self.builders,
            self.builders.specifiers_for_builder(builder))

    def expectations_for_builder(self, builder):
        if builder == 'foo-builder':
            return self._expectations_from_test_data(
                builder, {
                    'tests': {
                        'pass.html': {
                            'results': [[2, 'FFFP']],
                            'expected': 'PASS'
                        },
                    }
                })

        if builder == 'bar-builder':
            return self._expectations_from_test_data(
                builder, {
                    'tests': {
                        'pass.html': {
                            'results': [[2, 'TTTP']],
                            'expected': 'PASS'
                        },
                    }
                })

        return FakeBotTestExpectations()


class FlakyTestsTest(CommandsTest):
    @staticmethod
    def fake_builders_list():
        return BuilderList({
            'foo-builder': {
                'port_name': 'dummy-port',
                'specifiers': ['Linux', 'Release']
            },
            'bar-builder': {
                'port_name': 'dummy-port',
                'specifiers': ['Mac', 'Debug']
            },
        })

    def test_merge_lines(self):
        command = flaky_tests.FlakyTests()
        factory = FakeBotTestExpectationsFactory(self.fake_builders_list())

        lines = command._collect_expectation_lines(
            ['foo-builder', 'bar-builder'], factory)
        self.assertEqual(len(lines), 2)
        self.assertEqual(lines[0].results, set(['FAIL', 'PASS']))
        self.assertEqual(set(lines[0].tags), set(['Linux']))
        self.assertEqual(lines[1].results, set(['TIMEOUT', 'PASS']))
        self.assertEqual(set(lines[1].tags), set(['Mac']))

    def test_integration(self):
        command = flaky_tests.FlakyTests()
        tool = MockBlinkTool()
        tool.builders = self.fake_builders_list()
        command.expectations_factory = FakeBotTestExpectationsFactory
        options = optparse.Values({'upload': True})
        expected_stdout = flaky_tests.FlakyTests.OUTPUT % (
            flaky_tests.FlakyTests.HEADER, '',
            flaky_tests.FlakyTests.FLAKINESS_DASHBOARD_URL % '') + '\n'

        self.assert_execute_outputs(
            command,
            options=options,
            tool=tool,
            expected_stdout=expected_stdout)
