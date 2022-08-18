# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import contextlib
import io
import json
import optparse
import unittest
from unittest.mock import patch

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

    def test_execute_basic(self):
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
                'expected': 'OK',
                'status': 'CRASH',
            }],
        }).encode()
        exit_code = self.command.main([])
        self.assertEqual(exit_code, 0)
        self.assertLog([
            'INFO: All builds finished.\n',
            'INFO: Processing wptrunner report (1/1)\n',
            'INFO: 1 test (product: chrome, os: mac, os_version: 12, '
            'cpu: arm-64, flag_specific: -)\n',
        ])

    def test_execute_with_no_issue_number_aborts(self):
        self.command.git_cl = MockGitCL(self.tool, issue_number='None')
        exit_code = self.command.main([])
        self.assertEqual(exit_code, 1)
        self.assertLog(['ERROR: No issue number for current branch.\n'])

    def test_execute_trigger_jobs(self):
        self.command.git_cl = MockGitCL(
            self.tool, {
                Build('test-linux-rel', 1000, '1000'):
                TryJobStatus.from_bb_status('STARTED'),
            })
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
        with patch.object(self.command.git_cl.bb_client,
                          'execute_batch',
                          side_effect=error):
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
        exit_code = self.command.main(['--no-trigger-jobs'])
        self.assertEqual(exit_code, 1)
        self.assertLog([
            "ERROR: Aborted: no try jobs and '--no-trigger-jobs' or "
            "'--dry-run' passed.\n",
        ])
        exit_code = self.command.main(['--dry-run'])
        self.assertEqual(exit_code, 1)
        self.assertLog([
            "ERROR: Aborted: no try jobs and '--no-trigger-jobs' or "
            "'--dry-run' passed.\n",
        ])
        self.assertEqual(self.command.git_cl.calls, [])

    def test_gather_reports(self):
        self.tool.filesystem.write_text_file(
            'wptreport.json',
            json.dumps({
                'run_info': {
                    'os': 'mac',
                },
                'results': [],
            },
                       indent=2))
        self.tool.web.append_prpc_response({
            'artifacts': [{
                'artifactId':
                'wpt_reports_chrome_01.json',
                'fetchUrl':
                'https://cr.dev/123/wptreport.json?token=abc',
            }],
        })

        report_from_builder = json.dumps({
            'run_info': {
                'os': 'linux',
            },
            'results': [],
        }) + '\n'
        # Simulate a retry within a shard.
        url = 'https://cr.dev/123/wptreport.json?token=abc'
        self.tool.web.urls[url] = 2 * report_from_builder.encode()

        report1, report2, report3 = sorted(
            self.command.gather_reports(self.builds, ['wptreport.json']),
            key=lambda report: report['run_info']['os'])
        self.assertEqual(report1['run_info'], {'os': 'linux'})
        self.assertEqual(report2['run_info'], {'os': 'linux'})
        self.assertEqual(report3['run_info'], {'os': 'mac'})
        self.assertLog([
            'INFO: Processing wptrunner report (1/2)\n',
            'INFO: Processing wptrunner report (2/2)\n',
        ])

    def test_gather_reports_invalid_json(self):
        self.tool.filesystem.write_text_file('invalid.json', '{')
        with self.assertRaises(json.JSONDecodeError):
            list(self.command.gather_reports({}, ['invalid.json']))

    def test_gather_reports_no_artifacts(self):
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
