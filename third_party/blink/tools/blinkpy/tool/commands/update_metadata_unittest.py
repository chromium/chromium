# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import contextlib
import io
import json
import optparse
import textwrap
import unittest
from unittest.mock import patch

from blinkpy.common.path_finder import PathFinder
from blinkpy.common.net.git_cl import TryJobStatus
from blinkpy.common.net.git_cl_mock import MockGitCL
from blinkpy.common.net.rpc import Build, RPCError
from blinkpy.common.system.log_testing import LoggingTestCase
from blinkpy.tool.mock_tool import MockBlinkTool
from blinkpy.tool.commands.update_metadata import UpdateMetadata
from blinkpy.web_tests.builder_list import BuilderList


@patch('concurrent.futures.ThreadPoolExecutor.map', new=map)
class UpdateMetadataTest(LoggingTestCase):
    def setUp(self):
        super().setUp()
        self.maxDiff = None
        self.tool = MockBlinkTool()
        self.tool.builders = BuilderList({
            'test-linux-rel': {
                'port_name': 'test-linux-trusty',
                'specifiers': ['Trusty', 'Release'],
                'is_try_builder': True,
            },
            'test-mac-rel': {
                'port_name': 'test-mac-mac12',
                'specifiers': ['Mac12', 'Release'],
                'is_try_builder': True,
            },
            'Test Linux Tests': {
                'port_name': 'test-linux-trusty',
                'specifiers': ['Trusty', 'Release'],
            },
        })
        self.builds = {
            Build('test-linux-rel', 1000, '1000'):
            TryJobStatus.from_bb_status('FAILURE'),
            Build('test-mac-rel', 2000, '2000'):
            TryJobStatus.from_bb_status('SUCCESS'),
        }
        self.git_cl = MockGitCL(self.tool, self.builds)
        self.command = UpdateMetadata(self.tool, self.git_cl)

        finder = PathFinder(self.tool.filesystem)
        self.tool.filesystem.write_text_file(
            finder.path_from_web_tests('external', 'wpt', 'MANIFEST.json'),
            json.dumps({
                'version': 8,
                'items': {
                    'reftest': {
                        'fail.html': [
                            'c3f2fb6f436da59d43aeda0a7e8a018084557033',
                            [None, [['reftest-ref.html', '==']], {}],
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
            finder.path_from_web_tests('wpt_internal', 'MANIFEST.json'),
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

    @contextlib.contextmanager
    def _patch_builtins(self):
        with contextlib.ExitStack() as stack:
            stack.enter_context(self.tool.filesystem.patch_builtins())
            stack.enter_context(self.tool.executive.patch_builtins())
            yield stack

    def test_execute_all(self):
        with self._patch_builtins():
            exit_code = self.command.main([])
        self.assertEqual(exit_code, 0)
        # Even tests that pass may require an update if a subtest was added or
        # removed.
        self.assertLog([
            'INFO: All builds finished.\n',
            'INFO: Processing wptrunner report (1/1)\n',
            "INFO: Updating 'crash.html' (1/5)\n",
            "INFO: Updating 'dir/multiglob.https.any.js' (2/5)\n",
            "INFO: Updating 'fail.html' (3/5)\n",
            "INFO: Updating 'pass.html' (4/5)\n",
            "INFO: Updating 'variant.html' (5/5)\n",
        ])

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
            "INFO: Updating 'dir/multiglob.https.any.js' (1/3)\n",
            "INFO: Updating 'pass.html' (2/3)\n",
            "INFO: Updating 'variant.html' (3/3)\n",
        ])

    def test_execute_with_no_issue_number_aborts(self):
        self.command.git_cl = MockGitCL(self.tool, issue_number='None')
        with self._patch_builtins():
            exit_code = self.command.main([])
        self.assertEqual(exit_code, 1)
        self.assertLog(['ERROR: No issue number for current branch.\n'])

    def test_execute_trigger_jobs(self):
        self.command.git_cl = MockGitCL(
            self.tool, {
                Build('test-linux-rel', 1000, '1000'):
                TryJobStatus.from_bb_status('STARTED'),
            })
        with self._patch_builtins():
            exit_code = self.command.main([])
        self.assertEqual(exit_code, 1)
        self.assertLog([
            'INFO: No finished builds.\n',
            'INFO: Scheduled or started builds:\n',
            'INFO:   BUILDER              NUMBER  STATUS    BUCKET\n',
            'INFO:   test-linux-rel       1000    STARTED   try   \n',
            'INFO:   test-mac-rel         --      TRIGGERED try   \n',
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
            exit_code = self.command.main(['--build=Test Linux Tests'])
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
            '--build=Linux Tests:100,linux-rel', '--build=mac-rel:200',
            '--build=Mac12 Tests'
        ])
        self.assertEqual(options.builds, [
            Build('Linux Tests', 100),
            Build('linux-rel'),
            Build('mac-rel', 200),
            Build('Mac12 Tests'),
        ])
        with self.assert_parse_error('invalid build number'):
            self.command.parse_args(['--build=linux-rel:'])
        with self.assert_parse_error('invalid build number'):
            self.command.parse_args(['--build=linux-rel:nan'])

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
