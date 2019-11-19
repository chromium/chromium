# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from blinkpy.common.host_mock import MockHost
from blinkpy.common.system.executive import ScriptError
from blinkpy.common.system.executive_mock import MockExecutive
from blinkpy.w3c.wpt_manifest import WPTManifest
from blinkpy.web_tests.port.test import TestPort, WEB_TEST_DIR


class WPTManifestUnitTest(unittest.TestCase):

    def test_ensure_manifest_copies_new_manifest(self):
        host = MockHost()
        port = TestPort(host)
        manifest_path = WEB_TEST_DIR + '/external/wpt/MANIFEST.json'

        self.assertFalse(host.filesystem.exists(manifest_path))
        WPTManifest.ensure_manifest(port)
        self.assertTrue(host.filesystem.exists(manifest_path))
        self.assertEqual(host.filesystem.written_files, {manifest_path: '{"manifest": "base"}'})

        self.assertEqual(
            host.executive.calls,
            [
                [
                    'python',
                    '/mock-checkout/third_party/blink/tools/blinkpy/third_party/wpt/wpt/wpt',
                    'manifest',
                    '--no-download',
                    '--tests-root',
                    WEB_TEST_DIR + '/external/wpt',
                ]
            ]
        )

    def test_ensure_manifest_updates_manifest_if_it_exists(self):
        host = MockHost()
        port = TestPort(host)
        manifest_path = WEB_TEST_DIR + '/external/wpt/MANIFEST.json'

        host.filesystem.write_text_file(manifest_path, '{"manifest": "NOT base"}')

        self.assertTrue(host.filesystem.exists(manifest_path))
        WPTManifest.ensure_manifest(port)
        self.assertTrue(host.filesystem.exists(manifest_path))
        self.assertEqual(host.filesystem.written_files, {manifest_path: '{"manifest": "base"}'})

        self.assertEqual(
            host.executive.calls,
            [
                [
                    'python',
                    '/mock-checkout/third_party/blink/tools/blinkpy/third_party/wpt/wpt/wpt',
                    'manifest',
                    '--no-download',
                    '--tests-root',
                    WEB_TEST_DIR + '/external/wpt',
                ]
            ]
        )

    def test_ensure_manifest_raises_exception(self):
        host = MockHost()
        host.executive = MockExecutive(should_throw=True)
        port = TestPort(host)

        with self.assertRaises(ScriptError):
            WPTManifest.ensure_manifest(port)

    def test_ensure_manifest_takes_optional_dest(self):
        host = MockHost()
        port = TestPort(host)
        WPTManifest.ensure_manifest(port, 'wpt_internal')
        self.assertEqual(
            host.executive.calls,
            [
                [
                    'python',
                    '/mock-checkout/third_party/blink/tools/blinkpy/third_party/wpt/wpt/wpt',
                    'manifest',
                    '--no-download',
                    '--tests-root',
                    WEB_TEST_DIR + '/wpt_internal',
                ]
            ]
        )

    def test_does_not_throw_when_missing_some_test_types(self):
        manifest_json = '''
{
    "items": {
        "testharness": {
            "test.any.js": [
                ["/test.any.html", {}]
            ]
        }
    }
}
        '''
        manifest = WPTManifest(manifest_json)
        self.assertTrue(manifest.is_test_file('test.any.js'))
        self.assertEqual(manifest.all_url_items(),
                         {u'/test.any.html': [u'/test.any.html', {}]})
        self.assertEqual(manifest.extract_reference_list('/foo/bar.html'), [])

    def test_all_url_items_skips_jsshell_tests(self):
        manifest_json = '''
{
    "items": {
        "manual": {},
        "reftest": {},
        "testharness": {
            "test.any.js": [
                ["/test.any.html", {}],
                ["/test.any.js", {"jsshell": true}]
            ]
        }
    }
}
        '''
        manifest = WPTManifest(manifest_json)
        self.assertEqual(manifest.all_url_items(),
                         {u'/test.any.html': [u'/test.any.html', {}]})

    def test_file_for_test(self):
        # Test that we can lookup a test's filename for various cases like
        # variants and multi-globals.
        manifest_json = '''
 {
    "items": {
        "manual": {},
        "reftest": {},
        "testharness": {
            "test.any.js": [
                ["/test.any.html", {}],
                ["/test.any.worker.html", {}]
            ]
        }
    }
}       '''
        manifest = WPTManifest(manifest_json)
        self.assertEqual(manifest.all_url_items(),
                         {u'/test.any.html': [u'/test.any.html', {}],
                          u'/test.any.worker.html': [u'/test.any.worker.html', {}]})
        # Ensure that we can get back to `test.any.js` from both of the tests.
        self.assertEqual(manifest.file_path_for_test_url('/test.any.html'),
                         'test.any.js')
        self.assertEqual(manifest.file_path_for_test_url('/test.any.worker.html'),
                         'test.any.js')
