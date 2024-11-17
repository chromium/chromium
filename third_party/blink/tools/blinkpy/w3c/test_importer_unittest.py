# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import dataclasses
import json
import textwrap
import unittest
from unittest import mock

from blinkpy.common.checkout.git import (
    CommitRange,
    FileStatus,
    FileStatusType,
)
from blinkpy.common.checkout.git_mock import MockGit
from blinkpy.common.host_mock import MockHost
from blinkpy.common.net.git_cl import BuildStatus
from blinkpy.common.net.git_cl_mock import MockGitCL
from blinkpy.common.net.network_transaction import NetworkTimeout
from blinkpy.common.net.results_fetcher import Build
from blinkpy.common.path_finder import RELATIVE_WEB_TESTS
from blinkpy.common.system.executive_mock import MockCall
from blinkpy.common.system.executive_mock import MockExecutive
from blinkpy.common.system.log_testing import LoggingTestCase
from blinkpy.w3c.buganizer import BuganizerIssue
from blinkpy.w3c.chromium_commit_mock import MockChromiumCommit
from blinkpy.w3c.directory_owners_extractor import WPTDirMetadata
from blinkpy.w3c.import_notifier import DirectoryFailures, ImportNotifier
from blinkpy.w3c.local_wpt import LocalWPT
from blinkpy.w3c.local_wpt_mock import MockLocalWPT
from blinkpy.w3c.test_importer import TestImporter, ROTATIONS_URL, SHERIFF_EMAIL_FALLBACK, RUBBER_STAMPER_BOT
from blinkpy.w3c.wpt_github_mock import MockWPTGitHub
from blinkpy.w3c.wpt_manifest import BASE_MANIFEST_NAME
from blinkpy.web_tests.builder_list import BuilderList
from blinkpy.web_tests.models import typ_types
from unittest.mock import patch

MOCK_WEB_TESTS = '/mock-checkout/' + RELATIVE_WEB_TESTS
MANIFEST_INSTALL_CMD = [
    'python3',
    '/mock-checkout/third_party/wpt_tools/wpt/wpt',
    'manifest',
    '-v',
    '--no-download',
    f'--tests-root={MOCK_WEB_TESTS + "external/wpt"}',
    '--url-base=/',
]


