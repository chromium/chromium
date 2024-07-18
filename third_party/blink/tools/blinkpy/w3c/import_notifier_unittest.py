# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import dataclasses
import json
import textwrap
import unittest
from typing import List
from unittest import mock

from blinkpy.common.checkout.git import (
    CommitRange,
    FileStatus,
    FileStatusType,
)
from blinkpy.common.host_mock import MockHost
from blinkpy.common.net.git_cl import CLRevisionID
from blinkpy.common.net.rpc import RESPONSE_PREFIX
from blinkpy.common.path_finder import (
    RELATIVE_WEB_TESTS,
    PathFinder,
)
from blinkpy.common.system.executive_mock import MockExecutive
from blinkpy.common.system.filesystem_mock import MockFileSystem
from blinkpy.w3c.buganizer import BuganizerError, BuganizerIssue
from blinkpy.w3c.directory_owners_extractor import WPTDirMetadata
from blinkpy.w3c.gerrit import GerritAPI, GerritError
from blinkpy.w3c.local_wpt_mock import MockLocalWPT
from blinkpy.w3c.import_notifier import (
    BaselineFailure,
    DirectoryFailures,
    ImportNotifier,
    CHECKS_URL_TEMPLATE,
)
from blinkpy.web_tests.models import typ_types
from blinkpy.web_tests.models.testharness_results import parse_testharness_baseline

MOCK_WEB_TESTS = '/mock-checkout/' + RELATIVE_WEB_TESTS


