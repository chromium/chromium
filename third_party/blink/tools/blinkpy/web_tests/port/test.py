# Copyright (C) 2010 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the Google name nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import base64
import json
import time

from blinkpy.common import exit_codes
from blinkpy.common.memoized import memoized
from blinkpy.common.system.crash_logs import CrashLogs
from blinkpy.web_tests.models.test_configuration import TestConfiguration
from blinkpy.web_tests.models.typ_types import FileSystemForwardingTypHost
from blinkpy.web_tests.port.base import Port, VirtualTestSuite
from blinkpy.web_tests.port.driver import DeviceFailure, Driver, DriverOutput
from blinkpy.w3c.wpt_manifest import BASE_MANIFEST_NAME

MOCK_ROOT = '/mock-checkout/'
MOCK_WEB_TESTS = MOCK_ROOT + 'third_party/blink/web_tests/'
PERF_TEST_DIR = MOCK_ROOT + 'PerformanceTests'


# This sets basic expectations for a test. Each individual expectation
# can be overridden by a keyword argument in TestList.add().
class TestInstance(object):
    def __init__(self, name):
        self.name = name
        self.base = name[(name.rfind('/') + 1):name.rfind('.')]
        self.crash = False
        self.web_process_crash = False
        self.exception = False
        self.keyboard = False
        self.error = ''
        self.timeout = False
        self.device_failure = False
        self.leak = False

        # The values of each field are treated as raw byte strings. They
        # will be converted to unicode strings where appropriate using
        # FileSystem.read_text_file().
        self.actual_text = self.base + '-txt'
        self.actual_checksum = self.base + '-checksum'

        # We add the '\x8a' for the image file to prevent the value from
        # being treated as UTF-8 (the character is invalid)
        self.actual_image = (self.base.encode('utf8') + b'\x8a' + b'-png' +
                             b'tEXtchecksum\x00' +
                             self.actual_checksum.encode('utf8'))

        self.expected_text = self.actual_text
        self.expected_image = self.actual_image

        self.actual_audio = None
        self.expected_audio = None


# This is an in-memory list of tests, what we want them to produce, and
# what we want to claim are the expected results.
class TestList(object):
    def __init__(self):
        self.tests = {}

    def add(self, name, **kwargs):
        test = TestInstance(name)
        for key, value in kwargs.items():
            test.__dict__[key] = value
        self.tests[name] = test

    def add_reference(self,
                      name,
                      actual_checksum='checksum',
                      actual_image=b'FAIL'):
        self.add(
            name,
            actual_checksum=actual_checksum,
            actual_image=actual_image,
            actual_text=None,
            expected_text=None,
            expected_image=None)

    def add_reftest(self,
                    name,
                    reference_name,
                    same_image=True,
                    actual_text=None,
                    expected_text=None,
                    crash=False,
                    error=b''):
        self.add(name,
                 actual_checksum='checksum',
                 actual_image=b'FAIL',
                 expected_image=None,
                 actual_text=actual_text,
                 expected_text=expected_text,
                 crash=crash,
                 error=error)
        if same_image:
            self.add_reference(reference_name)
        else:
            self.add_reference(reference_name,
                               actual_checksum='diff',
                               actual_image=b'DIFF')

    def keys(self):
        return self.tests.keys()

    def __contains__(self, item):
        return item in self.tests

    def __getitem__(self, item):
        return self.tests[item]


#
# These numbers may need to be updated whenever we add or delete tests. This includes virtual tests.
#
TOTAL_TESTS = 176
TOTAL_WONTFIX = 3
TOTAL_SKIPS = 26 + TOTAL_WONTFIX
TOTAL_CRASHES = 78

UNEXPECTED_PASSES = 2
UNEXPECTED_NON_VIRTUAL_FAILURES = 34
UNEXPECTED_FAILURES = 67


