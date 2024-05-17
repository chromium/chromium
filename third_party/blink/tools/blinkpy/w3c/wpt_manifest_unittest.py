# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import unittest

from blinkpy.common.host_mock import MockHost
from blinkpy.common.system.executive import ScriptError
from blinkpy.common.system.executive_mock import MockExecutive
from blinkpy.w3c.wpt_manifest import WPTManifest
from blinkpy.web_tests.port.test import TestPort, MOCK_WEB_TESTS


class WPTManifestUnitTest(unittest.TestCase):
    def test_ensure_manifest_copies_new_manifest(self):
        host = MockHost()
        port = TestPort(host)
        manifest_path = MOCK_WEB_TESTS + 'external/wpt/MANIFEST.json'

        self.assertFalse(host.filesystem.exists(manifest_path))
        WPTManifest.ensure_manifest(port)
        self.assertTrue(host.filesystem.exists(manifest_path))
        self.assertEqual(host.filesystem.written_files,
                         {manifest_path: b'{"manifest": "base"}'})

        self.assertEqual(host.executive.calls, [[
            port.python3_command(),
            '/mock-checkout/third_party/wpt_tools/wpt/wpt',
            'manifest',
            '-v',
            '--no-download',
            f'--tests-root={MOCK_WEB_TESTS + "external/wpt"}',
            '--url-base=/',
        ]])

    def test_ensure_manifest_updates_manifest_if_it_exists(self):
        host = MockHost()
        port = TestPort(host)
        manifest_path = MOCK_WEB_TESTS + 'external/wpt/MANIFEST.json'

        host.filesystem.write_text_file(manifest_path,
                                        '{"manifest": "NOT base"}')

        self.assertTrue(host.filesystem.exists(manifest_path))
        WPTManifest.ensure_manifest(port)
        self.assertTrue(host.filesystem.exists(manifest_path))
        self.assertEqual(host.filesystem.written_files,
                         {manifest_path: b'{"manifest": "base"}'})

        self.assertEqual(host.executive.calls, [[
            port.python3_command(),
            '/mock-checkout/third_party/wpt_tools/wpt/wpt',
            'manifest',
            '-v',
            '--no-download',
            f'--tests-root={MOCK_WEB_TESTS + "external/wpt"}',
            '--url-base=/',
        ]])

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
        self.assertEqual(host.executive.calls, [[
            port.python3_command(),
            '/mock-checkout/third_party/wpt_tools/wpt/wpt',
            'manifest',
            '-v',
            '--no-download',
            f'--tests-root={MOCK_WEB_TESTS + "wpt_internal"}',
            '--url-base=/wpt_internal/',
        ]])

    def test_all_test_types_are_identified(self):
        manifest_json = '''
{
    "items": {
        "manual": {
            "test-manual.html": [
                "d23fbb8c66def47e31ad01aa7a311064ba8fddbd",
                [null, {}]
            ]
        },
        "reftest": {
            "test-reference.html": [
                "d23fbb8c66def47e31ad01aa7a311064ba8fddbd",
                [null, {}]
            ]
        },
        "print-reftest": {
            "test-print.html": [
                "d23fbb8c66def47e31ad01aa7a311064ba8fddbd",
                [null, {}]
            ]
        },
        "testharness": {
            "test-harness.html": [
                "d23fbb8c66def47e31ad01aa7a311064ba8fddbd",
                [null, {}]
            ]
        },
        "crashtest": {
            "test-crash.html": [
                "d23fbb8c66def47e31ad01aa7a311064ba8fddbd",
                [null, {}]
            ]
        }
    }
}
        '''
        host = MockHost()
        host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'external/wpt/MANIFEST.json', manifest_json)
        manifest = WPTManifest.from_file(
            host.port_factory.get(),
            MOCK_WEB_TESTS + 'external/wpt/MANIFEST.json')

        self.assertEqual(manifest.get_test_type('test-manual.html'), 'manual')
        self.assertEqual(manifest.get_test_type('test-reference.html'),
                         'reftest')
        self.assertEqual(manifest.get_test_type('test-print.html'),
                         'print-reftest')
        self.assertEqual(manifest.get_test_type('test-harness.html'),
                         'testharness')
        self.assertEqual(manifest.get_test_type('test-crash.html'),
                         'crashtest')

    def test_does_not_throw_when_missing_some_test_types(self):
        manifest_json = '''
{
    "items": {
        "testharness": {
            "test.any.js": [
                "8d4b9a583f484741f4cd4e4940833a890c612656",
                ["test.any.html", {}]
            ]
        }
    }
}
        '''
        host = MockHost()
        host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'external/wpt/MANIFEST.json', manifest_json)
        manifest = WPTManifest.from_file(
            host.port_factory.get(),
            MOCK_WEB_TESTS + 'external/wpt/MANIFEST.json')
        self.assertTrue(manifest.is_test_file('test.any.js'))
        self.assertEqual(manifest.extract_reference_list('foo/bar.html'), [])

    def test_all_url_items_skips_jsshell_tests(self):
        manifest_json = '''
{
    "items": {
        "manual": {},
        "reftest": {},
        "testharness": {
            "test.any.js": [
                "d23fbb8c66def47e31ad01aa7a311064ba8fddbd",
                ["test.any.html", {}],
                [null, {"jsshell": true}]
            ]
        }
    }
}
        '''
        host = MockHost()
        host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'external/wpt/MANIFEST.json', manifest_json)
        manifest = WPTManifest.from_file(
            host.port_factory.get(),
            MOCK_WEB_TESTS + 'external/wpt/MANIFEST.json')

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
                "d23fbb8c66def47e31ad01aa7a311064ba8fddbd",
                ["test.any.html", {}],
                ["test.any.worker.html", {}]
            ]
        }
    }
}       '''
        host = MockHost()
        host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'external/wpt/MANIFEST.json', manifest_json)
        manifest = WPTManifest.from_file(
            host.port_factory.get(),
            MOCK_WEB_TESTS + 'external/wpt/MANIFEST.json')
        # Ensure that we can get back to `test.any.js` from both of the tests.
        self.assertEqual(
            manifest.file_path_for_test_url('test.any.html'), 'test.any.js')
        self.assertEqual(
            manifest.file_path_for_test_url('test.any.worker.html'),
            'test.any.js')

    def test_crash_tests(self):
        # Test that the manifest recognizes crash tests and that is_crash_test
        # correctly identifies only crash tests in the manifest.
        manifest_json = '''
{
    "items": {
        "manual": {},
        "reftest": {},
        "testharness": {
            "test.html": [
                "d23fbb8c66def47e31ad01aa7a311064ba8fddbd",
                [null, {}]
            ]
        },
        "crashtest": {
            "test-crash.html": [
                "d23fbb8c66def47e31ad01aa7a311064ba8fddbd",
                [null, {}]
            ],
            "test-with-variant-crash.html": [
                "d23fbb8c66def47e31ad01aa7a311064ba8fddbd",
                ["/test-with-variant-crash.html?xyz", {}]
            ]
        }
    }
}
        '''
        host = MockHost()
        host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'external/wpt/MANIFEST.json', manifest_json)
        manifest = WPTManifest.from_file(
            host.port_factory.get(),
            MOCK_WEB_TESTS + 'external/wpt/MANIFEST.json')

        self.assertTrue(manifest.is_crash_test('test-crash.html'))
        self.assertTrue(
            manifest.is_crash_test('test-with-variant-crash.html?xyz'))
        self.assertFalse(manifest.is_crash_test('test.html'))
        self.assertFalse(manifest.is_crash_test('test-variant-crash.html'))
        self.assertFalse(manifest.is_crash_test('different-test-crash.html'))

    def test_extract_reference_list(self):
        host = MockHost()
        host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'external/wpt/MANIFEST.json',
            json.dumps({
                'items': {
                    'reftest': {
                        'dir': {
                            'reftest.html': [
                                'd23fbb8c66def47e31ad01aa7a311064ba8fddbd',
                                [
                                    None,
                                    [['/dir/reftest-ref.html', '=='],
                                     ['/dir/reftest-mismatch-ref.html', '!=']],
                                    {},
                                ],
                            ],
                            'reftest-with-variant.html': [
                                'd23fbb8c66def47e31ad01aa7a311064ba8fddbd',
                                [
                                    '/dir/reftest-with-variant.html?xyz',
                                    [['about:blank', '==']],
                                    {},
                                ],
                            ],
                        },
                    },
                },
            }))
        manifest = WPTManifest.from_file(
            host.port_factory.get(),
            MOCK_WEB_TESTS + 'external/wpt/MANIFEST.json')

        self.assertEqual(manifest.extract_reference_list('dir/reftest.html'), [
            ('==', '/dir/reftest-ref.html'),
            ('!=', '/dir/reftest-mismatch-ref.html'),
        ])
        self.assertEqual(
            manifest.extract_reference_list(
                'dir/reftest-with-variant.html?xyz'), [
                    ('==', 'about:blank'),
                ])

    def test_extract_fuzzy_metadata(self):
        manifest_json = '''
{
    "items": {
        "reftest": {
            "not_fuzzy.html": [
                "d23fbb8c66def47e31ad01aa7a311064ba8fddbd",
                [
                    null,
                    [
                        [
                            "not_fuzzy-ref.html",
                            "=="
                        ]
                    ],
                    {}
                ]
            ],
            "fuzzy.html": [
                "d23fbb8c66def47e31ad01aa7a311064ba8fddbd",
                [
                    null,
                    [
                        [
                            "fuzzy-ref.html",
                            "=="
                        ]
                    ],
                    {
                        "fuzzy": [
                            [
                                null,
                                [
                                    [2, 2],
                                    [40, 40]
                                ]
                            ]
                        ]
                    }
                ]
            ]
        },
        "print-reftest": {
            "fuzzy-print.html": [
                "d23fbb8c66def47e31ad01aa7a311064ba8fddbd",
                [
                    null,
                    [
                        [
                            "fuzzy-ref.html",
                            "=="
                        ]
                    ],
                    {
                        "fuzzy": [
                            [
                                null,
                                [
                                    [3, 10],
                                    [20, 100]
                                ]
                            ]
                        ]
                    }
                ]
            ]
        },
        "testharness": {
            "not_a_reftest.html": [
                "d23fbb8c66def47e31ad01aa7a311064ba8fddbd",
                [null, {}]
            ]
        }
    }
}
        '''

        host = MockHost()
        host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'external/wpt/MANIFEST.json', manifest_json)
        manifest = WPTManifest.from_file(
            host.port_factory.get(),
            MOCK_WEB_TESTS + 'external/wpt/MANIFEST.json')

        self.assertEqual(
            manifest.extract_fuzzy_metadata('fuzzy.html'),
            ([2, 2], [40, 40]),
        )

        self.assertEqual(
            manifest.extract_fuzzy_metadata('fuzzy-print.html'),
            ([3, 10], [20, 100]),
        )

        self.assertEqual(manifest.extract_fuzzy_metadata('not_fuzzy.html'),
                         (None, None))
        self.assertEqual(manifest.extract_fuzzy_metadata('not_a_reftest.html'),
                         (None, None))

    def test_extract_pac(self):
        manifest_json = '''
{
    "items": {
        "testharness": {
     "without-pac.html": [
      "df2f8b048c370d3ab009946d73d7de6f8a412471",
      [
       null,
       {
       }
      ]
     ],
     "with-pac.html": [
      "598836d376813b754366d49616b39a4183cd3753",
      [
       null,
       {
        "pac": "proxy.pac"
       }
      ]
     ]
    }
    }
}
        '''

        host = MockHost()
        host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'external/wpt/MANIFEST.json', manifest_json)
        manifest = WPTManifest.from_file(
            host.port_factory.get(),
            MOCK_WEB_TESTS + 'external/wpt/MANIFEST.json')

        self.assertEqual(
            manifest.extract_test_pac('with-pac.html'),
            "proxy.pac",
        )

        self.assertEqual(manifest.extract_test_pac('without-pac.html'), None)
