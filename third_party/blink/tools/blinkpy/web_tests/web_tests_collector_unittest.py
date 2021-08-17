# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from blinkpy.common.path_finder import RELATIVE_WEB_TESTS
from blinkpy.common.system.log_testing import LoggingTestCase
from blinkpy.common.system.system_host_mock import MockSystemHost
from blinkpy.web_tests.port.base import ALL_TESTS_BY_DIRECTORIES
from blinkpy.web_tests.port.factory import PortFactory
from blinkpy.web_tests.web_tests_collector import WebTestsCollector
from blinkpy.w3c.wpt_manifest import BASE_MANIFEST_NAME

MOCK_WEB_TESTS = '/mock-checkout/' + RELATIVE_WEB_TESTS

class WebTestsCollectorTest(LoggingTestCase):

    def mock_host(self):
        host = MockSystemHost()
        host.port_factory = PortFactory(host)
        fs = host.filesystem
        fs.write_text_file(MOCK_WEB_TESTS + 'http/pass/foo.html', ' ')
        fs.write_text_file(MOCK_WEB_TESTS + 'http/pass/bar.html', ' ')
        fs.write_text_file(MOCK_WEB_TESTS + 'dom/dom1.html', ' ')
        fs.write_text_file(MOCK_WEB_TESTS + 'dom/dom2.html', ' ')
        fs.write_text_file(MOCK_WEB_TESTS + 'external/wpt/css/foo.html', ' ')
        fs.write_text_file(fs.join(MOCK_WEB_TESTS, 'external', BASE_MANIFEST_NAME),
                           '{"manifest": "base"}')
        data = ('{\n'
                '  "dom/": [\n'
                '    "dom/dom1.html", \n'
                '    "dom/dom2.html"\n'
                '  ], \n'
                '  "external/wpt/css/": [\n'
                '    "external/wpt/css/foo.html"\n'
                '  ], \n'
                '  "http/pass/": [\n'
                '    "http/pass/bar.html", \n'
                '    "http/pass/foo.html"\n'
                '  ]\n'
                '}')
        fs.write_text_file(MOCK_WEB_TESTS + ALL_TESTS_BY_DIRECTORIES, data)
        return host

    def test_add_test(self):
        host = self.mock_host()
        fs = host.filesystem
        fs.write_text_file(MOCK_WEB_TESTS + 'dom/dom3.html', ' ')
        fs.write_text_file(MOCK_WEB_TESTS + 'css/bar.html', ' ')
        collector = WebTestsCollector(host)
        collector.port.web_tests_dir = lambda: MOCK_WEB_TESTS
        path = MOCK_WEB_TESTS + ALL_TESTS_BY_DIRECTORIES
        collector.collect_tests(path, ['css', 'dom'])
        data = ('{\n'
                '  "css/": [\n'
                '    "css/bar.html"\n'
                '  ], \n'
                '  "dom/": [\n'
                '    "dom/dom1.html", \n'
                '    "dom/dom2.html", \n'
                '    "dom/dom3.html"\n'
                '  ], \n'
                '  "external/wpt/css/": [\n'
                '    "external/wpt/css/foo.html"\n'
                '  ], \n'
                '  "http/pass/": [\n'
                '    "http/pass/bar.html", \n'
                '    "http/pass/foo.html"\n'
                '  ]\n'
                '}')
        self.assertEqual(fs.read_text_file(path), data)

    def test_remove_test(self):
        host = self.mock_host()
        fs = host.filesystem
        fs.remove(MOCK_WEB_TESTS + 'dom/dom2.html')
        fs.remove(MOCK_WEB_TESTS + 'http/pass/foo.html')
        fs.remove(MOCK_WEB_TESTS + 'http/pass/bar.html')
        collector = WebTestsCollector(host)
        collector.port.web_tests_dir = lambda: MOCK_WEB_TESTS
        path = fs.join(MOCK_WEB_TESTS, ALL_TESTS_BY_DIRECTORIES)
        collector.collect_tests(path, ['dom', 'http'])
        data = ('{\n'
                '  "dom/": [\n'
                '    "dom/dom1.html"\n'
                '  ], \n'
                '  "external/wpt/css/": [\n'
                '    "external/wpt/css/foo.html"\n'
                '  ]\n'
                '}')
        self.assertEqual(fs.read_text_file(path), data)