def unit_test_list():
    tests = TestList()
    tests.add('failures/expected/crash.html', crash=True)
    tests.add('failures/expected/exception.html', exception=True)
    tests.add('failures/expected/device_failure.html', device_failure=True)
    tests.add('failures/expected/timeout.html', timeout=True)
    tests.add('failures/expected/leak.html', leak=True)
    tests.add('failures/expected/image.html',
              actual_image=b'image_fail-pngtEXtchecksum\x00checksum_fail',
              expected_image=b'image-pngtEXtchecksum\x00checksum-png')
    tests.add('failures/expected/image_checksum.html',
              actual_checksum='image_checksum_fail-checksum',
              actual_image=b'image_checksum_fail-png')
    tests.add('failures/expected/audio.html',
              actual_audio=base64.b64encode(b'audio_fail-wav'),
              expected_audio=b'audio-wav',
              actual_text=None,
              expected_text=None,
              actual_image=None,
              expected_image=None,
              actual_checksum=None)
    tests.add('failures/unexpected/image-mismatch.html',
              actual_image=b'image_fail-pngtEXtchecksum\x00checksum_fail',
              expected_image=b'image-pngtEXtchecksum\x00checksum-png')
    tests.add('failures/unexpected/no-image-generated.html',
              expected_image=b'image-pngtEXtchecksum\x00checksum-png',
              actual_image=None,
              actual_checksum=None)
    tests.add('failures/unexpected/no-image-baseline.html',
              actual_image=b'image_fail-pngtEXtchecksum\x00checksum_fail',
              expected_image=None)
    tests.add('failures/unexpected/audio-mismatch.html',
              actual_audio=base64.b64encode(b'audio_fail-wav'),
              expected_audio=b'audio-wav',
              actual_text=None,
              expected_text=None,
              actual_image=None,
              expected_image=None,
              actual_checksum=None)
    tests.add('failures/unexpected/no-audio-baseline.html',
              actual_audio=base64.b64encode(b'audio_fail-wav'),
              actual_text=None,
              expected_text=None,
              actual_image=None,
              expected_image=None,
              actual_checksum=None)
    tests.add('failures/unexpected/no-audio-generated.html',
              expected_audio=base64.b64encode(b'audio_fail-wav'),
              actual_text=None,
              expected_text=None,
              actual_image=None,
              expected_image=None,
              actual_checksum=None)
    tests.add(
        'failures/unexpected/text-mismatch-overlay.html',
        actual_text='"invalidations": [\nfail',
        expected_text='"invalidations": [\npass')
    tests.add(
        'failures/unexpected/no-text-baseline.html',
        actual_text='"invalidations": [\nfail',
        expected_text=None)
    tests.add(
        'failures/unexpected/no-text-generated.html',
        actual_text=None,
        expected_text='"invalidations": [\npass')
    tests.add('failures/expected/keyboard.html', keyboard=True)
    tests.add(
        'failures/expected/newlines_leading.html',
        expected_text='\nfoo\n',
        actual_text='foo\n')
    tests.add(
        'failures/expected/newlines_trailing.html',
        expected_text='foo\n\n',
        actual_text='foo\n')
    tests.add(
        'failures/expected/newlines_with_excess_CR.html',
        expected_text='foo\r\r\r\n',
        actual_text='foo\n')
    tests.add('failures/expected/text.html', actual_text='text_fail-png')
    tests.add('failures/expected/crash_then_text.html')
    tests.add('failures/expected/skip_text.html', actual_text='text diff')
    tests.add('failures/flaky/text.html')
    tests.add('failures/unexpected/*/text.html', actual_text='text_fail-png')
    tests.add('failures/unexpected/missing_text.html', expected_text=None)
    tests.add('failures/unexpected/missing_check.html',
              expected_image=b'missing-check-png')
    tests.add('failures/unexpected/missing_image.html', expected_image=None)
    tests.add(
        'failures/unexpected/missing_render_tree_dump.html',
        actual_text="""layer at (0,0) size 800x600
  RenderView at (0,0) size 800x600
layer at (0,0) size 800x34
  RenderBlock {HTML} at (0,0) size 800x34
    RenderBody {BODY} at (8,8) size 784x18
      RenderText {#text} at (0,0) size 133x18
        text run at (0,0) width 133: "This is an image test!"
""",
        expected_text=None)
    tests.add('failures/unexpected/crash.html', crash=True)
    tests.add('failures/unexpected/crash-with-sample.html', crash=True)
    tests.add('failures/unexpected/crash-with-delayed-log.html', crash=True)
    tests.add('failures/unexpected/crash-with-stderr.html',
              crash=True,
              error=b'mock-std-error-output')
    tests.add('failures/unexpected/web-process-crash-with-stderr.html',
              web_process_crash=True,
              error=b'mock-std-error-output')
    tests.add('failures/unexpected/pass.html')
    tests.add(
        'failures/unexpected/text-checksum.html',
        actual_text='text-checksum_fail-txt',
        actual_checksum='text-checksum_fail-checksum')
    tests.add('failures/unexpected/text-image-checksum.html',
              actual_text='text-image-checksum_fail-txt',
              actual_image=
              b'text-image-checksum_fail-pngtEXtchecksum\x00checksum_fail',
              actual_checksum='text-image-checksum_fail-checksum')
    tests.add(
        'failures/unexpected/checksum-with-matching-image.html',
        actual_checksum='text-image-checksum_fail-checksum')
    tests.add('failures/unexpected/image-only.html',
              expected_text=None,
              actual_text=None,
              actual_image=b'image-only_fail-pngtEXtchecksum\x00checksum_fail',
              actual_checksum='image-only_fail-checksum')
    tests.add('failures/unexpected/skip_pass.html')
    tests.add('failures/unexpected/text.html', actual_text='text_fail-txt')
    tests.add('failures/unexpected/text_then_crash.html')
    tests.add('failures/unexpected/timeout.html', timeout=True)
    tests.add('failures/unexpected/leak.html', leak=True)
    tests.add('http/tests/passes/text.html')
    tests.add('http/tests/passes/image.html')
    tests.add('http/tests/ssl/text.html')
    tests.add('passes/args.html')
    tests.add('passes/error.html', error=b'stuff going to stderr')
    tests.add('passes/image.html', actual_text=None, expected_text=None)
    tests.add('passes/audio.html',
              actual_audio=base64.b64encode(b'audio-wav'),
              expected_audio=b'audio-wav',
              actual_text=None,
              expected_text=None,
              actual_image=None,
              expected_image=None,
              actual_checksum=None,
              expected_checksum=None)
    tests.add('passes/platform_image.html')
    tests.add('passes/slow.html')
    tests.add('passes/checksum_in_image.html',
              expected_image=b'tEXtchecksum\x00checksum_in_image-checksum')
    tests.add('passes/skipped/skip.html')
    tests.add(
        'failures/unexpected/testharness.html',
        actual_text=
        'This is a testharness.js-based test.\nFAIL: bah\nHarness: the test ran to completion.'
    )

    # Note that here the checksums don't match/ but the images do, so this test passes "unexpectedly".
    # See https://bugs.webkit.org/show_bug.cgi?id=69444 .
    tests.add(
        'failures/unexpected/checksum.html',
        actual_checksum='checksum_fail-checksum')

    # Text output files contain "\r\n" on Windows.  This may be
    # helpfully filtered to "\r\r\n" by our Python/Cygwin tooling.
    tests.add(
        'passes/text.html',
        expected_text='\nfoo\n\n',
        actual_text='\nfoo\r\n\r\r\n')

    # For reftests.
    tests.add_reftest('passes/reftest.html', 'passes/reftest-expected.html')
    # This adds a different virtual reference to ensure that that also works.
    tests.add_reference('virtual/virtual_passes/passes/reftest-expected.html')

    tests.add_reftest('passes/reftest-with-text.html',
                      'passes/reftest-with-text-expected.html',
                      actual_text='reftest',
                      expected_text='reftest')
    tests.add_reftest('passes/mismatch.html',
                      'passes/mismatch-expected-mismatch.html',
                      same_image=False)
    tests.add_reftest('passes/svgreftest.svg',
                      'passes/svgreftest-expected.svg')
    tests.add_reftest('passes/xhtreftest.xht',
                      'passes/xhtreftest-expected.html')
    tests.add_reftest('passes/phpreftest.php',
                      'passes/phpreftest-expected-mismatch.svg',
                      same_image=False)
    tests.add_reftest('failures/expected/reftest.html',
                      'failures/expected/reftest-expected.html',
                      same_image=False)
    tests.add_reftest(
        'failures/unexpected/reftest-with-matching-text.html',
        'failures/unexpected/reftest-with-matching-text-expected.html',
        same_image=False,
        actual_text='reftest',
        expected_text='reftest')
    tests.add_reftest(
        'failures/unexpected/reftest-with-mismatching-text.html',
        'failures/unexpected/reftest-with-mismatching-text-expected.html',
        actual_text='reftest',
        expected_text='reftest-different')
    tests.add_reftest('failures/expected/mismatch.html',
                      'failures/expected/mismatch-expected-mismatch.html')
    tests.add_reftest('failures/unexpected/crash-reftest.html',
                      'failures/unexpected/crash-reftest-expected.html',
                      crash=True)
    tests.add_reftest('failures/unexpected/reftest.html',
                      'failures/unexpected/reftest-expected.html',
                      same_image=False)
    tests.add_reftest(
        'failures/unexpected/reftest-mismatch-with-text-mismatch-with-stderr.html',
        'failures/unexpected/reftest-mismatch-with-text-mismatch-with-stderr-expected.html',
        same_image=False,
        actual_text='actual',
        expected_text='expected',
        error=b'oops')
    tests.add_reftest('failures/unexpected/mismatch.html',
                      'failures/unexpected/mismatch-expected-mismatch.html')
    tests.add(
        'failures/unexpected/reftest-nopixel.html',
        actual_checksum=None,
        actual_image=None,
        expected_image=None)
    tests.add(
        'failures/unexpected/reftest-nopixel-expected.html',
        actual_checksum=None,
        actual_image=None)

    tests.add('websocket/tests/passes/text.html')

    # For testing that we don't run tests under platform/. Note that these don't contribute to TOTAL_TESTS.
    tests.add('platform/test-mac-10.10/http/test.html')
    tests.add('platform/test-win-win7/http/test.html')

    # For testing if perf tests are running in a locked shard.
    tests.add('perf/foo/test.html')
    tests.add('perf/foo/test-ref.html')

    # For testing that virtual test suites don't expand names containing themselves
    # See webkit.org/b/97925 and base_unittest.PortTest.test_tests().
    tests.add('passes/test-virtual-passes.html')
    tests.add('passes/virtual_passes/test-virtual-passes.html')

    tests.add('passes_two/test-virtual-passes.html')

    tests.add('passes/testharness.html',
              actual_text='This is a testharness.js-based test.\n[PASS] bah\n'
              'Harness: the test ran to completion.',
              expected_text=None,
              actual_checksum=None,
              actual_image=None,
              expected_checksum=None,
              expected_image=None)
    tests.add('failures/unexpected/testharness.html',
              actual_text='This is a testharness.js-based test.\n[FAIL] bah\n'
              'Harness: the test ran to completion.',
              expected_text=None,
              actual_checksum=None,
              actual_image=None,
              expected_checksum=None,
              expected_image=None)

    tests.add('virtual/virtual_empty_bases/physical1.html')
    tests.add('virtual/virtual_empty_bases/dir/physical2.html')

    return tests


