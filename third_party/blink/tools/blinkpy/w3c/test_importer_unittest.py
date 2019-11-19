# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import datetime
import json

from blinkpy.common.checkout.git_mock import MockGit
from blinkpy.common.host_mock import MockHost
from blinkpy.common.net.git_cl import CLStatus
from blinkpy.common.net.git_cl import TryJobStatus
from blinkpy.common.net.git_cl_mock import MockGitCL
from blinkpy.common.net.network_transaction import NetworkTimeout
from blinkpy.common.net.results_fetcher import Build
from blinkpy.common.path_finder import RELATIVE_WEB_TESTS
from blinkpy.common.system.executive_mock import MockCall
from blinkpy.common.system.executive_mock import MockExecutive
from blinkpy.common.system.log_testing import LoggingTestCase
from blinkpy.w3c.chromium_commit_mock import MockChromiumCommit
from blinkpy.w3c.local_wpt import LocalWPT
from blinkpy.w3c.local_wpt_mock import MockLocalWPT
from blinkpy.w3c.test_importer import TestImporter, ROTATIONS_URL, TBR_FALLBACK
from blinkpy.w3c.wpt_github_mock import MockWPTGitHub
from blinkpy.w3c.wpt_manifest import BASE_MANIFEST_NAME
from blinkpy.web_tests.builder_list import BuilderList


MOCK_WEB_TESTS = '/mock-checkout/' + RELATIVE_WEB_TESTS

