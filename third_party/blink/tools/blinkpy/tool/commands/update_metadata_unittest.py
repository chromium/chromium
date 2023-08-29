# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import contextlib
import io
import json
import textwrap
import unittest
from unittest.mock import Mock, call, patch

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
    sort_metadata_ast,
)
from blinkpy.w3c.wpt_metadata import TestConfigurations
from blinkpy.web_tests.builder_list import BuilderList
from blinkpy.web_tests.port.base import VirtualTestSuite

path_finder.bootstrap_wpt_imports()
from manifest.manifest import Manifest
from wptrunner import metadata, wptmanifest


class BaseUpdateMetadataTest(LoggingTestCase):
    def setUp(self):
        super().setUp()
        self.maxDiff = None
        # Lower this threshold so that test results do not need to be repeated.
        MetadataUpdater.min_results_for_update = 1

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
                            ['variant.html?foo=bar/abc', {
                                'timeout': 'long'
                            }],
                            ['variant.html?foo=baz', {
                                'timeout': 'long'
                            }],
                        ],
                    },
                    'manual': {
                        'manual.html': [
                            'e933fd981d4a33ba82fb2b000234859bdda1494e',
                            [None, {}],
                        ],
                    },
                    'support': {
                        'helper.js': [
                            'f933fd981d4a33ba82fb2b000234859bdda1494e',
                            [None, {}],
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
            default_port = Mock(wraps=self.tool.port_factory.get('test'))
            default_port.FLAG_EXPECTATIONS_PREFIX = 'FlagExpectations'
            default_port.default_smoke_test_only.return_value = False
            default_port.skipped_due_to_smoke_tests.return_value = False
            default_port.virtual_test_suites.return_value = []
            stack.enter_context(
                patch.object(self.tool.port_factory,
                             'get',
                             return_value=default_port))
            yield stack


# Do not re-request try build information to check for interrupted steps.
@patch(
    'blinkpy.common.net.rpc.BuildbucketClient.execute_batch', lambda self: [])
class UpdateMetadataExecuteTest(BaseUpdateMetadataTest):
    """Verify the tool's frontend and build infrastructure interactions."""

    def setUp(self):
        super().setUp()
        self.tool.builders = BuilderList({
            'test-linux-wpt-rel': {
                'port_name': 'test-linux-trusty',
                'specifiers': ['Trusty', 'Release'],
                'is_try_builder': True,
                'steps': {
                    'wpt_tests_suite (with patch)': {},
                    'wpt_tests_suite_highdpi (with patch)': {
                        'flag_specific': 'highdpi',
                    },
                },
            },
            'test-mac-wpt-rel': {
                'port_name': 'test-mac-mac10.11',
                'specifiers': ['Mac10.11', 'Debug'],
                'is_try_builder': True,
                'steps': {
                    'wpt_tests_suite (with patch)': {},
                },
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
        self.command = UpdateMetadata(self.tool, git_cl=self.git_cl)

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
                'port': 'mac12',
                'product': 'content_shell',
                'flag_specific': '',
                'virtual_suite': '',
                'debug': False,
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
                'os': 'mac',
                'port': 'mac12',
                'product': 'content_shell',
                'flag_specific': '',
                'virtual_suite': '',
                'debug': False,
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
        lines = self.tool.filesystem.read_text_file(
            self.finder.path_from_web_tests('external', 'wpt',
                                            'variant.html.ini')).splitlines()
        self.assertIn('[variant.html?foo=bar/abc]', lines)
        # The other variant is not updated.
        self.assertNotIn('[variant.html?foo=baz]', lines)

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

    def test_execute_with_infra_failure(self):
        self.command.git_cl = MockGitCL(
            self.tool, {
                Build('test-linux-wpt-rel', 1000, '1000'):
                TryJobStatus.from_bb_status('INFRA_FAILURE'),
                Build('test-mac-wpt-rel', 2000, '2000'):
                TryJobStatus.from_bb_status('SUCCESS'),
            })
        with self._patch_builtins():
            exit_code = self.command.main([])
        self.assertEqual(exit_code, 1)
        self.assertLog([
            'WARNING: Some builds have infrastructure failures:\n',
            'WARNING:   "test-linux-wpt-rel" build 1000\n',
            'WARNING: Examples of infrastructure failures include:\n',
            'WARNING:   * Shard terminated the harness after timing out.\n',
            'WARNING:   * Harness exited early due to excessive unexpected '
            'failures.\n',
            'WARNING:   * Build failed on a non-test step.\n',
            'WARNING: Please consider retrying the failed builders or '
            'giving the builders more shards.\n',
            'WARNING: See https://chromium.googlesource.com/chromium/src/+/'
            'HEAD/docs/testing/web_test_expectations.md#handle-bot-timeouts\n',
            'INFO: All builds finished.\n',
            'INFO: Continue?\n',
            'ERROR: Aborting update due to build(s) with infrastructure '
            'failures.\n',
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
        self.assertIn(['luci-auth', 'token'], self.tool.executive.calls)
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

    def test_execute_warn_parsing_error(self):
        self.tool.filesystem.write_text_file(
            self.finder.path_from_web_tests('external', 'wpt',
                                            'crash.html.ini'),
            textwrap.dedent("""\
                [crash.html]
                  expected: [OK, CRASH  # Unclosed list
                """))
        with self._patch_builtins():
            exit_code = self.command.main(['crash.html'])
        self.assertEqual(exit_code, 0)
        self.assertLog([
            'INFO: All builds finished.\n',
            'INFO: Processing wptrunner report (1/1)\n',
            'INFO: Updating expectations for up to 1 test file.\n',
            "ERROR: Failed to parse 'external/wpt/crash.html.ini': "
            'EOL in list value (comment):  line 2\n',
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

    def test_generate_configs(self):
        virtual_suite = VirtualTestSuite(
            prefix='fake-vts',
            platforms=['Mac', 'Linux'],
            bases=['external/wpt/fail.html'],
            args=['--enable-features=FakeFeature'])
        with patch('blinkpy.web_tests.port.test.TestPort.virtual_test_suites',
                   return_value=[virtual_suite]):
            config_order = lambda config: (
                config['os'],
                config['flag_specific'],
                config['virtual_suite'],
            )
            linux, _, linux_highdpi, _, mac, mac_virtual = sorted(
                TestConfigurations.generate(self.tool), key=config_order)

            self.assertEqual(linux['os'], 'linux')
            self.assertEqual(linux['port'], 'trusty')
            self.assertFalse(linux['debug'])
            self.assertEqual(linux['flag_specific'], '')
            self.assertEqual(linux['virtual_suite'], '')

            self.assertEqual(linux_highdpi['os'], 'linux')
            self.assertEqual(linux_highdpi['flag_specific'], 'highdpi')

            self.assertEqual(mac['os'], 'mac')
            self.assertEqual(mac['port'], 'mac10.11')
            self.assertTrue(mac['debug'])
            self.assertEqual(mac['flag_specific'], '')

            self.assertEqual(mac_virtual['os'], 'mac')
            self.assertEqual(mac_virtual['virtual_suite'], 'fake-vts')


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
            manifests, configs = load_and_update_manifests(self.finder), {}
            for report in reports:
                report['run_info'] = base_run_info = {
                    'product': 'content_shell',
                    'os': 'mac',
                    'port': 'mac12',
                    'flag_specific': '',
                    'debug': False,
                    **(report.get('run_info') or {}),
                }
                report['results'] = [{
                    **result_defaults,
                    **result
                } for result in report['results']]
                subsuites = report.get('subsuites', {})
                subsuites.setdefault('', {'virtual_suite': ''})
                test_port = report.pop('test_port',
                                       self.tool.port_factory.get())
                for subsuite_run_info in subsuites.values():
                    run_info = metadata.RunInfo({
                        **base_run_info,
                        **subsuite_run_info,
                    })
                    configs[run_info] = test_port

            configs = TestConfigurations(self.tool.filesystem, configs)
            updater = MetadataUpdater.from_manifests(
                manifests, configs, self.tool.port_factory.get(), **options)
            updater.collect_results(
                io.StringIO(json.dumps(report)) for report in reports)
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

    def test_migrate_comments(self):
        self.write_contents(
            'external/wpt/variant.html.ini', """\
            # Comment 0
            [variant.html?foo=bar/abc]
              bug: crbug.com/123
            """)
        self.write_contents(
            'wpt_internal/dir/__dir__.ini', """\
            bug: crbug.com/321
            """)
        self.write_contents(
            'TestExpectations', """\
            # tags: [ Linux Mac Win ]
            # results: [ Failure Pass ]

            # This comment should not be transferred because it's in its own
            # paragraph block.

            # Comment 1
            #   Comment 2
            crbug.com/456 [ Linux ] external/wpt/variant.html?foo=bar/abc [ Failure ]  # Comment 3
            crbug.com/789 [ Mac ] virtual/virtual_wpt/external/wpt/variant.html?foo=bar/abc [ Failure ]  # Comment 4
            # Not transferred (`?foo=baz` variant has no section)
            external/wpt/variant.html?foo=baz [ Failure ]

            # Group of tests that fail for the same reason.
            non/wpt/test.html [ Failure ]  # Not transferred
            wpt_internal/dir/multiglob.https.any.worker.html [ Failure ] #Comment 5
            crbug.com/654 wpt_internal/dir/* [ Failure ]    # Comment 6
            """)
        self.update(
            {
                'results': [{
                    'test': '/variant.html?foo=bar/abc',
                    'status': 'OK',
                }, {
                    'test': '/variant.html?foo=baz',
                    'status': 'OK',
                }, {
                    'test':
                    '/wpt_internal/dir/multiglob.https.any.worker.html',
                    'status': 'ERROR',
                    'expected': 'OK',
                }],
            },
            migrate=True)
        # Note: Comments/bugs are migrated, even if all results are as expected.
        self.assert_contents(
            'external/wpt/variant.html.ini', """\
            # Comment 1
            #   Comment 2
            # Comment 3
            # Comment 4
            [variant.html?foo=bar/abc]
              bug: [crbug.com/123, crbug.com/456, crbug.com/789]
            """)
        self.assert_contents(
            'wpt_internal/dir/__dir__.ini', """\
            # Group of tests that fail for the same reason.
            # Comment 6
            bug: [crbug.com/321, crbug.com/654]
            """)
        self.assert_contents(
            'wpt_internal/dir/multiglob.https.any.js.ini', """\
            # Group of tests that fail for the same reason.
            #Comment 5
            [multiglob.https.any.worker.html]
              expected: ERROR
            """)

    def test_migrate_disables(self):
        self.write_contents(
            'external/wpt/variant.html.ini', """\
            [variant.html?foo=bar/abc]
              disabled:
                if os == "win": overwrite this
            """)
        self.write_contents(
            'NeverFixTests', """\
            # tags: [ Linux Mac Win ]
            # results: [ Skip ]
            [ Mac ] external/wpt/variant.html?foo=bar/abc [ Skip ]
            """)
        self.write_contents(
            'TestExpectations', """\
            # tags: [ Linux Mac Win ]
            # results: [ Skip ]
            virtual/fake-vts/external/wpt/variant.html?foo=baz [ Skip ]
            """)
        self.update(
            {
                'run_info': {
                    'product': 'content_shell',
                    'os': 'mac',
                },
                'results': [],
                'test_port': self.tool.port_factory.get('test-mac-mac10.11'),
            }, {
                'run_info': {
                    'product': 'content_shell',
                    'os': 'linux',
                },
                'subsuites': {
                    '': {
                        'virtual_suite': '',
                    },
                    'fake-vts': {
                        'virtual_suite': 'fake-vts',
                    },
                },
                'results': [],
                'test_port': self.tool.port_factory.get('test-linux-trusty'),
            }, {
                'run_info': {
                    'product': 'chrome',
                    'os': 'linux',
                },
                'results': [],
                'test_port': self.tool.port_factory.get('test-linux-trusty'),
            },
            migrate=True)
        self.assert_contents(
            'external/wpt/variant.html.ini', """\
            [variant.html?foo=bar/abc]
              disabled:
                if (product == "content_shell") and (os == "mac"): neverfix

            [variant.html?foo=baz]
              disabled:
                if (product == "content_shell") and (virtual_suite == "fake-vts"): skipped in TestExpectations
            """)

    def test_migrate_disables_glob_flag_specific(self):
        self.write_contents('FlagSpecificConfig',
                            json.dumps([{
                                'name': 'fake-flag',
                                'args': [],
                            }]))
        self.write_contents(
            'FlagExpectations/fake-flag', """\
            # results: [ Pass Skip ]
            wpt_internal/dir/* [ Skip ]
            wpt_internal/dir/multiglob.https.any.worker.html [ Pass ]
            """)
        flag_port = self.tool.port_factory.get('test-linux-trusty')
        flag_port.set_option_default('flag_specific', 'fake-flag')
        self.update(
            {
                'run_info': {
                    'product': 'content_shell',
                    'os': 'mac',
                    'flag_specific': '',
                },
                'results': [],
                'test_port': self.tool.port_factory.get('test-mac-mac10.11'),
            }, {
                'run_info': {
                    'product': 'content_shell',
                    'os': 'linux',
                    'flag_specific': '',
                },
                'results': [],
                'test_port': self.tool.port_factory.get('test-linux-trusty'),
            }, {
                'run_info': {
                    'product': 'chrome',
                    'os': 'linux',
                    'flag_specific': '',
                },
                'results': [],
                'test_port': self.tool.port_factory.get('test-linux-trusty'),
            }, {
                'run_info': {
                    'product': 'content_shell',
                    'os': 'linux',
                    'flag_specific': 'fake-flag',
                },
                'results': [],
                'test_port': flag_port,
            },
            migrate=True)
        self.assert_contents(
            'wpt_internal/dir/__dir__.ini', """\
            disabled:
              if (product == "content_shell") and (os == "linux") and (flag_specific == "fake-flag"): skipped in TestExpectations
            """)
        self.assert_contents(
            'wpt_internal/dir/multiglob.https.any.js.ini', """\
            [multiglob.https.any.worker.html]
              disabled:
                if (product == "content_shell") and (os == "linux") and (flag_specific == "fake-flag"): @False
            """)

    def test_migrate_disables_glob_virtual(self):
        self.write_contents(
            'TestExpectations', """\
            # results: [ Pass Skip ]
            wpt_internal/dir/* [ Skip ]
            virtual/fake-vts/wpt_internal/dir/* [ Pass ]
            """)
        self.update(
            {
                'run_info': {
                    'product': 'chrome',
                    'os': 'linux',
                },
                'results': [],
                'test_port': self.tool.port_factory.get('test-linux-trusty'),
            }, {
                'run_info': {
                    'product': 'content_shell',
                    'os': 'linux',
                },
                'subsuites': {
                    '': {
                        'virtual_suite': '',
                    },
                    'fake-vts': {
                        'virtual_suite': 'fake-vts',
                    },
                },
                'results': [],
                'test_port': self.tool.port_factory.get('test-linux-trusty'),
            },
            migrate=True)
        self.assert_contents(
            'wpt_internal/dir/__dir__.ini', """\
            disabled:
              if (product == "content_shell") and (virtual_suite == "fake-vts"): @False
              if product == "chrome": skipped in TestExpectations
              if (product == "content_shell") and (virtual_suite == ""): skipped in TestExpectations
            """)

    def test_migrate_disables_non_directory_glob(self):
        self.write_contents(
            'TestExpectations', """\
            # results: [ Pass Failure Skip ]
            wpt_internal/dir/* [ Failure ]
            wpt_internal/dir/multiglob* [ Skip ]
            """)
        self.update(
            {
                'run_info': {
                    'os': 'mac',
                },
                'results': [],
                'test_port': self.tool.port_factory.get('test-mac-mac10.11'),
            }, {
                'run_info': {
                    'os': 'linux',
                },
                'results': [],
                'test_port': self.tool.port_factory.get('test-linux-trusty'),
            },
            migrate=True)
        self.assertFalse(self.exists('wpt_internal/dir/__dir__.ini'))
        self.assert_contents(
            'wpt_internal/dir/multiglob.https.any.js.ini', """\
            [multiglob.https.any.window.html]
              disabled: skipped in TestExpectations

            [multiglob.https.any.worker.html]
              disabled: skipped in TestExpectations
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

    def test_remove_orphaned_test(self):
        self.write_contents(
            'external/wpt/variant.html.ini', """\
            [variant.html?does-not-exist]
              expected: ERROR
              [subtest]
                expected: FAIL

            [variant.html?also-does-not-exist]
              expected: ERROR
            """)
        self.update({
            'results': [{
                'test': '/variant.html?foo=baz',
                'status': 'OK',
                'subtests': [],
            }],
        })
        self.assertFalse(self.exists('external/wpt/variant.html.ini'))

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

    def test_no_change_without_enough_results(self):
        """The updater does not modify metadata without enough test results.

        A single test run cannot surface flakiness, and therefore does not
        suffice to update or remove existing expectations confidently. In
        Chromium/Blink, a WPT test is not retried when it runs as expected or
        unexpectedly passes. Unexpected passes should be removed separately
        using long-term test history.
        """
        MetadataUpdater.min_results_for_update = 2
        self.write_contents(
            'external/wpt/fail.html.ini', """\
            [fail.html]
              expected: FAIL
            """)
        self.update({
            'results': [{
                'test': '/fail.html',
                'status': 'PASS',
                'expected': 'FAIL',
            }],
        })
        self.assert_contents(
            'external/wpt/fail.html.ini', """\
            [fail.html]
              expected: FAIL
            """)

    def test_remove_stale_expectation(self):
        """The updater removes stale expectations by default."""
        self.write_contents(
            'external/wpt/fail.html.ini', """\
            [fail.html]
              expected: [PASS, FAIL]
            """)
        self.update({
            'results': [{
                'test': '/fail.html',
                'status': 'FAIL',
                'expected': 'PASS',
                'known_intermittent': ['FAIL'],
            }, {
                'test': '/fail.html',
                'status': 'CRASH',
                'expected': 'PASS',
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
              expected: [PASS, FAIL]
            """)
        self.update(
            {
                'results': [{
                    'test': '/fail.html',
                    'status': 'CRASH',
                    'expected': 'PASS',
                    'known_intermittent': ['FAIL'],
                }],
            },
            keep_statuses=True)
        # The disable only works for flaky results in a single run.
        self.assert_contents(
            'external/wpt/fail.html.ini', """\
            [fail.html]
              expected: [PASS, CRASH, FAIL]
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

            # Keep this comment, even as the bug is updated.
            [variant.html?foo=baz]
              bug: crbug.com/456
              expected: FAIL
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

            # Keep this comment, even as the bug is updated.
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
                if product == "content_shell": FAIL
            """)
        self.update(
            {
                'run_info': {
                    'product': 'content_shell',
                    'os': 'mac',
                    'port': 'mac12',
                },
                'results': [{
                    'test': '/fail.html',
                    'status': 'TIMEOUT',
                    'expected': 'FAIL',
                }],
            }, {
                'run_info': {
                    'product': 'content_shell',
                    'os': 'win',
                    'port': 'win11',
                },
                'results': [{
                    'test': '/fail.html',
                    'status': 'FAIL',
                    'expected': 'FAIL',
                }],
            }, {
                'run_info': {
                    'product': 'chrome',
                    'os': 'linux',
                    'port': 'trusty',
                },
                'results': [{
                    'test': '/fail.html',
                    'status': 'PASS',
                    'expected': 'PASS',
                }],
            },
            overwrite_conditions='yes')
        self.assert_contents(
            'external/wpt/fail.html.ini', """\
            [fail.html]
              expected:
                if (product == "content_shell") and (os == "win"): FAIL
                if (product == "content_shell") and (os == "mac"): TIMEOUT
            """)

    def test_condition_split_on_virtual_suite(self):
        # The test relies on a feature only enabled by `fake-vts`.
        self.update(
            {
                'subsuites': {
                    '': {
                        'virtual_suite': '',
                    },
                    'fake-vts': {
                        'virtual_suite': 'fake-vts',
                    },
                },
                'results': [{
                    'test': '/fail.html',
                    'status': 'FAIL',
                    'expected': 'PASS',
                }, {
                    'test': '/fail.html',
                    'subsuite': 'fake-vts',
                    'status': 'PASS',
                }],
            }, {
                'run_info': {
                    'product': 'chrome'
                },
                'results': [],
            })
        self.assert_contents(
            'external/wpt/fail.html.ini', """\
            [fail.html]
              expected:
                if (product == "content_shell") and (virtual_suite == ""): FAIL
            """)

    def test_condition_merge(self):
        """Results that become property-agnostic consolidate conditions."""
        self.write_contents(
            'external/wpt/fail.html.ini', """\
            [fail.html]
              expected:
                if product == "content_shell" and os == "mac": FAIL
                if product == "content_shell" and os == "linux": TIMEOUT
            """)
        self.update(
            {
                'run_info': {
                    'product': 'content_shell',
                    'os': 'linux',
                },
                'results': [{
                    'test': '/fail.html',
                    'status': 'FAIL',
                    'expected': 'TIMEOUT',
                }],
            }, {
                'run_info': {
                    'product': 'content_shell',
                    'os': 'mac',
                },
                'results': [{
                    'test': '/fail.html',
                    'status': 'FAIL',
                    'expected': 'FAIL',
                }],
            }, {
                'run_info': {
                    'product': 'chrome',
                    'os': 'linux',
                    'port': 'trusty',
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
                    if product == "chrome": OK
                    FAIL
                """),
                textwrap.dedent("""\
                [fail.html]
                  expected:
                    if product == "content_shell": FAIL
                    OK
                """),
            })

    def test_condition_keep(self):
        """Updating one platform's results should preserve those of others.

        This test exercises the `--overwrite-conditions=fill` option (the
        default).
        """
        self.write_contents(
            'external/wpt/pass.html.ini', """\
            [pass.html]
              [subtest]
                expected:
                  if (product == "content_shell") and (os == "win"): PASS
                  if product == "chrome": [FAIL, PASS]
                  FAIL
            """)
        self.update(
            {
                'run_info': {
                    'product': 'content_shell',
                    'os': 'win'
                },
                'results': [{
                    'test':
                    '/pass.html',
                    'status':
                    'TIMEOUT',
                    'expected':
                    'OK',
                    'subtests': [{
                        'name': 'subtest',
                        'status': 'TIMEOUT',
                        'expected': 'PASS',
                    }],
                }],
            }, {
                'run_info': {
                    'product': 'content_shell',
                    'os': 'mac'
                },
                'results': [],
            }, {
                'run_info': {
                    'product': 'chrome',
                    'os': 'linux'
                },
                'results': [],
            })
        # Without result replay, the `FAIL` expectation is erroneously deleted,
        # which will give either:
        #   expected: TIMEOUT
        #
        # with a full update alone (i.e., `--overwrite-conditions=yes`), or
        #   expected:
        #     if os == "win": TIMEOUT
        #
        # without a full update (i.e., `--overwrite-conditions=no`).
        self.assert_contents(
            'external/wpt/pass.html.ini', """\
            [pass.html]
              expected:
                if (product == "content_shell") and (os == "win"): TIMEOUT
              [subtest]
                expected:
                  if (product == "content_shell") and (os == "win"): TIMEOUT
                  if product == "chrome": [FAIL, PASS]
                  FAIL
            """)

    def test_no_fill_for_disabled_configs(self):
        self.write_contents(
            'external/wpt/variant.html.ini', """\
            [variant.html?foo=baz]
              disabled:
                if product == "chrome": @False
                if product == "content_shell": needs webdriver
            """)
        self.write_contents(
            'external/wpt/__dir__.ini', """\
            disabled:
                if product == "chrome": not tested by default
                if product == "android_webview": not tested by default
            """)

        smoke_test_port = Mock()
        smoke_test_port.skips_test.return_value = True
        self.update(
            {
                'run_info': {
                    'product': 'chrome',
                    'flag_specific': '',
                },
                'results': [{
                    'test': '/variant.html?foo=baz',
                    'status': 'FAIL',
                    'expected': 'PASS',
                    'subtests': [],
                }],
            }, {
                'run_info': {
                    'product': 'chrome',
                    'flag_specific': 'fake-flag',
                },
                'results': [],
                'test_port': smoke_test_port,
            }, {
                'run_info': {
                    'product': 'content_shell',
                },
                'results': [],
            }, {
                'run_info': {
                    'product': 'android_webview',
                },
                'results': [],
            })

        # Only non-flag-specific 'chrome' runs, so there's no need to write its
        # failure expectation conditionally like:
        #   if (product == "chrome") and (flag_specific == ""): FAIL
        #
        # A `PASS` also should not be added for skipped configurations, which
        # would result in:
        #   [FAIL, PASS]
        self.assert_contents(
            'external/wpt/variant.html.ini', """\
            [variant.html?foo=baz]
              disabled:
                if product == "chrome": @False
                if product == "content_shell": needs webdriver
              expected: FAIL
            """)
        smoke_test_port.skips_test.assert_has_calls([
            call('external/wpt/variant.html?foo=baz'),
        ])

    def test_no_fill_for_unsupported_configs(self):
        from wptrunner.browsers import content_shell
        browser_info = {
            **content_shell.__wptrunner__,
            # Pretend `content_shell` does not support reftests.
            'executor': {},
        }
        with patch('wptrunner.browsers.content_shell.__wptrunner__',
                   browser_info):
            self.update(
                {
                    'run_info': {
                        'product': 'chrome',
                    },
                    'results': [{
                        'test': '/fail.html',
                        'status': 'FAIL',
                        'expected': 'PASS',
                    }],
                }, {
                    'run_info': {
                        'product': 'content_shell',
                    },
                    'results': [],
                })
        # `update-metadata` should write the expectation unconditionally instead
        # of as:
        #   if product == "chrome": FAIL
        self.assert_contents(
            'external/wpt/fail.html.ini', """\
            [fail.html]
              expected: FAIL
            """)

    def test_no_fill_for_exclusive_virtual_test(self):
        """Check that exclusive tests restrict the configurations recognized.

        In particular, no implicit PASS should be preserved for configurations
        that don't apply.
        """
        test_port = self.tool.port_factory.get('test-mac-mac10.11')
        exclusive_suite = VirtualTestSuite(
            prefix='exclusive-vts',
            platforms=['Mac'],
            bases=['external/wpt/fail.html'],
            exclusive_tests=['external/wpt/fail.html'],
            args=['--enable-features=FakeFeature'])
        unrelated_suite = VirtualTestSuite(
            prefix='unrelated-vts',
            platforms=['Mac'],
            bases=['external/wpt/variant.html'],
            args=['--enable-features=FakeFeature'])
        with patch.object(test_port,
                          'virtual_test_suites',
                          return_value=[exclusive_suite, unrelated_suite]):
            self.update(
                {
                    'run_info': {
                        'product': 'content_shell',
                    },
                    'subsuites': {
                        '': {
                            'virtual_suite': '',
                        },
                        'exclusive-vts': {
                            'virtual_suite': 'exclusive-vts',
                        },
                        'unrelated-vts': {
                            'virtual_suite': 'unrelated-vts',
                        },
                    },
                    'results': [{
                        'test': '/fail.html',
                        'status': 'FAIL',
                        'expected': 'PASS',
                        'subsuite': 'exclusive-vts',
                    }],
                    'test_port':
                    test_port,
                }, {
                    'run_info': {
                        'product': 'chrome',
                    },
                    'results': [],
                    'test_port': test_port,
                })
        self.assert_contents(
            'external/wpt/fail.html.ini', """\
            [fail.html]
              expected: FAIL
            """)

    def test_condition_initialization_without_starting_metadata(self):
        self.update(
            {
                'run_info': {
                    'product': 'content_shell'
                },
                'results': [{
                    'test': '/variant.html?foo=baz',
                    'status': 'FAIL',
                    'expected': 'OK',
                }],
            }, {
                'run_info': {
                    'product': 'chrome'
                },
                'results': [],
            })
        self.assert_contents(
            'external/wpt/variant.html.ini', """\
            [variant.html?foo=baz]
              expected:
                if product == "content_shell": FAIL
            """)

    def test_condition_initialization_without_starting_subtest(self):
        self.write_contents(
            'external/wpt/variant.html.ini', """\
            [variant.html?foo=baz]
            """)
        self.update(
            {
                'run_info': {
                    'product': 'content_shell'
                },
                'results': [{
                    'test':
                    '/variant.html?foo=baz',
                    'status':
                    'OK',
                    'subtests': [{
                        'name': 'new subtest',
                        'status': 'FAIL',
                        'expected': 'PASS',
                    }],
                }],
            }, {
                'run_info': {
                    'product': 'chrome'
                },
                'results': [],
            })
        self.assert_contents(
            'external/wpt/variant.html.ini', """\
            [variant.html?foo=baz]
              [new subtest]
                expected:
                  if product == "content_shell": FAIL
            """)

    def test_condition_no_change(self):
        self.write_contents(
            'external/wpt/variant.html.ini', """\
            [variant.html?foo=baz]
              [subtest]
                expected: FAIL
            """)
        self.update()
        self.assert_contents(
            'external/wpt/variant.html.ini', """\
            [variant.html?foo=baz]
              [subtest]
                expected: FAIL
            """)

    def test_disable_slow_timeouts(self):
        self.update(
            {
                'run_info': {
                    'product': 'content_shell',
                },
                'results': [],
            }, {
                'run_info': {
                    'product': 'chrome',
                },
                'results': [{
                    'test': '/variant.html?foo=baz',
                    'status': 'TIMEOUT',
                }],
            })
        self.assert_contents(
            'external/wpt/variant.html.ini', """\
            [variant.html?foo=baz]
              disabled:
                if product == "chrome": times out even with `timeout=long`
              expected:
                if product == "chrome": TIMEOUT
            """)

    def test_stable_rendering(self):
        buf = io.BytesIO(
            textwrap.dedent("""\
                [variant.html?foo=baz]
                  [subtest 2]
                    expected:
                      if os == "win": FAIL
                      if os == "mac": FAIL
                    disabled: @False
                  [subtest 1]
                  expected: [OK, CRASH]

                bug: crbug.com/123

                [variant.html?foo=bar/abc]
                """).encode())
        ast = wptmanifest.parse(buf)
        sort_metadata_ast(ast)
        # Unlike keys/sections, the ordering of conditions is significant, so
        # they should not be sorted.
        self.assertEqual(
            wptmanifest.serialize(ast),
            textwrap.dedent("""\
                bug: crbug.com/123
                [variant.html?foo=bar/abc]

                [variant.html?foo=baz]
                  expected: [OK, CRASH]
                  [subtest 1]

                  [subtest 2]
                    disabled: @False
                    expected:
                      if os == "win": FAIL
                      if os == "mac": FAIL
                """))


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
