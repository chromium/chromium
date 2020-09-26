# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import logging

from blinkpy.common.host_mock import MockHost
from blinkpy.common.system.log_testing import LoggingTestCase
from blinkpy.web_tests.port.test import TestPort
from blinkpy.web_tests.servers.wptserve import WPTServe


class TestWPTServe(LoggingTestCase):
    def setUp(self):
        super(TestWPTServe, self).setUp()
        self.host = MockHost()
        self.port = TestPort(self.host)
        self.host.filesystem.write_text_file(
            '/mock-checkout/third_party/blink/tools/blinkpy/third_party/wpt/wpt.config.json',
            '{"ports": {}, "aliases": []}')

    # pylint: disable=protected-access

    def test_init_start_cmd_without_ws_handlers(self):
        server = WPTServe(self.port, '/foo')
        self.assertEqual(server._start_cmd, [
            'python',
            '-u',
            '/mock-checkout/third_party/blink/tools/blinkpy/third_party/wpt/wpt/wpt',
            'serve',
            '--config',
            server._config_file,
            '--doc_root',
            '/test.checkout/wtests/external/wpt',
            '--no-h2',
        ])

    def test_init_start_cmd_with_ws_handlers(self):
        self.host.filesystem.maybe_make_directory(
            '/test.checkout/wtests/external/wpt/websockets/handlers')
        server = WPTServe(self.port, '/foo')
        self.assertEqual(server._start_cmd, [
            'python',
            '-u',
            '/mock-checkout/third_party/blink/tools/blinkpy/third_party/wpt/wpt/wpt',
            'serve',
            '--config',
            server._config_file,
            '--doc_root',
            '/test.checkout/wtests/external/wpt',
            '--no-h2',
            '--ws_doc_root',
            '/test.checkout/wtests/external/wpt/websockets/handlers',
        ])

    def test_init_env(self):
        server = WPTServe(self.port, '/foo')
        self.assertEqual(
            server._env, {
                'MOCK_ENVIRON_COPY': '1',
                'PATH': '/bin:/mock/bin',
                'PYTHONPATH': '/mock-checkout/third_party/pywebsocket3/src'
            })

    def test_prepare_config(self):
        server = WPTServe(self.port, '/foo')
        server._prepare_config()
        config = json.loads(
            self.port._filesystem.read_text_file(server._config_file))
        self.assertEqual(len(config['aliases']), 1)
        self.assertDictEqual(config['aliases'][0], {
            'url-path': '/gen/',
            'local-dir': '/mock-checkout/out/Release/gen'
        })

    def test_start_with_stale_pid(self):
        # Allow asserting about debug logs.
        self.set_logging_level(logging.DEBUG)

        self.host.filesystem.write_text_file('/log_file_dir/access_log', 'foo')
        self.host.filesystem.write_text_file('/log_file_dir/error_log', 'foo')
        self.host.filesystem.write_text_file('/tmp/pidfile', '7')

        server = WPTServe(self.port, '/log_file_dir')
        server._pid_file = '/tmp/pidfile'
        server._check_that_all_ports_are_available = lambda: True
        server._is_server_running_on_all_ports = lambda: True

        server.start()
        # PID file should be overwritten (MockProcess.pid == 42)
        self.assertEqual(server._pid, 42)
        self.assertEqual(self.host.filesystem.files[server._pid_file], '42')
        # Config file should exist.
        json.loads(self.port._filesystem.read_text_file(server._config_file))

        logs = self.logMessages()
        self.assertEqual(len(logs), 4)
        self.assertEqual(logs[:2], [
            'DEBUG: stale wptserve pid file, pid 7\n',
            'DEBUG: pid 7 is not running\n',
        ])
        self.assertTrue(logs[-2].startswith('DEBUG: Starting wptserve server'))
        self.assertEqual(logs[-1],
                         'DEBUG: wptserve successfully started (pid = 42)\n')

    def test_start_with_unkillable_zombie_process(self):
        # Allow asserting about debug logs.
        self.set_logging_level(logging.DEBUG)

        self.host.filesystem.write_text_file('/log_file_dir/access_log', 'foo')
        self.host.filesystem.write_text_file('/log_file_dir/error_log', 'foo')
        self.host.filesystem.write_text_file('/tmp/pidfile', '7')

        server = WPTServe(self.port, '/log_file_dir')
        server._pid_file = '/tmp/pidfile'
        server._check_that_all_ports_are_available = lambda: True
        server._is_server_running_on_all_ports = lambda: True

        # Simulate a process that never gets killed.
        self.host.executive.check_running_pid = lambda _: True

        server.start()
        self.assertEqual(server._pid, 42)
        self.assertEqual(self.host.filesystem.files[server._pid_file], '42')

        # In this case, we'll try to kill the process repeatedly,
        # then give up and just try to start a new process anyway.
        logs = self.logMessages()
        self.assertEqual(len(logs), 43)
        self.assertEqual(logs[:2], [
            'DEBUG: stale wptserve pid file, pid 7\n',
            'DEBUG: pid 7 is running, killing it\n'
        ])
        self.assertTrue(logs[-2].startswith('DEBUG: Starting wptserve server'))
        self.assertEqual(logs[-1],
                         'DEBUG: wptserve successfully started (pid = 42)\n')

    def test_stop_running_server_removes_temp_files(self):
        server = WPTServe(self.port, '/foo')
        server._stop_running_server()
        self.assertFalse(self.host.filesystem.exists(server._pid_file))
        self.assertFalse(self.host.filesystem.exists(server._config_file))
