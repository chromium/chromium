# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import contextlib
import io
import json
import textwrap
import unittest
from unittest.mock import patch

from blinkpy.common import path_finder
from blinkpy.common.net.git_cl import TryJobStatus
from blinkpy.common.net.git_cl_mock import MockGitCL
from blinkpy.common.net.rpc import Build, RPCError
from blinkpy.common.system.log_testing import LoggingTestCase
from blinkpy.tool.mock_tool import MockBlinkTool
from blinkpy.tool.commands.update_metadata import (
    UpdateMetadata,
    MetadataUpdater,
    load_and_update_manifests,
)
from blinkpy.web_tests.builder_list import BuilderList

path_finder.bootstrap_wpt_imports()
from manifest.manifest import Manifest


@patch('concurrent.futures.ThreadPoolExecutor.map', new=map)
class BaseUpdateMetadataTest(LoggingTestCase):
    def setUp(self):
        super().setUp()
        self.maxDiff = None

        self.tool = MockBlinkTool()
        self.finder = path_finder.PathFinder(self.tool.filesystem)
        self.tool.filesystem.write_text_file(
            self.finder.path_from_web_tests('external', 'wpt',
                                            'MANIFEST.json'),
            json.dumps({
                'version': 8,
                'items': {
                    'reftest': {
                        'fail.html': [
                            'c3f2fb6f436da59d43aeda0a7e8a018084557033',
                            [None, [['fail-ref.html', '==']], {}],
                        ],
                    },
                    'testharness': {
                        'pass.html': [
                            'd933fd981d4a33ba82fb2b000234859bdda1494e',
                            [None, {}],
                        ],
                        'crash.html': [
                            'd933fd981d4a33ba82fb2b000234859bdda1494e',
                            [None, {}],
                        ],
                        'variant.html': [
                            'b8db5972284d1ac6bbda0da81621d9bca5d04ee7',
                            ['variant.html?foo=bar/abc', {}],
                            ['variant.html?foo=baz', {}],
                        ],
                    },
                },
            }))
        self.tool.filesystem.write_text_file(
            self.finder.path_from_web_tests('wpt_internal', 'MANIFEST.json'),
            json.dumps({
                'version': 8,
                'url_base': '/wpt_internal/',
                'items': {
                    'testharness': {
                        'dir': {
                            'multiglob.https.any.js': [
                                'd6498c3e388e0c637830fa080cca78b0ab0e5305',
                                ['dir/multiglob.https.any.window.html', {}],
                                ['dir/multiglob.https.any.worker.html', {}],
                            ],
                        },
                    },
                },
            }))

    def _manifest_load_and_update(self,
                                  tests_root,
                                  manifest_path,
                                  url_base,
                                  types=None,
                                  **_):
        with self.tool.filesystem.open_text_file_for_reading(
                manifest_path) as manifest_file:
            manifest = json.load(manifest_file)
        return Manifest.from_json(tests_root,
                                  manifest,
                                  types=types,
                                  callee_owns_obj=True)

    @contextlib.contextmanager
    def _patch_builtins(self):
        with contextlib.ExitStack() as stack:
            stack.enter_context(self.tool.filesystem.patch_builtins())
            stack.enter_context(self.tool.executive.patch_builtins())
            stack.enter_context(
                patch('manifest.manifest.load_and_update',
                      self._manifest_load_and_update))
            yield stack


