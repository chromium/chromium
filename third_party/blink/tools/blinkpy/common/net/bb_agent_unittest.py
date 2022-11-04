# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from blinkpy.common.host_mock import MockHost
from blinkpy.common.net.bb_agent import BBAgent
from blinkpy.common.net.results_fetcher import Build
from blinkpy.common.system.executive_mock import MockExecutive


class BBAgentTest(unittest.TestCase):
    def setUp(self):
        self._host = MockHost()
        self._host.executive = MockExecutive(output='')
        self._bb_agent = BBAgent(self._host)

    def test_get_latest_build(self):
        self._bb_agent.get_finished_build('linux-blink-rel', 0)
        self.assertEqual(self._host.executive.calls[-1],
                         [self._bb_agent.bb_bin_path, 'ls', '-1', '-json',
                          '-status', 'ended', 'chromium/ci/linux-blink-rel'])

    def test_get_latest_try_build(self):
        self._bb_agent.get_finished_build('linux-blink-rel', 0, try_build=True)
        self.assertEqual(self._host.executive.calls[-1],
                         [self._bb_agent.bb_bin_path, 'ls', '-1', '-json',
                          '-status', 'ended', 'chromium/try/linux-blink-rel'])

    def test_get_not_latest_build(self):
        self._bb_agent.get_finished_build('linux-blink-rel', 1024)
        self.assertEqual(self._host.executive.calls[-1], [
            self._bb_agent.bb_bin_path, 'ls', '100', '-json', '-status',
            'ended', 'chromium/ci/linux-blink-rel'
        ])

    def test_get_not_latest_try_build(self):
        self._bb_agent.get_finished_build('linux-blink-rel',
                                          1024,
                                          try_build=True)
        self.assertEqual(self._host.executive.calls[-1], [
            self._bb_agent.bb_bin_path, 'ls', '100', '-json', '-status',
            'ended', 'chromium/try/linux-blink-rel'
        ])

    def test_get_latest_build_results(self):
        host = MockHost()
        host.executive = MockExecutive(
            output='{"number": 422, "id": "abcd"}')

        bb_agent = BBAgent(host)
        build = bb_agent.get_finished_build('linux-blink-rel', 0)
        self.assertEqual(build, Build('linux-blink-rel', 422, 'abcd'))
        bb_agent.get_build_test_results(build, 'blink_web_tests')
        self.assertEqual(host.executive.calls[-1],
                         [bb_agent.bb_bin_path, 'log', '-nocolor',
                          build.build_id, 'blink_web_tests', 'json.output'])

    def test_get_not_latest_build_results(self):
        host = MockHost()
        host.executive = MockExecutive(output='{"number": 420, "id": "1234"}\r'
                                       '{"number": 422, "id": "abcd"}\r')

        bb_agent = BBAgent(host)
        build = bb_agent.get_finished_build('linux-blink-rel', 422)
        self.assertEqual(build, Build('linux-blink-rel', 422, 'abcd'))

        host.executive = MockExecutive(output='{}')
        bb_agent.get_build_test_results(build, 'blink_web_tests')
        self.assertEqual(host.executive.calls[-1], [
            bb_agent.bb_bin_path, 'log', '-nocolor', build.build_id,
            'blink_web_tests', 'json.output'
        ])