# Here we synthesize an in-memory filesystem from the test list
# in order to fully control the test output and to demonstrate that
# we don't need a real filesystem to run the tests.
def add_unit_tests_to_mock_filesystem(filesystem):
    # Add the test_expectations file.
    filesystem.maybe_make_directory(MOCK_WEB_TESTS)
    if not filesystem.exists(MOCK_WEB_TESTS + 'TestExpectations'):
        filesystem.write_text_file(
            MOCK_WEB_TESTS + 'TestExpectations', """
# results: [ Pass Failure Crash Timeout Skip ]
failures/expected/audio.html [ Failure ]
failures/expected/crash.html [ Crash ]
failures/expected/crash_then_text.html [ Failure ]
failures/expected/device_failure.html [ Crash ]
failures/expected/exception.html [ Crash ]
failures/expected/image.html [ Failure ]
failures/expected/image_checksum.html [ Failure ]
failures/expected/keyboard.html [ Crash ]
failures/expected/leak.html [ Failure ]
failures/expected/mismatch.html [ Failure ]
failures/expected/newlines_leading.html [ Failure ]
failures/expected/newlines_trailing.html [ Failure ]
failures/expected/newlines_with_excess_CR.html [ Failure ]
failures/expected/reftest.html [ Failure ]
failures/expected/skip_text.html [ Skip ]
failures/expected/text.html [ Failure ]
failures/expected/timeout.html [ Timeout ]
failures/unexpected/pass.html [ Failure ]
failures/unexpected/skip_pass.html [ Skip ]
crbug.com/123 passes/skipped/skip.html [ Skip ]
passes/text.html [ Pass ]
virtual/skipped/failures/expected* [ Skip ]
""")

    if not filesystem.exists(MOCK_WEB_TESTS + 'NeverFixTests'):
        filesystem.write_text_file(
            MOCK_WEB_TESTS + 'NeverFixTests', """
# results: [ Pass Failure Crash Timeout Skip ]
failures/expected/keyboard.html [ Skip ]
failures/expected/exception.html [ Skip ]
failures/expected/device_failure.html [ Skip ]
virtual/virtual_failures/failures/expected/keyboard.html [ Skip ]
virtual/virtual_failures/failures/expected/exception.html [ Skip ]
virtual/virtual_failures/failures/expected/device_failure.html [ Skip ]
""")

    if not filesystem.exists(MOCK_WEB_TESTS + 'SlowTests'):
        filesystem.write_text_file(
            MOCK_WEB_TESTS + 'SlowTests', """
# results: [ Slow ]
passes/slow.html [ Slow ]
""")

    # FIXME: This test was only being ignored because of missing a leading '/'.
    # Fixing the typo causes several tests to assert, so disabling the test entirely.
    # Add in a file should be ignored by port.find_test_files().
    #files[MOCK_WEB_TESTS + 'userscripts/resources/iframe.html'] = 'iframe'

    def add_file(test, suffix, contents):
        dirname = filesystem.join(MOCK_WEB_TESTS,
                                  test.name[0:test.name.rfind('/')])
        base = test.base
        filesystem.maybe_make_directory(dirname)
        filesystem.write_binary_file(
            filesystem.join(dirname, base + suffix), contents)

    # Add each test and the expected output, if any.
    test_list = unit_test_list()
    for test in test_list.tests.values():
        add_file(test, test.name[test.name.rfind('.'):], b'')
        if test.expected_audio:
            add_file(test, '-expected.wav', test.expected_audio)
        if test.expected_text:
            add_file(test, '-expected.txt', test.expected_text.encode('utf-8'))
        if test.expected_image:
            add_file(test, '-expected.png', test.expected_image)

    filesystem.write_text_file(
        filesystem.join(MOCK_WEB_TESTS, 'virtual', 'virtual_passes', 'passes',
                        'args-expected.txt'), 'args-txt --virtual-arg')

    filesystem.maybe_make_directory(
        filesystem.join(MOCK_WEB_TESTS, 'external', 'wpt'))
    filesystem.write_text_file(
        filesystem.join(MOCK_WEB_TESTS, 'external', BASE_MANIFEST_NAME),
        '{"manifest": "base"}')

    # Clear the list of written files so that we can watch what happens during testing.
    filesystem.clear_written_files()