class ImportNotifierTest(unittest.TestCase):
    def setUp(self):
        self.host = MockHost()
        # Mock a virtual test suite at virtual/gpu/external/wpt/foo.
        self.host.filesystem = MockFileSystem({
            MOCK_WEB_TESTS + 'VirtualTestSuites':
            b'[{"prefix": "gpu", "platforms": ["Linux", "Mac", "Win"], '
            b'"bases": ["external/wpt/foo"], "args": ["--foo"], '
            b'"expires": "never"}]',
            # Ensure `Port.all_expectations_dict()` recognizes the main file.
            MOCK_WEB_TESTS + 'TestExpectations':
            b'',
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
        self.buganizer_client.NewIssue.side_effect = lambda bug: BuganizerIssue(
            **{
                **dataclasses.asdict(bug),
                'issue_id': 111,
            })
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

    def _write_and_commit(self,
                          file_contents,
                          message: str = 'fake commit message'):
        for path, contents in file_contents.items():
            self.host.filesystem.write_text_file(path, contents)
            self.git.add(path)
        self.git.commit_locally_with_message(message)

    def _setup_checkout_with_new_failure(self):
        contents_before = textwrap.dedent("""\
            # results: [ Failure Pass Timeout ]
            """)
        self._write_and_commit(
            {
                MOCK_WEB_TESTS + 'external/wpt/foo/DIR_METADATA': '',
                MOCK_WEB_TESTS + 'TestExpectations': contents_before,
            },
            message=f'Import wpt@{"e" * 40}')
        contents_after = textwrap.dedent("""\
            # results: [ Failure Pass Timeout ]
            external/wpt/foo/bar.html [ Failure ]
            """)
        self._write_and_commit(
            {
                MOCK_WEB_TESTS + 'TestExpectations': contents_after,
            },
            message=f'Import wpt@{"f" * 40}')

    def _setup_import_cl(self, messages: List[str]):
        gerrit_query = (
            'https://chromium-review.googlesource.com/changes/'
            '?q=owner:wpt-autoroller%40chops-service-accounts.'
            'iam.gserviceaccount.com'
            f'+prefixsubject:"Import+wpt%40{"f" * 40}"+status:merged'
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
            'messages': [{
                'message': message
            } for message in messages],
        }
        self.host.web.urls = {
            gerrit_query:
            RESPONSE_PREFIX + b'\n' + json.dumps([payload]).encode(),
        }
        # Respond to the Gerrit POST message request with success.
        self.host.web.responses = [{'status_code': 200}]

    def test_main(self):
        """Exercise the `ImportNotifier` end-to-end happy path."""
        self._setup_checkout_with_new_failure()
        self._setup_import_cl(['Patch set 1: ignore this message\n'])
        dir_metadata = WPTDirMetadata(buganizer_public_component='123',
                                      should_notify=True)
        with mock.patch.object(self.notifier.owners_extractor,
                               'read_dir_metadata',
                               return_value=dir_metadata):
            bugs, _ = self.notifier.main()

        self.assertEqual(set(bugs), {'external/wpt/foo'})
        self.assertEqual(bugs['external/wpt/foo'].link,
                         'https://crbug.com/111')

        self.buganizer_client.NewIssue.assert_called_once()
        issue = self.buganizer_client.NewIssue.call_args.args[0]
        self.assertEqual(
            issue.title, '[WPT] New failures introduced in external/wpt/foo '
            'by import https://crrev.com/c/77777')
        self.assertEqual(issue.component_id, '123')
        self.assertEqual(issue.cc, [])
        self.assertIn(f'{"e" * 40}...{"f" * 40}', issue.description,
                      'description does not contain WPT revision range')

        self.assertEqual(len(self.host.web.requests), 1,
                         'Gerrit client should post exactly one comment')
        url, payload = self.host.web.requests[0]
        self.assertEqual(
            url, 'https://chromium-review.googlesource.com/a/changes/'
            'chromium%2Fsrc~main~I8888/revisions/current/review')
        self.assertEqual(
            json.loads(payload), {
                'message': ('Filed bugs for failures introduced by this CL: '
                            'https://crbug.com/111'),
            })

    def test_main_with_bugs_already_filed(self):
        self._setup_checkout_with_new_failure()
        self._setup_import_cl([
            'Patch set 1: ignore this message\n',
            'Filed bugs for failures introduced by this CL: '
            'https://crbug.com/111',
        ])
        dir_metadata = WPTDirMetadata(buganizer_public_component='123',
                                      should_notify=True)
        with mock.patch.object(self.notifier.owners_extractor,
                               'read_dir_metadata',
                               return_value=dir_metadata):
            bugs, _ = self.notifier.main()
            self.assertEqual(bugs, {})

        self.buganizer_client.NewIssue.assert_not_called()
        self.assertEqual(len(self.host.web.requests), 0,
                         'Gerrit client should not post a comment')

    def test_main_with_no_failures_to_notify(self):
        self.git.commit_locally_with_message(f'Import wpt@{"e" * 40}')
        self.git.commit_locally_with_message(f'Import wpt@{"f" * 40}')
        self._setup_import_cl(['Patch set 1: ignore this message\n'])
        dir_metadata = WPTDirMetadata(buganizer_public_component='123',
                                      should_notify=True)
        with mock.patch.object(self.notifier.owners_extractor,
                               'read_dir_metadata',
                               return_value=dir_metadata):
            bugs, _ = self.notifier.main()
            self.assertEqual(bugs, {})

        self.buganizer_client.NewIssue.assert_not_called()
        self.assertEqual(len(self.host.web.requests), 0,
                         'Gerrit client should not post a comment')

    def test_main_with_gerrit_outage(self):
        self._setup_checkout_with_new_failure()
        with mock.patch.object(self.gerrit_api,
                               'query_cls',
                               side_effect=GerritError):
            with self.assertRaises(GerritError):
                self.notifier.main()

        self.buganizer_client.NewIssue.assert_not_called()
        self.assertEqual(len(self.host.web.requests), 0,
                         'Gerrit client should not post a comment')

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

        self.notifier.examine_baseline_changes('HEAD', CLRevisionID(12345, 3))
        base_url = 'https://crrev.com/c/12345/3/third_party/blink/web_tests/'
        self.assertEqual(set(self.notifier.new_failures_by_directory),
                         {'external/wpt/foo'})
        failures = self.notifier.new_failures_by_directory['external/wpt/foo']
        self.assertEqual(failures.exp_by_file, {})
        self.assertEqual(failures.baseline_failures, [
            BaselineFailure('external/wpt/foo/bar.html?a',
                            base_url + 'external/wpt/foo/bar_a-expected.txt'),
            BaselineFailure(
                'external/wpt/foo/bar.html?b', base_url +
                'flag-specific/fake-flag/external/wpt/foo/bar_b-expected.txt'),
            BaselineFailure(
                'external/wpt/foo/bar.html?a', base_url +
                'platform/linux/external/wpt/foo/bar_a-expected.txt'),
        ])

    def test_examine_baseline_changes_pure_rename_no_new_failures(self):
        contents = textwrap.dedent("""\
            [FAIL] subtest
            """)
        self._write_and_commit({
            MOCK_WEB_TESTS + 'external/wpt/foo/DIR_METADATA':
            '',
            MOCK_WEB_TESTS + 'external/wpt/foo/bar.tentative.html':
            '',
            MOCK_WEB_TESTS + 'external/wpt/foo/bar.tentative_a-expected.txt':
            contents,
        })
        self._write_and_commit({
            MOCK_WEB_TESTS + 'external/wpt/foo/bar.html':
            '',
            MOCK_WEB_TESTS + 'external/wpt/foo/bar_a-expected.txt':
            contents,
        })

        changed_files = {
            RELATIVE_WEB_TESTS + 'external/wpt/foo/bar.html':
            FileStatus(
                FileStatusType.RENAME,
                RELATIVE_WEB_TESTS + 'external/wpt/foo/bar.tentative.html'),
            RELATIVE_WEB_TESTS + 'external/wpt/foo/bar_a-expected.txt':
            FileStatus(
                FileStatusType.RENAME, RELATIVE_WEB_TESTS +
                'external/wpt/foo/bar.tentative_a-expected.txt'),
        }
        with mock.patch.object(self.notifier.git,
                               'changed_files',
                               return_value=changed_files):
            self.notifier.examine_baseline_changes('HEAD',
                                                   CLRevisionID(12345, 3))

        self.assertEqual(self.notifier.new_failures_by_directory, {})

    def test_examine_baseline_changes_rename_with_new_failures(self):
        contents_before = textwrap.dedent("""\
            [FAIL] subtest
            """)
        self._write_and_commit({
            MOCK_WEB_TESTS + 'external/wpt/foo/DIR_METADATA':
            '',
            MOCK_WEB_TESTS + 'external/wpt/foo/bar.tentative.html':
            '',
            MOCK_WEB_TESTS + 'external/wpt/foo/bar.tentative_a-expected.txt':
            contents_before,
        })
        contents_after = contents_before + '[FAIL] new subtest\n'
        self._write_and_commit({
            MOCK_WEB_TESTS + 'external/wpt/foo/bar.html':
            '',
            MOCK_WEB_TESTS + 'external/wpt/foo/bar_a-expected.txt':
            contents_after,
        })

        changed_files = {
            RELATIVE_WEB_TESTS + 'external/wpt/foo/bar.html':
            FileStatus(
                FileStatusType.RENAME,
                RELATIVE_WEB_TESTS + 'external/wpt/foo/bar.tentative.html'),
            RELATIVE_WEB_TESTS + 'external/wpt/foo/bar_a-expected.txt':
            FileStatus(
                FileStatusType.RENAME, RELATIVE_WEB_TESTS +
                'external/wpt/foo/bar.tentative_a-expected.txt'),
        }
        with mock.patch.object(self.notifier.git,
                               'changed_files',
                               return_value=changed_files):
            self.notifier.examine_baseline_changes('HEAD',
                                                   CLRevisionID(12345, 3))

        base_url = 'https://crrev.com/c/12345/3/third_party/blink/web_tests/'
        self.assertEqual(set(self.notifier.new_failures_by_directory),
                         {'external/wpt/foo'})
        failures = self.notifier.new_failures_by_directory['external/wpt/foo']
        self.assertEqual(failures.baseline_failures, [
            BaselineFailure('external/wpt/foo/bar.html?a',
                            base_url + 'external/wpt/foo/bar_a-expected.txt'),
        ])

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

        self.notifier.examine_new_test_expectations('HEAD')
        self.assertEqual(set(self.notifier.new_failures_by_directory),
                         {'external/wpt/foo'})
        failures = self.notifier.new_failures_by_directory['external/wpt/foo']
        self.assertEqual(set(failures.exp_by_file),
                         {RELATIVE_WEB_TESTS + 'TestExpectations'})
        lines = failures.exp_by_file[RELATIVE_WEB_TESTS + 'TestExpectations']
        self.assertEqual([line.to_string() for line in lines], [
            'crbug.com/12345 [ Linux ] external/wpt/foo/bar.html [ Failure ]',
            'crbug.com/12345 [ Win ] external/wpt/foo/bar.html [ Timeout ]',
        ])
        self.assertEqual(failures.baseline_failures, [])

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

        failures = self.notifier.new_failures_by_directory['external/wpt/foo']
        failures.exp_by_file[RELATIVE_WEB_TESTS + 'TestExpectations'].append(
            typ_types.Expectation('crbug.com/12345',
                                  'external/wpt/foo/baz.html',
                                  results={typ_types.ResultType.Failure},
                                  lineno=100))
        failures = self.notifier.new_failures_by_directory['external/wpt/bar']
        failures.exp_by_file[RELATIVE_WEB_TESTS + 'TestExpectations'].append(
            typ_types.Expectation('crbug.com/12345',
                                  'external/wpt/bar/baz.html',
                                  results={typ_types.ResultType.Failure},
                                  lineno=200))

        bugs = self.notifier.create_bugs_from_new_failures(
            CommitRange('SHA_START', 'SHA_END'), CLRevisionID(12345, 4))

        # Only one directory has WPT-NOTIFY enabled.
        self.assertEqual(set(bugs), {'external/wpt/foo'})
        bug = bugs['external/wpt/foo']
        # The formatting of imported commits and new failures are already tested.
        self.assertEqual(set(bug.cc),
                         {'team-email@chromium.org', 'foolip@chromium.org'})
        self.assertEqual(bug.component_id, '999')
        self.assertEqual(
            bug.title, '[WPT] New failures introduced in '
            'external/wpt/foo by import https://crrev.com/c/12345')
        self.assertIn(
            'crbug.com/12345 external/wpt/foo/baz.html [ Failure ]: '
            'https://crrev.com/c/12345/4/'
            'third_party/blink/web_tests/TestExpectations#100',
            bug.description.splitlines())
        checks_url = ('See ' + CHECKS_URL_TEMPLATE + ' for details.').format(
            '12345', '1')
        self.assertIn(checks_url, bug.description.splitlines())
        self.assertIn(
            'This bug was filed automatically due to a new WPT test failure '
            'for which you are marked an OWNER. If you do not want to receive '
            'these reports, please add "wpt { notify: NO }"  to the relevant '
            'DIR_METADATA file.', bug.description.splitlines())

        self.notifier.file_bugs(bugs)
        self.buganizer_client.NewIssue.assert_called_once()

    def test_file_bug_without_owners(self):
        """A bug should be filed, even without OWNERS next to DIR_METADATA."""
        failures = self.notifier.new_failures_by_directory['external/wpt/foo']
        failures.exp_by_file[RELATIVE_WEB_TESTS + 'TestExpectations'].append(
            typ_types.Expectation('crbug.com/12345',
                                  'external/wpt/foo/baz.html',
                                  results={typ_types.ResultType.Failure}))
        dir_metadata = WPTDirMetadata(buganizer_public_component='123',
                                      should_notify=True)
        with mock.patch.object(self.notifier.owners_extractor,
                               'read_dir_metadata',
                               return_value=dir_metadata):
            bugs = self.notifier.create_bugs_from_new_failures(
                CommitRange('SHA_START', 'SHA_END'), CLRevisionID(12345, 4))

        self.assertEqual(set(bugs), {'external/wpt/foo'})
        bug = bugs['external/wpt/foo']
        self.assertEqual(bug.cc, [])
        self.assertEqual(bug.component_id, '123')
        self.assertEqual(
            bug.title, '[WPT] New failures introduced in external/wpt/foo '
            'by import https://crrev.com/c/12345')

    def test_no_bugs_filed_in_dry_run(self):
        failures = self.notifier.new_failures_by_directory['external/wpt/foo']
        failures.exp_by_file[RELATIVE_WEB_TESTS + 'TestExpectations'].append(
            typ_types.Expectation('crbug.com/12345',
                                  'external/wpt/foo/baz.html',
                                  results={typ_types.ResultType.Failure}))
        dir_metadata = WPTDirMetadata(buganizer_public_component='123',
                                      should_notify=True)
        with mock.patch.object(self.notifier.owners_extractor,
                               'read_dir_metadata',
                               return_value=dir_metadata):
            bugs = self.notifier.create_bugs_from_new_failures(
                CommitRange('SHA_START', 'SHA_END'), CLRevisionID(12345, 4))
        self.notifier.file_bugs(bugs, dry_run=True)
        self.buganizer_client.NewIssue.assert_not_called()

    def test_file_bugs_with_best_effort(self):
        """Failing to file a bug should not prevent additional attempts."""
        failures = self.notifier.new_failures_by_directory['external/wpt/foo']
        failures.exp_by_file[RELATIVE_WEB_TESTS + 'TestExpectations'].append(
            typ_types.Expectation('crbug.com/12345',
                                  'external/wpt/foo/baz.html',
                                  results={typ_types.ResultType.Failure}))
        failures = self.notifier.new_failures_by_directory['external/wpt/bar']
        failures.exp_by_file[RELATIVE_WEB_TESTS + 'TestExpectations'].append(
            typ_types.Expectation('crbug.com/12345',
                                  'external/wpt/bar/baz.html',
                                  results={typ_types.ResultType.Failure}))
        dir_metadata = WPTDirMetadata(buganizer_public_component='123',
                                      should_notify=True)
        with mock.patch.object(self.notifier.owners_extractor,
                               'read_dir_metadata',
                               return_value=dir_metadata):
            bugs = self.notifier.create_bugs_from_new_failures(
                CommitRange('SHA_START', 'SHA_END'), CLRevisionID(12345, 4))
        self.assertEqual(len(bugs), 2)

        self.buganizer_client.NewIssue.side_effect = BuganizerError
        self.notifier.file_bugs(bugs)
        self.assertEqual(self.buganizer_client.NewIssue.call_count, 2)


class DirectoryFailuresTest(unittest.TestCase):

    def test_directory_failures_to_str_baseline_change(self):
        failures = DirectoryFailures(baseline_failures=[
            BaselineFailure(
                'external/wpt/foo/bar.html',
                'https://crrev.com/c/12345/3/third_party/blink/web_tests/'
                'external/wpt/foo/bar-expected.txt'),
            BaselineFailure(
                'external/wpt/foo/bar.html',
                'https://crrev.com/c/12345/3/third_party/blink/web_tests/'
                'platform/linux/external/wpt/foo/bar-expected.txt'),
        ])
        self.assertEqual(
            failures.format_for_description(CLRevisionID(56789, 3)),
            textwrap.dedent("""\
                external/wpt/foo/bar.html new failing tests: https://crrev.com/c/12345/3/third_party/blink/web_tests/external/wpt/foo/bar-expected.txt
                [ Linux ] external/wpt/foo/bar.html new failing tests: https://crrev.com/c/12345/3/third_party/blink/web_tests/platform/linux/external/wpt/foo/bar-expected.txt
                """))

    def test_directory_failures_to_str_new_expectation(self):
        failures = DirectoryFailures({
            RELATIVE_WEB_TESTS + 'TestExpectations': [
                typ_types.Expectation('crbug.com/12345',
                                      'external/wpt/foo/bar.html', {'Linux'},
                                      {typ_types.ResultType.Failure},
                                      lineno=100),
                typ_types.Expectation(test='external/wpt/foo/baz.html',
                                      results={typ_types.ResultType.Failure},
                                      lineno=200),
            ],
        })
        self.assertEqual(
            failures.format_for_description(CLRevisionID(56789, 3)),
            textwrap.dedent("""\
                crbug.com/12345 [ Linux ] external/wpt/foo/bar.html [ Failure ]: https://crrev.com/c/56789/3/third_party/blink/web_tests/TestExpectations#100
                external/wpt/foo/baz.html [ Failure ]: https://crrev.com/c/56789/3/third_party/blink/web_tests/TestExpectations#200
                """))
