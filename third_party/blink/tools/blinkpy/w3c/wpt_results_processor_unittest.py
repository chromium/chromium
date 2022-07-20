# Copyright (c) 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import base64
import json
import re
import unittest

from blinkpy.common.host_mock import MockHost as BlinkMockHost
from blinkpy.common.system.log_testing import LoggingTestCase
from blinkpy.web_tests.port.factory_mock import MockPortFactory
from blinkpy.w3c.wpt_manifest import BASE_MANIFEST_NAME
from blinkpy.w3c.wpt_results_processor import WPTResultsProcessor
from typ.fakes.host_fake import FakeHost as TypFakeHost

# The path where the output of a wpt run was written. This is the file that
# gets processed by WPTResultsProcessor.
OUTPUT_JSON_FILENAME = "out.json"


class MockResultSink(object):
    def __init__(self):
        self.sink_requests = []
        self.invocation_level_artifacts = {}
        self.host = TypFakeHost()

    def report_individual_test_result(self, test_name_prefix, result,
                                      artifact_output_dir, expectations,
                                      test_file_location):
        assert not expectations, 'expectation parameter should always be None'
        self.sink_requests.append({
            'test_name_prefix': test_name_prefix,
            'test_path': test_file_location,
            'result': {
                'name': result.name,
                'actual': result.actual,
                'expected': result.expected,
                'unexpected': result.unexpected,
                'took': result.took,
                'flaky': result.flaky,
                'artifacts': result.artifacts,
            },
        })

    def report_invocation_level_artifacts(self, artifacts):
        self.invocation_level_artifacts.update(artifacts)