class UpdateMetadataExecuteTest(BaseUpdateMetadataTest):
    """Verify the tool's frontend and build infrastructure interactions."""

    def setUp(self):
        super().setUp()
        self.tool.builders = BuilderList({
            'test-linux-wpt-rel': {
                'port_name': 'test-linux-trusty',
                'specifiers': ['Trusty', 'Release'],
                'is_try_builder': True,
            },
            'test-mac-wpt-rel': {
                'port_name': 'test-mac-mac12',
                'specifiers': ['Mac12', 'Release'],
                'is_try_builder': True,
            },
            'Test Linux Tests (wpt)': {
                'port_name': 'test-linux-trusty',
                'specifiers': ['Trusty', 'Release'],
            },
        })
        self.builds = {
            Build('test-linux-wpt-rel', 1000, '1000'):
            TryJobStatus.from_bb_status('FAILURE'),
            Build('test-mac-wpt-rel', 2000, '2000'):
            TryJobStatus.from_bb_status('SUCCESS'),
        }
        self.git_cl = MockGitCL(self.tool, self.builds)
        self.command = UpdateMetadata(self.tool, self.git_cl)

        self.tool.web.append_prpc_response({
            'artifacts': [{
                'artifactId':
                'wpt_reports_chrome_01.json',
                'fetchUrl':
                'https://cr.dev/123/wptreport.json?token=abc',
            }],
        })
        url = 'https://cr.dev/123/wptreport.json?token=abc'
        self.tool.web.urls[url] = json.dumps({
            'run_info': {
                'os': 'mac',
                'version': '12',
                'processor': 'arm',
                'bits': 64,
                'product': 'chrome',
            },
            'results': [{
                'test':
                '/crash.html',
                'subtests': [{
                    'name': 'this assertion crashes',
                    'status': 'CRASH',
                    'message': None,
                    'expected': 'PASS',
                    'known_intermittent': [],
                }],
                'expected':
                'OK',
                'status':
                'CRASH',
            }],
        }).encode()

    def _unstaged_changes(self):
        wpt_glob = self.finder.path_from_web_tests('external', 'wpt', '*.ini')
        return patch.object(
            self.command.git,
            'unstaged_changes',
            side_effect=lambda: self.tool.filesystem.glob(wpt_glob))

    def test_execute_all(self):
        with self._patch_builtins() as stack:
            stack.enter_context(self._unstaged_changes())
            exit_code = self.command.main([])
        self.assertEqual(exit_code, 0)
        # Even tests that pass may require an update if a subtest was added or
        # removed.
        self.assertLog([
            'INFO: All builds finished.\n',
            'INFO: Processing wptrunner report (1/1)\n',
            'INFO: Updating expectations for up to 5 test files.\n',
            "INFO: Updated 'crash.html'\n",
            'INFO: Staged 1 metadata file.\n',
        ])
        self.assertEqual(self.command.git.added_paths, {
            self.finder.path_from_web_tests('external', 'wpt',
                                            'crash.html.ini')
        })

    def test_execute_explicit_include_patterns(self):
        self.tool.filesystem.write_text_file(
            'test-names.txt',
            textwrap.dedent("""\
                # Ignore this line
                wpt_internal/dir
                  # Ignore this line too
                """))
        with self._patch_builtins():
            exit_code = self.command.main([
                '--test-name-file=test-names.txt',
                'variant.html',
                'pass.html',
            ])
        self.assertEqual(exit_code, 0)
        self.assertLog([
            'INFO: All builds finished.\n',
            'INFO: Processing wptrunner report (1/1)\n',
            'INFO: Updating expectations for up to 3 test files.\n',
            'INFO: Staged 0 metadata files.\n',
        ])

    def test_execute_exclude_patterns(self):
        url = 'https://cr.dev/123/wptreport.json?token=abc'
        self.tool.web.urls[url] = json.dumps({
            'run_info': {
                'os': 'mac'
            },
            'results': [{
                'test': '/variant.html?foo=bar/abc',
                'subtests': [],
                'expected': 'PASS',
                'status': 'FAIL',
            }, {
                'test': '/variant.html?foo=baz',
                'subtests': [],
                'expected': 'PASS',
                'status': 'FAIL',
            }],
        }).encode()
        with self._patch_builtins() as stack:
            stack.enter_context(self._unstaged_changes())
            exit_code = self.command.main([
                '--exclude=wpt_internal',
                '--exclude=variant.html?foo=baz',
                '--exclude=crash.html',
            ])
        self.assertEqual(exit_code, 0)
        self.assertLog([
            'INFO: All builds finished.\n',
            'INFO: Processing wptrunner report (1/1)\n',
            'INFO: Updating expectations for up to 3 test files.\n',
            "INFO: Updated 'variant.html'\n",
            'INFO: Staged 1 metadata file.\n',
        ])
        # The other variant is not updated.
        self.assertEqual(
            self.tool.filesystem.read_text_file(
                self.finder.path_from_web_tests('external', 'wpt',
                                                'variant.html.ini')),
            textwrap.dedent("""\
                [variant.html?foo=bar/abc]
                  expected: FAIL
                """))

    def test_execute_with_no_issue_number_aborts(self):
        self.command.git_cl = MockGitCL(self.tool, issue_number='None')
        with self._patch_builtins():
            exit_code = self.command.main([])
        self.assertEqual(exit_code, 1)
        self.assertLog(['ERROR: No issue number for current branch.\n'])

    def test_execute_trigger_jobs(self):
        self.command.git_cl = MockGitCL(
            self.tool, {
                Build('test-linux-wpt-rel', 1000, '1000'):
                TryJobStatus.from_bb_status('STARTED'),
            })
        with self._patch_builtins():
            exit_code = self.command.main([])
        self.assertEqual(exit_code, 1)
        self.assertLog([
            'INFO: No finished builds.\n',
            'INFO: Scheduled or started builds:\n',
            'INFO:   BUILDER              NUMBER  STATUS    BUCKET\n',
            'INFO:   test-linux-wpt-rel   1000    STARTED   try   \n',
            'INFO:   test-mac-wpt-rel     --      TRIGGERED try   \n',
            'ERROR: Once all pending try jobs have finished, '
            'please re-run the tool to fetch new results.\n',
        ])

    def test_execute_with_rpc_error(self):
        error = RPCError('mock error', 'getBuild', {'id': '123'}, 400)
        with self._patch_builtins() as stack:
            stack.enter_context(
                patch.object(self.command.git_cl.bb_client,
                             'execute_batch',
                             side_effect=error))
            exit_code = self.command.main(['--build=Test Linux Tests (wpt)'])
            self.assertEqual(exit_code, 1)
            self.assertLog([
                'ERROR: getBuild: mock error (code: 400)\n',
                'ERROR: Request payload: {\n'
                '  "id": "123"\n'
                '}\n',
            ])

    def test_execute_no_trigger_jobs(self):
        self.command.git_cl = MockGitCL(self.tool, {})
        with self._patch_builtins():
            exit_code = self.command.main(['--no-trigger-jobs'])
        self.assertEqual(exit_code, 1)
        self.assertLog([
            "ERROR: Aborted: no try jobs and '--no-trigger-jobs' or "
            "'--dry-run' passed.\n",
        ])
        with self._patch_builtins():
            exit_code = self.command.main(['--dry-run'])
        self.assertEqual(exit_code, 1)
        self.assertLog([
            "ERROR: Aborted: no try jobs and '--no-trigger-jobs' or "
            "'--dry-run' passed.\n",
        ])
        self.assertEqual(self.command.git_cl.calls, [])

    def test_execute_dry_run(self):
        self.tool.filesystem.write_text_file(
            self.finder.path_from_web_tests('external', 'wpt', 'dir', 'is',
                                            'orphaned.html.ini'),
            '[orphaned.html]\n')
        files_before = dict(self.tool.filesystem.files)
        with self._patch_builtins():
            exit_code = self.command.main(['--dry-run'])
        self.assertEqual(exit_code, 0)
        self.assertLog([
            'INFO: All builds finished.\n',
            'INFO: Processing wptrunner report (1/1)\n',
            'WARNING: Deleting 1 orphaned metadata file:\n',
            'WARNING:   external/wpt/dir/is/orphaned.html.ini\n',
            'INFO: Updating expectations for up to 5 test files.\n',
            "INFO: Updated 'crash.html'\n",
        ])
        self.assertEqual(self.tool.filesystem.files, files_before)
        self.assertEqual(self.tool.executive.calls, [['luci-auth', 'token']])
        self.assertEqual(self.tool.git().added_paths, set())

    def test_execute_only_changed_tests(self):
        with self._patch_builtins() as stack:
            stack.enter_context(
                patch.object(
                    self.command.git,
                    'changed_files',
                    return_value=[
                        'third_party/blink/web_tests/external/wpt/crash.html',
                    ]))
            stack.enter_context(self._unstaged_changes())
            exit_code = self.command.main(['--only-changed-tests'])
        self.assertEqual(exit_code, 0)
        self.assertLog([
            'INFO: All builds finished.\n',
            'INFO: Processing wptrunner report (1/1)\n',
            'INFO: Updating expectations for up to 1 test file.\n',
            "INFO: Updated 'crash.html'\n",
            'INFO: Staged 1 metadata file.\n',
        ])

    def test_execute_only_changed_tests_none(self):
        with self._patch_builtins() as stack:
            stack.enter_context(
                patch.object(self.command.git,
                             'changed_files',
                             return_value=[]))
            exit_code = self.command.main(['--only-changed-tests'])
        self.assertEqual(exit_code, 1)
        self.assertLog([
            'ERROR: No metadata to update.\n',
        ])

    def test_execute_abort_with_uncommitted_change(self):
        uncommitted_changes = [
            'third_party/blink/web_tests/external/wpt/fail.html.ini',
            'third_party/blink/web_tests/external/wpt/pass.html.ini',
            'third_party/blink/web_tests/external/wpt/variant.html.ini',
        ]
        with self._patch_builtins() as stack:
            stack.enter_context(
                patch.object(self.command.git,
                             'uncommitted_changes',
                             return_value=uncommitted_changes))
            exit_code = self.command.main(['fail.html', 'pass.html'])
        self.assertEqual(exit_code, 1)
        self.assertLog([
            'ERROR: Aborting: there are uncommitted metadata files:\n',
            'ERROR:   external/wpt/fail.html.ini\n',
            'ERROR:   external/wpt/pass.html.ini\n',
            'ERROR: Please commit or reset these files to continue.\n',
        ])

    def test_execute_warn_absent_tests(self):
        url = 'https://cr.dev/123/wptreport.json?token=abc'
        self.tool.web.urls[url] = json.dumps({
            'run_info': {},
            'results': [{
                'test': '/new-test-on-tot.html',
                'subtests': [],
                'status': 'PASS',
            }],
        }).encode()
        with self._patch_builtins():
            exit_code = self.command.main(['fail.html'])
        self.assertEqual(exit_code, 0)
        self.assertLog([
            'INFO: All builds finished.\n',
            'INFO: Processing wptrunner report (1/1)\n',
            'WARNING: Some builders have results for tests that are absent '
            'from your local checkout.\n',
            'WARNING: To update metadata for these tests, please rebase-update '
            'on tip-of-tree.\n',
            'INFO: Updating expectations for up to 1 test file.\n',
            'INFO: Staged 0 metadata files.\n',
        ])

    def test_gather_reports(self):
        local_report = {
            'run_info': {
                'os': 'mac',
            },
            'results': [],
        }
        self.tool.filesystem.write_text_file(
            'report1.json', json.dumps(local_report, indent=2))
        # Simulate a retry within a shard.
        self.tool.filesystem.write_text_file(
            'report2.json', '\n'.join([json.dumps(local_report)] * 2))

        reports = list(
            self.command.gather_reports(self.builds,
                                        ['report1.json', 'report2.json']))
        self.assertEqual(len(reports), 3)
        self.assertLog([
            'INFO: Processing wptrunner report (1/3)\n',
            'INFO: Processing wptrunner report (2/3)\n',
            'INFO: Processing wptrunner report (3/3)\n',
        ])

    def test_gather_reports_no_artifacts(self):
        self.tool.web.responses.clear()
        self.tool.web.append_prpc_response({'artifacts': []})
        self.assertEqual(list(self.command.gather_reports(self.builds, [])),
                         [])
        self.assertLog([
            'WARNING: All builds are missing report artifacts.\n',
            'WARNING: No reports to process.\n',
        ])

    def test_remove_orphaned_metadata(self):
        """Verify that the tool removes orphaned metadata files.

        A metadata file is orphaned when its corresponding test no longer exists
        in the manifest.
        """
        self.tool.filesystem.write_text_file(
            self.finder.path_from_web_tests('external', 'wpt', 'dir', 'is',
                                            'orphaned.html.ini'),
            '[orphaned.html]\n')
        self.tool.filesystem.write_text_file(
            self.finder.path_from_web_tests('external', 'wpt',
                                            'infrastructure', 'metadata',
                                            'testdriver.html.ini'),
            '[testdriver.html]\n')
        self.tool.filesystem.write_text_file(
            self.finder.path_from_web_tests('external', 'wpt', 'dir', 'is',
                                            '__dir__.ini'), 'expected: FAIL\n')
        with self._patch_builtins():
            manifests = load_and_update_manifests(self.finder)
            self.command.remove_orphaned_metadata(manifests)
        self.assertFalse(
            self.tool.filesystem.exists(
                self.finder.path_from_web_tests('external', 'wpt', 'dir', 'is',
                                                'orphaned.html.ini')))
        self.assertTrue(
            self.tool.filesystem.exists(
                self.finder.path_from_web_tests('external', 'wpt', 'dir', 'is',
                                                '__dir__.ini')))
        self.assertTrue(
            self.tool.filesystem.exists(
                self.finder.path_from_web_tests('external', 'wpt',
                                                'infrastructure', 'metadata',
                                                'testdriver.html.ini')))