def add_manifest_to_mock_filesystem(port):
    # Disable manifest update otherwise they'll be overwritten.
    port.set_option_default('manifest_update', False)
    filesystem = port.host.filesystem
    filesystem.write_text_file(
        MOCK_WEB_TESTS + 'external/wpt/MANIFEST.json',
        json.dumps({
            'items': {
                'testharness': {
                    'dom': {
                        'ranges': {
                            'Range-attributes.html': ['acbdef123', [None, {}]],
                            'Range-attributes-slow.html':
                            ['abcdef123', [None, {
                                'timeout': 'long'
                            }]],
                        },
                    },
                    'console': {
                        'console-is-a-namespace.any.js': [
                            'abcdef1234',
                            ['console/console-is-a-namespace.any.html', {}],
                            [
                                'console/console-is-a-namespace.any.worker.html',
                                {
                                    'timeout': 'long'
                                }
                            ],
                        ],
                    },
                    'html': {
                        'parse.html': [
                            'abcdef123',
                            ['html/parse.html?run_type=uri', {}],
                            [
                                'html/parse.html?run_type=write', {
                                    'timeout': 'long'
                                }
                            ],
                        ],
                    },
                },
                'manual': {},
                'reftest': {
                    'html': {
                        'dom': {
                            'elements': {
                                'global-attributes': {
                                    'dir_auto-EN-L.html': [
                                        'abcdef123',
                                        [
                                            None,
                                            [[
                                                '/html/dom/elements/global-attributes/dir_auto-EN-L-ref.html',
                                                '=='
                                            ]], {
                                                'timeout':
                                                'long',
                                                'fuzzy':
                                                [[None, [[0, 255], [0, 200]]]]
                                            }
                                        ],
                                    ]
                                }
                            }
                        }
                    }
                },
                'print-reftest': {
                    'foo': {
                        'bar': {
                            'test-print.html': [
                                'abcdef123',
                                [
                                    None,
                                    [['/foo/bar/test-print-ref.html', '==']], {
                                        'timeout': 'long'
                                    }
                                ]
                            ]
                        },
                        'print': {
                            'test.html': [
                                'abcdef123',
                                [
                                    None,
                                    [['/foo/bar/test-print-ref.html', '==']], {
                                        'timeout': 'long'
                                    }
                                ]
                            ]
                        }
                    }
                },
                'crashtest': {
                    'portals': {
                        'portals-no-frame-crash.html':
                        ['abcdef123', [None, {}]],
                    },
                },
            }
        }))
    filesystem.write_text_file(
        MOCK_WEB_TESTS + 'external/wpt/dom/ranges/Range-attributes.html', '')
    filesystem.write_text_file(
        MOCK_WEB_TESTS + 'external/wpt/dom/ranges/Range-attributes-slow.html',
        '')
    filesystem.write_text_file(
        MOCK_WEB_TESTS + 'external/wpt/console/console-is-a-namespace.any.js',
        '')
    filesystem.write_text_file(
        MOCK_WEB_TESTS + 'external/wpt/common/blank.html', 'foo')
    filesystem.write_text_file(
        MOCK_WEB_TESTS + 'external/wpt/foo/bar/test-print.html', '')
    filesystem.write_text_file(
        MOCK_WEB_TESTS + 'external/wpt/foo/print/test.html', '')

    filesystem.write_text_file(
        MOCK_WEB_TESTS + 'wpt_internal/MANIFEST.json',
        json.dumps({
            'items': {
                'testharness': {
                    'dom': {
                        'bar.html': ['abcdef123', [None, {}]]
                    }
                }
            }
        }))
    filesystem.write_text_file(MOCK_WEB_TESTS + 'wpt_internal/dom/bar.html',
                               'baz')