class WPTResultsProcessorTest(LoggingTestCase):
    def setUp(self):
        super(WPTResultsProcessorTest, self).setUp()

        self.host = BlinkMockHost()
        self.host.port_factory = MockPortFactory(self.host)
        self.fs = self.host.filesystem
        port = self.host.port_factory.get()
        self.wpt_report_path = self.fs.join('out', 'Default',
                                            'wpt_report.json')

        # Create a testing manifest containing any test files that we
        # might interact with.
        self.fs.write_text_file(
            self.fs.join(port.web_tests_dir(), 'external', BASE_MANIFEST_NAME),
            json.dumps({
                'items': {
                    'reftest': {
                        'reftest.html': [
                            'c3f2fb6f436da59d43aeda0a7e8a018084557033',
                            [None, [['reftest-ref.html', '==']], {}],
                        ]
                    },
                    'testharness': {
                        'test.html': [
                            'd933fd981d4a33ba82fb2b000234859bdda1494e',
                            [None, {}]
                        ],
                        'crash.html': [
                            'd933fd981d4a33ba82fb2b000234859bdda1494e',
                            [None, {}]
                        ],
                        'variant.html': [
                            'b8db5972284d1ac6bbda0da81621d9bca5d04ee7',
                            ['variant.html?foo=bar/abc', {}],
                            ['variant.html?foo=baz', {}],
                        ],
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
        self.fs.write_text_file(
            self.fs.join(port.web_tests_dir(), 'fast', 'harness',
                         'results.html'), 'results-viewer-body')
        self.fs.write_text_file(
            self.fs.join(port.web_tests_dir(), 'external', 'Version'),
            'Version: afd66ac5976672821b2788cd5f6ae57701240308\n')
        self.fs.write_text_file(
            self.wpt_report_path,
            json.dumps({
                'run_info': {
                    'os': 'linux',
                    'os_version': '18.04',
                    'product': 'chrome',
                    'revision': '57a5dfb2d7d6253fbb7dbd7c43e7588f9339f431',
                },
                'results': [],
            }))

        self.processor = WPTResultsProcessor(
            self.host,
            port=port,
            web_tests_dir=port.web_tests_dir(),
            artifacts_dir=self.fs.join('out', 'Default',
                                       'layout-test-results'),
            results_dir=self.fs.join('out', 'Default'),
            sink=MockResultSink())

    def _create_json_output(self, json_dict):
        """Writing some json output for processing."""
        self.fs.write_text_file(OUTPUT_JSON_FILENAME, json.dumps(json_dict))

    def _load_json_output(self, filename=OUTPUT_JSON_FILENAME):
        """Loads the json output after post-processing."""
        return json.loads(self.fs.read_text_file(filename))

    def test_result_sink_for_test_expected_result(self):
        json_dict = {
            'tests': {
                'fail': {
                    'test.html?variant1': {
                        'expected': 'PASS FAIL',
                        'actual': 'FAIL',
                        'artifacts': {
                            'wpt_actual_status': ['OK'],
                            'wpt_actual_metadata': ['test.html actual text'],
                        },
                    },
                },
            },
            'path_delimiter': '/',
        }
        self._create_json_output(json_dict)

        self.processor.process_wpt_results(OUTPUT_JSON_FILENAME)
        test_name = self.fs.join('external', 'wpt', 'fail',
                                 'test.html?variant1')
        test_abs_path = self.fs.join(self.processor.web_tests_dir, 'external',
                                     'wpt', 'fail', 'test.html')
        path_from_out_dir = self.fs.join('layout-test-results', 'external',
                                         'wpt', 'fail',
                                         'test_variant1-actual.txt')
        self.assertEqual(self.processor.sink.sink_requests,
                         [{
                             'test_name_prefix': '',
                             'test_path': test_abs_path,
                             'result': {
                                 'name': test_name,
                                 'actual': 'FAIL',
                                 'expected': {'PASS', 'FAIL'},
                                 'unexpected': False,
                                 'took': 0,
                                 'flaky': False,
                                 'artifacts': {
                                     'actual_text': [path_from_out_dir],
                                 },
                             },
                         }])

    def test_result_sink_for_test_variant(self):
        json_dict = {
            'tests': {
                'fail': {
                    'test.html?variant1': {
                        'expected': 'PASS',
                        'actual': 'FAIL',
                        'artifacts': {
                            'wpt_actual_status': ['OK'],
                            'wpt_actual_metadata': ['test.html actual text'],
                        },
                    },
                },
            },
            'path_delimiter': '/',
        }
        self._create_json_output(json_dict)

        self.processor.process_wpt_results(OUTPUT_JSON_FILENAME)
        test_name = self.fs.join('external', 'wpt', 'fail',
                                 'test.html?variant1')
        test_abs_path = self.fs.join(self.processor.web_tests_dir, 'external',
                                     'wpt', 'fail', 'test.html')
        path_from_out_dir = self.fs.join('layout-test-results', 'external',
                                         'wpt', 'fail',
                                         'test_variant1-actual.txt')
        self.assertEqual(self.processor.sink.sink_requests,
                         [{
                             'test_name_prefix': '',
                             'test_path': test_abs_path,
                             'result': {
                                 'name': test_name,
                                 'actual': 'FAIL',
                                 'expected': {'PASS'},
                                 'unexpected': True,
                                 'took': 0,
                                 'flaky': False,
                                 'artifacts': {
                                     'actual_text': [path_from_out_dir],
                                 },
                             },
                         }])

    def test_result_sink_with_prefix_through_metadata(self):
        """Verify that the sink uploads results with a test name prefix.

        The JSON results format allows passing arbitrary key-value data through
        the "metadata" field. Some test runners include a "test_name_prefix"
        metadata key that should be prepended to each test path in the trie.

        See Also:
            https://source.chromium.org/chromium/_/chromium/catapult.git/+/0c6b8d6722cc0e4a35b51d5104374b8cf9cc264e:third_party/typ/typ/runner.py;l=243-244
        """
        self._create_json_output({
            'tests': {
                'fail': {
                    'test.html': {
                        'expected': 'PASS',
                        'actual': 'FAIL',
                        'artifacts': {
                            'wpt_actual_status': ['OK'],
                            'wpt_actual_metadata': ['test.html actual text'],
                        },
                    },
                },
            },
            'path_delimiter': '/',
            'metadata': {
                'test_name_prefix': 'with_finch_seed',
            },
        })

        self.processor.process_wpt_results(OUTPUT_JSON_FILENAME)
        test_name = self.fs.join('external', 'wpt', 'fail', 'test.html')
        test_abs_path = self.fs.join(self.processor.web_tests_dir, 'external',
                                     'wpt', 'fail', 'test.html')
        path_from_out_dir = self.fs.join('layout-test-results', 'external',
                                         'wpt', 'fail', 'test-actual.txt')
        self.assertEqual(self.processor.sink.sink_requests,
                         [{
                             'test_name_prefix': 'with_finch_seed/',
                             'test_path': test_abs_path,
                             'result': {
                                 'name': test_name,
                                 'actual': 'FAIL',
                                 'expected': {'PASS'},
                                 'unexpected': True,
                                 'took': 0,
                                 'flaky': False,
                                 'artifacts': {
                                     'actual_text': [path_from_out_dir],
                                 },
                             },
                         }])

    def test_result_sink_for_multiple_runs(self):
        json_dict = {
            'tests': {
                'fail': {
                    'test.html': {
                        'expected': 'PASS',
                        'actual': 'PASS FAIL',
                        'times': [2, 3],
                        'artifacts': {},
                    },
                },
            },
            'path_delimiter': '/',
        }
        self._create_json_output(json_dict)

        self.processor.process_wpt_results(OUTPUT_JSON_FILENAME)
        test_name = self.fs.join('external', 'wpt', 'fail', 'test.html')
        test_abs_path = self.fs.join(self.processor.web_tests_dir, 'external',
                                     'wpt', 'fail', 'test.html')
        self.assertEqual(self.processor.sink.sink_requests,
                         [{
                             'test_name_prefix': '',
                             'test_path': test_abs_path,
                             'result': {
                                 'name': test_name,
                                 'actual': 'PASS',
                                 'expected': {'PASS'},
                                 'unexpected': False,
                                 'took': 2,
                                 'flaky': True,
                                 'artifacts': {},
                             },
                         }, {
                             'test_name_prefix': '',
                             'test_path': test_abs_path,
                             'result': {
                                 'name': test_name,
                                 'actual': 'FAIL',
                                 'expected': {'PASS'},
                                 'unexpected': True,
                                 'took': 3,
                                 'flaky': True,
                                 'artifacts': {},
                             },
                         }])

    def test_result_sink_artifacts(self):
        json_dict = {
            'tests': {
                'fail': {
                    'test.html': {
                        'expected': 'PASS',
                        'actual': 'FAIL',
                        'artifacts': {
                            'wpt_actual_status': ['OK'],
                            'wpt_actual_metadata': ['test.html actual text'],
                        },
                    },
                },
            },
            'path_delimiter': '/',
        }
        self._create_json_output(json_dict)

        self.processor.process_wpt_results(OUTPUT_JSON_FILENAME)
        test_abs_path = self.fs.join(self.processor.web_tests_dir, 'external',
                                     'wpt', 'fail', 'test.html')
        path_from_out_dir = self.fs.join('layout-test-results', 'external',
                                         'wpt', 'fail', 'test-actual.txt')
        self.assertEqual(
            self.processor.sink.sink_requests, [{
                'test_name_prefix': '',
                'test_path': test_abs_path,
                'result': {
                    'name': self.fs.join('external', 'wpt', 'fail',
                                         'test.html'),
                    'actual': 'FAIL',
                    'expected': {'PASS'},
                    'unexpected': True,
                    'took': 0,
                    'flaky': False,
                    'artifacts': {
                        'actual_text': [path_from_out_dir],
                    },
                },
            }])

    def test_write_jsons(self):
        # Ensure that various JSONs are written to the correct location.
        json_dict = {
            'tests': {
                'pass': {
                    'test.html': {
                        'expected': 'PASS',
                        'actual': 'PASS',
                        'artifacts': {
                            'wpt_actual_status': ['OK'],
                        },
                    },
                },
                'unexpected_pass.html': {
                    'expected': 'FAIL',
                    'actual': 'PASS',
                    'artifacts': {
                        'wpt_actual_status': ['OK'],
                    },
                    'is_unexpected': True,
                },
                'fail.html': {
                    'expected': 'PASS',
                    'actual': 'FAIL',
                    'artifacts': {
                        'wpt_actual_status': ['ERROR'],
                    },
                    'is_unexpected': True,
                    'is_regression': True,
                },
            },
            'path_delimiter': '/',
        }
        self._create_json_output(json_dict)

        self.processor.process_wpt_results(OUTPUT_JSON_FILENAME)
        # The correctness of the output JSON is verified by other tests.
        written_files = self.fs.written_files
        artifact_path = self.fs.join(self.processor.artifacts_dir,
                                     'full_results.json')
        self.assertEqual(written_files[OUTPUT_JSON_FILENAME],
                         written_files[artifact_path])

        # Verify JSONP
        artifact_path = self.fs.join(self.processor.artifacts_dir,
                                     'full_results_jsonp.js')
        full_results_jsonp = written_files[artifact_path].decode('utf-8')
        match = re.match(r'ADD_FULL_RESULTS\((.*)\);$', full_results_jsonp)
        self.assertIsNotNone(match)
        self.assertEqual(
            match.group(1),
            written_files[OUTPUT_JSON_FILENAME].decode(encoding='utf-8'))

        artifact_path = self.fs.join(self.processor.artifacts_dir,
                                     'failing_results.json')
        failing_results_jsonp = written_files[artifact_path].decode('utf-8')
        match = re.match(r'ADD_RESULTS\((.*)\);$', failing_results_jsonp)
        self.assertIsNotNone(match)
        failing_results = json.loads(match.group(1))
        # Verify filtering of failing_results.json
        self.assertIn('fail.html', failing_results['tests']['external']['wpt'])
        # We shouldn't have unexpected passes or empty dirs after filtering.
        self.assertNotIn('unexpected_pass.html',
                         failing_results['tests']['external']['wpt'])
        self.assertNotIn('pass', failing_results['tests']['external']['wpt'])

    def test_write_text_outputs(self):
        # Ensure that text outputs are written to the correct location.

        # We only generate an actual.txt if our actual wasn't PASS, so in this
        # case we shouldn't write anything.
        json_dict = {
            'tests': {
                'test.html': {
                    'expected': 'PASS',
                    'actual': 'PASS',
                    'artifacts': {
                        'wpt_actual_status': ['OK'],
                        'wpt_actual_metadata': ['test.html actual text'],
                    },
                },
            },
            'path_delimiter': '/',
        }
        self._create_json_output(json_dict)

        self.processor.process_wpt_results(OUTPUT_JSON_FILENAME)
        written_files = self.fs.written_files
        artifacts_subdir = self.fs.join(self.processor.artifacts_dir,
                                        'external', 'wpt')
        actual_path = self.fs.join(artifacts_subdir, 'test-actual.txt')
        diff_path = self.fs.join(artifacts_subdir, 'test-diff.txt')
        pretty_diff_path = self.fs.join(artifacts_subdir,
                                        'test-pretty-diff.html')
        self.assertNotIn(actual_path, written_files)
        self.assertNotIn(diff_path, written_files)
        self.assertNotIn(pretty_diff_path, written_files)

        # Now we change the outcome to be a FAIL, which should generate an
        # actual.txt
        json_dict['tests']['test.html']['actual'] = 'FAIL'
        self._create_json_output(json_dict)
        self.processor.process_wpt_results(OUTPUT_JSON_FILENAME)
        self.assertEqual("test.html actual text",
                         written_files[actual_path].decode(encoding='utf-8'))
        # Ensure the artifact in the json was replaced with the location of
        # the newly-created file.
        updated_json = self._load_json_output()
        test_node = updated_json['tests']['external']['wpt']['test.html']
        path_from_out_dir = self.fs.join('layout-test-results', 'external',
                                         'wpt', 'test-actual.txt')
        self.assertNotIn('wpt_actual_metadata', test_node['artifacts'])
        self.assertEqual([path_from_out_dir],
                         test_node['artifacts']['actual_text'])
        self.assertIn(actual_path, written_files)
        self.assertNotIn(diff_path, written_files)
        self.assertNotIn(pretty_diff_path, written_files)

    def test_write_log_artifact(self):
        # Ensure that crash log artifacts are written to the correct location.
        json_dict = {
            'tests': {
                'test.html': {
                    'expected': 'PASS',
                    'actual': 'FAIL',
                    'artifacts': {
                        'wpt_actual_status': ['ERROR'],
                        'wpt_log': ['test.html exceptions'],
                    },
                },
            },
            'path_delimiter': '/',
        }
        self._create_json_output(json_dict)

        self.processor.process_wpt_results(OUTPUT_JSON_FILENAME)
        written_files = self.fs.written_files
        artifacts_subdir = self.fs.join(self.processor.artifacts_dir,
                                        'external', 'wpt')
        stderr_path = self.fs.join(artifacts_subdir, 'test-stderr.txt')
        self.assertEqual(written_files[stderr_path].decode('utf-8'),
                         'test.html exceptions')

        # Ensure the artifact in the json was replaced with the location of
        # the newly-created file.
        updated_json = self._load_json_output()
        test_node = updated_json['tests']['external']['wpt']['test.html']
        self.assertNotIn('wpt_log', test_node['artifacts'])
        path_from_out_dir = self.fs.join('layout-test-results', 'external',
                                         'wpt', 'test-stderr.txt')
        self.assertEqual([path_from_out_dir], test_node['artifacts']['stderr'])
        self.assertTrue(test_node['has_stderr'])

    def test_write_crashlog_artifact(self):
        # Ensure that crash log artifacts are written to the correct location.
        json_dict = {
            'tests': {
                'test.html': {
                    'expected': 'PASS',
                    'actual': 'CRASH',
                    'artifacts': {
                        'wpt_actual_status': ['CRASH'],
                        'wpt_crash_log': ['test.html crashed!'],
                    },
                },
            },
            'path_delimiter': '/',
        }
        self._create_json_output(json_dict)

        self.processor.process_wpt_results(OUTPUT_JSON_FILENAME)
        written_files = self.fs.written_files
        artifacts_subdir = self.fs.join(self.processor.artifacts_dir,
                                        'external', 'wpt')
        crash_log_path = self.fs.join(artifacts_subdir, 'test-crash-log.txt')
        self.assertEqual(
            "test.html crashed!",
            written_files[crash_log_path].decode(encoding='utf-8'))

        # Ensure the artifact in the json was replaced with the location of
        # the newly-created file.
        updated_json = self._load_json_output()
        test_node = updated_json['tests']['external']['wpt']['test.html']
        path_from_out_dir = self.fs.join('layout-test-results', 'external',
                                         'wpt', 'test-crash-log.txt')
        self.assertNotIn('wpt_crash_log', test_node['artifacts'])
        self.assertEqual([path_from_out_dir],
                         test_node['artifacts']['crash_log'])

    def test_write_screenshot_artifacts(self):
        # Ensure that screenshots are written to the correct filenames and
        # their bytes are base64 decoded. The image diff should also be created.
        json_dict = {
            'tests': {
                'reftest.html': {
                    'expected': 'PASS',
                    'actual': 'PASS',
                    'artifacts': {
                        'wpt_actual_status': ['PASS'],
                        'screenshots': [
                            'reftest.html:abcd',
                            'reftest-ref.html:bcde',
                        ],
                    },
                },
            },
            'path_delimiter': '/',
        }
        self._create_json_output(json_dict)

        self.processor.process_wpt_results(OUTPUT_JSON_FILENAME)
        written_files = self.fs.written_files
        artifacts_subdir = self.fs.join(self.processor.artifacts_dir,
                                        'external', 'wpt')
        actual_path = self.fs.join(artifacts_subdir, 'reftest-actual.png')
        self.assertEqual(base64.b64decode('abcd'), written_files[actual_path])
        expected_path = self.fs.join(artifacts_subdir, 'reftest-expected.png')
        self.assertEqual(base64.b64decode('bcde'),
                         written_files[expected_path])
        diff_path = self.fs.join(artifacts_subdir, 'reftest-diff.png')
        self.assertEqual('\n'.join([
            '< bcde',
            '---',
            '> abcd',
        ]), written_files[diff_path])

        # Ensure the artifacts in the json were replaced with the location of
        # the newly-created files.
        updated_json = self._load_json_output()
        test_node = updated_json['tests']['external']['wpt']['reftest.html']
        path_from_out_dir_base = self.fs.join('layout-test-results',
                                              'external', 'wpt')
        self.assertNotIn('screenshots', test_node['artifacts'])
        self.assertEqual(
            [self.fs.join(path_from_out_dir_base, 'reftest-actual.png')],
            test_node['artifacts']['actual_image'])
        self.assertEqual(
            [self.fs.join(path_from_out_dir_base, 'reftest-expected.png')],
            test_node['artifacts']['expected_image'])
        self.assertEqual(
            [self.fs.join(path_from_out_dir_base, 'reftest-diff.png')],
            test_node['artifacts']['image_diff'])

    def test_copy_expected_output(self):
        # Check that an -expected.txt file is created from a checked-in metadata
        # ini file if it exists for a test
        json_dict = {
            'tests': {
                'test.html': {
                    'expected': 'PASS',
                    'actual': 'FAIL',
                    'artifacts': {
                        'wpt_actual_status': ['OK'],
                        'wpt_actual_metadata': ['test.html actual text'],
                    },
                },
            },
            'path_delimiter': '/',
        }
        self._create_json_output(json_dict)
        # Also create a checked-in metadata file for this test
        self.fs.write_text_file(
            self.fs.join(self.processor.web_tests_dir, 'external', 'wpt',
                         'test.html.ini'), 'test.html checked-in metadata')

        self.processor.process_wpt_results(OUTPUT_JSON_FILENAME)
        written_files = self.fs.written_files
        artifacts_subdir = self.fs.join(self.processor.artifacts_dir,
                                        'external', 'wpt')
        actual_path = self.fs.join(artifacts_subdir, 'test-actual.txt')
        self.assertEqual("test.html actual text",
                         written_files[actual_path].decode(encoding='utf-8'))
        # The checked-in metadata file gets renamed from .ini to -expected.txt
        expected_path = self.fs.join(artifacts_subdir, 'test-expected.txt')
        self.assertEqual("test.html checked-in metadata",
                         written_files[expected_path].decode(encoding='utf-8'))

        # Ensure the artifacts in the json were replaced with the locations of
        # the newly-created files.
        path_from_out_dir_base = self.fs.join('layout-test-results',
                                              'external', 'wpt')
        updated_json = self._load_json_output()
        test_node = updated_json['tests']['external']['wpt']['test.html']
        self.assertNotIn('wpt_actual_metadata', test_node['artifacts'])
        self.assertEqual(
            [self.fs.join(path_from_out_dir_base, 'test-actual.txt')],
            test_node['artifacts']['actual_text'])
        self.assertEqual(
            [self.fs.join(path_from_out_dir_base, 'test-expected.txt')],
            test_node['artifacts']['expected_text'])

        # Ensure that a diff was also generated. There should be both additions
        # and deletions for this test since we have expected output. We don't
        # validate the entire diff files to avoid checking line numbers/markup.
        diff_path = self.fs.join(artifacts_subdir, 'test-diff.txt')
        self.assertIn('-test.html checked-in metadata',
                      written_files[diff_path].decode(encoding='utf-8'))
        self.assertIn('+test.html actual text',
                      written_files[diff_path].decode(encoding='utf-8'))
        self.assertEqual(
            [self.fs.join(path_from_out_dir_base, 'test-diff.txt')],
            test_node['artifacts']['text_diff'])
        pretty_diff_path = self.fs.join(artifacts_subdir,
                                        'test-pretty-diff.html')
        self.assertIn("test.html checked-in metadata",
                      written_files[pretty_diff_path].decode(encoding='utf-8'))
        self.assertIn("test.html actual text",
                      written_files[pretty_diff_path].decode(encoding='utf-8'))
        self.assertEqual(
            [self.fs.join(path_from_out_dir_base, 'test-pretty-diff.html')],
            test_node['artifacts']['pretty_text_diff'])

    def test_expected_output_for_variant(self):
        # Check that an -expected.txt file is created from a checked-in metadata
        # ini file for a variant test. Variants are a little different because
        # we have to use the manifest to map a test name to the test file, and
        # determine the associated metadata from the test file.
        # Check that an -expected.txt file is created from a checked-in metadata
        # ini file if it exists for a test
        json_dict = {
            'tests': {
                'variant.html?foo=bar/abc': {
                    'expected': 'PASS',
                    'actual': 'FAIL',
                    'artifacts': {
                        'wpt_actual_status': ['OK'],
                        'wpt_actual_metadata': ['variant bar/abc actual text'],
                    },
                },
            },
            'path_delimiter': '/',
        }
        self._create_json_output(json_dict)
        # Also create a checked-in metadata file for this test. This filename
        # matches the test *file* name, not the test name (which includes the
        # variant).
        self.fs.write_text_file(
            self.fs.join(self.processor.web_tests_dir, 'external', 'wpt',
                         'variant.html.ini'),
            "variant.html checked-in metadata")

        self.processor.process_wpt_results(OUTPUT_JSON_FILENAME)
        written_files = self.fs.written_files
        artifacts_subdir = self.fs.join(self.processor.artifacts_dir,
                                        'external', 'wpt')
        actual_path = self.fs.join(artifacts_subdir,
                                   "variant_foo=bar_abc-actual.txt")
        self.assertEqual("variant bar/abc actual text",
                         written_files[actual_path].decode(encoding='utf-8'))
        # The checked-in metadata file gets renamed from .ini to -expected.txt
        expected_path = self.fs.join(artifacts_subdir,
                                     "variant_foo=bar_abc-expected.txt")
        self.assertEqual("variant.html checked-in metadata",
                         written_files[expected_path].decode(encoding='utf-8'))

        # Ensure the artifacts in the json were replaced with the locations of
        # the newly-created files.
        updated_json = self._load_json_output()
        test_node_parent = updated_json['tests']['external']['wpt']
        test_node = test_node_parent['variant.html?foo=bar/abc']
        path_from_out_dir_base = self.fs.join('layout-test-results',
                                              'external', 'wpt')
        actual_path = self.fs.join(path_from_out_dir_base,
                                   'variant_foo=bar_abc-actual.txt')
        expected_path = self.fs.join(path_from_out_dir_base,
                                     'variant_foo=bar_abc-expected.txt')
        self.assertNotIn('wpt_actual_metadata', test_node['artifacts'])
        self.assertEqual([actual_path], test_node['artifacts']['actual_text'])
        self.assertEqual([expected_path],
                         test_node['artifacts']['expected_text'])

    def test_expected_output_for_multiglob(self):
        # Check that an -expected.txt file is created from a checked-in metadata
        # ini file for a multiglobal test. Multi-globals are a little different
        # because we have to use the manifest to map a test name to the test
        # file, and determine the associated metadata from the test file.
        #
        # Also note that the "dir" is both a directory and a part of the test
        # name, so the path delimiter remains a / (ie: dir/multiglob) even on
        # Windows.
        json_dict = {
            'tests': {
                'dir/multiglob.https.any.worker.html': {
                    'expected': 'PASS',
                    'actual': 'FAIL',
                    'artifacts': {
                        'wpt_actual_status': ['OK'],
                        'wpt_actual_metadata': [
                            'dir/multiglob worker actual text',
                        ],
                    },
                },
            },
            'path_delimiter': '/',
        }
        self._create_json_output(json_dict)
        # Also create a checked-in metadata file for this test. This filename
        # matches the test *file* name, not the test name (which includes test
        # scope).
        self.fs.write_text_file(
            self.fs.join(self.processor.web_tests_dir, 'external', 'wpt',
                         'dir/multiglob.https.any.js.ini'),
            "dir/multiglob checked-in metadata")

        self.processor.process_wpt_results(OUTPUT_JSON_FILENAME)
        written_files = self.fs.written_files
        artifacts_subdir = self.fs.join(self.processor.artifacts_dir,
                                        'external', 'wpt')
        actual_path = self.fs.join(
            artifacts_subdir, 'dir/multiglob.https.any.worker-actual.txt')
        self.assertEqual("dir/multiglob worker actual text",
                         written_files[actual_path].decode(encoding='utf-8'))
        # The checked-in metadata file gets renamed from .ini to -expected.txt
        expected_path = self.fs.join(
            artifacts_subdir, 'dir/multiglob.https.any.worker-expected.txt')
        self.assertEqual("dir/multiglob checked-in metadata",
                         written_files[expected_path].decode(encoding='utf-8'))

        # Ensure the artifacts in the json were replaced with the locations of
        # the newly-created files.
        updated_json = self._load_json_output()
        test_node_parent = updated_json['tests']['external']['wpt']
        test_node = test_node_parent['dir/multiglob.https.any.worker.html']
        path_from_out_dir_base = self.fs.join('layout-test-results',
                                              'external', 'wpt')
        self.assertNotIn('wpt_actual_metadata', test_node['artifacts'])
        self.assertEqual([
            self.fs.join(path_from_out_dir_base,
                         'dir/multiglob.https.any.worker-actual.txt')
        ], test_node['artifacts']['actual_text'])
        self.assertEqual([
            self.fs.join(path_from_out_dir_base,
                         'dir/multiglob.https.any.worker-expected.txt')
        ], test_node['artifacts']['expected_text'])

    def test_process_wpt_report(self):
        output_path = self.processor.process_wpt_report(self.wpt_report_path)
        self.assertEqual(self.fs.dirname(output_path),
                         self.processor.artifacts_dir)
        report = json.loads(self.fs.read_text_file(output_path))
        run_info = report['run_info']
        self.assertEqual(run_info['os'], 'linux')
        self.assertEqual(run_info['os_version'], '18.04')
        self.assertEqual(run_info['product'], 'chrome')
        self.assertEqual(run_info['revision'],
                         'afd66ac5976672821b2788cd5f6ae57701240308')
        artifacts = self.processor.sink.invocation_level_artifacts
        artifact_path = self.fs.join(self.processor.artifacts_dir,
                                     'wpt_report.json')
        self.assertIn('wpt_report.json', artifacts)
        self.assertEqual(artifacts['wpt_report.json'],
                         {'filePath': artifact_path})