class TestImporterTest(LoggingTestCase):

    def test_update_expectations_for_cl_no_results(self):
        host = MockHost()
        host.filesystem.write_text_file(MOCK_WEB_TESTS + 'W3CImportExpectations', '')
        importer = TestImporter(host)
        importer.git_cl = MockGitCL(host, time_out=True)
        success = importer.update_expectations_for_cl()
        self.assertFalse(success)
        self.assertLog([
            'INFO: Triggering try jobs for updating expectations.\n',
            'ERROR: No initial try job results, aborting.\n',
        ])
        self.assertEqual(importer.git_cl.calls[-1], ['git', 'cl', 'set-close'])

    def test_update_expectations_for_cl_closed_cl(self):
        host = MockHost()
        host.filesystem.write_text_file(MOCK_WEB_TESTS + 'W3CImportExpectations', '')
        importer = TestImporter(host)
        importer.git_cl = MockGitCL(host, status='closed', try_job_results={
            Build('builder-a', 123): TryJobStatus('COMPLETED', 'SUCCESS'),
        })
        success = importer.update_expectations_for_cl()
        self.assertFalse(success)
        self.assertLog([
            'INFO: Triggering try jobs for updating expectations.\n',
            'ERROR: The CL was closed, aborting.\n',
        ])

    def test_update_expectations_for_cl_all_jobs_pass(self):
        host = MockHost()
        host.filesystem.write_text_file(MOCK_WEB_TESTS + 'W3CImportExpectations', '')
        importer = TestImporter(host)
        importer.git_cl = MockGitCL(host, status='lgtm', try_job_results={
            Build('builder-a', 123): TryJobStatus('COMPLETED', 'SUCCESS'),
        })
        success = importer.update_expectations_for_cl()
        self.assertLog([
            'INFO: Triggering try jobs for updating expectations.\n',
            'INFO: All jobs finished.\n',
        ])
        self.assertTrue(success)

    def test_update_expectations_for_cl_fail_but_no_changes(self):
        host = MockHost()
        host.filesystem.write_text_file(MOCK_WEB_TESTS + 'W3CImportExpectations', '')
        importer = TestImporter(host)
        importer.git_cl = MockGitCL(host, status='lgtm', try_job_results={
            Build('builder-a', 123): TryJobStatus('COMPLETED', 'FAILURE'),
        })
        importer.fetch_new_expectations_and_baselines = lambda: None
        success = importer.update_expectations_for_cl()
        self.assertTrue(success)
        self.assertLog([
            'INFO: Triggering try jobs for updating expectations.\n',
            'INFO: All jobs finished.\n',
        ])

    def test_run_commit_queue_for_cl_pass(self):
        host = MockHost()
        host.filesystem.write_text_file(MOCK_WEB_TESTS + 'W3CImportExpectations', '')
        importer = TestImporter(host)
        # Only the latest job for each builder is counted.
        importer.git_cl = MockGitCL(host, status='lgtm', try_job_results={
            Build('cq-builder-a', 120): TryJobStatus('COMPLETED', 'FAILURE'),
            Build('cq-builder-a', 123): TryJobStatus('COMPLETED', 'SUCCESS'),
        })
        success = importer.run_commit_queue_for_cl()
        self.assertTrue(success)
        self.assertLog([
            'INFO: Triggering CQ try jobs.\n',
            'INFO: All jobs finished.\n',
            'INFO: CQ appears to have passed; trying to commit.\n',
            'INFO: Update completed.\n',
        ])
        self.assertEqual(importer.git_cl.calls, [
            ['git', 'cl', 'try'],
            ['git', 'cl', 'upload', '-f', '--send-mail'],
            ['git', 'cl', 'set-commit'],
        ])

    def test_run_commit_queue_for_cl_fail_cq(self):
        host = MockHost()
        host.filesystem.write_text_file(MOCK_WEB_TESTS + 'W3CImportExpectations', '')
        importer = TestImporter(host)
        importer.git_cl = MockGitCL(host, status='lgtm', try_job_results={
            Build('cq-builder-a', 120): TryJobStatus('COMPLETED', 'SUCCESS'),
            Build('cq-builder-a', 123): TryJobStatus('COMPLETED', 'FAILURE'),
            Build('cq-builder-b', 200): TryJobStatus('COMPLETED', 'SUCCESS'),
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
            ['git', 'cl', 'set-close'],
        ])

    def test_run_commit_queue_for_cl_fail_to_land(self):
        host = MockHost()
        host.filesystem.write_text_file(MOCK_WEB_TESTS + 'W3CImportExpectations', '')
        importer = TestImporter(host)
        # Only the latest job for each builder is counted.
        importer.git_cl = MockGitCL(host, status='lgtm', try_job_results={
            Build('cq-builder-a', 120): TryJobStatus('COMPLETED', 'FAILURE'),
            Build('cq-builder-a', 123): TryJobStatus('COMPLETED', 'SUCCESS'),
        })
        importer.git_cl.wait_for_closed_status = lambda: False
        success = importer.run_commit_queue_for_cl()
        self.assertFalse(success)
        self.assertLog([
            'INFO: Triggering CQ try jobs.\n',
            'INFO: All jobs finished.\n',
            'INFO: CQ appears to have passed; trying to commit.\n',
            'ERROR: Cannot submit CL; aborting.\n',
        ])
        self.assertEqual(importer.git_cl.calls, [
            ['git', 'cl', 'try'],
            ['git', 'cl', 'upload', '-f', '--send-mail'],
            ['git', 'cl', 'set-commit'],
            ['git', 'cl', 'set-close'],
        ])

    def test_run_commit_queue_for_cl_closed_cl(self):
        host = MockHost()
        host.filesystem.write_text_file(MOCK_WEB_TESTS + 'W3CImportExpectations', '')
        importer = TestImporter(host)
        importer.git_cl = MockGitCL(host, status='closed', try_job_results={
            Build('cq-builder-a', 120): TryJobStatus('COMPLETED', 'SUCCESS'),
            Build('cq-builder-b', 200): TryJobStatus('COMPLETED', 'SUCCESS'),
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
        host = MockHost()
        importer = TestImporter(host)
        importer.git_cl = MockGitCL(host, time_out=True)
        success = importer.run_commit_queue_for_cl()
        self.assertFalse(success)
        self.assertLog([
            'INFO: Triggering CQ try jobs.\n',
            'ERROR: Timed out waiting for CQ; aborting.\n'
        ])
        self.assertEqual(
            importer.git_cl.calls,
            [['git', 'cl', 'try'], ['git', 'cl', 'set-close']])

    def test_submit_cl_timeout_and_already_merged(self):
        # Here we simulate a case where we timeout waiting for the CQ to submit a
        # CL because we miss the notification that it was merged. We then get an
        # error when trying to close the CL because it's already been merged.
        host = MockHost()
        host.filesystem.write_text_file(MOCK_WEB_TESTS + 'W3CImportExpectations', '')
        importer = TestImporter(host)
        # Define some error text that looks like a typical ScriptError.
        git_error_text = (
            'This is a git Script Error\n'
            '...there is usually a stack trace here with some calls\n'
            '...and maybe other calls\n'
            'And finally, there is the exception:\n'
            'GerritError: Conflict: change is merged\n'
        )
        importer.git_cl = MockGitCL(
            host, status='lgtm', git_error_output={'set-close': git_error_text},
            # Only the latest job for each builder is counted.
            try_job_results={
                Build('cq-builder-a', 120): TryJobStatus('COMPLETED', 'FAILURE'),
                Build('cq-builder-a', 123): TryJobStatus('COMPLETED', 'SUCCESS')})
        importer.git_cl.wait_for_closed_status = lambda: False
        success = importer.run_commit_queue_for_cl()
        # Since the CL is already merged, we absorb the error and treat it as success.
        self.assertTrue(success)
        self.assertLog([
            'INFO: Triggering CQ try jobs.\n',
            'INFO: All jobs finished.\n',
            'INFO: CQ appears to have passed; trying to commit.\n',
            'ERROR: Cannot submit CL; aborting.\n',
            'ERROR: CL is already merged; treating as success.\n',
        ])
        self.assertEqual(importer.git_cl.calls, [
            ['git', 'cl', 'try'],
            ['git', 'cl', 'upload', '-f', '--send-mail'],
            ['git', 'cl', 'set-commit'],
            ['git', 'cl', 'set-close'],
        ])

    def test_apply_exportable_commits_locally(self):
        # TODO(robertma): Consider using MockLocalWPT.
        host = MockHost()
        importer = TestImporter(host, wpt_github=MockWPTGitHub(pull_requests=[]))
        importer.wpt_git = MockGit(cwd='/tmp/wpt', executive=host.executive)
        fake_commit = MockChromiumCommit(
            host, subject='My fake commit',
            patch=(
                'Fake patch contents...\n'
                '--- a/' + RELATIVE_WEB_TESTS + 'external/wpt/css/css-ui-3/outline-004.html\n'
                '+++ b/' + RELATIVE_WEB_TESTS + 'external/wpt/css/css-ui-3/outline-004.html\n'
                '@@ -20,7 +20,7 @@\n'
                '...'))
        importer.exportable_but_not_exported_commits = lambda _: [fake_commit]
        applied = importer.apply_exportable_commits_locally(LocalWPT(host))
        self.assertEqual(applied, [fake_commit])
        # This assertion is implementation details of LocalWPT.apply_patch.
        # TODO(robertma): Move this to local_wpt_unittest.py.
        self.assertEqual(host.executive.full_calls, [
            MockCall(
                ['git', 'apply', '-'],
                {
                    'input': (
                        'Fake patch contents...\n'
                        '--- a/css/css-ui-3/outline-004.html\n'
                        '+++ b/css/css-ui-3/outline-004.html\n'
                        '@@ -20,7 +20,7 @@\n'
                        '...'),
                    'cwd': '/tmp/wpt',
                    'env': None
                }),
            MockCall(
                ['git', 'add', '.'],
                kwargs={'input': None, 'cwd': '/tmp/wpt', 'env': None})
        ])
        self.assertEqual(importer.wpt_git.local_commits(),
                         [['Applying patch 14fd77e88e42147c57935c49d9e3b2412b8491b7']])

    def test_apply_exportable_commits_locally_returns_none_on_failure(self):
        host = MockHost()
        wpt_github = MockWPTGitHub(pull_requests=[])
        importer = TestImporter(host, wpt_github=wpt_github)
        commit = MockChromiumCommit(host, subject='My fake commit')
        importer.exportable_but_not_exported_commits = lambda _: [commit]
        local_wpt = MockLocalWPT(apply_patch=['Failed'])    # Failure to apply patch.
        applied = importer.apply_exportable_commits_locally(local_wpt)
        self.assertIsNone(applied)

    def test_update_all_test_expectations_files(self):
        host = MockHost()
        host.filesystem.files[MOCK_WEB_TESTS + 'TestExpectations'] = (
            'Bug(test) some/test/a.html [ Failure ]\n'
            'Bug(test) some/test/b.html [ Failure ]\n'
            'Bug(test) some/test/c.html [ Failure ]\n')
        host.filesystem.files[MOCK_WEB_TESTS + 'WebDriverExpectations'] = (
            'Bug(test) external/wpt/webdriver/some/test/a.html>>foo [ Failure ]\n'
            'Bug(test) external/wpt/webdriver/some/test/a.html>>bar [ Failure ]\n'
            'Bug(test) external/wpt/webdriver/some/test/b.html>>foo [ Failure ]\n'
            'Bug(test) external/wpt/webdriver/some/test/c.html>>a [ Failure ]\n')
        host.filesystem.files[MOCK_WEB_TESTS + 'VirtualTestSuites'] = '[]'
        host.filesystem.files[MOCK_WEB_TESTS + 'new/a.html'] = ''
        host.filesystem.files[MOCK_WEB_TESTS + 'new/b.html'] = ''
        importer = TestImporter(host)
        deleted_tests = ['some/test/b.html', 'external/wpt/webdriver/some/test/b.html']
        renamed_test_pairs = {
            'some/test/a.html': 'new/a.html',
            'some/test/c.html': 'new/c.html',
            'external/wpt/webdriver/some/test/a.html': 'old/a.html',
            'external/wpt/webdriver/some/test/c.html': 'old/c.html',
        }
        importer.update_all_test_expectations_files(deleted_tests, renamed_test_pairs)
        self.assertMultiLineEqual(
            host.filesystem.read_text_file(MOCK_WEB_TESTS + 'TestExpectations'),
            ('Bug(test) new/a.html [ Failure ]\n'
             'Bug(test) new/c.html [ Failure ]\n'))
        self.assertMultiLineEqual(
            host.filesystem.read_text_file(MOCK_WEB_TESTS + 'WebDriverExpectations'),
            ('Bug(test) old/a.html>>foo [ Failure ]\n'
             'Bug(test) old/a.html>>bar [ Failure ]\n'
             'Bug(test) old/c.html>>a [ Failure ]\n'))

    def test_get_directory_owners(self):
        host = MockHost()
        host.filesystem.write_text_file(MOCK_WEB_TESTS + 'W3CImportExpectations', '')
        host.filesystem.write_text_file(MOCK_WEB_TESTS + 'external/wpt/foo/OWNERS',
                                        'someone@chromium.org\n')
        importer = TestImporter(host)
        importer.chromium_git.changed_files = lambda: [RELATIVE_WEB_TESTS + 'external/wpt/foo/x.html']
        self.assertEqual(importer.get_directory_owners(), {('someone@chromium.org',): ['external/wpt/foo']})

    def test_get_directory_owners_no_changed_files(self):
        host = MockHost()
        host.filesystem.write_text_file(MOCK_WEB_TESTS + 'W3CImportExpectations', '')
        host.filesystem.write_text_file(MOCK_WEB_TESTS + 'external/wpt/foo/OWNERS',
                                        'someone@chromium.org\n')
        importer = TestImporter(host)
        self.assertEqual(importer.get_directory_owners(), {})

    # Tests for protected methods - pylint: disable=protected-access

    def test_commit_changes(self):
        host = MockHost()
        importer = TestImporter(host)
        importer._commit_changes('dummy message')
        self.assertEqual(importer.chromium_git.local_commits(), [['dummy message']])

    def test_commit_message(self):
        importer = TestImporter(MockHost())
        self.assertEqual(
            importer._commit_message('aaaa', '1111'),
            'Import 1111\n\n'
            'Using wpt-import in Chromium aaaa.\n\n'
            'No-Export: true')

    def test_cl_description_with_empty_environ(self):
        host = MockHost()
        host.executive = MockExecutive(output='Last commit message\n\n')
        importer = TestImporter(host)
        description = importer._cl_description(directory_owners={})
        self.assertEqual(
            description,
            'Last commit message\n\n'
            'Note to sheriffs: This CL imports external tests and adds\n'
            'expectations for those tests; if this CL is large and causes\n'
            'a few new failures, please fix the failures by adding new\n'
            'lines to TestExpectations rather than reverting. See:\n'
            'https://chromium.googlesource.com'
            '/chromium/src/+/master/docs/testing/web_platform_tests.md\n\n'
            'NOAUTOREVERT=true\n'
            'No-Export: true')
        self.assertEqual(host.executive.calls, [['git', 'log', '-1', '--format=%B']])

    def test_cl_description_moves_noexport_tag(self):
        host = MockHost()
        host.executive = MockExecutive(output='Summary\n\nNo-Export: true\n\n')
        importer = TestImporter(host)
        description = importer._cl_description(directory_owners={})
        self.assertIn(
            'No-Export: true',
            description)

    def test_cl_description_with_directory_owners(self):
        host = MockHost()
        host.executive = MockExecutive(output='Last commit message\n\n')
        importer = TestImporter(host)
        description = importer._cl_description(directory_owners={
            ('someone@chromium.org',): ['external/wpt/foo', 'external/wpt/bar'],
            ('x@chromium.org', 'y@chromium.org'): ['external/wpt/baz'],
        })
        self.assertIn(
            'Directory owners for changes in this CL:\n'
            'someone@chromium.org:\n'
            '  external/wpt/foo\n'
            '  external/wpt/bar\n'
            'x@chromium.org, y@chromium.org:\n'
            '  external/wpt/baz\n\n',
            description)

    def test_tbr_reviewer_no_response_uses_backup(self):
        host = MockHost()
        importer = TestImporter(host)
        self.assertEqual(TBR_FALLBACK, importer.tbr_reviewer())
        self.assertLog([
            'ERROR: Exception while fetching current sheriff: '
            'No JSON object could be decoded\n'
        ])

    def test_tbr_reviewer_date_not_found(self):
        host = MockHost()
        yesterday = (datetime.date.fromtimestamp(host.time()) -
                     datetime.timedelta(days=1)).isoformat()
        host.web.urls[ROTATIONS_URL] = json.dumps({
            'calendar': [
                {
                    'date': yesterday,
                    'participants': [['some-sheriff'], ['other-sheriff']],
                },
            ],
            'rotations': ['ecosystem_infra', 'other_rotation']
        })
        importer = TestImporter(host)
        self.assertEqual(TBR_FALLBACK, importer.tbr_reviewer())
        # Use a variable here, otherwise we get different values depending on
        # the machine's time zone settings (e.g. "1969-12-31" vs "1970-01-01").
        today = datetime.date.fromtimestamp(host.time()).isoformat()
        self.assertLog([
            'ERROR: No entry found for date %s in rotations table.\n' % today
        ])

    def test_tbr_reviewer_nobody_on_rotation(self):
        host = MockHost()
        today = datetime.date.fromtimestamp(host.time()).isoformat()
        host.web.urls[ROTATIONS_URL] = json.dumps({
            'calendar': [
                {
                    'date': today,
                    'participants': [[], ['some-sheriff']],
                },
            ],
            'rotations': ['ecosystem_infra', 'other-rotation']
        })
        importer = TestImporter(host)
        self.assertEqual(TBR_FALLBACK, importer.tbr_reviewer())
        self.assertLog([
            'INFO: No sheriff today.\n'
        ])

    def test_tbr_reviewer_rotations_url_unavailable(self):
        def raise_exception(*_):
            raise NetworkTimeout

        host = MockHost()
        host.web.get_binary = raise_exception
        importer = TestImporter(host)
        self.assertEqual(TBR_FALLBACK, importer.tbr_reviewer())
        self.assertLog([
            'ERROR: Cannot fetch %s\n' % ROTATIONS_URL
        ])

    def test_tbr_reviewer(self):
        host = MockHost()
        today = datetime.date.fromtimestamp(host.time())
        yesterday = today - datetime.timedelta(days=1)
        host.web.urls[ROTATIONS_URL] = json.dumps({
            'calendar': [
                {
                    'date': yesterday.isoformat(),
                    'participants': [['other-sheriff'], ['last-sheriff']],
                },
                {
                    'date': today.isoformat(),
                    'participants': [['other-sheriff'], ['current-sheriff']],
                },
            ],
            'rotations': ['other-rotation', 'ecosystem_infra']
        })
        importer = TestImporter(host)
        self.assertEqual('current-sheriff', importer.tbr_reviewer())
        self.assertLog([])

    def test_tbr_reviewer_with_full_email_address(self):
        host = MockHost()
        today = datetime.date.fromtimestamp(host.time()).isoformat()
        host.web.urls[ROTATIONS_URL] = json.dumps({
            'calendar': [
                {
                    'date': today,
                    'participants': [['external@example.com']],
                },
            ],
            'rotations': ['ecosystem_infra']
        })
        importer = TestImporter(host)
        self.assertEqual('external@example.com', importer.tbr_reviewer())
        self.assertLog([])

    def test_tbr_reviewer_skips_non_committer(self):
        host = MockHost()
        importer = TestImporter(host)
        importer._fetch_ecosystem_infra_sheriff_username = lambda: 'kyleju'
        self.assertEqual(TBR_FALLBACK, importer.tbr_reviewer())
        self.assertLog(['WARNING: Cannot TBR by kyleju: not a committer\n'])

    def test_generate_manifest_successful_run(self):
        # This test doesn't test any aspect of the real manifest script, it just
        # asserts that TestImporter._generate_manifest would invoke the script.
        host = MockHost()
        importer = TestImporter(host)
        host.filesystem.write_text_file(MOCK_WEB_TESTS + 'external/wpt/MANIFEST.json', '{}')
        importer._generate_manifest()
        self.assertEqual(
            host.executive.calls,
            [
                [
                    'python',
                    '/mock-checkout/third_party/blink/tools/blinkpy/third_party/wpt/wpt/wpt',
                    'manifest',
                    '--no-download',
                    '--tests-root',
                    MOCK_WEB_TESTS + 'external/wpt',
                ]
            ])
        self.assertEqual(importer.chromium_git.added_paths,
                         {MOCK_WEB_TESTS + 'external/' + BASE_MANIFEST_NAME})

    def test_only_wpt_manifest_changed(self):
        host = MockHost()
        importer = TestImporter(host)
        importer.chromium_git.changed_files = lambda: [
            RELATIVE_WEB_TESTS + 'external/' + BASE_MANIFEST_NAME,
            RELATIVE_WEB_TESTS + 'external/wpt/foo/x.html']
        self.assertFalse(importer._only_wpt_manifest_changed())

        importer.chromium_git.changed_files = lambda: [
            RELATIVE_WEB_TESTS + 'external/' + BASE_MANIFEST_NAME]
        self.assertTrue(importer._only_wpt_manifest_changed())

    def test_delete_orphaned_baselines_basic(self):
        host = MockHost()
        importer = TestImporter(host)
        dest_path = importer.dest_path
        host.filesystem.write_text_file(
            dest_path + '/MANIFEST.json',
            json.dumps({
                'items': {
                    'testharness': {
                        'a.html': [['/a.html', {}]],
                    },
                    'manual': {},
                    'reftest': {},
                },
            }))
        host.filesystem.write_text_file(dest_path + '/a.html', '')
        host.filesystem.write_text_file(dest_path + '/a-expected.txt', '')
        host.filesystem.write_text_file(dest_path + '/orphaned-expected.txt', '')
        importer._delete_orphaned_baselines()
        self.assertFalse(host.filesystem.exists(dest_path + '/orphaned-expected.txt'))
        self.assertTrue(host.filesystem.exists(dest_path + '/a-expected.txt'))

    def test_delete_orphaned_baselines_worker_js_tests(self):
        # This test checks that baselines for existing tests shouldn't be
        # deleted, even if the test name isn't the same as the file name.
        host = MockHost()
        importer = TestImporter(host)
        dest_path = importer.dest_path
        host.filesystem.write_text_file(
            dest_path + '/MANIFEST.json',
            json.dumps({
                'items': {
                    'testharness': {
                        'a.any.js': [
                            ['/a.any.html', {}],
                            ['/a.any.worker.html', {}],
                        ],
                        'b.worker.js': [['/b.worker.html', {}]],
                        'c.html': [
                            ['/c.html?q=1', {}],
                            ['/c.html?q=2', {}],
                        ],
                    },
                    'manual': {},
                    'reftest': {},
                },
            }))
        host.filesystem.write_text_file(dest_path + '/a.any.js', '')
        host.filesystem.write_text_file(dest_path + '/a.any-expected.txt', '')
        host.filesystem.write_text_file(dest_path + '/a.any.worker-expected.txt', '')
        host.filesystem.write_text_file(dest_path + '/b.worker.js', '')
        host.filesystem.write_text_file(dest_path + '/b.worker-expected.txt', '')
        host.filesystem.write_text_file(dest_path + '/c.html', '')
        host.filesystem.write_text_file(dest_path + '/c-expected.txt', '')
        importer._delete_orphaned_baselines()
        self.assertTrue(host.filesystem.exists(dest_path + '/a.any-expected.txt'))
        self.assertTrue(host.filesystem.exists(dest_path + '/a.any.worker-expected.txt'))
        self.assertTrue(host.filesystem.exists(dest_path + '/b.worker-expected.txt'))
        self.assertTrue(host.filesystem.exists(dest_path + '/c-expected.txt'))

    def test_clear_out_dest_path(self):
        host = MockHost()
        importer = TestImporter(host)
        dest_path = importer.dest_path
        host.filesystem.write_text_file(dest_path + '/foo-test.html', '')
        host.filesystem.write_text_file(dest_path + '/foo-test-expected.txt', '')
        host.filesystem.write_text_file(dest_path + '/OWNERS', '')
        host.filesystem.write_text_file(dest_path + '/bar/baz/OWNERS', '')
        # When the destination path is cleared, OWNERS files and baselines
        # are kept.
        importer._clear_out_dest_path()
        self.assertFalse(host.filesystem.exists(dest_path + '/foo-test.html'))
        self.assertTrue(host.filesystem.exists(dest_path + '/foo-test-expected.txt'))
        self.assertTrue(host.filesystem.exists(dest_path + '/OWNERS'))
        self.assertTrue(host.filesystem.exists(dest_path + '/bar/baz/OWNERS'))
