# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import contextlib
import json
import textwrap
import unittest
from unittest import mock

from blinkpy.common.checkout.git_mock import MockGit
from blinkpy.common.host_mock import MockHost
from blinkpy.common.path_finder import (
    RELATIVE_WEB_TESTS,
    PathFinder,
    bootstrap_wpt_imports,
)
from blinkpy.common.system.executive import ScriptError
from blinkpy.common.system.executive_mock import mock_git_commands, MockExecutive
from blinkpy.common.system.filesystem_mock import MockFileSystem
from blinkpy.w3c import wpt_metadata
from blinkpy.w3c.directory_owners_extractor import WPTDirMetadata
from blinkpy.w3c.local_wpt_mock import MockLocalWPT
from blinkpy.w3c.import_notifier import ImportNotifier, TestFailure
from blinkpy.w3c.wpt_expectations_updater import WPTExpectationsUpdater

bootstrap_wpt_imports()
from wptrunner import metadata

UMBRELLA_BUG = WPTExpectationsUpdater.UMBRELLA_BUG
MOCK_WEB_TESTS = '/mock-checkout/' + RELATIVE_WEB_TESTS


class ImportNotifierTest(unittest.TestCase):
    def setUp(self):
        self.host = MockHost()
        # Mock a virtual test suite at virtual/gpu/external/wpt/foo.
        self.host.filesystem = MockFileSystem({
            MOCK_WEB_TESTS + 'VirtualTestSuites':
            b'[{"prefix": "gpu", "platforms": ["Linux", "Mac", "Win"], '
            b'"bases": ["external/wpt/foo"], "args": ["--foo"], '
            b'"expires": "never"}]'
        })
        self.finder = PathFinder(self.host.filesystem)
        self.host.filesystem.write_text_file(
            self.finder.path_from_wpt_tests('MANIFEST.json'),
            json.dumps({
                'version': 8,
                'items': {
                    'testharness': {
                        'foo': {
                            'bar.html': [
                                'abc',
                                ['foo/bar.html?a', {}],
                                ['foo/bar.html?b', {}],
                            ],
                        },
                    },
                    'wdspec': {
                        'webdriver': {
                            'foo.py': ['abcdef', [None, {}]],
                        },
                    },
                },
            }))
        self.git = self.host.git()
        self.local_wpt = MockLocalWPT()
        configs = wpt_metadata.TestConfigurations(self.host.filesystem, [
            metadata.RunInfo({'os': 'win'}),
            metadata.RunInfo({'os': 'mac'}),
        ])
        self.notifier = ImportNotifier(self.host, self.git, self.local_wpt,
                                       configs)

    def test_find_changed_baselines_of_tests(self):
        changed_files = [
            RELATIVE_WEB_TESTS + 'external/wpt/foo/bar.html',
            RELATIVE_WEB_TESTS + 'external/wpt/foo/bar-expected.txt',
            RELATIVE_WEB_TESTS +
            'platform/linux/external/wpt/foo/bar-expected.txt',
            RELATIVE_WEB_TESTS + 'external/wpt/random_stuff.html',
        ]
        self.git.changed_files = lambda: changed_files
        self.assertEqual(
            self.notifier.find_changed_baselines_of_tests(
                ['external/wpt/foo/bar.html']),
            {
                'external/wpt/foo/bar.html': [
                    RELATIVE_WEB_TESTS + 'external/wpt/foo/bar-expected.txt',
                    RELATIVE_WEB_TESTS +
                    'platform/linux/external/wpt/foo/bar-expected.txt',
                ]
            })

        self.assertEqual(
            self.notifier.find_changed_baselines_of_tests(set()), {})

    def test_more_failures_in_baseline_more_fails(self):
        # Replacing self.host.executive won't work here, because ImportNotifier
        # has been instantiated with a MockGit backed by an empty MockExecutive.
        executive = mock_git_commands({
            'diff': ('diff --git a/foo-expected.txt b/foo-expected.txt\n'
                     '--- a/foo-expected.txt\n'
                     '+++ b/foo-expected.txt\n'
                     '-FAIL an old failure\n'
                     '+FAIL new failure 1\n'
                     '+FAIL new failure 2\n')
        })
        self.notifier.git = MockGit(executive=executive)
        self.assertTrue(
            self.notifier.more_failures_in_baseline('foo-expected.txt'))
        self.assertEqual(
            executive.calls,
            [['git', 'diff', '-U0', 'origin/main', '--', 'foo-expected.txt']
             ])

    def test_more_failures_in_baseline_fewer_fails(self):
        executive = mock_git_commands({
            'diff': ('diff --git a/foo-expected.txt b/foo-expected.txt\n'
                     '--- a/foo-expected.txt\n'
                     '+++ b/foo-expected.txt\n'
                     '-FAIL an old failure\n'
                     '-FAIL new failure 1\n'
                     '+FAIL new failure 2\n')
        })
        self.notifier.git = MockGit(executive=executive)
        self.assertFalse(
            self.notifier.more_failures_in_baseline('foo-expected.txt'))

    def test_more_failures_in_baseline_same_fails(self):
        executive = mock_git_commands({
            'diff': ('diff --git a/foo-expected.txt b/foo-expected.txt\n'
                     '--- a/foo-expected.txt\n'
                     '+++ b/foo-expected.txt\n'
                     '-FAIL an old failure\n'
                     '+FAIL a new failure\n')
        })
        self.notifier.git = MockGit(executive=executive)
        self.assertFalse(
            self.notifier.more_failures_in_baseline('foo-expected.txt'))

    def test_more_failures_in_baseline_new_error(self):
        executive = mock_git_commands({
            'diff': ('diff --git a/foo-expected.txt b/foo-expected.txt\n'
                     '--- a/foo-expected.txt\n'
                     '+++ b/foo-expected.txt\n'
                     '-PASS an existing pass\n'
                     '+Harness Error. harness_status.status = 1 , harness_status.message = bad\n')
        })
        self.notifier.git = MockGit(executive=executive)
        self.assertTrue(
            self.notifier.more_failures_in_baseline('foo-expected.txt'))

    def test_more_failures_in_baseline_remove_error(self):
        executive = mock_git_commands({
            'diff': ('diff --git a/foo-expected.txt b/foo-expected.txt\n'
                     '--- a/foo-expected.txt\n'
                     '+++ b/foo-expected.txt\n'
                     '-Harness Error. harness_status.status = 1 , harness_status.message = bad\n'
                     '+PASS a new pass\n')
        })
        self.notifier.git = MockGit(executive=executive)
        self.assertFalse(
            self.notifier.more_failures_in_baseline('foo-expected.txt'))

    def test_more_failures_in_baseline_changing_error(self):
        executive = mock_git_commands({
            'diff': ('diff --git a/foo-expected.txt b/foo-expected.txt\n'
                     '--- a/foo-expected.txt\n'
                     '+++ b/foo-expected.txt\n'
                     '-Harness Error. harness_status.status = 1 , harness_status.message = bad\n'
                     '+Harness Error. new text, still an error\n')
        })
        self.notifier.git = MockGit(executive=executive)
        self.assertFalse(
            self.notifier.more_failures_in_baseline('foo-expected.txt'))

    def test_more_failures_in_baseline_fail_to_error(self):
        executive = mock_git_commands({
            'diff': ('diff --git a/foo-expected.txt b/foo-expected.txt\n'
                     '--- a/foo-expected.txt\n'
                     '+++ b/foo-expected.txt\n'
                     '-FAIL a previous failure\n'
                     '+Harness Error. harness_status.status = 1 , harness_status.message = bad\n')
        })
        self.notifier.git = MockGit(executive=executive)
        self.assertTrue(
            self.notifier.more_failures_in_baseline('foo-expected.txt'))

    def test_more_failures_in_baseline_error_to_fail(self):
        executive = mock_git_commands({
            'diff': ('diff --git a/foo-expected.txt b/foo-expected.txt\n'
                     '--- a/foo-expected.txt\n'
                     '+++ b/foo-expected.txt\n'
                     '-Harness Error. harness_status.status = 1 , harness_status.message = bad\n'
                     '+FAIL a new failure\n')
        })
        self.notifier.git = MockGit(executive=executive)
        self.assertTrue(
            self.notifier.more_failures_in_baseline('foo-expected.txt'))

    def test_examine_baseline_changes(self):
        self.host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'external/wpt/foo/DIR_METADATA', '')
        changed_test_baselines = {
            'external/wpt/foo/bar.html': [
                RELATIVE_WEB_TESTS + 'external/wpt/foo/bar-expected.txt',
                RELATIVE_WEB_TESTS +
                'platform/linux/external/wpt/foo/bar-expected.txt',
            ]
        }
        gerrit_url_with_ps = 'https://crrev.com/c/12345/3/'
        self.notifier.more_failures_in_baseline = lambda _: True
        self.notifier.examine_baseline_changes(changed_test_baselines,
                                               gerrit_url_with_ps)

        self.assertEqual(
            self.notifier.new_failures_by_directory, {
                'external/wpt/foo': [
                    TestFailure.from_file(
                        'external/wpt/foo/bar.html',
                        baseline_path=RELATIVE_WEB_TESTS +
                        'external/wpt/foo/bar-expected.txt',
                        gerrit_url_with_ps=gerrit_url_with_ps),
                    TestFailure.from_file(
                        'external/wpt/foo/bar.html',
                        baseline_path=RELATIVE_WEB_TESTS +
                        'platform/linux/external/wpt/foo/bar-expected.txt',
                        gerrit_url_with_ps=gerrit_url_with_ps),
                ]
            })

    def test_examine_metadata_changes_existing_failures(self):
        self.host.filesystem.write_text_file(
            self.finder.path_from_wpt_tests('webdriver', 'DIR_METADATA'), '')
        old_contents = textwrap.dedent("""\
            [foo.py]
              [subtest]
                expected: [FAIL, PASS]
            """).encode()
        path = RELATIVE_WEB_TESTS + 'external/wpt/webdriver/foo.py.ini'
        self.host.filesystem.write_text_file(
            self.finder.path_from_chromium_base(path),
            textwrap.dedent("""\
                [foo.py]
                  [subtest]
                    expected:
                      if os == "mac": FAIL
                      PASS
                """))
        with contextlib.ExitStack() as mocks:
            show_blob = mocks.enter_context(
                mock.patch.object(self.git,
                                  'show_blob',
                                  return_value=old_contents))
            mocks.enter_context(
                mock.patch.object(self.git,
                                  'changed_files',
                                  return_value=[path]))
            self.notifier.examine_metadata_changes(
                'https://crrev.com/c/12345/3/')
        show_blob.assert_called_with(
            'third_party/blink/web_tests/external/wpt/webdriver/foo.py.ini')
        self.assertEqual(self.notifier.new_failures_by_directory, {})

    def test_examine_metadata_changes_new_harness_error(self):
        self.host.filesystem.write_text_file(
            self.finder.path_from_wpt_tests('webdriver', 'DIR_METADATA'), '')
        old_contents = textwrap.dedent("""\
            [foo.py]
              expected:
                if os == "mac": TIMEOUT
            """).encode()
        path = RELATIVE_WEB_TESTS + 'external/wpt/webdriver/foo.py.ini'
        self.host.filesystem.write_text_file(
            self.finder.path_from_chromium_base(path),
            textwrap.dedent("""\
                [foo.py]
                  expected: TIMEOUT
                """))
        with contextlib.ExitStack() as mocks:
            show_blob = mocks.enter_context(
                mock.patch.object(self.git,
                                  'show_blob',
                                  return_value=old_contents))
            mocks.enter_context(
                mock.patch.object(self.git,
                                  'changed_files',
                                  return_value=[path]))
            self.notifier.examine_metadata_changes(
                'https://crrev.com/c/12345/3/')

        show_blob.assert_called_with(
            'third_party/blink/web_tests/external/wpt/webdriver/foo.py.ini')
        self.assertEqual(
            self.notifier.new_failures_by_directory, {
                'external/wpt/webdriver': [
                    TestFailure(
                        'external/wpt/webdriver/foo.py new failing tests: '
                        'https://crrev.com/c/12345/3/'
                        'third_party/blink/web_tests/external/wpt/webdriver/foo.py.ini',
                        'external/wpt/webdriver/foo.py'),
                ],
            })

    def test_examine_metadata_changes_new_subtest_failures(self):
        self.host.filesystem.write_text_file(
            self.finder.path_from_wpt_tests('webdriver', 'DIR_METADATA'), '')
        path = RELATIVE_WEB_TESTS + 'external/wpt/webdriver/foo.py.ini'
        self.host.filesystem.write_text_file(
            self.finder.path_from_chromium_base(path),
            textwrap.dedent("""\
                [foo.py]
                  [subtest]
                    expected: FAIL
                """))
        with contextlib.ExitStack() as mocks:
            show_blob = mocks.enter_context(
                mock.patch.object(self.git,
                                  'show_blob',
                                  side_effect=ScriptError))
            mocks.enter_context(
                mock.patch.object(self.git,
                                  'changed_files',
                                  return_value=[path]))
            self.notifier.examine_metadata_changes(
                'https://crrev.com/c/12345/3/')

        show_blob.assert_called_with(
            'third_party/blink/web_tests/external/wpt/webdriver/foo.py.ini')
        self.assertEqual(
            self.notifier.new_failures_by_directory, {
                'external/wpt/webdriver': [
                    TestFailure(
                        'external/wpt/webdriver/foo.py new failing tests: '
                        'https://crrev.com/c/12345/3/'
                        'third_party/blink/web_tests/external/wpt/webdriver/foo.py.ini',
                        'external/wpt/webdriver/foo.py'),
                ],
            })

    def test_examine_metadata_changes_variants(self):
        self.host.filesystem.write_text_file(
            self.finder.path_from_wpt_tests('foo', 'DIR_METADATA'), '')
        path = RELATIVE_WEB_TESTS + 'external/wpt/foo/bar.html.ini'
        self.host.filesystem.write_text_file(
            self.finder.path_from_chromium_base(path),
            textwrap.dedent("""\
                [bar.html?a]
                  [subtest]
                    expected:
                      if os == "mac": FAIL

                [bar.html?b]
                  [subtest]
                    expected:
                      if os == "mac": FAIL
                """))
        with contextlib.ExitStack() as mocks:
            show_blob = mocks.enter_context(
                mock.patch.object(self.git,
                                  'show_blob',
                                  side_effect=ScriptError))
            mocks.enter_context(
                mock.patch.object(self.git,
                                  'changed_files',
                                  return_value=[path]))
            self.notifier.examine_metadata_changes(
                'https://crrev.com/c/12345/3/')

        # TODO(crbug.com/1474702): After the switch to wptrunner, check that
        # non-wdspec tests can generate failures (i.e., delete the first
        # assertion, and turn the second one into `assertEqual`). For now,
        # require that no such failures are generated.
        self.assertEqual(self.notifier.new_failures_by_directory, {})
        self.assertNotEqual(
            self.notifier.new_failures_by_directory, {
                'external/wpt/foo': [
                    TestFailure(
                        'external/wpt/foo/bar.html?a new failing tests: '
                        'https://crrev.com/c/12345/3/'
                        'third_party/blink/web_tests/external/wpt/foo/bar.html.ini',
                        'external/wpt/foo/bar.html?a'),
                    TestFailure(
                        'external/wpt/foo/bar.html?b new failing tests: '
                        'https://crrev.com/c/12345/3/'
                        'third_party/blink/web_tests/external/wpt/foo/bar.html.ini',
                        'external/wpt/foo/bar.html?b'),
                ],
            })

    def test_examine_new_test_expectations(self):
        self.host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'external/wpt/foo/DIR_METADATA', '')
        test_expectations = {
            'external/wpt/foo/bar.html': [
                'crbug.com/12345 [ Linux ] external/wpt/foo/bar.html [ Fail ]',
                'crbug.com/12345 [ Win ] external/wpt/foo/bar.html [ Timeout ]',
            ]
        }
        self.notifier.examine_new_test_expectations(test_expectations)
        self.assertEqual(
            self.notifier.new_failures_by_directory, {
                'external/wpt/foo': [
                    TestFailure.from_expectation_line(
                        'external/wpt/foo/bar.html',
                        'crbug.com/12345 [ Linux ] external/wpt/foo/bar.html [ Fail ]'
                    ),
                    TestFailure.from_expectation_line(
                        'external/wpt/foo/bar.html',
                        'crbug.com/12345 [ Win ] external/wpt/foo/bar.html [ Timeout ]'
                    ),
                ]
            })

        self.notifier.new_failures_by_directory = {}
        self.notifier.examine_new_test_expectations({})
        self.assertEqual(self.notifier.new_failures_by_directory, {})

    def test_format_commit_list(self):
        imported_commits = [
            ('SHA1', 'Subject 1'),
            # Use non-ASCII chars to really test Unicode handling.
            ('SHA2', u'ABC~‾¥≈¤･・•∙·☼★星🌟星★☼·∙•・･¤≈¥‾~XYZ')
        ]

        def _is_commit_affecting_directory(commit, directory):
            self.assertIn(commit, ('SHA1', 'SHA2'))
            self.assertEqual(directory, 'foo')
            return commit == 'SHA1'

        self.local_wpt.is_commit_affecting_directory = _is_commit_affecting_directory
        self.assertEqual(
            self.notifier.format_commit_list(
                imported_commits, MOCK_WEB_TESTS + 'external/wpt/foo'),
            u'Subject 1: https://github.com/web-platform-tests/wpt/commit/SHA1 [affecting this directory]\n'
            u'ABC~‾¥≈¤･・•∙·☼★星🌟星★☼·∙•・･¤≈¥‾~XYZ: https://github.com/web-platform-tests/wpt/commit/SHA2\n'
        )

    def test_find_directory_for_bug_non_virtual(self):
        self.host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'external/wpt/foo/DIR_METADATA', '')
        self.assertEqual(
            self.notifier.find_directory_for_bug('external/wpt/foo/bar.html'),
            'external/wpt/foo')
        self.assertEqual(
            self.notifier.find_directory_for_bug(
                'external/wpt/foo/bar/baz.html'), 'external/wpt/foo')

    def test_find_directory_for_bug_virtual(self):
        self.host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'external/wpt/foo/DIR_METADATA', '')
        self.assertEqual(
            self.notifier.find_directory_for_bug(
                'virtual/gpu/external/wpt/foo/bar.html'), 'external/wpt/foo')

    def test_create_bugs_from_new_failures(self):
        self.host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'external/wpt/foo/DIR_METADATA', '')
        self.host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'external/wpt/foo/OWNERS', 'foolip@chromium.org')
        self.host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'external/wpt/bar/DIR_METADATA', '')

        data = (
            '{"dirs":{"third_party/blink/web_tests/external/wpt/foo":{"monorail":{"component":'
            '"Blink>Infra>Ecosystem"},"teamEmail":"team-email@chromium.org","wpt":{'
            '"notify":"YES"}}}}')

        def mock_run_command(args):
            if args[-1].endswith('external/wpt/foo'):
                return data
            return ''

        self.notifier.owners_extractor.executive = MockExecutive(
            run_command_fn=mock_run_command)

        self.notifier.new_failures_by_directory = {
            'external/wpt/foo': [
                TestFailure.from_expectation_line(
                    'external/wpt/foo/baz.html',
                    'crbug.com/12345 external/wpt/foo/baz.html [ Fail ]')
            ],
            'external/wpt/bar': [
                TestFailure.from_expectation_line(
                    'external/wpt/bar/baz.html',
                    'crbug.com/12345 external/wpt/bar/baz.html [ Fail ]')
            ]
        }
        bugs = self.notifier.create_bugs_from_new_failures(
            'SHA_START', 'SHA_END', 'https://crrev.com/c/12345')

        # Only one directory has WPT-NOTIFY enabled.
        self.assertEqual(len(bugs), 1)
        # The formatting of imported commits and new failures are already tested.
        self.assertEqual(set(bugs[0].body['cc']),
                         {'team-email@chromium.org', 'foolip@chromium.org'})
        self.assertEqual(bugs[0].body['components'], ['Blink>Infra>Ecosystem'])
        self.assertEqual(
            bugs[0].body['summary'],
            '[WPT] New failures introduced in external/wpt/foo by import https://crrev.com/c/12345'
        )
        self.assertIn('crbug.com/12345 external/wpt/foo/baz.html [ Fail ]',
                      bugs[0].body['description'].splitlines())
        self.assertIn(
            'This bug was filed automatically due to a new WPT test failure '
            'for which you are marked an OWNER. If you do not want to receive '
            'these reports, please add "wpt { notify: NO }"  to the relevant '
            'DIR_METADATA file.', bugs[0].body['description'].splitlines())

    def test_file_bug_without_owners(self):
        """A bug should be filed, even without OWNERS next to DIR_METADATA."""
        self.notifier.new_failures_by_directory = {
            'external/wpt/foo': [
                TestFailure.from_expectation_line(
                    'external/wpt/foo/baz.html',
                    'crbug.com/12345 external/wpt/foo/baz.html [ Fail ]'),
            ],
        }
        dir_metadata = WPTDirMetadata(component='Blink>Infra>Ecosystem',
                                      should_notify=True)
        with mock.patch.object(self.notifier.owners_extractor,
                               'read_dir_metadata',
                               return_value=dir_metadata):
            (bug, ) = self.notifier.create_bugs_from_new_failures(
                'SHA_START', 'SHA_END', 'https://crrev.com/c/12345')
            self.assertEqual(bug.body['cc'], [])
            self.assertEqual(bug.body['components'], ['Blink>Infra>Ecosystem'])
            self.assertEqual(
                bug.body['summary'],
                '[WPT] New failures introduced in external/wpt/foo '
                'by import https://crrev.com/c/12345')

    def test_no_bugs_filed_in_dry_run(self):
        def unreachable(_):
            self.fail('MonorailAPI should not be instantiated in dry_run.')

        self.notifier._get_monorail_api = unreachable  # pylint: disable=protected-access
        self.notifier.file_bugs([], True)

    def test_file_bugs_calls_luci_auth(self):
        test = self

        class FakeAPI(object):
            def __init__(self,
                         service_account_key_json=None,
                         access_token=None):
                test.assertIsNone(service_account_key_json)
                test.assertEqual(access_token, 'MOCK output of child process')

        self.notifier._monorail_api = FakeAPI  # pylint: disable=protected-access
        self.notifier.file_bugs([], False)
        self.assertEqual(self.host.executive.calls, [['luci-auth', 'token']])


