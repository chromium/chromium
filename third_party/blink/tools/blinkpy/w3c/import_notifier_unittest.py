# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import textwrap
import unittest
from unittest import mock

from blinkpy.common.checkout.git import CommitRange
from blinkpy.common.host_mock import MockHost
from blinkpy.common.net.git_cl import CLRevisionID
from blinkpy.common.net.rpc import RESPONSE_PREFIX
from blinkpy.common.path_finder import (
    RELATIVE_WEB_TESTS,
    PathFinder,
)
from blinkpy.common.system.executive_mock import MockExecutive
from blinkpy.common.system.filesystem_mock import MockFileSystem
from blinkpy.w3c.buganizer import BuganizerError
from blinkpy.w3c.directory_owners_extractor import WPTDirMetadata
from blinkpy.w3c.gerrit import GerritAPI
from blinkpy.w3c.local_wpt_mock import MockLocalWPT
from blinkpy.w3c.import_notifier import (ImportNotifier, TestFailure,
                                         CHECKS_URL_TEMPLATE)
from blinkpy.w3c.wpt_expectations_updater import WPTExpectationsUpdater
from blinkpy.web_tests.models.testharness_results import parse_testharness_baseline

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
        self.gerrit_api = GerritAPI(self.host, 'wpt-autoroller', 'fake-token')
        self.notifier = ImportNotifier(self.host, self.git, self.local_wpt,
                                       self.gerrit_api, self.buganizer_client)
        self.notifier.default_port.set_option_default('manifest_update', False)

    def test_more_failures_in_baseline_more_fails(self):
        old_contents = textwrap.dedent("""\
            [FAIL] an old failure
            """)
        new_contents = textwrap.dedent("""\
            [FAIL] new failure 1
            [FAIL] new failure 2
            """)
        self.assertTrue(
            self.notifier.more_failures_in_baseline(
                parse_testharness_baseline(old_contents),
                parse_testharness_baseline(new_contents)))

    def test_more_failures_in_baseline_fewer_fails(self):
        old_contents = textwrap.dedent("""\
            [FAIL] an old failure
            [FAIL] new failure 1
            """)
        new_contents = textwrap.dedent("""\
            [FAIL] new failure 2
            """)
        self.assertFalse(
            self.notifier.more_failures_in_baseline(
                parse_testharness_baseline(old_contents),
                parse_testharness_baseline(new_contents)))

    def test_more_failures_in_baseline_same_fails(self):
        old_contents = textwrap.dedent("""\
            [FAIL] an old failure
            """)
        new_contents = textwrap.dedent("""\
            [FAIL] a new failure
            """)
        self.assertFalse(
            self.notifier.more_failures_in_baseline(
                parse_testharness_baseline(old_contents),
                parse_testharness_baseline(new_contents)))

    def test_more_failures_in_baseline_new_error(self):
        new_contents = textwrap.dedent("""\
            Harness Error. harness_status.status = 1 , harness_status.message = bad
            """)
        self.assertTrue(
            self.notifier.more_failures_in_baseline(
                [], parse_testharness_baseline(new_contents)))

    def test_more_failures_in_baseline_remove_error(self):
        old_contents = textwrap.dedent("""\
            Harness Error. harness_status.status = 1 , harness_status.message = bad
            """)
        self.assertFalse(
            self.notifier.more_failures_in_baseline(
                parse_testharness_baseline(old_contents), []))

    def test_more_failures_in_baseline_changing_error(self):
        old_contents = textwrap.dedent("""\
            Harness Error. harness_status.status = 1 , harness_status.message = bad
            """)
        new_contents = textwrap.dedent("""\
            Harness Error. harness_status.status = 2, harness_status.message = still an error
            """)
        self.assertFalse(
            self.notifier.more_failures_in_baseline(
                parse_testharness_baseline(old_contents),
                parse_testharness_baseline(new_contents)))

    def test_more_failures_in_baseline_fail_to_error(self):
        old_contents = textwrap.dedent("""\
            [FAIL] a previous failure
            """)
        new_contents = textwrap.dedent("""\
            Harness Error. harness_status.status = 1 , harness_status.message = bad
            """)
        self.assertTrue(
            self.notifier.more_failures_in_baseline(
                parse_testharness_baseline(old_contents),
                parse_testharness_baseline(new_contents)))

    def test_more_failures_in_baseline_error_to_fail(self):
        old_contents = textwrap.dedent("""\
            Harness Error. harness_status.status = 1 , harness_status.message = bad
            """)
        new_contents = textwrap.dedent("""\
            [PASS FAIL] a new (flaky) failure
            """)
        self.assertTrue(
            self.notifier.more_failures_in_baseline(
                parse_testharness_baseline(old_contents),
                parse_testharness_baseline(new_contents)))

    def _write_and_commit(self, file_contents):
        for path, contents in file_contents.items():
            self.host.filesystem.write_text_file(path, contents)
            self.git.add(path)
        self.git.commit_locally_with_message('commit')

    def test_main(self):
        """Exercise the `ImportNotifier` end-to-end happy path."""
        contents_before = textwrap.dedent("""\
            # results: [ Failure Pass Timeout ]
            """)
        self._write_and_commit({
            MOCK_WEB_TESTS + 'external/wpt/foo/DIR_METADATA':
            '',
            MOCK_WEB_TESTS + 'TestExpectations':
            contents_before,
        })
        contents_after = textwrap.dedent("""\
            # results: [ Failure Pass Timeout ]
            external/wpt/foo/bar.html [ Failure ]
            """)
        self._write_and_commit({
            MOCK_WEB_TESTS + 'TestExpectations':
            contents_after,
        })
        gerrit_query = (
            'https://chromium-review.googlesource.com/changes/'
            '?q=owner:wpt-autoroller%40chops-service-accounts.'
            'iam.gserviceaccount.com'
            '+prefixsubject:"Import+wpt%40abcdef"+status:merged'
            '&n=1&o=CURRENT_FILES&o=CURRENT_REVISION&o=COMMIT_FOOTERS'
            '&o=DETAILED_ACCOUNTS&o=MESSAGES')
        payload = {
            '_number': 77777,
            'change_id': 'I8888',
            'current_revision': '999999',
            'revisions': {
                '999999': {
                    '_number': 4,
                },
            },
        }
        self.host.web.urls = {
            gerrit_query:
            RESPONSE_PREFIX + b'\n' + json.dumps([payload]).encode(),
        }

        dir_metadata = WPTDirMetadata(buganizer_public_component='123',
                                      should_notify=True)
        with mock.patch.object(self.notifier.owners_extractor,
                               'read_dir_metadata',
                               return_value=dir_metadata):
            self.notifier.main(CommitRange('HEAD~1', 'HEAD'), '543210',
                               'abcdef')

        self.buganizer_client.NewIssue.assert_called_once()
        issue = self.buganizer_client.NewIssue.call_args.args[0]
        self.assertEqual(
            issue.title, '[WPT] New failures introduced in external/wpt/foo '
            'by import https://crrev.com/c/77777')
        self.assertEqual(issue.component_id, '123')
        self.assertEqual(issue.cc, [])
        self.assertIn('543210...abcdef', issue.description)

    def test_examine_baseline_changes(self):
        contents_before = textwrap.dedent("""\
            [PASS] subtest
            """)
        self._write_and_commit({
            MOCK_WEB_TESTS + 'external/wpt/foo/DIR_METADATA':
            '',
            MOCK_WEB_TESTS + 'external/wpt/foo/bar_a-expected.txt':
            contents_before,
        })
        contents_after = textwrap.dedent("""\
            [FAIL] subtest
            """)
        self._write_and_commit({
            MOCK_WEB_TESTS + 'external/wpt/foo/bar.html':
            '',
            MOCK_WEB_TESTS + 'external/wpt/foo/bar_a-expected.txt':
            contents_after,
            MOCK_WEB_TESTS + 'platform/linux/external/wpt/foo/bar_a-expected.txt':
            contents_after,
            MOCK_WEB_TESTS + 'flag-specific/fake-flag/external/wpt/foo/bar_b-expected.txt':
            contents_after,
            '/mock-checkout/unrelated.cc':
            '',
        })

        gerrit_url_with_ps = 'https://crrev.com/c/12345/3/'
        self.notifier.examine_baseline_changes(CommitRange('HEAD~1', 'HEAD'),
                                               CLRevisionID(12345, 3))
        self.assertEqual(
            self.notifier.new_failures_by_directory, {
                'external/wpt/foo': [
                    TestFailure.from_file(
                        'external/wpt/foo/bar.html?a',
                        baseline_path=RELATIVE_WEB_TESTS +
                        'external/wpt/foo/bar_a-expected.txt',
                        gerrit_url_with_ps=gerrit_url_with_ps),
                    TestFailure.from_file(
                        'external/wpt/foo/bar.html?b',
                        baseline_path=RELATIVE_WEB_TESTS +
                        'flag-specific/fake-flag/external/wpt/foo/bar_b-expected.txt',
                        gerrit_url_with_ps=gerrit_url_with_ps),
                    TestFailure.from_file(
                        'external/wpt/foo/bar.html?a',
                        baseline_path=RELATIVE_WEB_TESTS +
                        'platform/linux/external/wpt/foo/bar_a-expected.txt',
                        gerrit_url_with_ps=gerrit_url_with_ps),
                ]
            })

    def test_examine_new_test_expectations(self):
        contents_before = textwrap.dedent("""\
            # tags: [ Linux Mac Win ]
            # results: [ Failure Pass Timeout ]
            external/wpt/foo/existing.html [ Failure ]
            """)
        self._write_and_commit({
            MOCK_WEB_TESTS + 'external/wpt/foo/DIR_METADATA':
            '',
            MOCK_WEB_TESTS + 'TestExpectations':
            contents_before,
        })
        contents_after = textwrap.dedent("""\
            # tags: [ Linux Mac Win ]
            # results: [ Failure Pass Timeout ]
            external/wpt/foo/existing.html [ Failure ]

            # ====== New tests from wpt-importer added here ======
            crbug.com/12345 [ Linux ] external/wpt/foo/bar.html [ Failure ]
            crbug.com/12345 [ Win ] external/wpt/foo/bar.html [ Timeout ]
            """)
        self._write_and_commit({
            MOCK_WEB_TESTS + 'TestExpectations':
            contents_after,
            MOCK_WEB_TESTS + 'external/wpt/unrelated.html':
            '',
        })

        self.notifier.examine_new_test_expectations(
            CommitRange('HEAD~1', 'HEAD'))
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
            'SHA_START', 'SHA_END', CLRevisionID(12345))

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
                'SHA_START', 'SHA_END', CLRevisionID(12345))
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
                'SHA_START', 'SHA_END', CLRevisionID(12345))
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
                'SHA_START', 'SHA_END', CLRevisionID(12345))
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