class TestPort(Port):
    port_name = 'test'
    default_port_name = 'test-mac-mac10.10'

    FALLBACK_PATHS = {
        'win7': ['test-win-win7', 'test-win-win10'],
        'win10': ['test-win-win10'],
        'mac10.10': ['test-mac-mac10.10', 'test-mac-mac10.11'],
        'mac10.11': ['test-mac-mac10.11'],
        'trusty': ['test-linux-trusty', 'test-win-win10'],
        'precise':
        ['test-linux-precise', 'test-linux-trusty', 'test-win-win10'],
    }

    @classmethod
    def determine_full_port_name(cls, host, options, port_name):
        if port_name == 'test':
            return TestPort.default_port_name
        return port_name

    def __init__(self, host, port_name=None, **kwargs):
        Port.__init__(self, host, port_name or TestPort.default_port_name,
                      **kwargs)
        self._tests = unit_test_list()
        self._flakes = set()

        # FIXME: crbug.com/279494. This needs to be in the "real web tests
        # dir" in a mock filesystem, rather than outside of the checkout, so
        # that tests that want to write to a TestExpectations file can share
        # this between "test" ports and "real" ports.  This is the result of
        # rebaseline_unittest.py having tests that refer to "real" port names
        # and real builders instead of fake builders that point back to the
        # test ports. rebaseline_unittest.py needs to not mix both "real" ports
        # and "test" ports

        self._generic_expectations_path = MOCK_WEB_TESTS + 'TestExpectations'
        self._results_directory = None

        self._operating_system = 'mac'
        if self._name.startswith('test-win'):
            self._operating_system = 'win'
        elif self._name.startswith('test-linux'):
            self._operating_system = 'linux'

        self.port_name = self._operating_system

        version_map = {
            'test-win-win7': 'win7',
            'test-win-win10': 'win10',
            'test-win-win10-arm64': 'win10-arm64',
            'test-mac-mac10.10': 'mac10.10',
            'test-mac-mac10.11': 'mac10.11',
            'test-mac-mac11': 'mac11',
            'test-mac-mac11-arm64': 'mac11-arm64',
            'test-linux-precise': 'precise',
            'test-linux-trusty': 'trusty',
        }
        self._version = version_map[self._name]

        if self._operating_system == 'linux':
            self._architecture = 'x86_64'
        elif self._operating_system == 'mac':
            self._architecture = 'arm64'

        self.all_systems = (
            ('mac10.10', 'x86'),
            ('mac10.11', 'x86'),
            ('mac11', 'x86_64'),
            ('mac11-arm64', 'arm64'),
            ('win7', 'x86'),
            ('win10', 'x86'),
            ('win10-arm64', 'arm64'),
            ('precise', 'x86_64'),
            ('trusty', 'x86_64'),
        )

        self.all_build_types = ('debug', 'release')

        # To avoid surprises when introducing new macros, these are
        # intentionally fixed in time.
        self.configuration_specifier_macros_dict = {
            'mac': ['mac10.10', 'mac10.11', 'mac11', 'mac11-arm64'],
            'win': ['win7', 'win10', 'win10-arm64'],
            'linux': ['precise', 'trusty']
        }

    def look_for_new_samples(self, crashed_processes, start_time):
        del start_time
        sample_files = {}
        for cp in crashed_processes:
            if cp[0].endswith('crash-with-sample.html'):
                sample_file = cp[0].replace('.html', '_sample.txt')
                self._filesystem.maybe_make_directory(
                    self._filesystem.dirname(sample_file))
                self._filesystem.write_binary_file(sample_file,
                                                   'crash sample file')
                sample_files[cp[0]] = sample_file
        return sample_files

    def look_for_new_crash_logs(self, crashed_processes, start_time):
        del start_time
        crash_logs = {}
        for cp in crashed_processes:
            if cp[0].endswith('-with-delayed-log.html'):
                crash_logs[cp[0]] = (b'delayed crash log', '/tmp')
        return crash_logs

    def path_to_driver(self, target=None):
        # This routine shouldn't normally be called, but it is called by
        # the mock_drt Driver. We return something, but make sure it's useless.
        return 'MOCK _path_to_driver'

    def default_child_processes(self):
        return 1

    def check_build(self, needs_http, printer):
        return exit_codes.OK_EXIT_STATUS

    def check_sys_deps(self):
        return exit_codes.OK_EXIT_STATUS

    def default_configuration(self):
        return 'Release'

    def diff_image(self,
                   expected_contents,
                   actual_contents,
                   max_channel_diff=None,
                   max_pixels_diff=None):
        diffed = actual_contents != expected_contents
        if not actual_contents and not expected_contents:
            return (None, None, None)
        if not actual_contents or not expected_contents:
            return (True, None, None)
        if diffed:
            mock_diff = '\n'.join([
                '< %s' % base64.b64encode(expected_contents).decode('utf-8'),
                '---',
                '> %s' % base64.b64encode(actual_contents).decode('utf-8'),
            ])
            mock_stats = {
                "maxDifference": 100,
                "maxPixels": len(actual_contents)
            }
            return (mock_diff, mock_stats, None)
        return (None, None, None)

    def web_tests_dir(self):
        return MOCK_WEB_TESTS

    def _perf_tests_dir(self):
        return PERF_TEST_DIR

    def name(self):
        return self._name

    def operating_system(self):
        return self._operating_system

    def default_results_directory(self):
        return '/tmp'

    @memoized
    def typ_host(self):
        return FileSystemForwardingTypHost(self._filesystem)

    def setup_test_run(self):
        pass

    def _driver_class(self):
        return TestDriver

    def start_http_server(self, additional_dirs, number_of_drivers):
        pass

    def start_websocket_server(self):
        pass

    def acquire_http_lock(self):
        pass

    def stop_http_server(self):
        pass

    def stop_websocket_server(self):
        pass

    def release_http_lock(self):
        pass

    def path_to_apache(self):
        return '/usr/sbin/httpd'

    def path_to_apache_config_file(self):
        return self._filesystem.join(self.apache_config_directory(),
                                     'httpd.conf')

    def path_to_generic_test_expectations_file(self):
        return self._generic_expectations_path

    def all_test_configurations(self):
        """Returns a sequence of the TestConfigurations the port supports."""
        # By default, we assume we want to test every graphics type in
        # every configuration on every system.
        test_configurations = []
        for version, architecture in self.all_systems:
            for build_type in self.all_build_types:
                test_configurations.append(
                    TestConfiguration(
                        version=version,
                        architecture=architecture,
                        build_type=build_type))
        return test_configurations

    def configuration_specifier_macros(self):
        return self.configuration_specifier_macros_dict

    def virtual_test_suites(self):
        return [
            VirtualTestSuite(
                prefix='virtual_console',
                platforms=['Linux', 'Mac', 'Win'],
                bases=['external/wpt/console/console-is-a-namespace.any.js'],
                args=['--virtual-console']),
            VirtualTestSuite(prefix='virtual_passes',
                             platforms=['Linux', 'Mac', 'Win'],
                             bases=['passes', 'passes_two'],
                             args=['--virtual-arg']),
            VirtualTestSuite(prefix='skipped',
                             platforms=['Linux', 'Mac', 'Win'],
                             bases=['failures/expected'],
                             args=['--virtual-arg-skipped']),
            VirtualTestSuite(
                prefix='virtual_failures',
                platforms=['Linux', 'Mac', 'Win'],
                bases=['failures/expected', 'failures/unexpected'],
                args=['--virtual-arg-failures']),
            VirtualTestSuite(prefix='virtual_wpt',
                             platforms=['Linux', 'Mac', 'Win'],
                             bases=['external/wpt'],
                             args=['--virtual-arg-wpt']),
            VirtualTestSuite(prefix='virtual_wpt_dom',
                             platforms=['Linux', 'Mac', 'Win'],
                             bases=['external/wpt/dom', 'wpt_internal/dom'],
                             args=['--virtual-arg-wpt-dom']),
            VirtualTestSuite(prefix='virtual_empty_bases',
                             platforms=['Linux', 'Mac', 'Win'],
                             bases=[],
                             args=['--virtual-arg-empty-bases']),
            VirtualTestSuite(
                prefix='generated_wpt',
                platforms=['Linux', 'Mac', 'Win'],
                bases=[
                    'external/wpt/html/parse.html?run_type=uri',
                    'external/wpt/console/console-is-a-namespace.any.html',
                ],
                args=['--fake-switch']),
            VirtualTestSuite(
                prefix='mixed_wpt',
                platforms=['Linux', 'Mac', 'Win'],
                bases=[
                    'http',
                    'external/wpt/dom',
                    # Should use the physical tests located under
                    # `virtual/virtual_empty_bases`.
                    'virtual/virtual_empty_bases',
                ],
                args=['--virtual-arg']),
        ]