class UpdateMetadataASTSerializationTest(BaseUpdateMetadataTest):
    """Verify the metadata ASTs are manipulated and written correctly.

    The update algorithm is already tested in wptrunner, but is particularly
    complex. This auxiliary test suite ensures Blink-specific scenarios work
    as intended.
    """

    def update(self, *reports, **options):
        result_defaults = {
            'subtests': [],
            'message': None,
            'duration': 1000,
            'expected': 'OK',
            'known_intermittent': [],
        }
        with self._patch_builtins():
            manifests = load_and_update_manifests(self.finder)
            updater = MetadataUpdater.from_manifests(manifests, **options)
            for report in reports:
                report['run_info'] = {
                    'os': 'mac',
                    'version': '12',
                    'processor': 'arm',
                    'bits': 64,
                    'flag_specific': None,
                    'product': 'chrome',
                    'debug': False,
                    **(report.get('run_info') or {}),
                }
                report['results'] = [{
                    **result_defaults,
                    **result
                } for result in report['results']]
                buf = io.StringIO(json.dumps(report))
                updater.collect_results([buf])
            for test_file in updater.test_files_to_update():
                updater.update(test_file)

    def write_contents(self, path_to_metadata, contents):
        path = self.finder.path_from_web_tests(path_to_metadata)
        self.tool.filesystem.write_text_file(path, textwrap.dedent(contents))

    def assert_contents(self, path_to_metadata, contents):
        path = self.finder.path_from_web_tests(path_to_metadata)
        self.assertEqual(self.tool.filesystem.read_text_file(path),
                         textwrap.dedent(contents))

    def exists(self, path_to_metadata):
        path = self.finder.path_from_web_tests(path_to_metadata)
        return self.tool.filesystem.exists(path)

    def test_create_new_expectations(self):
        """The updater creates metadata for new unexpected results."""
        self.assertFalse(self.exists('external/wpt/variant.html.ini'))
        self.update({
            'results': [{
                'test': '/fail.html',
                'status': 'FAIL',
            }, {
                'test': '/fail.html',
                'status': 'OK',
            }],
        })
        # 'OK' is prioritized as the primary expected status.
        self.assert_contents(
            'external/wpt/fail.html.ini', """\
            [fail.html]
              expected: [OK, FAIL]
            """)

    def test_remove_all_pass(self):
        """The updater removes metadata for a test that became all-pass."""
        self.write_contents(
            'external/wpt/variant.html.ini', """\
            [variant.html?foo=baz]
              [formerly failing subtest]
                expected: FAIL
            """)
        self.update({
            'results': [{
                'test':
                '/variant.html?foo=baz',
                'status':
                'OK',
                'subtests': [{
                    'name': 'formerly failing subtest',
                    'status': 'PASS',
                    'message': None,
                    'expected': 'FAIL',
                    'known_intermittent': [],
                }],
            }],
        })
        self.assertFalse(self.exists('external/wpt/variant.html.ini'))

    def test_retain_other_keys(self):
        """The updater retains non-`expected` keys, even for all-pass tests."""
        self.write_contents(
            'external/wpt/variant.html.ini', """\
            [variant.html?foo=baz]
              implementation-status: implementing
              [formerly failing subtest]
                expected: FAIL
            """)
        self.update({
            'results': [{
                'test':
                '/variant.html?foo=baz',
                'status':
                'OK',
                'subtests': [{
                    'name': 'formerly failing subtest',
                    'status': 'PASS',
                    'message': None,
                    'expected': 'FAIL',
                    'known_intermittent': [],
                }],
            }],
        })
        self.assert_contents(
            'external/wpt/variant.html.ini', """\
            [variant.html?foo=baz]
              implementation-status: implementing
            """)

    def test_remove_nonexistent_subtest(self):
        """A full update cleans up subtests that no longer exist."""
        self.write_contents(
            'external/wpt/variant.html.ini', """\
            [variant.html?foo=baz]
              expected: CRASH

              [subtest that was removed]
                expected: CRASH
                custom_key: should not prevent removal
            """)
        self.update(
            {
                'results': [{
                    'test': '/variant.html?foo=baz',
                    'status': 'CRASH',
                    'subtests': [],
                }],
            },
            overwrite_conditions='yes')
        self.assert_contents(
            'external/wpt/variant.html.ini', """\
            [variant.html?foo=baz]
              expected: CRASH
            """)

    def test_keep_unobserved_subtest(self):
        """A partial update should not remove an unobserved subtest."""
        self.write_contents(
            'external/wpt/variant.html.ini', """\
            [variant.html?foo=baz]
              [subtest that should not be removed]
                expected: CRASH
            """)
        self.update(
            {
                'results': [{
                    'test': '/variant.html?foo=baz',
                    'status': 'CRASH',
                    'subtests': [],
                }],
            },
            overwrite_conditions='no')
        self.write_contents(
            'external/wpt/variant.html.ini', """\
            [variant.html?foo=baz]
              [subtest that should not be removed]
                expected: CRASH
            """)

    def test_no_change_for_expected(self):
        """The updater does not modify metadata for an expected result."""
        self.write_contents(
            'external/wpt/fail.html.ini', """\
            [fail.html]
              expected: [FAIL, CRASH]
            """)
        self.update(
            {
                'results': [{
                    'test': '/fail.html',
                    'status': 'CRASH',
                    'expected': 'FAIL',
                    'known_intermittent': ['CRASH'],
                }],
            },
            disable_intermittent='flaky')
        self.assert_contents(
            'external/wpt/fail.html.ini', """\
            [fail.html]
              expected: [FAIL, CRASH]
            """)

    def test_remove_stale_expectation(self):
        """The updater removes stale expectations by default."""
        self.write_contents(
            'external/wpt/fail.html.ini', """\
            [fail.html]
              expected: [OK, FAIL]
            """)
        self.update({
            'results': [{
                'test': '/fail.html',
                'status': 'FAIL',
                'expected': 'OK',
                'known_intermittent': ['FAIL'],
            }, {
                'test': '/fail.html',
                'status': 'CRASH',
                'expected': 'OK',
                'known_intermittent': ['FAIL'],
            }],
        })
        self.assert_contents(
            'external/wpt/fail.html.ini', """\
            [fail.html]
              expected: [FAIL, CRASH]
            """)

    def test_keep_existing_expectations(self):
        """The updater keeps stale expectations if explicitly requested."""
        self.write_contents(
            'external/wpt/fail.html.ini', """\
            [fail.html]
              expected: [OK, FAIL]
            """)
        self.update(
            {
                'results': [{
                    'test': '/fail.html',
                    'status': 'CRASH',
                    'expected': 'OK',
                    'known_intermittent': ['FAIL'],
                }],
            },
            keep_statuses=True)
        # The disable only works for flaky results in a single run.
        self.assert_contents(
            'external/wpt/fail.html.ini', """\
            [fail.html]
              expected: [CRASH, OK, FAIL]
            """)

    def test_disable_intermittent(self):
        """The updater can disable flaky tests.

        Note that the status list is not updated.
        """
        self.write_contents(
            'external/wpt/fail.html.ini', """\
            [fail.html]
              expected: FAIL
            """)
        self.update(
            {
                'results': [{
                    'test': '/fail.html',
                    'status': 'OK',
                    'expected': 'FAIL',
                }, {
                    'test': '/fail.html',
                    'status': 'FAIL',
                    'expected': 'FAIL',
                }],
            },
            disable_intermittent='flaky')
        self.assert_contents(
            'external/wpt/fail.html.ini', """\
            [fail.html]
              disabled: flaky
              expected: FAIL
            """)

    def test_update_bug_url(self):
        """The updater updates the 'bug' field for affected test IDs."""
        self.write_contents(
            'external/wpt/variant.html.ini', """\
            [variant.html?foo=bar/abc]
              bug: crbug.com/123

            [variant.html?foo=baz]
              bug: crbug.com/456
            """)
        self.update(
            {
                'results': [{
                    'test': '/variant.html?foo=baz',
                    'status': 'FAIL',
                }],
            },
            bug=789)
        self.assert_contents(
            'external/wpt/variant.html.ini', """\
            [variant.html?foo=bar/abc]
              bug: crbug.com/123

            [variant.html?foo=baz]
              bug: crbug.com/789
              expected: FAIL
            """)

    def test_condition_split(self):
        """A new status on a platform creates a new condition branch."""
        self.write_contents(
            'external/wpt/fail.html.ini', """\
            [fail.html]
              expected:
                if os == 'mac': FAIL
            """)
        self.update(
            {
                'run_info': {
                    'os': 'mac',
                    'version': '12',
                },
                'results': [{
                    'test': '/fail.html',
                    'status': 'TIMEOUT',
                    'expected': 'FAIL',
                }],
            }, {
                'run_info': {
                    'os': 'mac',
                    'version': '11',
                },
                'results': [{
                    'test': '/fail.html',
                    'status': 'FAIL',
                    'expected': 'FAIL',
                }],
            }, {
                'run_info': {
                    'os': 'win',
                    'version': '11',
                },
                'results': [{
                    'test': '/fail.html',
                    'status': 'OK',
                    'expected': 'OK',
                }],
            },
            overwrite_conditions='yes')
        path = self.finder.path_from_web_tests('external', 'wpt',
                                               'fail.html.ini')
        lines = self.tool.filesystem.read_text_file(path).splitlines()
        expected = textwrap.dedent("""\
            [fail.html]
              expected:
                if (os == "mac") and (version == "12"): TIMEOUT
                if (os == "mac") and (version == "11"): FAIL
                OK
            """)
        # TODO(crbug.com/1299650): The branch order appears unstable, which we
        # should fix upstream to avoid create spurious diffs.
        self.assertEqual(sorted(lines, reverse=True), expected.splitlines())

    def test_condition_merge(self):
        """Results that become property-agnostic consolidate conditions."""
        self.write_contents(
            'external/wpt/fail.html.ini', """\
            [fail.html]
              expected:
                if os == 'mac' and version == '11': FAIL
                if os == 'mac' and version == '12': TIMEOUT
            """)
        self.update(
            {
                'run_info': {
                    'os': 'mac',
                    'version': '12',
                },
                'results': [{
                    'test': '/fail.html',
                    'status': 'FAIL',
                    'expected': 'TIMEOUT',
                }],
            }, {
                'run_info': {
                    'os': 'mac',
                    'version': '11',
                },
                'results': [{
                    'test': '/fail.html',
                    'status': 'FAIL',
                    'expected': 'FAIL',
                }],
            }, {
                'run_info': {
                    'os': 'win',
                    'version': '11',
                },
                'results': [{
                    'test': '/fail.html',
                    'status': 'OK',
                    'expected': 'OK',
                }],
            },
            overwrite_conditions='yes')
        path = self.finder.path_from_web_tests('external', 'wpt',
                                               'fail.html.ini')
        contents = self.tool.filesystem.read_text_file(path)
        self.assertIn(
            contents, {
                textwrap.dedent("""\
                [fail.html]
                  expected:
                    if os == "mac": FAIL
                    OK
                """),
                textwrap.dedent("""\
                [fail.html]
                  expected:
                    if os == "win": OK
                    FAIL
                """),
            })


