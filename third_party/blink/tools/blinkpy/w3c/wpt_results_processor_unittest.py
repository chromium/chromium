# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import base64
import json
import re
import textwrap

from blinkpy.common.host_mock import MockHost as BlinkMockHost
from blinkpy.common.path_finder import PathFinder
from blinkpy.common.system.log_testing import LoggingTestCase
from blinkpy.web_tests.port.factory_mock import MockPortFactory
from blinkpy.w3c.wpt_manifest import BASE_MANIFEST_NAME
from blinkpy.w3c.wpt_results_processor import WPTResultsProcessor

# The path where the output of a wpt run was written. This is the file that
# gets processed by WPTResultsProcessor.
OUTPUT_JSON_FILENAME = "out.json"


class MockResultSink(object):
    def __init__(self, host):
        self.sink_requests = []
        self.invocation_level_artifacts = {}
        self.host = host

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
        self.path_finder = PathFinder(self.fs)
        port = self.host.port_factory.get()
        # `MockFileSystem` and `TestPort` use different web test directory
        # placements to test nonstandard layouts. That is not a goal of this
        # test case, so we settle on the former for consistency.
        port.web_tests_dir = self.path_finder.web_tests_dir

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
                            ['variant.html?foo=bar', {}],
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
            self.path_finder.path_from_blink_tools('blinkpy', 'web_tests',
                                                   'results.html'),
            'results-viewer-body')
        self.wpt_report = {
            'run_info': {
                'os': 'linux',
                'version': '18.04',
                'product': 'chrome',
                'revision': '57a5dfb2d7d6253fbb7dbd7c43e7588f9339f431',
                'used_upstream': True,
            },
            'results': [{
                'test':
                '/a/b.html',
                'subtests': [{
                    'name': 'subtest',
                    'status': 'FAIL',
                    'message': 'remove this message',
                    'expected': 'PASS',
                    'known_intermittent': [],
                }],
                'status':
                'OK',
                'expected':
                'OK',
                'message':
                'remove this message from the compact version',
                'duration':
                1000,
                'known_intermittent': ['CRASH'],
            }],
        }

        self.processor = WPTResultsProcessor(
            self.host,
            port=port,
            web_tests_dir=port.web_tests_dir(),
            artifacts_dir=self.fs.join('/mock-checkout', 'out', 'Default',
                                       'layout-test-results'),
            results_dir=self.fs.join('/mock-checkout', 'out', 'Default'),
            sink=MockResultSink(port.typ_host()))

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
                'wpt_internal': {
                    'fail': {
                        'test.html?variant1': {
                            'expected': 'PASS FAIL',
                            'actual': 'FAIL',
                            'artifacts': {
                                'wpt_actual_status': ['OK'],
                                'wpt_actual_metadata':
                                ['test.html actual text'],
                            },
                        },
                    },
                },
            },
            'path_delimiter': '/',
        }
        self._create_json_output(json_dict)

        self.processor.process_wpt_results(OUTPUT_JSON_FILENAME)
        test_name = self.fs.join('fail', 'test.html?variant1')
        test_abs_path = self.fs.join(self.processor.web_tests_dir, 'external',
                                     'wpt', 'fail', 'test.html')
        path_from_out_dir = self.fs.join('layout-test-results', 'fail',
                                         'test_variant1-actual.txt')
        internal_test_name = self.fs.join('wpt_internal', 'fail',
                                          'test.html?variant1')
        internal_test_abs_path = self.fs.join(self.processor.web_tests_dir,
                                              'wpt_internal', 'fail',
                                              'test.html')
        internal_path_from_out_dir = self.fs.join('layout-test-results',
                                                  'wpt_internal', 'fail',
                                                  'test_variant1-actual.txt')
        self.assertEqual(
            self.processor.sink.sink_requests, [{
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
            }, {
                'test_name_prefix': '',
                'test_path': internal_test_abs_path,
                'result': {
                    'name': internal_test_name,
                    'actual': 'FAIL',
                    'expected': {'PASS', 'FAIL'},
                    'unexpected': False,
                    'took': 0,
                    'flaky': False,
                    'artifacts': {
                        'actual_text': [internal_path_from_out_dir],
                    },
                },
            }])

    def test_result_sink_for_test_variant(self):
        json_dict = {
            'tests': {
                'fail': {
                    'test.html?variant1': {
                        'expected': 'TIMEOUT',
                        'actual': 'TIMEOUT',
                        'artifacts': {
                            'wpt_actual_status': ['TIMEOUT'],
                            'wpt_actual_metadata': ['test.html actual text'],
                        },
                        'time': 1000,
                        'times': [1000],
                    },
                },
            },
            'path_delimiter': '/',
        }
        self._create_json_output(json_dict)

        self.processor.process_wpt_results(OUTPUT_JSON_FILENAME)
        test_name = self.fs.join('fail', 'test.html?variant1')
        test_abs_path = self.fs.join(self.processor.web_tests_dir, 'external',
                                     'wpt', 'fail', 'test.html')
        path_from_out_dir = self.fs.join('layout-test-results', 'fail',
                                         'test_variant1-actual.txt')
        self.assertEqual(self.processor.sink.sink_requests,
                         [{
                             'test_name_prefix': '',
                             'test_path': test_abs_path,
                             'result': {
                                 'name': test_name,
                                 'actual': 'TIMEOUT',
                                 'expected': {'TIMEOUT'},
                                 'unexpected': False,
                                 'took': 1000,
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
        test_name = self.fs.join('fail', 'test.html')
        test_abs_path = self.fs.join(self.processor.web_tests_dir, 'external',
                                     'wpt', 'fail', 'test.html')
        path_from_out_dir = self.fs.join('layout-test-results', 'fail',
                                         'test-actual.txt')
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

    def test_result_sink_with_sanitizer(self):
        self._create_json_output({
            'tests': {
                'fail': {
                    'test.html': {
                        'expected': 'PASS',
                        'actual': 'FAIL',
                    },
                },
            },
            'path_delimiter': '/',
        })
        self.processor.run_info['sanitizer_enabled'] = True
        self.processor.process_wpt_results(OUTPUT_JSON_FILENAME)
        (request, ) = self.processor.sink.sink_requests
        self.assertEqual(request['result']['actual'], 'PASS')
        self.assertEqual(request['result']['expected'], {'PASS'})
        self.assertEqual(request['result']['unexpected'], False)

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
        test_name = self.fs.join('fail', 'test.html')
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
        path_from_out_dir = self.fs.join('layout-test-results', 'fail',
                                         'test-actual.txt')
        self.assertEqual(self.processor.sink.sink_requests,
                         [{
                             'test_name_prefix': '',
                             'test_path': test_abs_path,
                             'result': {
                                 'name': self.fs.join('fail', 'test.html'),
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
        full_results_jsonp = self.fs.read_text_file(artifact_path)
        match = re.match(r'ADD_FULL_RESULTS\((.*)\);$', full_results_jsonp)
        self.assertIsNotNone(match)
        self.assertEqual(match.group(1),
                         self.fs.read_text_file(OUTPUT_JSON_FILENAME))

        artifact_path = self.fs.join(self.processor.artifacts_dir,
                                     'failing_results.json')
        failing_results_jsonp = self.fs.read_text_file(artifact_path)
        match = re.match(r'ADD_RESULTS\((.*)\);$', failing_results_jsonp)
        self.assertIsNotNone(match)
        failing_results = json.loads(match.group(1))
        # Verify filtering of failing_results.json
        self.assertIn('fail.html', failing_results['tests'])
        # We shouldn't have unexpected passes or empty dirs after filtering.
        self.assertNotIn('unexpected_pass.html', failing_results['tests'])
        self.assertNotIn('pass', failing_results['tests'])

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
        artifacts_subdir = self.fs.join(self.processor.artifacts_dir)
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
        self.assertEqual('test.html actual text',
                         self.fs.read_text_file(actual_path))
        # Ensure the artifact in the json was replaced with the location of
        # the newly-created file.
        updated_json = self._load_json_output()
        test_node = updated_json['tests']['test.html']
        path_from_out_dir = self.fs.join('layout-test-results',
                                         'test-actual.txt')
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
        artifacts_subdir = self.fs.join(self.processor.artifacts_dir)
        stderr_path = self.fs.join(artifacts_subdir, 'test-stderr.txt')
        self.assertEqual('test.html exceptions',
                         self.fs.read_text_file(stderr_path))

        # Ensure the artifact in the json was replaced with the location of
        # the newly-created file.
        updated_json = self._load_json_output()
        test_node = updated_json['tests']['test.html']
        self.assertNotIn('wpt_log', test_node['artifacts'])
        path_from_out_dir = self.fs.join('layout-test-results',
                                         'test-stderr.txt')
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
        artifacts_subdir = self.fs.join(self.processor.artifacts_dir)
        crash_log_path = self.fs.join(artifacts_subdir, 'test-crash-log.txt')
        self.assertEqual('test.html crashed!',
                         self.fs.read_text_file(crash_log_path))

        # Ensure the artifact in the json was replaced with the location of
        # the newly-created file.
        updated_json = self._load_json_output()
        test_node = updated_json['tests']['test.html']
        path_from_out_dir = self.fs.join('layout-test-results',
                                         'test-crash-log.txt')
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
        artifacts_subdir = self.fs.join(self.processor.artifacts_dir)
        actual_path = self.fs.join(artifacts_subdir, 'reftest-actual.png')
        self.assertEqual(base64.b64decode('abcd'),
                         self.fs.read_binary_file(actual_path))
        expected_path = self.fs.join(artifacts_subdir, 'reftest-expected.png')
        self.assertEqual(base64.b64decode('bcde'),
                         self.fs.read_binary_file(expected_path))
        diff_path = self.fs.join(artifacts_subdir, 'reftest-diff.png')
        self.assertEqual('\n'.join([
            '< bcde',
            '---',
            '> abcd',
        ]), self.fs.read_binary_file(diff_path))

        # Ensure the artifacts in the json were replaced with the location of
        # the newly-created files.
        updated_json = self._load_json_output()
        test_node = updated_json['tests']['reftest.html']
        path_from_out_dir_base = self.fs.join('layout-test-results')
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
        self.assertEqual({
            'maxDifference': 100,
            'maxPixels': 3
        }, test_node['image_diff_stats'])

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
                        'wpt_actual_metadata': [
                            '[test.html]\n  expected: OK\n',
                            '  [Assert something]\n    expected: CRASH\n',
                        ],
                    },
                },
            },
            'path_delimiter': '/',
        }
        self._create_json_output(json_dict)
        # Also create a checked-in metadata file for this test
        checked_in_metadata = textwrap.dedent("""\
            [test.html]
              expected:
                if flag_specific != "highdpi": OK
              [Assert something]
                expected:
                  if flag_specific != "highdpi": PASS
                  [FAIL, TIMEOUT]
            """)
        self.fs.write_text_file(
            self.fs.join(self.processor.web_tests_dir, 'external', 'wpt',
                         'test.html.ini'), checked_in_metadata)

        self.processor.run_info['flag_specific'] = 'highdpi'
        with self.fs.patch_builtins():
            self.processor.process_wpt_results(OUTPUT_JSON_FILENAME)
        artifacts_subdir = self.fs.join(self.processor.artifacts_dir)
        actual_path = self.fs.join(artifacts_subdir, 'test-actual.txt')
        self.assertEqual(
            textwrap.dedent("""\
                [test.html]
                  expected: OK

                  [Assert something]
                    expected: CRASH
                """), self.fs.read_text_file(actual_path))

        # The checked-in metadata file gets renamed from .ini to -expected.txt.
        # Any conditions are also evaluated against the test run's properties.
        expected_path = self.fs.join(artifacts_subdir, 'test-expected.txt')
        self.assertEqual(
            textwrap.dedent("""\
                [test.html]
                  [Assert something]
                    expected: [FAIL, TIMEOUT]
                """), self.fs.read_text_file(expected_path))

        # Ensure the artifacts in the json were replaced with the locations of
        # the newly-created files.
        path_from_out_dir_base = self.fs.join('layout-test-results')
        updated_json = self._load_json_output()
        test_node = updated_json['tests']['test.html']
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
        diff_lines = self.fs.read_text_file(
            self.fs.join(artifacts_subdir, 'test-diff.txt')).splitlines()
        self.assertIn('-    expected: [FAIL, TIMEOUT]', diff_lines)
        self.assertIn('+    expected: CRASH', diff_lines)
        self.assertEqual(
            [self.fs.join(path_from_out_dir_base, 'test-diff.txt')],
            test_node['artifacts']['text_diff'])

        pretty_diff_contents = self.fs.read_text_file(
            self.fs.join(artifacts_subdir, 'test-pretty-diff.html'))
        self.assertIn('expected: [FAIL, TIMEOUT]', pretty_diff_contents)
        self.assertIn('expected: CRASH', pretty_diff_contents)
        self.assertEqual(
            [self.fs.join(path_from_out_dir_base, 'test-pretty-diff.html')],
            test_node['artifacts']['pretty_text_diff'])

    def test_invalid_checked_in_metadata(self):
        """Verify the processor handles a syntactically invalid metadata file:
          * The tool does not crash.
          * The actual text is created, but the expected text and diffs are not.
        """
        json_dict = {
            'tests': {
                'test.html': {
                    'expected': 'PASS',
                    'actual': 'FAIL',
                    'artifacts': {
                        'wpt_actual_status': ['OK'],
                        'wpt_actual_metadata': [
                            '[test.html]\n  expected: OK\n',
                            '  [Assert something]\n    expected: CRASH\n',
                        ],
                    },
                },
            },
            'path_delimiter': '/',
        }
        self._create_json_output(json_dict)
        # Also create a checked-in metadata file for this test
        self.fs.write_text_file(
            self.fs.join(self.processor.web_tests_dir, 'external', 'wpt',
                         'test.html.ini'),
            textwrap.dedent("""\
                [test.html]
                  [bracket is not matched
                """))

        with self.fs.patch_builtins():
            self.processor.process_wpt_results(OUTPUT_JSON_FILENAME)

        path_from_out_dir_base = self.fs.join('layout-test-results')
        updated_json = self._load_json_output()
        test_node = updated_json['tests']['test.html']
        self.assertNotIn('wpt_actual_metadata', test_node['artifacts'])
        self.assertNotIn('expected_text', test_node['artifacts'])
        self.assertNotIn('text_diff', test_node['artifacts'])
        self.assertNotIn('pretty_text_diff', test_node['artifacts'])
        self.assertEqual(
            [self.fs.join(path_from_out_dir_base, 'test-actual.txt')],
            test_node['artifacts']['actual_text'])

    def test_expected_output_for_variant(self):
        # Check that an -expected.txt file is created from a checked-in metadata
        # ini file for a variant test. Variants are a little different because
        # we have to use the manifest to map a test name to the test file, and
        # determine the associated metadata from the test file.
        # Check that an -expected.txt file is created from a checked-in metadata
        # ini file if it exists for a test
        json_dict = {
            'tests': {
                'variant.html?foo=bar': {
                    'expected': 'PASS',
                    'actual': 'FAIL',
                    'artifacts': {
                        'wpt_actual_status': ['OK'],
                        'wpt_actual_metadata': [
                            '[variant.html?foo=bar]\n  expected: OK\n',
                        ],
                    },
                },
            },
            'path_delimiter': '/',
        }
        self._create_json_output(json_dict)
        # Also create a checked-in metadata file for this test. This filename
        # matches the test *file* name, not the test ID. The checked-in file
        # contains expectations for all variants.
        self.fs.write_text_file(
            self.fs.join(self.processor.web_tests_dir, 'external', 'wpt',
                         'variant.html.ini'),
            textwrap.dedent("""\
                [variant.html?foo=bar]
                  expected: OK

                [variant.html?foo=baz]
                  expected: TIMEOUT
                """))
        with self.fs.patch_builtins():
            self.processor.process_wpt_results(OUTPUT_JSON_FILENAME)
        variant_metadata = textwrap.dedent("""\
            [variant.html?foo=bar]
              expected: OK
            """)
        artifacts_subdir = self.fs.join(self.processor.artifacts_dir)
        actual_path = self.fs.join(artifacts_subdir,
                                   'variant_foo=bar-actual.txt')
        self.assertEqual(variant_metadata, self.fs.read_text_file(actual_path))
        # The checked-in metadata file gets renamed from .ini to -expected.txt
        expected_path = self.fs.join(artifacts_subdir,
                                     'variant_foo=bar-expected.txt')
        # Exclude the `foo=baz` variant from the expected text.
        self.assertEqual(variant_metadata,
                         self.fs.read_text_file(expected_path))

        # Ensure the artifacts in the json were replaced with the locations of
        # the newly-created files.
        updated_json = self._load_json_output()
        test_node_parent = updated_json['tests']
        test_node = test_node_parent['variant.html?foo=bar']
        path_from_out_dir_base = self.fs.join('layout-test-results')
        actual_path = self.fs.join(path_from_out_dir_base,
                                   'variant_foo=bar-actual.txt')
        expected_path = self.fs.join(path_from_out_dir_base,
                                     'variant_foo=bar-expected.txt')
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
                            '[multiglob.https.any.worker.html]\n'
                            '  expected: OK\n',
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
            textwrap.dedent("""\
                [multiglob.https.any.worker.html]
                  expected: OK

                [multiglob.https.any.window.html]
                  expected: FAIL
                """))

        with self.fs.patch_builtins():
            self.processor.process_wpt_results(OUTPUT_JSON_FILENAME)
        artifacts_subdir = self.fs.join(self.processor.artifacts_dir)
        actual_path = self.fs.join(
            artifacts_subdir, 'dir/multiglob.https.any.worker-actual.txt')
        self.assertEqual(
            textwrap.dedent("""\
                [multiglob.https.any.worker.html]
                  expected: OK
                """), self.fs.read_text_file(actual_path))
        # The checked-in metadata file gets renamed from .ini to -expected.txt
        # and cut to the `worker` scope.
        expected_path = self.fs.join(
            artifacts_subdir, 'dir/multiglob.https.any.worker-expected.txt')
        self.assertEqual(
            textwrap.dedent("""\
                [multiglob.https.any.worker.html]
                  expected: OK
                """), self.fs.read_text_file(expected_path))

        # Ensure the artifacts in the json were replaced with the locations of
        # the newly-created files.
        updated_json = self._load_json_output()
        test_node_parent = updated_json['tests']
        test_node = test_node_parent['dir/multiglob.https.any.worker.html']
        path_from_out_dir_base = self.fs.join('layout-test-results')
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
        report_src = self.fs.join('/mock-checkout', 'out', 'Default',
                                  'wpt_report.json')
        self.fs.write_text_file(report_src,
                                (json.dumps(self.wpt_report) + '\n') * 2)
        self.processor.process_wpt_report(report_src)
        artifacts = self.processor.sink.invocation_level_artifacts
        report_dest = self.fs.join('/mock-checkout', 'out', 'Default',
                                   'layout-test-results', 'wpt_report.json')
        self.assertEqual(artifacts['wpt_report.json'], {
            'filePath': report_dest,
        })
        report = json.loads(self.fs.read_text_file(report_dest))
        self.assertEqual(report['run_info'], self.wpt_report['run_info'])
        self.assertEqual(report['results'], self.wpt_report['results'] * 2)

    def test_process_wpt_report_compact(self):
        report_src = self.fs.join('/mock-checkout', 'out', 'Default',
                                  'wpt_report.json')
        self.wpt_report['run_info']['used_upstream'] = False
        self.fs.write_text_file(report_src, json.dumps(self.wpt_report))
        self.processor.process_wpt_report(report_src)
        artifacts = self.processor.sink.invocation_level_artifacts
        report_dest = self.fs.join('/mock-checkout', 'out', 'Default',
                                   'layout-test-results', 'wpt_report.json')
        self.assertEqual(artifacts['wpt_report.json'], {
            'filePath': report_dest,
        })
        report = json.loads(self.fs.read_text_file(report_dest))
        self.assertEqual(
            report['run_info'], {
                'os': 'linux',
                'version': '18.04',
                'product': 'chrome',
                'revision': '57a5dfb2d7d6253fbb7dbd7c43e7588f9339f431',
                'used_upstream': False,
            })
        self.assertEqual(report['results'], [{
            'test':
            '/a/b.html',
            'subtests': [{
                'name': 'subtest',
                'status': 'FAIL',
                'expected': 'PASS',
            }],
            'status':
            'OK',
            'known_intermittent': ['CRASH'],
        }])
