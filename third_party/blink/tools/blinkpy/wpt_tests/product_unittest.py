# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import unittest
from unittest import mock

from blinkpy.common.host_mock import MockHost
from blinkpy.wpt_tests import product


class _FakeSyncParallelizer:
    """Minimal synchronous stand-in for `devil.utils.parallelizer.SyncParallelizer`."""

    def __init__(self, objs):
        self._objs = list(objs)

    def __getattr__(self, name):
        def _call_all(*args, **kwargs):
            return [getattr(obj, name)(*args, **kwargs) for obj in self._objs]

        return _call_all


class ChromeAndroidProductTest(unittest.TestCase):
    def setUp(self):
        self.host = MockHost()
        self.port = self.host.port_factory.get('test-linux-trusty')
        self.avd_mock = mock.MagicMock()
        self.avd_instance = mock.MagicMock()
        self.avd_mock.AvdConfig.return_value.CreateInstance.return_value = (
            self.avd_instance
        )
        self.devil_env_mock = mock.MagicMock()
        self.devil_env_mock.config.FetchPath.return_value = '/mock/adb'
        self.device_utils_mock = mock.MagicMock()
        self.device_utils_mock.DeviceUtils.HealthyDevices.return_value = [
            mock.MagicMock(serial='emulator-5554')
        ]

        self._patchers = [
            mock.patch.object(product, 'do_delay_imports'),
            mock.patch.object(product, 'avd', self.avd_mock, create=True),
            mock.patch.object(
                product, 'devil_env', self.devil_env_mock, create=True
            ),
            mock.patch.object(
                product, 'device_utils', self.device_utils_mock, create=True
            ),
            mock.patch.object(
                product,
                'SyncParallelizer',
                _FakeSyncParallelizer,
                create=True,
            ),
        ]
        for patcher in self._patchers:
            patcher.start()

    def tearDown(self):
        for patcher in reversed(self._patchers):
            patcher.stop()

    def _make_options(self, **overrides):
        defaults = {
            'avd_config': '/mock/android.textpb',
            'browser_apk': None,
            'webview_provider': None,
            'emulator_window': False,
            'child_processes': 1,
            'wrapper': None,
        }
        defaults.update(overrides)
        return argparse.Namespace(**defaults)

    def test_chrome_android_starts_emulator_with_read_only_system(self):
        target = product.ChromeAndroid(self.port, self._make_options())
        target.get_devices()
        self.avd_instance.Start.assert_called_once_with(
            writable_system=False,
            window=False,
        )

    def test_webview_starts_emulator_with_writable_system(self):
        target = product.WebView(self.port, self._make_options())
        target.get_devices()
        self.avd_instance.Start.assert_called_once_with(
            writable_system=True,
            window=False,
        )