class TestFailureTest(unittest.TestCase):
    def test_test_failure_to_str_baseline_change(self):
        failure = TestFailure.from_file(
            'external/wpt/foo/bar.html',
            baseline_path=RELATIVE_WEB_TESTS +
            'external/wpt/foo/bar-expected.txt',
            gerrit_url_with_ps='https://crrev.com/c/12345/3/')
        self.assertEqual(
            failure.message,
            'external/wpt/foo/bar.html new failing tests: https://crrev.com/c/12345/3/'
            + RELATIVE_WEB_TESTS + 'external/wpt/foo/bar-expected.txt')

        platform_failure = TestFailure.from_file(
            'external/wpt/foo/bar.html',
            baseline_path=RELATIVE_WEB_TESTS +
            'platform/linux/external/wpt/foo/bar-expected.txt',
            gerrit_url_with_ps='https://crrev.com/c/12345/3/')
        self.assertEqual(
            platform_failure.message,
            '[ Linux ] external/wpt/foo/bar.html new failing tests: https://crrev.com/c/12345/3/'
            + RELATIVE_WEB_TESTS +
            'platform/linux/external/wpt/foo/bar-expected.txt')

    def test_test_failure_to_str_new_expectation(self):
        failure = TestFailure.from_expectation_line(
            'external/wpt/foo/bar.html',
            'crbug.com/12345 [ Linux ] external/wpt/foo/bar.html [ Fail ]')
        self.assertEqual(
            failure.message,
            'crbug.com/12345 [ Linux ] external/wpt/foo/bar.html [ Fail ]')

        failure_with_umbrella_bug = TestFailure.from_expectation_line(
            'external/wpt/foo/bar.html',
            UMBRELLA_BUG + ' external/wpt/foo/bar.html [ Fail ]')
        self.assertEqual(failure_with_umbrella_bug.message,
                         'external/wpt/foo/bar.html [ Fail ]')
