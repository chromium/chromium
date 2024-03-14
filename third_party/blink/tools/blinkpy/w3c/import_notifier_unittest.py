# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import textwrap
import unittest
from unittest import mock

from blinkpy.common.checkout.git import CommitRange
from blinkpy.common.host_mock import MockHost
from blinkpy.common.path_finder import (
    RELATIVE_WEB_TESTS,
    RELATIVE_WPT_TESTS,
    PathFinder,
)
from blinkpy.common.system.executive import ScriptError
from blinkpy.common.system.executive_mock import MockExecutive
from blinkpy.common.system.filesystem_mock import MockFileSystem
from blinkpy.w3c.buganizer import BuganizerError
from blinkpy.w3c.directory_owners_extractor import WPTDirMetadata
from blinkpy.w3c.local_wpt_mock import MockLocalWPT
from blinkpy.w3c.import_notifier import (ImportNotifier, TestFailure,
                                         CHECKS_URL_TEMPLATE)
from blinkpy.w3c.wpt_expectations_updater import WPTExpectationsUpdater

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
        self.buganizer_client = mock.Mock()
        self.notifier = ImportNotifier(self.host, self.git, self.local_wpt,
                                       self.buganizer_client)

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
        old_contents = textwrap.dedent("""\
            [FAIL] an old failure
            """).encode()
        new_contents = textwrap.dedent("""\
            [FAIL] new failure 1
            [FAIL] new failure 2
            """)
        self.host.filesystem.write_text_file(
            self.finder.path_from_wpt_tests('foo-expected.txt'), new_contents)
        with mock.patch.object(self.git,
                               'show_blob',
                               return_value=old_contents) as show_blob:
            self.assertTrue(
                self.notifier.more_failures_in_baseline(
                    f'{RELATIVE_WPT_TESTS}foo-expected.txt'))
        show_blob.assert_called_once_with(
            f'{RELATIVE_WPT_TESTS}foo-expected.txt')

    def test_more_failures_in_baseline_fewer_fails(self):
        old_contents = textwrap.dedent("""\
            [FAIL] an old failure
            [FAIL] new failure 1
            """).encode()
        new_contents = textwrap.dedent("""\
            [FAIL] new failure 2
            """)
        self.host.filesystem.write_text_file(
            self.finder.path_from_wpt_tests('foo-expected.txt'), new_contents)
        with mock.patch.object(self.git,
                               'show_blob',
                               return_value=old_contents):
            self.assertFalse(
                self.notifier.more_failures_in_baseline(
                    f'{RELATIVE_WPT_TESTS}foo-expected.txt'))

    def test_more_failures_in_baseline_same_fails(self):
        old_contents = textwrap.dedent("""\
            [FAIL] an old failure
            """).encode()
        new_contents = textwrap.dedent("""\
            [FAIL] a new failure
            """)
        self.host.filesystem.write_text_file(
            self.finder.path_from_wpt_tests('foo-expected.txt'), new_contents)
        with mock.patch.object(self.git,
                               'show_blob',
                               return_value=old_contents):
            self.assertFalse(
                self.notifier.more_failures_in_baseline(
                    f'{RELATIVE_WPT_TESTS}foo-expected.txt'))

    def test_more_failures_in_baseline_new_error(self):
        new_contents = textwrap.dedent("""\
            Harness Error. harness_status.status = 1 , harness_status.message = bad
            """)
        self.host.filesystem.write_text_file(
            self.finder.path_from_wpt_tests('foo-expected.txt'), new_contents)
        with mock.patch.object(self.git, 'show_blob', side_effect=ScriptError):
            self.assertTrue(
                self.notifier.more_failures_in_baseline(
                    f'{RELATIVE_WPT_TESTS}foo-expected.txt'))

    def test_more_failures_in_baseline_remove_error(self):
        old_contents = textwrap.dedent("""\
            Harness Error. harness_status.status = 1 , harness_status.message = bad
            """).encode()
        with mock.patch.object(self.git,
                               'show_blob',
                               return_value=old_contents):
            self.assertFalse(
                self.notifier.more_failures_in_baseline(
                    f'{RELATIVE_WPT_TESTS}foo-expected.txt'))

    def test_more_failures_in_baseline_changing_error(self):
        old_contents = textwrap.dedent("""\
            Harness Error. harness_status.status = 1 , harness_status.message = bad
            """).encode()
        new_contents = textwrap.dedent("""\
            Harness Error. harness_status.status = 2, harness_status.message = still an error
            """)
        self.host.filesystem.write_text_file(
            self.finder.path_from_wpt_tests('foo-expected.txt'), new_contents)
        with mock.patch.object(self.git,
                               'show_blob',
                               return_value=old_contents):
            self.assertFalse(
                self.notifier.more_failures_in_baseline(
                    f'{RELATIVE_WPT_TESTS}foo-expected.txt'))

    def test_more_failures_in_baseline_fail_to_error(self):
        old_contents = textwrap.dedent("""\
            [FAIL] a previous failure
            """).encode()
        new_contents = textwrap.dedent("""\
            Harness Error. harness_status.status = 1 , harness_status.message = bad
            """)
        self.host.filesystem.write_text_file(
            self.finder.path_from_wpt_tests('foo-expected.txt'), new_contents)
        with mock.patch.object(self.git,
                               'show_blob',
                               return_value=old_contents):
            self.assertTrue(
                self.notifier.more_failures_in_baseline(
                    f'{RELATIVE_WPT_TESTS}foo-expected.txt'))

    def test_more_failures_in_baseline_error_to_fail(self):
        old_contents = textwrap.dedent("""\
            Harness Error. harness_status.status = 1 , harness_status.message = bad
            """).encode()
        new_contents = textwrap.dedent("""\
            [PASS FAIL] a new (flaky) failure
            """)
        self.host.filesystem.write_text_file(
            self.finder.path_from_wpt_tests('foo-expected.txt'), new_contents)
        with mock.patch.object(self.git,
                               'show_blob',
                               return_value=old_contents):
            self.assertTrue(
                self.notifier.more_failures_in_baseline(
                    f'{RELATIVE_WPT_TESTS}foo-expected.txt'))

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

    def test_examine_new_test_expectations(self):
        self.host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'external/wpt/foo/DIR_METADATA', '')
        contents_before = textwrap.dedent("""\
            # tags: [ Linux Mac Win ]
            # results: [ Failure Pass Timeout ]
            external/wpt/foo/existing.html [ Failure ]
            """)
        contents_after = textwrap.dedent("""\
            # tags: [ Linux Mac Win ]
            # results: [ Failure Pass Timeout ]
            external/wpt/foo/existing.html [ Failure ]

            # ====== New tests from wpt-importer added here ======
            crbug.com/12345 [ Linux ] external/wpt/foo/bar.html [ Failure ]
            crbug.com/12345 [ Win ] external/wpt/foo/bar.html [ Timeout ]
            """)
        self.host.filesystem.write_text_file(
            self.finder.path_from_web_tests('TestExpectations'),
            contents_after)
        blobs = {
            ('third_party/blink/web_tests/TestExpectations', '012345'):
            contents_before.encode(),
            ('third_party/blink/web_tests/TestExpectations', 'abcdef'):
            contents_after.encode(),
        }

        with mock.patch.object(self.notifier, 'git') as git:
            git.show_blob = lambda path, ref=None: blobs[path, ref]
            git.changed_files.return_value = [
                'third_party/blink/web_tests/TestExpectations',
                'third_party/blink/web_tests/external/wpt/unrelated.html',
            ]
            self.notifier.examine_new_test_expectations(
                CommitRange('012345', 'abcdef'))

        self.assertEqual(
            self.notifier.new_failures_by_directory, {
                'external/wpt/foo': [
                    TestFailure.from_expectation_line(
                        'external/wpt/foo/bar.html',
                        'crbug.com/12345 [ Linux ] external/wpt/foo/bar.html [ Failure ]'
                    ),
                    TestFailure.from_expectation_line(
                        'external/wpt/foo/bar.html',
                        'crbug.com/12345 [ Win ] external/wpt/foo/bar.html [ Timeout ]'
                    ),
                ]
            })

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

        def mock_run_command(args):
            if args[-1].endswith('external/wpt/foo'):
                return json.dumps({
                    'dirs': {
                        'third_party/blink/web_tests/external/wpt/foo': {
                            'buganizerPublic': {
                                'componentId': '999',
                            },
                            'teamEmail': 'team-email@chromium.org',
                            'wpt': {
                                'notify': 'YES',
                            },
                        },
                    },
                })
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
            'SHA_START', 'SHA_END', '12345')

        # Only one directory has WPT-NOTIFY enabled.
        self.assertEqual(len(bugs), 1)
        # The formatting of imported commits and new failures are already tested.
        self.assertEqual(set(bugs[0].cc),
                         {'team-email@chromium.org', 'foolip@chromium.org'})
        self.assertEqual(bugs[0].component_id, '999')
        self.assertEqual(
            bugs[0].title, '[WPT] New failures introduced in '
            'external/wpt/foo by import https://crrev.com/c/12345')
        self.assertIn('crbug.com/12345 external/wpt/foo/baz.html [ Fail ]',
                      bugs[0].description.splitlines())
        checks_url = ('See ' + CHECKS_URL_TEMPLATE + ' for details.').format(
            '12345', '1')
        self.assertIn(checks_url, bugs[0].description.splitlines())
        self.assertIn(
            'This bug was filed automatically due to a new WPT test failure '
            'for which you are marked an OWNER. If you do not want to receive '
            'these reports, please add "wpt { notify: NO }"  to the relevant '
            'DIR_METADATA file.', bugs[0].description.splitlines())

        self.notifier.file_bugs(bugs)
        self.buganizer_client.NewIssue.assert_called_once()

    def test_file_bug_without_owners(self):
        """A bug should be filed, even without OWNERS next to DIR_METADATA."""
        self.notifier.new_failures_by_directory = {
            'external/wpt/foo': [
                TestFailure.from_expectation_line(
                    'external/wpt/foo/baz.html',
                    'crbug.com/12345 external/wpt/foo/baz.html [ Fail ]'),
            ],
        }
        dir_metadata = WPTDirMetadata(
            buganizer_public_component='123',
            should_notify=True)
        with mock.patch.object(self.notifier.owners_extractor,
                               'read_dir_metadata',
                               return_value=dir_metadata):
            (bug, ) = self.notifier.create_bugs_from_new_failures(
                'SHA_START', 'SHA_END', '12345')
            self.assertEqual(bug.cc, [])
            self.assertEqual(bug.component_id, '123')
            self.assertEqual(
                bug.title, '[WPT] New failures introduced in external/wpt/foo '
                'by import https://crrev.com/c/12345')

    def test_no_bugs_filed_in_dry_run(self):
        self.notifier.new_failures_by_directory = {
            'external/wpt/foo': [
                TestFailure.from_expectation_line(
                    'external/wpt/foo/baz.html',
                    'crbug.com/12345 external/wpt/foo/baz.html [ Fail ]'),
            ],
        }
        dir_metadata = WPTDirMetadata(buganizer_public_component='123',
                                      should_notify=True)
        with mock.patch.object(self.notifier.owners_extractor,
                               'read_dir_metadata',
                               return_value=dir_metadata):
            bugs = self.notifier.create_bugs_from_new_failures(
                'SHA_START', 'SHA_END', 'https://crrev.com/c/12345')
        self.notifier.file_bugs(bugs, dry_run=True)
        self.buganizer_client.NewIssue.assert_not_called()

    def test_file_bugs_with_best_effort(self):
        """Failing to file a bug should not prevent additional attempts."""
        self.notifier.new_failures_by_directory = {
            'external/wpt/foo': [
                TestFailure.from_expectation_line(
                    'external/wpt/foo/baz.html',
                    'crbug.com/12345 external/wpt/foo/baz.html [ Fail ]'),
            ],
            'external/wpt/bar': [
                TestFailure.from_expectation_line(
                    'external/wpt/bar/baz.html',
                    'crbug.com/12345 external/wpt/bar/baz.html [ Fail ]'),
            ],
        }
        dir_metadata = WPTDirMetadata(buganizer_public_component='123',
                                      should_notify=True)
        with mock.patch.object(self.notifier.owners_extractor,
                               'read_dir_metadata',
                               return_value=dir_metadata):
            bugs = self.notifier.create_bugs_from_new_failures(
                'SHA_START', 'SHA_END', 'https://crrev.com/c/12345')
        self.assertEqual(len(bugs), 2)

        self.buganizer_client.NewIssue.side_effect = BuganizerError
        self.notifier.file_bugs(bugs)
        self.assertEqual(self.buganizer_client.NewIssue.call_count, 2)


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