class TestDriver(Driver):
    """Test/Dummy implementation of the driver interface."""
    next_pid = 1

    # pylint: disable=protected-access

    def __init__(self, *args, **kwargs):
        super(TestDriver, self).__init__(*args, **kwargs)
        self.started = False
        self.pid = 0

    def cmd_line(self, per_test_args):
        return [
            self._port.path_to_driver(),
            *self._port.get_option('additional_driver_flag', []),
            *per_test_args,
        ]

    def run_test(self, driver_input):
        if not self.started:
            self.started = True
            self.pid = TestDriver.next_pid
            TestDriver.next_pid += 1

        start_time = time.time()
        test_name = driver_input.test_name
        test_args = driver_input.args or []
        test = self._port._tests[test_name]
        if test.keyboard:
            raise KeyboardInterrupt
        if test.exception:
            raise ValueError('exception from ' + test_name)
        if test.device_failure:
            raise DeviceFailure('device failure in ' + test_name)

        audio = None
        if test.actual_text:
            actual_text = test.actual_text.encode('utf8')
        else:
            actual_text = None
        crash = test.crash
        web_process_crash = test.web_process_crash
        leak = test.leak

        if 'flaky/text.html' in test_name and not test_name in self._port._flakes:
            self._port._flakes.add(test_name)
            actual_text = b'flaky text failure'

        if 'crash_then_text.html' in test_name:
            if test_name in self._port._flakes:
                actual_text = b'text failure'
            else:
                self._port._flakes.add(test_name)
                crashed_process_name = self._port.driver_name()
                crashed_pid = 1
                crash = True

        if 'text_then_crash.html' in test_name:
            if test_name in self._port._flakes:
                crashed_process_name = self._port.driver_name()
                crashed_pid = 1
                crash = True
            else:
                self._port._flakes.add(test_name)
                actual_text = b'text failure'

        if actual_text and test_args and test_name == 'passes/args.html':
            actual_text = actual_text + b' ' + (
                ' '.join(test_args).encode('utf8'))

        if test.actual_audio:
            audio = base64.b64decode(test.actual_audio)
        crashed_process_name = None
        crashed_pid = None

        leak_log = b''
        if leak:
            leak_log = b'leak detected'

        crash_log = b''
        if crash:
            crashed_process_name = self._port.driver_name()
            crashed_pid = 1
            crash_log = b'crash log'
        elif web_process_crash:
            crashed_process_name = 'WebProcess'
            crashed_pid = 2
            crash_log = b'web process crash log'

        if crashed_process_name:
            crash_logs = CrashLogs(self._port.host)
            crash_log = crash_logs.find_newest_log(crashed_process_name,
                                                   None) or crash_log

        if 'crash-reftest.html' in test_name:
            crashed_process_name = self._port.driver_name()
            crashed_pid = 3
            crash = True
            crash_log = b'reftest crash log'
        if test.actual_checksum == driver_input.image_hash:
            image = None
        else:
            image = test.actual_image
        return DriverOutput(
            actual_text,
            image,
            test.actual_checksum,
            audio,
            crash=(crash or web_process_crash),
            crashed_process_name=crashed_process_name,
            crashed_pid=crashed_pid,
            crash_log=crash_log,
            test_time=time.time() - start_time,
            timeout=test.timeout,
            error=test.error,
            pid=self.pid,
            leak=test.leak,
            leak_log=leak_log)

    def stop(self, timeout_secs=0.0, kill_tree=True, send_sigterm=False):
        self.started = False