class UpdateMetadataArgumentParsingTest(unittest.TestCase):
    def setUp(self):
        self.tool = MockBlinkTool()
        self.command = UpdateMetadata(self.tool)

    @contextlib.contextmanager
    def assert_parse_error(self, expected_pattern):
        buf = io.StringIO()
        with contextlib.redirect_stderr(buf):
            with self.assertRaises(SystemExit):
                yield
        self.assertRegex(buf.getvalue(), expected_pattern)

    def test_build_syntax(self):
        options, _args = self.command.parse_args([
            '--build=ci/Linux Tests:100,linux-rel', '--build=mac-rel:200',
            '--build=ci/Mac12 Tests:300-302'
        ])
        self.assertEqual(options.builds, [
            Build('Linux Tests', 100, bucket='ci'),
            Build('linux-rel'),
            Build('mac-rel', 200),
            Build('Mac12 Tests', 300, bucket='ci'),
            Build('Mac12 Tests', 301, bucket='ci'),
            Build('Mac12 Tests', 302, bucket='ci'),
        ])
        with self.assert_parse_error('invalid build specifier'):
            self.command.parse_args(['--build=linux-rel:'])
        with self.assert_parse_error('invalid build specifier'):
            self.command.parse_args(['--build=linux-rel:nan'])
        with self.assert_parse_error('start build number must precede end'):
            self.command.parse_args(['--build=Linux Tests:100-10'])

    def test_bug_number_patterns(self):
        options, _args = self.command.parse_args(['-b', '123'])
        self.assertEqual(options.bug, 123)
        options, _args = self.command.parse_args(['-b', 'crbug/123'])
        self.assertEqual(options.bug, 123)
        options, _args = self.command.parse_args(['--bug=crbug.com/123'])
        self.assertEqual(options.bug, 123)
        with self.assert_parse_error('invalid bug number or URL'):
            self.command.parse_args(['--bug=crbug.com/123a'])
        with self.assert_parse_error('invalid bug number or URL'):
            self.command.parse_args(['-b', 'cbug.com/123'])

    def test_report_files(self):
        self.tool.filesystem.write_text_file('path/to/a.json', 'a')
        self.tool.filesystem.write_text_file('path/to/b.json', 'b')
        options, _args = self.command.parse_args(
            ['--report=path/to/a.json', '--report=path/to/b.json'])
        self.assertEqual(options.reports, ['path/to/a.json', 'path/to/b.json'])

    def test_report_dir(self):
        self.tool.filesystem.write_text_file('path/to/logs/a.json', 'a')
        self.tool.filesystem.write_text_file('path/to/logs/b.json', 'b')
        # Do not recursively traverse, since it can be slow.
        self.tool.filesystem.write_text_file('path/to/logs/skip/nested/c.json',
                                             'c')
        options, _args = self.command.parse_args(['--report=path/to/logs'])
        self.assertEqual(options.reports, [
            'path/to/logs/a.json',
            'path/to/logs/b.json',
        ])

    def test_report_does_not_exist(self):
        with self.assert_parse_error('is neither a regular file '
                                     'nor a directory'):
            self.command.parse_args(['--report=does/not/exist'])
