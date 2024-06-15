# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import contextlib
import json
import textwrap
import unittest
from unittest import mock

from blinkpy.common import path_finder
from blinkpy.common.host_mock import MockHost
from blinkpy.wpt_tests.test_loader import (
    allow_any_subtests_on_timeout,
    TestLoader,
    wpt_url_to_blink_test,
)

path_finder.bootstrap_wpt_imports()
from tools.manifest.manifest import load_and_update
from wptrunner import wptlogging, wpttest
from wptrunner.testloader import Subsuite


class TestLoaderTestCase(unittest.TestCase):
    def setUp(self):
        self.host = MockHost()
        self.fs = self.host.filesystem
        self.finder = path_finder.PathFinder(self.fs)
        self.fs.write_text_file(
            self.finder.path_from_wpt_tests('MANIFEST.json'),
            json.dumps({
                'version': 8,
                'items': {
                    'testharness': {
                        'variant.html': [
                            'abcdef',
                            ['variant.html?foo=bar/abc', {}],
                            ['variant.html?foo=baz', {}],
                        ],
                        'dir': {
                            'multiglob.https.any.js': [
                                '123456',
                                ['dir/multiglob.https.any.html', {}],
                                ['dir/multiglob.https.any.worker.html', {}],
                            ],
                        },
                    },
                    'reftest': {
                        'reftest.html': [
                            '7890ab',
                            [None, [['reftest-ref.html', '==']], {}],
                        ],
                    },
                },
            }))
        wptlogging.setup({}, {})

    @contextlib.contextmanager
    def _make_loader(self, **kwargs):
        port = self.host.port_factory.get('test-linux-trusty')
        with self.fs.patch_builtins():
            manifest = load_and_update(
                self.finder.path_from_wpt_tests(),
                self.finder.path_from_wpt_tests('MANIFEST.json'),
                '/',
                update=False,
                parallel=False)
            test_root = {
                'url_base': manifest.url_base,
                'tests_path': manifest.tests_root,
                'metadata_path': manifest.tests_root,
            }
            yield TestLoader(port, {manifest: test_root},
                             ['testharness', 'reftest'],
                             base_run_info={},
                             **kwargs)

    def _load_metadata(self, test_path: str, virtual_suite: str = ''):
        with self._make_loader() as loader:
            run_info = {**loader.base_run_info, 'virtual_suite': virtual_suite}
            manifest, *_ = loader.manifests
            inherit_metadata, test_metadata = loader.load_metadata(
                run_info, manifest, manifest.tests_root, test_path)
        self.assertEqual(inherit_metadata, [])
        return test_metadata

    def test_load_all_pass(self):
        test_file = self._load_metadata('variant.html')
        self.assertIsNone(test_file)

    def test_load_exp_line_fail_testharness(self):
        self.fs.write_text_file(
            self.finder.path_from_web_tests('TestExpectations'),
            textwrap.dedent("""\
                # tags: [ Linux Mac Win ]
                # results: [ Failure ]
                [ Linux ] external/wpt/variant.html?foo=baz [ Failure ]
                """))
        test_file = self._load_metadata('variant.html')

        test = test_file.get_test('variant.html?foo=baz')
        self.assertEqual(test.expected, 'OK')
        self.assertEqual(test.known_intermittent,
                         ['ERROR', 'PRECONDITION_FAILED'])
        self.assertFalse(test.disabled)

        # Any status is allowed, even without an explicit baseline.
        subtest = test.get_subtest('implicit subtest')
        self.assertEqual(subtest.expected, 'FAIL')
        self.assertEqual(subtest.known_intermittent,
                         ['PASS', 'TIMEOUT', 'PRECONDITION_FAILED', 'NOTRUN'])
        self.assertFalse(subtest.disabled)
        self.assertFalse(subtest.has_key('expected-fail-message'))

    def test_load_exp_line_fail_reftest(self):
        self.fs.write_text_file(
            self.finder.path_from_web_tests('TestExpectations'),
            textwrap.dedent("""\
                # tags: [ Linux Mac Win ]
                # results: [ Failure ]
                [ Linux ] external/wpt/reftest* [ Failure ]
                """))
        test_file = self._load_metadata('reftest.html')
        test = test_file.get_test('reftest.html')
        self.assertEqual(test.expected, 'FAIL')
        self.assertEqual(test.known_intermittent, ['PASS', 'ERROR'])
        self.assertFalse(test.disabled)
        self.assertIsNone(test.get_subtest('should not exist'))

    def test_load_baseline_fail(self):
        self.fs.write_text_file(
            self.finder.path_from_web_tests(
                'external', 'wpt', 'dir', 'multiglob.https.any-expected.txt'),
            textwrap.dedent("""\
                This is a testharness.js-based test.
                CONSOLE WARNING: warning
                Harness Error. harness_status.status = 1 , harness_status.message = Uncaught ReferenceError: AriaUtils is not defined
                [FAIL] subtest2
                  assert_unreached:\\n  message 2
                [NOTRUN] sub\\n  test3
                  promise_rejects_dom: message\\n  3
                Harness: the test ran to completion.

                """))
        test_file = self._load_metadata('dir/multiglob.https.any.js')

        test = test_file.get_test('multiglob.https.any.html')
        self.assertEqual(test.expected, 'ERROR')
        self.assertEqual(test.known_intermittent, [])
        self.assertFalse(test.disabled)

        subtest = test.get_subtest('subtest1')
        self.assertEqual(subtest.expected, 'PASS')
        self.assertEqual(subtest.known_intermittent, [])
        self.assertFalse(subtest.disabled)
        self.assertFalse(subtest.has_key('expected-fail-message'))

        subtest = test.get_subtest('subtest2')
        self.assertEqual(subtest.expected, 'FAIL')
        self.assertEqual(subtest.known_intermittent, [])
        self.assertFalse(subtest.disabled)
        self.assertEqual(subtest.get('expected-fail-message'),
                         'assert_unreached:\n  message 2')

        subtest = test.get_subtest('sub\n  test3')
        self.assertEqual(subtest.expected, 'NOTRUN')
        self.assertEqual(subtest.known_intermittent, [])
        self.assertFalse(subtest.disabled)
        self.assertEqual(subtest.get('expected-fail-message'),
                         'promise_rejects_dom: message\n  3')

    def test_load_variant_one_skipped(self):
        self.fs.write_text_file(
            self.finder.path_from_web_tests('TestExpectations'),
            textwrap.dedent("""\
                # results: [ Failure Skip ]
                external/wpt/variant.html?foo=bar/abc [ Failure ]
                external/wpt/variant.html?foo=baz [ Skip ]
                """))
        self.fs.write_text_file(
            self.finder.path_from_wpt_tests('variant_foo=baz-expected.txt'),
            textwrap.dedent("""\
                This is a testharness.js-based test.
                [FAIL] subtest
                Harness: the test ran to completion.
                """))

        test_file = self._load_metadata('variant.html')
        test = test_file.get_test('variant.html?foo=bar/abc')
        self.assertEqual(test.expected, 'OK')
        self.assertEqual(test.known_intermittent,
                         ['ERROR', 'PRECONDITION_FAILED'])

        # Even though this test is annotated with `[ Skip ]`, the test loader
        # should still translate its expectations in case it was explicitly
        # specified on the command line, which overrides `[ Skip ]`. See
        # https://crbug.com/329898284.
        test = test_file.get_test('variant.html?foo=baz')
        self.assertEqual(test.expected, 'OK')
        self.assertEqual(test.known_intermittent, [])
        subtest = test.get_subtest('subtest')
        self.assertEqual(subtest.expected, 'FAIL')
        self.assertEqual(subtest.known_intermittent, [])

    def test_load_failure_with_baseline(self):
        """A `[ Failure ]` line allows harness OK, even with a baseline."""
        self.fs.write_text_file(
            self.finder.path_from_web_tests('TestExpectations'),
            textwrap.dedent("""\
                # results: [ Failure ]
                external/wpt/variant.html?foo=bar/abc [ Failure ]
                """))
        self.fs.write_text_file(
            self.finder.path_from_wpt_tests(
                'variant_foo=bar_abc-expected.txt'),
            textwrap.dedent("""\
                This is a testharness.js-based test.
                Harness Error. harness_status.status = 3 , harness_status.message =
                Harness: the test ran to completion.
                """))
        test_file = self._load_metadata('variant.html')
        test = test_file.get_test('variant.html?foo=bar/abc')
        self.assertEqual(test.expected, 'OK')
        self.assertEqual(test.known_intermittent,
                         ['ERROR', 'PRECONDITION_FAILED'])

    def test_load_baseline_precondition_failed(self):
        self.fs.write_text_file(
            self.finder.path_from_web_tests(
                'external', 'wpt', 'dir', 'multiglob.https.any-expected.txt'),
            textwrap.dedent("""\
                This is a testharness.js-based test.
                Harness Error. harness_status.status = 3 , harness_status.message = Uncaught ReferenceError: AriaUtils is not defined
                Harness: the test ran to completion.

                """))
        test_file = self._load_metadata('dir/multiglob.https.any.js')

        test = test_file.get_test('multiglob.https.any.html')
        self.assertEqual(test.expected, 'PRECONDITION_FAILED')
        self.assertEqual(test.known_intermittent, [])
        self.assertFalse(test.disabled)

    def test_load_baseline_harness_ok(self):
        self.fs.write_text_file(
            self.finder.path_from_web_tests(
                'external', 'wpt', 'dir', 'multiglob.https.any-expected.txt'),
            textwrap.dedent("""\
                This is a testharness.js-based test.
                [FAIL] subtest
                Harness: the test ran to completion.

                """))
        test_file = self._load_metadata('dir/multiglob.https.any.js')

        test = test_file.get_test('multiglob.https.any.html')
        self.assertEqual(test.expected, 'OK')
        self.assertEqual(test.known_intermittent, [])
        self.assertFalse(test.disabled)

    def test_ignore_irrelevant_expectations(self):
        self.fs.write_text_file(
            self.finder.path_from_web_tests(
                'platform', 'test-mac-mac10.11', 'external', 'wpt', 'dir',
                'multiglob.https.any-expected.txt'),
            textwrap.dedent("""\
                This is a testharness.js-based test.
                FAIL subtest message
                Harness: the test ran to completion.

                """))
        self.fs.write_text_file(
            self.finder.path_from_web_tests('TestExpectations'),
            textwrap.dedent("""\
                # tags: [ Linux Mac Win ]
                # results: [ Pass Failure Timeout Crash Skip ]
                [ Mac ] external/wpt/dir/multiglob.https.any.worker.html [ Failure ]
                """))
        test_file = self._load_metadata('dir/multiglob.https.any.js')
        self.assertIsNone(test_file)

    def test_load_exp_line_timeout(self):
        self.fs.write_text_file(
            self.finder.path_from_web_tests('TestExpectations'),
            textwrap.dedent("""\
                # results: [ Timeout ]
                external/wpt/variant.html?foo=baz [ Timeout ]
                """))
        test_file = self._load_metadata('variant.html')
        test = test_file.get_test('variant.html?foo=baz')
        self.assertEqual(test.expected, 'OK')
        self.assertEqual(test.known_intermittent, ['TIMEOUT'])
        self.assertFalse(test.disabled)

    def test_load_virtual_expectations(self):
        self.fs.write_text_file(
            self.finder.path_from_web_tests('TestExpectations'),
            textwrap.dedent("""\
                # results: [ Pass Failure Crash Timeout Skip ]
                virtual/fake-vts/external/wpt/variant.html?foo=baz [ Pass Crash Timeout ]
                """))
        self.fs.write_text_file(
            self.finder.path_from_web_tests('virtual', 'fake-vts', 'external',
                                            'wpt',
                                            'variant_foo=baz-expected.txt'),
            textwrap.dedent("""\
                This is a testharness.js-based test.
                [FAIL] subtest1
                  assert_equals: message
                Harness: the test ran to completion.

                """))
        test_file = self._load_metadata('variant.html',
                                        virtual_suite='fake-vts')

        test = test_file.get_test('variant.html?foo=baz')
        self.assertEqual(test.expected, 'OK')
        self.assertEqual(test.known_intermittent, ['TIMEOUT', 'CRASH'])
        self.assertFalse(test.disabled)

        subtest = test.get_subtest('subtest1')
        self.assertEqual(subtest.expected, 'FAIL')
        self.assertEqual(subtest.known_intermittent, [])

        subtest = test.get_subtest('subtest2')
        self.assertEqual(subtest.expected, 'PASS')
        self.assertEqual(subtest.known_intermittent, [])

    def test_wpt_url_to_exp_test(self):
        self.assertEqual(wpt_url_to_blink_test('/css/test.html?a'),
                         'external/wpt/css/test.html?a')
        self.assertEqual(wpt_url_to_blink_test('/wpt_internal/test.html'),
                         'wpt_internal/test.html')

    def test_allow_any_subtests_on_timeout(self):
        test = mock.Mock()
        test.expected_fail_message.return_value = 'expect this message'
        test_result = wpttest.TestharnessResult('TIMEOUT',
                                                message=None,
                                                expected='TIMEOUT')
        subtest_result = wpttest.TestharnessSubtestResult('subtest',
                                                          'TIMEOUT',
                                                          message=None,
                                                          expected='FAIL')

        test_result, (subtest_result, ) = allow_any_subtests_on_timeout(
            test, (test_result, [subtest_result]))
        self.assertEqual(subtest_result.expected, 'TIMEOUT')
        self.assertEqual(subtest_result.known_intermittent, [])
        self.assertEqual(subtest_result.message, 'expect this message')
        test.expected_fail_message.assert_called_once_with('subtest')

    def test_do_not_allow_any_subtests_on_completion(self):
        test = mock.Mock()
        test.expected_fail_message.return_value = (
            'should not expect this message')
        test_result = wpttest.TestharnessResult('OK',
                                                message=None,
                                                expected='TIMEOUT')
        subtest_result = wpttest.TestharnessSubtestResult('subtest',
                                                          'FAIL',
                                                          message=None,
                                                          expected='TIMEOUT')

        test_result, (subtest_result, ) = allow_any_subtests_on_timeout(
            test, (test_result, [subtest_result]))
        self.assertEqual(subtest_result.expected, 'TIMEOUT')
        self.assertEqual(subtest_result.known_intermittent, [])
        self.assertIsNone(subtest_result.message)
        test.expected_fail_message.assert_not_called()

    def test_load_tests(self):
        subsuites = {
            '':
            Subsuite('', config={}),
            'fake-vts':
            Subsuite('fake-vts',
                     config={},
                     include=['/variant.html?foo=baz',
                              '/does-not-exist.html']),
        }
        with self._make_loader(subsuites=subsuites,
                               include=['reftest.html']) as loader:
            self.assertEqual(set(loader.tests), {'', 'fake-vts'})
            self.assertEqual(set(loader.tests['']), {'reftest'})
            self.assertEqual(set(loader.tests['fake-vts']), {'testharness'})
            (base_test, ) = loader.tests['']['reftest']
            (virtual_test, ) = loader.tests['fake-vts']['testharness']
            self.assertEqual(base_test.id, '/reftest.html')
            self.assertEqual(virtual_test.id, '/variant.html?foo=baz')
            self.assertEqual(loader.disabled_tests, {'': {}, 'fake-vts': {}})

    def test_load_no_tests(self):
        subsuites = {'': Subsuite('', config={})}
        with self._make_loader(subsuites=subsuites, include=[]) as loader:
            self.assertEqual(set(loader.tests), {''})
            self.assertEqual(loader.tests[''], {})
