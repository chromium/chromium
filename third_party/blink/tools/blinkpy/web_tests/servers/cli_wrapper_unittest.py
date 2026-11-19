# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import platform
import unittest
from unittest import mock

from blinkpy.web_tests.servers import cli_wrapper


class MockServer(object):
    def __init__(self, *args, **kwargs):
        self.args = args
        self.kwargs = kwargs
        self.start_called = False
        self.stop_called = False
        self.is_alive = True

    def start(self):
        self.start_called = True

    def stop(self):
        self.stop_called = True

    def alive(self):
        return self.is_alive


class CliWrapperTest(unittest.TestCase):
    def setUp(self):
        self.server = None

    @unittest.skipIf(
        platform.mac_ver()[0].startswith('12'),
        "Failing on macOS 12; see crbug.com/474036848",
    )
    def test_main_success(self):
        def mock_server_constructor(*args, **kwargs):
            self.server = MockServer(args, kwargs)
            return self.server

        def raise_exit():
            raise SystemExit

        cli_wrapper.main(mock_server_constructor, sleep_fn=raise_exit, argv=[])
        self.assertTrue(self.server.start_called)
        self.assertTrue(self.server.stop_called)

    @unittest.skipIf(
        platform.mac_ver()[0].startswith('12'),
        "Failing on macOS 12; see crbug.com/474036848",
    )
    def test_main_websocket_h3_opt_in(self):
        for websocket_h3_enabled in (False, True):
            with self.subTest(websocket_h3_enabled=websocket_h3_enabled):
                server_constructor = mock.Mock(return_value=MockServer())
                cli_wrapper.main(
                    server_constructor,
                    sleep_fn=mock.Mock(side_effect=SystemExit),
                    argv=['--enable-websocket-h3']
                    if websocket_h3_enabled
                    else [],
                )
                server_constructor.assert_called_once()
                port = server_constructor.call_args.args[0]
                self.assertEqual(
                    port.get_option('enable_websocket_h3'),
                    websocket_h3_enabled,
                )

    @unittest.skipIf(
        platform.mac_ver()[0].startswith('12'),
        "Failing on macOS 12; see crbug.com/474036848",
    )
    def test_main_server_error_after_start(self):
        def mock_server_constructor(*args, **kwargs):
            self.server = MockServer(args, kwargs)
            return self.server

        def server_error():
            self.server.is_alive = False

        cli_wrapper.main(
            mock_server_constructor, sleep_fn=server_error, argv=[]
        )
        self.assertTrue(self.server.start_called)
        self.assertTrue(self.server.stop_called)