class TestImporterTest(LoggingTestCase):

    def setUp(self):
        super().setUp()
        self.buganizer_client = mock.Mock()

    def mock_host(self):
        host = MockHost()
        host.builders = BuilderList({
            'cq-builder-a': {
                'port_name': 'linux-trusty',
                'specifiers': ['Trusty', 'Release'],
                'steps': {
                    'blink_web_tests': {},
                },
                'is_try_builder': True,
            },
            'cq-builder-b': {
                'port_name': 'mac-mac12',
                'specifiers': ['Mac12', 'Release'],
                'steps': {
                    'blink_web_tests': {},
                },
                'is_try_builder': True,
            },
            'cq-wpt-builder-c': {
                'port_name': 'linux-trusty',
                'specifiers': ['Trusty', 'Release'],
                'is_try_builder': True,
                'steps': {
                    'wpt_tests_suite': {},
                }
            },
            'CI Builder D': {
                'port_name': 'linux-trusty',
                'specifiers': ['Trusty', 'Release'],
            },
        })
        port = host.port_factory.get()
        MANIFEST_INSTALL_CMD[0] = port.python3_command()
        return host

    def _get_test_importer(self, host, github=None):
        port = host.port_factory.get()
        manifest = port.wpt_manifest('external/wpt')
        # Clear logs from manifest generation.
        self.logMessages().clear()
        return TestImporter(host,
                            github=github,
                            wpt_manifests=[manifest],
                            buganizer_client=self.buganizer_client)

    def test_update_expectations_for_cl_no_results(self):
        host = self.mock_host()
        host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'W3CImportExpectations', '')
        importer = self._get_test_importer(host)
        importer.git_cl = MockGitCL(host, time_out=True)
        success = importer.update_expectations_for_cl()
        self.assertFalse(success)
        self.assertLog([
            'INFO: Triggering try jobs for updating expectations:\n',
            'INFO:   cq-builder-a\n',
            'INFO:   cq-builder-b\n',
            'INFO:   cq-wpt-builder-c\n',
            'ERROR: No initial try job results, aborting.\n',
        ])

    def test_update_expectations_for_cl_closed_cl(self):
        host = self.mock_host()
        host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'W3CImportExpectations', '')
        importer = self._get_test_importer(host)
        importer.git_cl = MockGitCL(host,
                                    status='closed',
                                    try_job_results={
                                        Build('builder-a', 123):
                                        BuildStatus.SUCCESS,
                                    })
        success = importer.update_expectations_for_cl()
        self.assertFalse(success)
        self.assertLog([
            'INFO: Triggering try jobs for updating expectations:\n',
            'INFO:   cq-builder-a\n',
            'INFO:   cq-builder-b\n',
            'INFO:   cq-wpt-builder-c\n',
            'ERROR: The CL was closed, aborting.\n',
        ])

    def test_update_expectations_for_cl_all_jobs_pass(self):
        host = self.mock_host()
        host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'W3CImportExpectations', '')
        importer = self._get_test_importer(host)
        importer.git_cl = MockGitCL(host,
                                    status='lgtm',
                                    try_job_results={
                                        Build('builder-a', 123):
                                        BuildStatus.SUCCESS,
                                    })
        success = importer.update_expectations_for_cl()
        self.assertLog([
            'INFO: Triggering try jobs for updating expectations:\n',
            'INFO:   cq-builder-a\n',
            'INFO:   cq-builder-b\n',
            'INFO:   cq-wpt-builder-c\n',
            'INFO: All jobs finished.\n',
        ])
        self.assertTrue(success)

    def test_update_expectations_for_cl_fail_but_no_changes(self):
        host = self.mock_host()
        host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'W3CImportExpectations', '')
        importer = self._get_test_importer(host)
        importer.git_cl = MockGitCL(host,
                                    status='lgtm',
                                    try_job_results={
                                        Build('builder-a', 123):
                                        BuildStatus.FAILURE,
                                    })
        importer.fetch_new_expectations_and_baselines = lambda: None
        success = importer.update_expectations_for_cl()
        self.assertTrue(success)
        self.assertLog([
            'INFO: Triggering try jobs for updating expectations:\n',
            'INFO:   cq-builder-a\n',
            'INFO:   cq-builder-b\n',
            'INFO:   cq-wpt-builder-c\n',
            'INFO: All jobs finished.\n',
            'INFO: Skip Slow and Timeout tests.\n',
            'INFO: Generating MANIFEST.json\n',
        ])

    def test_run_commit_queue_for_cl_pass(self):
        host = self.mock_host()
        host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'W3CImportExpectations', '')
        importer = self._get_test_importer(host)
        # Only the latest job for each builder is counted.
        importer.git_cl = MockGitCL(host,
                                    status='lgtm',
                                    try_job_results={
                                        Build('cq-builder-a', 120):
                                        BuildStatus.FAILURE,
                                        Build('cq-builder-a', 123):
                                        BuildStatus.SUCCESS,
                                    })

        success = importer.run_commit_queue_for_cl()
        self.assertTrue(success)
        self.assertLog([
            'INFO: Triggering CQ try jobs.\n',
            'INFO: All jobs finished.\n',
            'INFO: CQ appears to have passed; sending to the rubber-stamper '
            'bot for CR+1 and commit.\n',
            'INFO: If the rubber-stamper bot rejects the CL, you either need '
            'to modify the benign file patterns, or manually CR+1 and land the '
            'import yourself if it touches code files. See https://chromium.'
            'googlesource.com/infra/infra/+/refs/heads/main/go/src/infra/'
            'appengine/rubber-stamper/README.md\n',
            'INFO: Update completed.\n',
        ])
        self.assertEqual(importer.git_cl.calls, [
            ['git', 'cl', 'try'],
            [
                'git', 'cl', 'upload', '-f', '--send-mail',
                '--enable-auto-submit', '--reviewers', RUBBER_STAMPER_BOT
            ],
        ])

    def test_run_commit_queue_for_cl_fail_cq(self):
        host = self.mock_host()
        host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'W3CImportExpectations', '')
        importer = self._get_test_importer(host)
        importer.git_cl = MockGitCL(host,
                                    status='lgtm',
                                    try_job_results={
                                        Build('cq-builder-a', 120):
                                        BuildStatus.SUCCESS,
                                        Build('cq-builder-a', 123):
                                        BuildStatus.FAILURE,
                                        Build('cq-builder-b', 200):
                                        BuildStatus.SUCCESS,
                                    })
        importer.fetch_new_expectations_and_baselines = lambda: None

        success = importer.run_commit_queue_for_cl()
        self.assertFalse(success)
        self.assertLog([
            'INFO: Triggering CQ try jobs.\n',
            'INFO: All jobs finished.\n',
            'ERROR: CQ appears to have failed; aborting.\n',
        ])
        self.assertEqual(importer.git_cl.calls, [
            ['git', 'cl', 'try'],
        ])

    def test_run_commit_queue_for_cl_fail_to_land(self):
        host = self.mock_host()
        host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'W3CImportExpectations', '')
        importer = self._get_test_importer(host)
        # Only the latest job for each builder is counted.
        importer.git_cl = MockGitCL(host,
                                    status='lgtm',
                                    try_job_results={
                                        Build('cq-builder-a', 120):
                                        BuildStatus.FAILURE,
                                        Build('cq-builder-a', 123):
                                        BuildStatus.SUCCESS,
                                    })
        importer._need_sheriff_attention = lambda: False
        importer.git_cl.wait_for_closed_status = lambda timeout_seconds: False

        success = importer.run_commit_queue_for_cl()
        self.assertFalse(success)
        self.assertLog([
            'INFO: Triggering CQ try jobs.\n',
            'INFO: All jobs finished.\n',
            'INFO: CQ appears to have passed; sending to the rubber-stamper '
            'bot for CR+1 and commit.\n',
            'INFO: If the rubber-stamper bot rejects the CL, you either need '
            'to modify the benign file patterns, or manually CR+1 and land the '
            'import yourself if it touches code files. See https://chromium.'
            'googlesource.com/infra/infra/+/refs/heads/main/go/src/infra/'
            'appengine/rubber-stamper/README.md\n',
            'ERROR: Cannot submit CL; aborting.\n',
        ])
        self.assertEqual(importer.git_cl.calls, [
            ['git', 'cl', 'try'],
            [
                'git', 'cl', 'upload', '-f', '--send-mail',
                '--enable-auto-submit', '--reviewers', RUBBER_STAMPER_BOT
            ],
        ])

    def test_run_commit_queue_for_cl_closed_cl(self):
        host = self.mock_host()
        host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'W3CImportExpectations', '')
        importer = self._get_test_importer(host)
        importer.git_cl = MockGitCL(host,
                                    status='closed',
                                    try_job_results={
                                        Build('cq-builder-a', 120):
                                        BuildStatus.SUCCESS,
                                        Build('cq-builder-b', 200):
                                        BuildStatus.SUCCESS,
                                    })

        success = importer.run_commit_queue_for_cl()
        self.assertFalse(success)
        self.assertLog([
            'INFO: Triggering CQ try jobs.\n',
            'ERROR: The CL was closed; aborting.\n',
        ])
        self.assertEqual(importer.git_cl.calls, [
            ['git', 'cl', 'try'],
        ])

    def test_run_commit_queue_for_cl_timeout(self):
        # This simulates the case where we time out while waiting for try jobs.
        host = self.mock_host()
        importer = self._get_test_importer(host)
        importer.git_cl = MockGitCL(host, time_out=True)
        success = importer.run_commit_queue_for_cl()
        self.assertFalse(success)
        self.assertLog([
            'INFO: Triggering CQ try jobs.\n',
            'ERROR: Timed out waiting for CQ; aborting.\n'
        ])
        self.assertEqual(importer.git_cl.calls, [['git', 'cl', 'try']])

    def test_submit_cl_timeout_and_already_merged(self):
        # Here we simulate a case where we timeout waiting for the CQ to submit a
        # CL because we miss the notification that it was merged. We then get an
        # error when trying to close the CL because it's already been merged.
        host = self.mock_host()
        host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'W3CImportExpectations', '')
        importer = self._get_test_importer(host)
        importer.git_cl = MockGitCL(
            host,
            status='lgtm',
            # Only the latest job for each builder is counted.
            try_job_results={
                Build('cq-builder-a', 120): BuildStatus.FAILURE,
                Build('cq-builder-a', 123): BuildStatus.SUCCESS,
            })
        importer._need_sheriff_attention = lambda: False
        importer.git_cl.wait_for_closed_status = lambda timeout_seconds: False
        success = importer.run_commit_queue_for_cl()
        # Since the CL is already merged, we absorb the error and treat it as success.
        self.assertFalse(success)
        self.assertLog([
            'INFO: Triggering CQ try jobs.\n',
            'INFO: All jobs finished.\n',
            'INFO: CQ appears to have passed; sending to the rubber-stamper '
            'bot for CR+1 and commit.\n',
            'INFO: If the rubber-stamper bot rejects the CL, you either need '
            'to modify the benign file patterns, or manually CR+1 and land the '
            'import yourself if it touches code files. See https://chromium.'
            'googlesource.com/infra/infra/+/refs/heads/main/go/src/infra/'
            'appengine/rubber-stamper/README.md\n',
            'ERROR: Cannot submit CL; aborting.\n',
        ])
        self.assertEqual(importer.git_cl.calls, [
            ['git', 'cl', 'try'],
            [
                'git', 'cl', 'upload', '-f', '--send-mail',
                '--enable-auto-submit', '--reviewers', RUBBER_STAMPER_BOT
            ],
        ])

    def test_apply_exportable_commits_locally(self):
        # TODO(robertma): Consider using MockLocalWPT.
        host = self.mock_host()
        importer = self._get_test_importer(
            host, github=MockWPTGitHub(pull_requests=[]))
        importer.wpt_git = MockGit(cwd='/tmp/wpt', executive=host.executive)
        fake_commit = MockChromiumCommit(
            host,
            subject='My fake commit',
            patch=('Fake patch contents...\n'
                   '--- a/' + RELATIVE_WEB_TESTS +
                   'external/wpt/css/css-ui-3/outline-004.html\n'
                   '+++ b/' + RELATIVE_WEB_TESTS +
                   'external/wpt/css/css-ui-3/outline-004.html\n'
                   '@@ -20,7 +20,7 @@\n'
                   '...'))
        importer.exportable_but_not_exported_commits = lambda _: [fake_commit]
        applied = importer.apply_exportable_commits_locally(LocalWPT(host))
        self.assertEqual(applied, [fake_commit])
        # This assertion is implementation details of LocalWPT.apply_patch.
        # TODO(robertma): Move this to local_wpt_unittest.py.
        self.assertEqual(host.executive.full_calls, [
            MockCall(MANIFEST_INSTALL_CMD,
                     kwargs={
                         'input': None,
                         'cwd': None,
                         'env': None
                     }),
            MockCall(
                ['git', 'apply', '-'], {
                    'input': ('Fake patch contents...\n'
                              '--- a/css/css-ui-3/outline-004.html\n'
                              '+++ b/css/css-ui-3/outline-004.html\n'
                              '@@ -20,7 +20,7 @@\n'
                              '...'),
                    'cwd':
                    '/tmp/wpt',
                    'env':
                    None
                }),
            MockCall(['git', 'add', '.'],
                     kwargs={
                         'input': None,
                         'cwd': '/tmp/wpt',
                         'env': None
                     })
        ])
        self.assertEqual(
            importer.wpt_git.local_commits(),
            [['Applying patch 14fd77e88e42147c57935c49d9e3b2412b8491b7']])

    def test_apply_exportable_commits_locally_returns_none_on_failure(self):
        host = self.mock_host()
        github = MockWPTGitHub(pull_requests=[])
        importer = self._get_test_importer(host, github=github)
        commit = MockChromiumCommit(host, subject='My fake commit')
        importer.exportable_but_not_exported_commits = lambda _: [commit]
        # Failure to apply patch.
        local_wpt = MockLocalWPT(apply_patch=['Failed'])
        applied = importer.apply_exportable_commits_locally(local_wpt)
        self.assertIsNone(applied)

    def test_get_directory_owners(self):
        host = self.mock_host()
        host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'W3CImportExpectations', '')
        host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'external/wpt/foo/OWNERS',
            'someone@chromium.org\n')
        importer = self._get_test_importer(host)
        importer.project_git.changed_files = lambda: {
            RELATIVE_WEB_TESTS + 'external/wpt/foo/x.html':
            FileStatus(FileStatusType.MODIFY),
        }
        self.assertEqual(importer.get_directory_owners(),
                         {('someone@chromium.org', ): ['external/wpt/foo']})

    def test_get_directory_owners_no_changed_files(self):
        host = self.mock_host()
        host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'W3CImportExpectations', '')
        host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'external/wpt/foo/OWNERS',
            'someone@chromium.org\n')
        importer = self._get_test_importer(host)
        self.assertEqual(importer.get_directory_owners(), {})

    def test_delete_orphaned_baselines(self):
        orphaned_baselines = {
            'external/wpt/dir/variants_orphaned-expected.txt',
            'platform/mac/virtual/fake-vts/'
            'external/wpt/dir/variants_orphaned-expected.txt',
            'external/wpt/orphaned-expected.txt',
            'flag-specific/fake-flag/external/wpt/orphaned-expected.txt',
        }
        valid_baselines = {
            'not-a-wpt-expected.txt',
            'external/wpt/dir/variants_not-orphaned-expected.txt',
            'external/wpt/not-orphaned-expected.txt',
        }

        host = self.mock_host()
        fs = host.filesystem
        manifest = {
            'items': {
                'testharness': {
                    'dir': {
                        'variants.html': [
                            '89ab',
                            ['dir/variants.html?not-orphaned', {}],
                        ],
                    },
                },
                'wdspec': {
                    'not-orphaned.py': ['cdef', [None, {}]],
                },
            },
        }
        fs.write_text_file(MOCK_WEB_TESTS + 'external/wpt/MANIFEST.json',
                           json.dumps(manifest))
        for baseline in [*orphaned_baselines, *valid_baselines]:
            fs.write_text_file(MOCK_WEB_TESTS + baseline, '')

        port = host.port_factory.get('test-linux-trusty')
        importer = TestImporter(host, buganizer_client=mock.Mock())
        with mock.patch.object(host.port_factory, 'get', return_value=port):
            importer.delete_orphaned_baselines()

        self.assertLog(['INFO: Deleted 4 orphaned baseline(s).\n'])
        for baseline in orphaned_baselines:
            self.assertFalse(fs.exists(MOCK_WEB_TESTS + baseline),
                             f'{baseline!r} should not exist')
        for baseline in valid_baselines:
            self.assertTrue(fs.exists(MOCK_WEB_TESTS + baseline),
                            f'{baseline!r} should exist')

    # Tests for protected methods - pylint: disable=protected-access

    def test_commit_changes(self):
        host = self.mock_host()
        importer = self._get_test_importer(host)
        importer._commit_changes('dummy message')
        self.assertEqual(importer.project_git.local_commits(),
                         [['dummy message']])

    def test_commit_message(self):
        importer = self._get_test_importer(self.mock_host())
        self.assertEqual(
            importer.commit_message('aaaa',
                                    CommitRange('0123456789', 'a123456789')),
            textwrap.dedent("""\
                Import wpt@a123456789

                https://github.com/web-platform-tests/wpt/compare/012345678...a12345678

                Using wpt-import in Chromium aaaa.
                """))

    def test_commit_message_with_pending_exportable_changes(self):
        host = self.mock_host()
        importer = self._get_test_importer(host)
        locally_applied_commits = [
            MockChromiumCommit(host, subject='Pending export 1'),
            MockChromiumCommit(
                host,
                'refs/heads/main@{#222)',
                subject=f'Pending export 2 with very long subject {"a" * 80}'),
        ]
        self.assertEqual(
            importer.commit_message('aaaa', CommitRange('0000', '1111'),
                                    locally_applied_commits),
            textwrap.dedent("""\
                Import wpt@1111

                https://github.com/web-platform-tests/wpt/compare/0000...1111

                Using wpt-import in Chromium aaaa.
                With Chromium commits locally applied on WPT:
                  14fd77e88e "Pending export 1"
                  3e977a7ce6 "Pending export 2 with very long subject [...]
                """)),

    def test_cl_description_with_empty_environ(self):
        host = self.mock_host()
        host.executive = MockExecutive(output='Last commit message\n\n')
        importer = self._get_test_importer(host)
        description = importer.cl_description(directory_owners={})
        self.assertEqual(
            description,
            textwrap.dedent("""\
                Last commit message

                Note to gardeners: This CL imports external tests and adds expectations
                for those tests; if this CL is large and causes a few new failures,
                please fix the failures by adding new lines to TestExpectations rather
                than reverting. See:
                https://chromium.googlesource.com/chromium/src/+/main/docs/testing/web_platform_tests.md

                NOAUTOREVERT=true
                No-Export: true
                Cq-Include-Trybots: luci.chromium.try:linux-blink-rel
                """))
        self.assertEqual(host.executive.calls, [MANIFEST_INSTALL_CMD] +
                         [['git', 'log', '-1', '--format=%B']])

    def test_cl_description_with_directory_owners(self):
        host = self.mock_host()
        host.executive = MockExecutive(output='Last commit message\n\n')
        importer = self._get_test_importer(host)
        description = importer.cl_description(
            directory_owners={
                ('someone@chromium.org', ):
                ['external/wpt/foo', 'external/wpt/bar'],
                ('x@chromium.org', 'y@chromium.org'): ['external/wpt/baz'],
            })
        self.assertIn(
            'Directory owners for changes in this CL:\n'
            'someone@chromium.org:\n'
            '  external/wpt/foo\n'
            '  external/wpt/bar\n'
            'x@chromium.org, y@chromium.org:\n'
            '  external/wpt/baz\n\n', description)

    def test_sheriff_email_no_response_uses_backup(self):
        host = self.mock_host()
        importer = self._get_test_importer(host)
        self.assertEqual(SHERIFF_EMAIL_FALLBACK, importer.sheriff_email())
        self.assertLog([
            'ERROR: Exception while fetching current sheriff: '
            'Expecting value: line 1 column 1 (char 0)\n'
        ])

    def test_sheriff_email_no_emails_field(self):
        host = self.mock_host()
        host.web.urls[ROTATIONS_URL] = json.dumps(
            {'updated_unix_timestamp': '1591108191'})
        importer = self._get_test_importer(host)
        self.assertEqual(SHERIFF_EMAIL_FALLBACK, importer.sheriff_email())
        self.assertLog([
            'ERROR: No email found for current sheriff. Retrieved content: %s\n'
            % host.web.urls[ROTATIONS_URL]
        ])

    def test_sheriff_email_nobody_on_rotation(self):
        host = self.mock_host()
        host.web.urls[ROTATIONS_URL] = json.dumps({
            'emails': [],
            'updated_unix_timestamp':
            '1591108191'
        })
        importer = self._get_test_importer(host)
        self.assertEqual(SHERIFF_EMAIL_FALLBACK, importer.sheriff_email())
        self.assertLog([
            'ERROR: No email found for current sheriff. Retrieved content: %s\n'
            % host.web.urls[ROTATIONS_URL]
        ])

    def test_sheriff_email_rotations_url_unavailable(self):
        def raise_exception(*_):
            raise NetworkTimeout

        host = self.mock_host()
        host.web.get_binary = raise_exception
        importer = self._get_test_importer(host)
        self.assertEqual(SHERIFF_EMAIL_FALLBACK, importer.sheriff_email())
        self.assertLog([
            'ERROR: Cannot fetch %s\n' % ROTATIONS_URL,
        ])

    def test_sheriff_email(self):
        host = self.mock_host()
        host.web.urls[ROTATIONS_URL] = json.dumps({
            'emails': ['current-sheriff@chromium.org'],
            'updated_unix_timestamp':
            '1591108191',
        })
        importer = self._get_test_importer(host)
        self.assertEqual('current-sheriff@chromium.org',
                         importer.sheriff_email())
        self.assertLog([])

    def test_generate_manifest_successful_run(self):
        # This test doesn't test any aspect of the real manifest script, it just
        # asserts that TestImporter._generate_manifest would invoke the script.
        host = self.mock_host()
        importer = self._get_test_importer(host)
        host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'external/wpt/MANIFEST.json', '{}')
        importer._generate_manifest()
        self.assertEqual(host.executive.calls, [MANIFEST_INSTALL_CMD] * 2)
        self.assertEqual(importer.project_git.added_paths,
                         {MOCK_WEB_TESTS + 'external/' + BASE_MANIFEST_NAME})

    def test_has_wpt_changes(self):
        host = self.mock_host()
        importer = self._get_test_importer(host)
        importer.project_git.changed_files = lambda: {
            RELATIVE_WEB_TESTS + 'external/' + BASE_MANIFEST_NAME:
            FileStatus(FileStatusType.MODIFY),
            RELATIVE_WEB_TESTS + 'external/wpt/foo/x.html':
            FileStatus(FileStatusType.MODIFY),
        }
        self.assertTrue(importer._has_wpt_changes())

        importer.project_git.changed_files = lambda: {
            RELATIVE_WEB_TESTS + 'external/' + BASE_MANIFEST_NAME:
            FileStatus(FileStatusType.MODIFY),
            RELATIVE_WEB_TESTS + 'TestExpectations':
            FileStatus(FileStatusType.MODIFY),
        }
        self.assertFalse(importer._has_wpt_changes())

        importer.project_git.changed_files = lambda: {
            RELATIVE_WEB_TESTS + 'external/' + BASE_MANIFEST_NAME:
            FileStatus(FileStatusType.MODIFY),
        }
        self.assertFalse(importer._has_wpt_changes())

    def test_file_and_record_bugs_update_bugs(self):
        host = self.mock_host()
        importer = self._get_test_importer(host)
        importer.git_cl = MockGitCL(host)

        git, fs = importer.project_git, host.filesystem
        fs.write_text_file(MOCK_WEB_TESTS + 'external/wpt/foo/DIR_METADATA',
                           '')
        git.new_branch('update_wpt')
        exp_path = MOCK_WEB_TESTS + 'TestExpectations'
        fs.write_text_file(
            exp_path,
            textwrap.dedent("""\
                # results: [ Failure Pass Timeout ]
                # tags: [ Linux Mac ]
                crbug.com/555 [ Mac ] external/wpt/foo/new-for-platform.html [ Failure ]
                """))
        git.add_list([exp_path])
        git.commit_locally_with_message(f'Import wpt@{"e" * 40}')
        fs.write_text_file(
            exp_path,
            textwrap.dedent("""\
                # results: [ Failure Pass Timeout ]
                # tags: [ Linux Mac ]
                external/wpt/foo/new.html [ Failure ]
                [ Linux ] external/wpt/foo/new-for-platform.html [ Failure ]
                crbug.com/555 [ Mac ] external/wpt/foo/new-for-platform.html [ Failure ]
                # Manually added expectation with existing bug
                crbug.com/444 external/wpt/foo/do-not-modify.html [ Failure ]
                """))
        git.add_list([exp_path])
        git.commit_locally_with_message(f'Import wpt@{"f" * 40}')

        local_wpt = MockLocalWPT()
        gerrit_cl = mock.Mock(messages=[], number=999)
        gerrit_api = mock.Mock()
        gerrit_api.query_cls.return_value = [gerrit_cl]
        self.buganizer_client.NewIssue.side_effect = lambda issue: BuganizerIssue(
            **{
                **dataclasses.asdict(issue),
                'issue_id': 111,
            })
        notifier = ImportNotifier(host, git, local_wpt, gerrit_api,
                                  self.buganizer_client)
        with mock.patch(
                'blinkpy.w3c.import_notifier.'
                'DirectoryOwnersExtractor.read_dir_metadata',
                return_value=WPTDirMetadata(should_notify=True)):
            importer.file_and_record_bugs(notifier)

        gerrit_cl.post_comment.assert_called_once_with(
            'Filed bugs for failures introduced by this CL: '
            'https://crbug.com/111')
        self.buganizer_client.NewIssue.assert_called_once()
        self.assertEqual(
            git.show_blob(RELATIVE_WEB_TESTS + 'TestExpectations',
                          'HEAD').decode(),
            textwrap.dedent("""\
                # results: [ Failure Pass Timeout ]
                # tags: [ Linux Mac ]
                crbug.com/111 external/wpt/foo/new.html [ Failure ]
                crbug.com/111 [ Linux ] external/wpt/foo/new-for-platform.html [ Failure ]
                crbug.com/555 [ Mac ] external/wpt/foo/new-for-platform.html [ Failure ]
                # Manually added expectation with existing bug
                crbug.com/444 external/wpt/foo/do-not-modify.html [ Failure ]
                """))
        expected_message = textwrap.dedent("""\
            Update `TestExpectations` with bugs filed for crrev.com/c/999

            Bug: 111
            """)
        self.assertEqual([[
            'git',
            'cl',
            'upload',
            '--bypass-hooks',
            '-f',
            f'--message={expected_message}',
            '--send-mail',
            '--enable-auto-submit',
            '--reviewers=rubber-stamper@appspot.gserviceaccount.com',
        ]], importer.git_cl.calls)
        self.assertEqual(git.tracking_branch, 'update_wpt')
        self.assertNotEqual(git.current_branch(), 'update_wpt')

    def test_file_and_record_bugs_no_upload_reformat(self):
        """Do not create CLs for cosmetic-only changes."""
        host = self.mock_host()
        importer = self._get_test_importer(host)
        importer.git_cl = MockGitCL(host)

        git, fs = importer.project_git, host.filesystem
        fs.write_text_file(MOCK_WEB_TESTS + 'external/wpt/foo/DIR_METADATA',
                           '')
        git.new_branch('update_wpt')
        exp_path = MOCK_WEB_TESTS + 'TestExpectations'
        contents = textwrap.dedent("""\
            # results: [ Failure Pass Timeout ]
            # Serializing this file will sort the statuses.
            crbug.com/123 external/wpt/no-change.html [ Timeout Failure ]
            """)
        fs.write_text_file(exp_path, contents)
        git.add_list([MOCK_WEB_TESTS])
        git.commit_locally_with_message(f'Import wpt@{"e" * 40}')
        git.commit_locally_with_message(f'Import wpt@{"f" * 40}')

        local_wpt = MockLocalWPT()
        gerrit_api = mock.Mock()
        gerrit_api.query_cls.return_value = [mock.Mock(messages=[])]
        notifier = ImportNotifier(host, git, local_wpt, gerrit_api,
                                  self.buganizer_client)
        with mock.patch(
                'blinkpy.w3c.import_notifier.'
                'DirectoryOwnersExtractor.read_dir_metadata',
                return_value=WPTDirMetadata(should_notify=True)):
            importer.file_and_record_bugs(notifier)

        self.assertEqual(
            git.show_blob(RELATIVE_WEB_TESTS + 'TestExpectations',
                          'HEAD').decode(), contents)
        self.assertEqual(importer.git_cl.calls, [])
        self.assertEqual(git.current_branch(), 'update_wpt')

    def test_file_and_record_bugs_notify_on_timeout(self):
        host = self.mock_host()
        importer = self._get_test_importer(host)
        importer.git_cl = MockGitCL(host, status='commit', time_out=True)

        git, fs = importer.project_git, host.filesystem
        exp_path = MOCK_WEB_TESTS + 'TestExpectations'
        fs.write_text_file(
            exp_path,
            textwrap.dedent("""\
                # results: [ Pass Failure ]
                external/wpt/foo/new.html [ Failure ]
                """))
        git.add_list([MOCK_WEB_TESTS])
        git.commit_locally_with_message(f'Import wpt@{"e" * 40}')

        # For this test, don't actually simulate bug filing.
        exp = typ_types.Expectation(test='external/wpt/foo/new.html',
                                    results=frozenset(
                                        [typ_types.ResultType.Failure]))
        notifier = mock.Mock(default_port=host.port_factory.get('test'))
        notifier.new_failures_by_directory = {
            'external/wpt/foo': DirectoryFailures({exp_path: [exp]}),
        }
        notifier.main.return_value = {
            'external/wpt/foo': BuganizerIssue('New failures', '', '', 111),
        }, mock.Mock()

        with importer:
            importer.file_and_record_bugs(notifier)
        self.assertLog([
            'INFO: Filing bugs for the last WPT import.\n',
            'INFO: Committing changes.\n',
            'INFO: Uploading change list.\n',
            'INFO: Issue: https://crrev.com/c/1234\n',
            'WARNING: Failed to automatically submit https://crrev.com/c/1234. '
            'Pinging https://crbug.com/111 for help.\n',
        ])
        self.buganizer_client.NewComment.assert_called_once_with(111, mock.ANY)
        _, message = self.buganizer_client.NewComment.call_args.args
        self.assertIn('https://crrev.com/c/1234 backfills TestExpectations',
                      message)
        self.assertIn(['git', 'cl', 'set-close'], importer.git_cl.calls)

    def test_find_insert_index_ignore_pattern_empty_list(self):
        host = self.mock_host()
        test_importer = self._get_test_importer(host)

        targets_list = []
        insert_key = "test1"

        insert_index = test_importer.find_insert_index_ignore_comments(
            targets_list, insert_key)

        self.assertEqual(insert_index, 0)

    def test_find_insert_index_ignore_pattern_with_duplicate(self):
        host = self.mock_host()
        test_importer = self._get_test_importer(host)

        targets_list = ["test1", "test2", "# test3", "test4", "test5"]
        insert_key = "test2"

        insert_index = test_importer.find_insert_index_ignore_comments(
            targets_list, insert_key)

        self.assertEqual(insert_index, 1)

    def test_find_insert_index_ignore_comments_with_middle_start_index(self):
        host = self.mock_host()
        test_importer = self._get_test_importer(host)

        targets_list = ["test1", "test2", "test3", "test4", "test5"]
        insert_key = "test0"
        start_index = 2

        insert_index = test_importer.find_insert_index_ignore_comments(
            targets_list, insert_key, start_index)

        self.assertEqual(insert_index, 2)

    def test_find_insert_index_ignore_comments_start_index_equal_to_list_length(
            self):
        host = self.mock_host()
        test_importer = self._get_test_importer(host)

        targets_list = ["test1", "test2", "test3", "test4", "test5"]

        # smaller than last item
        insert_index = test_importer.find_insert_index_ignore_comments(
            targets_list, "test3", 5)
        self.assertEqual(insert_index, 5)

        # larger than last item

        insert_index = test_importer.find_insert_index_ignore_comments(
            targets_list, "test9", 5)
        self.assertEqual(insert_index, 5)

    def test_find_insert_index_ignore_comments_start_index_equal_to_last_index(
            self):
        host = self.mock_host()
        test_importer = self._get_test_importer(host)

        targets_list = ["test1", "test2", "test3", "test4", "test5"]

        # smaller than last item
        insert_index = test_importer.find_insert_index_ignore_comments(
            targets_list, "test3", 4)
        self.assertEqual(insert_index, 4)

        # larger than last item
        insert_index = test_importer.find_insert_index_ignore_comments(
            targets_list, "test9", 4)
        self.assertEqual(insert_index, 5)

    def test_find_insert_index_ignore_pattern(self):
        host = self.mock_host()
        test_importer = self._get_test_importer(host)

        targets_list = ["test1", "# test3", "test4", "test5"]
        insert_key = "test2"
        filter = lambda key: key.startswith("test")

        insert_index = test_importer.find_insert_index_ignore_comments(
            targets_list, insert_key)

        self.assertEqual(insert_index, 2)

    def test_update_testlist_lines(self):
        host = self.mock_host()
        test_importer = self._get_test_importer(host)

        testlist_lines = [
            "# comment",
            "external/wpt/test1.html",
            "# comment",
            "external/wpt/test2.html",
            "# comment",
            "external/wpt/test3.html",
            "# comment",
        ]
        added_tests = ["external/wpt/test4.html", "external/wpt/test5.html"]
        deleted_tests = ["external/wpt/test2.html"]

        new_testlist_lines = test_importer.update_testlist_lines(
            testlist_lines, added_tests, deleted_tests)

        expected_new_testlist_lines = [
            "# comment",
            "external/wpt/test1.html",
            "# comment",
            "# comment",
            "external/wpt/test3.html",
            "external/wpt/test4.html",
            "external/wpt/test5.html",
            "# comment",
        ]

        self.assertEqual(new_testlist_lines, expected_new_testlist_lines)

    def test_update_testlist_with_idlharness_changes(self):
        host = self.mock_host()
        importer = self._get_test_importer(host)

        def _git_added_files():
            return [
                MOCK_WEB_TESTS + "external/wpt/2_added_idlharness.html",
                MOCK_WEB_TESTS + "external/wpt/3_duplicate_idlharness.html",
                MOCK_WEB_TESTS + "external/wpt/4_new_idlharness.html",
            ]

        def _git_deleted_files():
            return [
                MOCK_WEB_TESTS + "external/wpt/5_old_idlharness.html",
                MOCK_WEB_TESTS + "external/wpt/6_deleted_idlharness.html",
            ]

        importer.project_git.added_files = _git_added_files
        importer.project_git.deleted_files = _git_deleted_files
        importer.project_git._relative_to_web_test_dir = \
            lambda test_path: test_path
        testlist_path = importer.finder.path_from_web_tests(
            "TestLists", "android.filter")
        test_list_lines = [
            'external/wpt/1_first_idlharness.html',
            'external/wpt/3_duplicate_idlharness.html',
            'external/wpt/5_old_idlharness.html',
            'external/wpt/6_deleted_idlharness.html',
            'external/wpt/7_last_idlharness.html',
        ]
        expected_test_list_lines = [
            'external/wpt/1_first_idlharness.html',
            'external/wpt/2_added_idlharness.html',
            'external/wpt/3_duplicate_idlharness.html',
            'external/wpt/4_new_idlharness.html',
            'external/wpt/7_last_idlharness.html',
        ]
        host.filesystem.write_text_file(testlist_path,
                                        "\n".join(test_list_lines))
        with patch.object(importer.project_git, "run") as mock_git_run:
            importer.update_testlist_with_idlharness_changes(testlist_path)
            actual_test_list_lines = host.filesystem.open_text_file_for_reading(
                testlist_path).read().split("\n")
            self.assertEqual(actual_test_list_lines, expected_test_list_lines)
            mock_git_run.assert_called_with(['add', testlist_path])

    def test_update_testlist_with_idlharness_changes_with_comment(self):
        host = self.mock_host()
        importer = self._get_test_importer(host)

        def _git_added_files():
            return [
                MOCK_WEB_TESTS + "external/wpt/9_added_idlharness.html",
            ]

        def _git_deleted_files():
            return []

        importer.project_git.added_files = _git_added_files
        importer.project_git.deleted_files = _git_deleted_files
        importer.project_git._relative_to_web_test_dir = \
            lambda test_path: test_path
        testlist_path = importer.finder.path_from_web_tests(
            "TestLists", "android.filter")
        test_list_lines = [
            '# comment 1',
            'external/wpt/1_first_idlharness.html',
            '# comment 2',
            'external/wpt/7_last_idlharness.html',
            '# comment 3',
        ]
        expected_test_list_lines = [
            '# comment 1',
            'external/wpt/1_first_idlharness.html',
            '# comment 2',
            'external/wpt/7_last_idlharness.html',
            "external/wpt/9_added_idlharness.html",
            '# comment 3',
        ]
        host.filesystem.write_text_file(testlist_path,
                                        "\n".join(test_list_lines))
        with patch.object(importer.project_git, "run") as mock_git_run:
            importer.update_testlist_with_idlharness_changes(testlist_path)
            actual_test_list_lines = host.filesystem.open_text_file_for_reading(
                testlist_path).read().split("\n")
            self.assertEqual(actual_test_list_lines, expected_test_list_lines)
            mock_git_run.assert_called_with(['add', testlist_path])

    def test_need_sheriff_attention(self):
        host = self.mock_host()
        importer = self._get_test_importer(host)
        importer.project_git.changed_files = lambda: {
            RELATIVE_WEB_TESTS + 'external/' + BASE_MANIFEST_NAME:
            FileStatus(FileStatusType.MODIFY),
            RELATIVE_WEB_TESTS + 'external/wpt/foo/x.html':
            FileStatus(FileStatusType.MODIFY),
        }
        self.assertFalse(importer._need_sheriff_attention())

        importer.project_git.changed_files = lambda: {
            RELATIVE_WEB_TESTS + 'external/' + BASE_MANIFEST_NAME:
            FileStatus(FileStatusType.MODIFY),
            RELATIVE_WEB_TESTS + 'external/wpt/foo/x.html':
            FileStatus(FileStatusType.MODIFY),
            RELATIVE_WEB_TESTS + 'external/wpt/foo/y.sh':
            FileStatus(FileStatusType.MODIFY),
        }
        self.assertTrue(importer._need_sheriff_attention())

        importer.project_git.changed_files = lambda: {
            RELATIVE_WEB_TESTS + 'external/' + BASE_MANIFEST_NAME:
            FileStatus(FileStatusType.MODIFY),
            RELATIVE_WEB_TESTS + 'external/wpt/foo/x.html':
            FileStatus(FileStatusType.MODIFY),
            RELATIVE_WEB_TESTS + 'external/wpt/foo/y.py':
            FileStatus(FileStatusType.MODIFY),
        }
        self.assertTrue(importer._need_sheriff_attention())

        importer.project_git.changed_files = lambda: {
            RELATIVE_WEB_TESTS + 'external/' + BASE_MANIFEST_NAME:
            FileStatus(FileStatusType.MODIFY),
            RELATIVE_WEB_TESTS + 'external/wpt/foo/x.html':
            FileStatus(FileStatusType.MODIFY),
            RELATIVE_WEB_TESTS + 'external/wpt/foo/y.bat':
            FileStatus(FileStatusType.MODIFY),
        }
        self.assertTrue(importer._need_sheriff_attention())

    # TODO(crbug.com/800570): Fix orphan baseline finding in the presence of
    # variant tests.
    @unittest.skip('Finding orphaned baselines is broken')
    def test_delete_orphaned_baselines_basic(self):
        host = self.mock_host()
        importer = self._get_test_importer(host)
        dest_path = importer.dest_path
        host.filesystem.write_text_file(
            dest_path + '/MANIFEST.json',
            json.dumps({
                'items': {
                    'testharness': {
                        'a.html': ['abcdef123', [None, {}]],
                    },
                    'manual': {},
                    'reftest': {},
                },
            }))
        host.filesystem.write_text_file(dest_path + '/a.html', '')
        host.filesystem.write_text_file(dest_path + '/a-expected.txt', '')
        host.filesystem.write_text_file(dest_path + '/orphaned-expected.txt',
                                        '')
        importer._delete_orphaned_baselines()
        self.assertFalse(
            host.filesystem.exists(dest_path + '/orphaned-expected.txt'))
        self.assertTrue(host.filesystem.exists(dest_path + '/a-expected.txt'))

    # TODO(crbug.com/800570): Fix orphan baseline finding in the presence of
    # variant tests.
    @unittest.skip('Finding orphaned baselines is broken')
    def test_delete_orphaned_baselines_worker_js_tests(self):
        # This test checks that baselines for existing tests shouldn't be
        # deleted, even if the test name isn't the same as the file name.
        host = self.mock_host()
        importer = self._get_test_importer(host)
        dest_path = importer.dest_path
        host.filesystem.write_text_file(
            dest_path + '/MANIFEST.json',
            json.dumps({
                'items': {
                    'testharness': {
                        'a.any.js': [
                            'abcdef123',
                            ['a.any.html', {}],
                            ['a.any.worker.html', {}],
                        ],
                        'b.worker.js': ['abcdef123', ['b.worker.html', {}]],
                        'c.html': [
                            'abcdef123',
                            ['c.html?q=1', {}],
                            ['c.html?q=2', {}],
                        ],
                    },
                    'manual': {},
                    'reftest': {},
                },
            }))
        host.filesystem.write_text_file(dest_path + '/a.any.js', '')
        host.filesystem.write_text_file(dest_path + '/a.any-expected.txt', '')
        host.filesystem.write_text_file(
            dest_path + '/a.any.worker-expected.txt', '')
        host.filesystem.write_text_file(dest_path + '/b.worker.js', '')
        host.filesystem.write_text_file(dest_path + '/b.worker-expected.txt',
                                        '')
        host.filesystem.write_text_file(dest_path + '/c.html', '')
        host.filesystem.write_text_file(dest_path + '/c-expected.txt', '')
        importer._delete_orphaned_baselines()
        self.assertTrue(
            host.filesystem.exists(dest_path + '/a.any-expected.txt'))
        self.assertTrue(
            host.filesystem.exists(dest_path + '/a.any.worker-expected.txt'))
        self.assertTrue(
            host.filesystem.exists(dest_path + '/b.worker-expected.txt'))
        self.assertTrue(host.filesystem.exists(dest_path + '/c-expected.txt'))

    def test_clear_out_dest_path(self):
        host = self.mock_host()
        importer = self._get_test_importer(host)
        dest_path = importer.dest_path
        host.filesystem.write_text_file(dest_path + '/foo-test.html', '')
        host.filesystem.write_text_file(dest_path + '/foo-test-expected.txt',
                                        '')
        host.filesystem.write_text_file(dest_path + '/OWNERS', '')
        host.filesystem.write_text_file(dest_path + '/DIR_METADATA', '')
        host.filesystem.write_text_file(dest_path + '/bar/baz/OWNERS', '')
        # When the destination path is cleared, OWNERS files and baselines
        # are kept.
        importer._clear_out_dest_path()
        self.assertFalse(host.filesystem.exists(dest_path + '/foo-test.html'))
        self.assertTrue(
            host.filesystem.exists(dest_path + '/foo-test-expected.txt'))
        self.assertTrue(host.filesystem.exists(dest_path + '/OWNERS'))
        self.assertTrue(host.filesystem.exists(dest_path + '/DIR_METADATA'))
        self.assertTrue(host.filesystem.exists(dest_path + '/bar/baz/OWNERS'))
