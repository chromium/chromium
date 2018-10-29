# Copyright (C) 2011 Google Inc. All rights reserved.
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
#     * Neither the name of Google Inc. nor the names of its
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

"""Unit tests for MockDRT."""

import io
import optparse
import unittest

from blinkpy.common.system.system_host_mock import MockSystemHost
from blinkpy.web_tests.port import mock_drt
from blinkpy.web_tests.port import port_testcase
from blinkpy.web_tests.port import test
from blinkpy.web_tests.port.factory import PortFactory


class MockDRTPortTest(port_testcase.PortTestCase):

    def make_port(self, host=None, options=optparse.Values({'configuration': 'Release'})):
        host = host or MockSystemHost()
        test.add_unit_tests_to_mock_filesystem(host.filesystem)
        return mock_drt.MockDRTPort(host, port_name='mock-mac', options=options)

    def test_port_name_in_constructor(self):
        self.assertTrue(mock_drt.MockDRTPort(MockSystemHost(), port_name='mock-test'))

    def test_check_sys_deps(self):
        pass

    def test_default_max_locked_shards(self):
        pass

    def test_diff_image(self):
        pass

    def test_diff_image_crashed(self):
        pass

    def test_uses_apache(self):
        pass

    def test_get_crash_log(self):
        pass

    def test_check_build(self):
        pass

    def test_virtual_test_suites(self):
        pass

    def test_path_to_apache_config_file(self):
        pass


class MockDRTTest(unittest.TestCase):

    def input_line(self, port, test_name, checksum=None):
        url = port.create_driver(0).test_to_uri(test_name)
        if url.startswith('file://'):
            url = url[len('file://'):]
        if checksum:
            url += "'" + checksum
        return url + '\n'

    def make_drt(self, options, args, host, stdin, stdout, stderr):
        return mock_drt.MockDRT(options, args, host, stdin, stdout, stderr)

    def make_input_output(self, port, test_name, expected_checksum, drt_output,
                          drt_input=None, expected_text=None):
        if not expected_checksum:
            expected_checksum = port.expected_checksum(test_name)
        if not drt_input:
            drt_input = self.input_line(port, test_name, expected_checksum)
        text_output = expected_text or port.expected_text(test_name) or ''

        if not drt_output:
            drt_output = self.expected_output(port, test_name,
                                              text_output, expected_checksum)
        return (drt_input, drt_output)

    def expected_output(self, port, test_name, text_output, expected_checksum):
        output = ['#READY\n', 'Content-Type: text/plain\n']
        if text_output:
            output.append(text_output)
        output.append('#EOF\n')
        if expected_checksum:
            output.extend(['\n',
                           'ActualHash: %s\n' % expected_checksum,
                           'ExpectedHash: %s\n' % expected_checksum])
        output.append('#EOF\n')
        return output

    def assertTest(self, test_name, expected_checksum=None, drt_output=None, host=None, expected_text=None):
        port_name = 'test'
        host = host or MockSystemHost()
        test.add_unit_tests_to_mock_filesystem(host.filesystem)
        port = PortFactory(host).get(port_name)
        drt_input, drt_output = self.make_input_output(
            port, test_name, expected_checksum, drt_output, drt_input=None, expected_text=expected_text)

        args = ['--run-web-tests', '--platform', port_name, '-']
        stdin = io.BytesIO(drt_input)
        stdout = io.BytesIO()
        stderr = io.BytesIO()
        options, args = mock_drt.parse_options(args)

        drt = self.make_drt(options, args, host, stdin, stdout, stderr)
        res = drt.run()

        self.assertEqual(res, 0)

        self.assertEqual(stdout.getvalue(), ''.join(drt_output))
        self.assertEqual(stderr.getvalue(), '#EOF\n')

    def test_main(self):
        host = MockSystemHost()
        test.add_unit_tests_to_mock_filesystem(host.filesystem)
        stdin = io.BytesIO()
        stdout = io.BytesIO()
        stderr = io.BytesIO()
        res = mock_drt.main(['--run-web-tests', '--platform', 'test', '-'],
                            host, stdin, stdout, stderr)
        self.assertEqual(res, 0)
        self.assertEqual(stdout.getvalue(), '#READY\n')
        self.assertEqual(stderr.getvalue(), '')
        self.assertEqual(host.filesystem.written_files, {})

    def test_pixeltest_passes(self):
        # This also tests that we handle HTTP: test URLs properly.
        self.assertTest('http/tests/passes/text.html')

    def test_pixeltest__fails(self):
        self.assertTest('failures/expected/image_checksum.html',
                        expected_checksum='image_checksum-checksum',
                        drt_output=[
                            '#READY\n',
                            'Content-Type: text/plain\n',
                            'image_checksum-txt',
                            '#EOF\n',
                            '\n',
                            'ActualHash: image_checksum-checksum\n',
                            'ExpectedHash: image_checksum-checksum\n',
                            '#EOF\n',
                        ])

    def test_textonly(self):
        self.assertTest('passes/image.html')

    def test_checksum_in_png(self):
        self.assertTest('passes/checksum_in_image.html')

    def test_reftest_match(self):
        self.assertTest('passes/reftest.html', expected_checksum='mock-checksum', expected_text='reference text\n')

    def test_reftest_mismatch(self):
        self.assertTest('passes/mismatch.html', expected_checksum='mock-checksum', expected_text='reference text\n')

    def test_audio(self):
        self.assertTest('passes/audio.html',
                        drt_output=[
                            '#READY\n',
                            'Content-Type: audio/wav\n',
                            'Content-Transfer-Encoding: base64\n',
                            'YXVkaW8td2F2',
                            '\n',
                            '#EOF\n',
                            '#EOF\n',
                        ])

    def test_virtual(self):
        self.assertTest('virtual/passes/text.html')
