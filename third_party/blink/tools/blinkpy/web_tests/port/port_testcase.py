# Copyright (C) 2010 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#    * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#    * Neither the name of Google Inc. nor the names of its
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
"""Unit testing base class for Port implementations."""

import collections
import optparse

from blinkpy.common import exit_codes
from blinkpy.common.system.executive import ScriptError
from blinkpy.common.system.executive_mock import MockExecutive
from blinkpy.common.system.log_testing import LoggingTestCase
from blinkpy.common.system.platform_info_mock import MockPlatformInfo
from blinkpy.common.system.system_host import SystemHost
from blinkpy.common.system.system_host_mock import MockSystemHost
from blinkpy.web_tests.port.base import Port


class FakePrinter(object):
    def write_update(self, msg):
        pass

    def write_throttled_update(self, msg):
        pass


class PortTestCase(LoggingTestCase):
    """Tests that all Port implementations must pass."""

    # Some tests in this class test or override protected methods
    # pylint: disable=protected-access

    HTTP_PORTS = (8000, 8080, 8443)
    WEBSOCKET_PORTS = (8880, )

    # Subclasses override this to point to their Port subclass.
    os_name = None
    os_version = None
    machine = None
    port_maker = Port
    port_name = None
    full_port_name = None
    processor = None

    def make_port(self,
                  host=None,
                  port_name=None,
                  options=None,
                  os_name=None,
                  os_version=None,
                  machine=None,
                  processor=None,
                  **kwargs):
        host = host or MockSystemHost(os_name=(os_name or self.os_name),
                                      os_version=(os_version
                                                  or self.os_version),
                                      machine=(machine or self.machine),
                                      processor=(processor or self.processor))
        options = options or optparse.Values({
            'configuration': 'Release',
            'use_xvfb': True
        })
        port_name = port_name or self.port_name
        port_name = self.port_maker.determine_full_port_name(
            host, options, port_name)
        return self.port_maker(host, port_name, options=options, **kwargs)

    def test_check_build(self):
        port = self.make_port()

        # Here we override methods to make it appear as though the build
        # requirements are all met and the driver is found.
        port._check_file_exists = lambda path, desc: True
        if port._dump_reader:
            port._dump_reader.check_is_functional = lambda: True
        port._options.build = True
        port._check_driver_build_up_to_date = lambda config: True
        port.check_httpd = lambda: True
        self.assertEqual(
            port.check_build(needs_http=True, printer=FakePrinter()),
            exit_codes.OK_EXIT_STATUS)
        logs = ''.join(self.logMessages())
        self.assertNotIn('build requirements', logs)

        # And here, after changing it so that the driver binary is not found,
        # we get an error exit status and message about build requirements.
        port._check_file_exists = lambda path, desc: False
        self.assertEqual(
            port.check_build(needs_http=True, printer=FakePrinter()),
            exit_codes.UNEXPECTED_ERROR_EXIT_STATUS)
        logs = ''.join(self.logMessages())
        self.assertIn('build requirements', logs)

    def test_default_batch_size(self):
        port = self.make_port()

        # Test that we set a finite batch size for sanitizer builds.
        port._options.enable_sanitizer = True
        sanitized_batch_size = port.default_batch_size()
        self.assertIsNotNone(sanitized_batch_size)

    def test_default_child_processes(self):
        port = self.make_port()
        num_workers = port.default_child_processes()
        self.assertGreaterEqual(num_workers, 1)

    def test_default_max_locked_shards(self):
        port = self.make_port()
        port.default_child_processes = lambda: 16
        self.assertEqual(port.default_max_locked_shards(), 4)
        port.default_child_processes = lambda: 2
        self.assertEqual(port.default_max_locked_shards(), 1)

    def test_default_timeout_ms(self):
        self.assertEqual(self.make_port().timeout_ms(), 6000)

    def test_timeout_ms_release(self):
        self.assertEqual(
            self.make_port(options=optparse.Values(
                {'configuration': 'Release'})).timeout_ms(),
            self.make_port().timeout_ms())

    def test_timeout_ms_debug(self):
        self.assertEqual(
            self.make_port(options=optparse.Values({'configuration': 'Debug'
                                                    })).timeout_ms(),
            5 * self.make_port().timeout_ms())

    def make_dcheck_port(self, options):
        host = MockSystemHost(os_name=self.os_name, os_version=self.os_version)
        host.filesystem.write_text_file(
            self.make_port(host).build_path('args.gn'),
            'is_debug=false\ndcheck_always_on = true # comment\n')
        port = self.make_port(host, options=options)
        return port

    def test_timeout_ms_with_dcheck(self):
        default_timeout_ms = self.make_port().timeout_ms()
        self.assertEqual(
            self.make_dcheck_port(options=optparse.Values(
                {'configuration': 'Release'})).timeout_ms(),
            2 * default_timeout_ms)
        self.assertEqual(
            self.make_dcheck_port(options=optparse.Values(
                {'configuration': 'Debug'})).timeout_ms(),
            5 * default_timeout_ms)

    def test_driver_cmd_line(self):
        port = self.make_port()
        self.assertTrue(len(port.driver_cmd_line()))

        options = optparse.Values(
            dict(additional_driver_flag=['--foo=bar', '--foo=baz']))
        port = self.make_port(options=options)
        cmd_line = port.driver_cmd_line()
        self.assertTrue('--foo=bar' in cmd_line)
        self.assertTrue('--foo=baz' in cmd_line)

    def test_diff_image__missing_both(self):
        port = self.make_port()
        self.assertEqual(port.diff_image(None, None), (None, None, None))
        self.assertEqual(port.diff_image(None, ''), (None, None, None))
        self.assertEqual(port.diff_image('', None), (None, None, None))

        self.assertEqual(port.diff_image('', ''), (None, None, None))

    def test_diff_image__missing_actual(self):
        port = self.make_port()
        self.assertEqual(port.diff_image(None, 'foo'), ('foo', None, None))
        self.assertEqual(port.diff_image('', 'foo'), ('foo', None, None))

    def test_diff_image__missing_expected(self):
        port = self.make_port()
        self.assertEqual(port.diff_image('foo', None), ('foo', None, None))
        self.assertEqual(port.diff_image('foo', ''), ('foo', None, None))

    def test_diff_image(self):
        def _path_to_image_diff():
            return '/path/to/image_diff'

        port = self.make_port()
        port._path_to_image_diff = _path_to_image_diff

        mock_image_diff = 'MOCK Image Diff'

        def mock_run_command(args):
            port.host.filesystem.write_binary_file(args[4], mock_image_diff)
            raise ScriptError(
                output='Found pixels_different: 100, max_channel_diff: 30',
                exit_code=1)

        # Images are different.
        port._executive = MockExecutive(run_command_fn=mock_run_command)  # pylint: disable=protected-access
        diff, stats, err = port.diff_image('EXPECTED', 'ACTUAL')
        self.assertEqual(diff, mock_image_diff)
        self.assertEqual(stats, {"maxDifference": 30, "totalPixels": 100})
        self.assertEqual(err, None)

        # Images are the same.
        port._executive = MockExecutive(exit_code=0)  # pylint: disable=protected-access
        self.assertEqual(None, port.diff_image('EXPECTED', 'ACTUAL')[0])

        # Images are the same up to fuzzy diff.
        port._executive = MockExecutive(
            output='Found pixels_different: 250, max_channel_diff: 35',
            exit_code=0)  # pylint: disable=protected-access
        diff, stats, err = port.diff_image('EXPECTED',
                                           'ACTUAL',
                                           max_channel_diff=[10, 40],
                                           max_pixels_diff=[0, 500])
        self.assertEqual(diff, None)
        self.assertEqual(stats, {"maxDifference": 35, "totalPixels": 250})
        self.assertEqual(err, None)

        # There was some error running image_diff.
        port._executive = MockExecutive(exit_code=2)  # pylint: disable=protected-access
        exception_raised = False
        try:
            port.diff_image('EXPECTED', 'ACTUAL')
        except ValueError:
            exception_raised = True
        self.assertFalse(exception_raised)

    def test_diff_image_crashed(self):
        port = self.make_port()
        port._executive = MockExecutive(should_throw=True, exit_code=2)  # pylint: disable=protected-access
        self.assertEqual(port.diff_image('EXPECTED', 'ACTUAL'), (
            None, None,
            'Image diff returned an exit code of 2. See http://crbug.com/278596'
        ))

    def test_test_configuration(self):
        port = self.make_port()
        self.assertTrue(port.test_configuration())

    def test_get_crash_log_all_none(self):
        port = self.make_port()
        stderr, details, crash_site = port._get_crash_log(
            None, None, None, None, newer_than=None)
        self.assertIsNone(stderr)
        self.assertEqual(
            details, b'crash log for <unknown process name> (pid <unknown>):\n'
            b'STDOUT: <empty>\n'
            b'STDERR: <empty>\n')
        self.assertIsNone(crash_site)

    def test_get_crash_log_simple(self):
        port = self.make_port()
        stderr, details, crash_site = port._get_crash_log(
            'foo',
            1234,
            b'out bar\nout baz',
            b'err bar\nerr baz\n',
            newer_than=None)
        self.assertEqual(stderr, b'err bar\nerr baz\n')
        self.assertEqual(
            details, b'crash log for foo (pid 1234):\n'
            b'STDOUT: out bar\n'
            b'STDOUT: out baz\n'
            b'STDERR: err bar\n'
            b'STDERR: err baz\n')
        self.assertIsNone(crash_site)

    def test_get_crash_log_non_ascii(self):
        port = self.make_port()
        stderr, details, crash_site = port._get_crash_log('foo',
                                                          1234,
                                                          b'foo\xa6bar',
                                                          b'foo\xa6bar',
                                                          newer_than=None)
        self.assertEqual(stderr, b'foo\xa6bar')
        self.assertEqual(
            details.decode('utf8', 'replace'),
            u'crash log for foo (pid 1234):\n'
            u'STDOUT: foo\ufffdbar\n'
            u'STDERR: foo\ufffdbar\n')
        self.assertIsNone(crash_site)

    def test_get_crash_log_newer_than(self):
        port = self.make_port()
        stderr, details, crash_site = port._get_crash_log('foo',
                                                          1234,
                                                          b'foo\xa6bar',
                                                          b'foo\xa6bar',
                                                          newer_than=1.0)
        self.assertEqual(stderr, b'foo\xa6bar')
        self.assertEqual(
            details.decode('utf8', 'replace'),
            u'crash log for foo (pid 1234):\n'
            u'STDOUT: foo\ufffdbar\n'
            u'STDERR: foo\ufffdbar\n')
        self.assertIsNone(crash_site)

    def test_get_crash_log_crash_site(self):
        port = self.make_port()
        stderr, details, crash_site = port._get_crash_log(
            'foo',
            1234,
            b'out bar',
            b'[1:2:3:4:FATAL:example.cc(567)] Check failed.',
            newer_than=None)
        self.assertEqual(stderr,
                         b'[1:2:3:4:FATAL:example.cc(567)] Check failed.')
        self.assertEqual(
            details, b'crash log for foo (pid 1234):\n'
            b'STDOUT: out bar\n'
            b'STDERR: [1:2:3:4:FATAL:example.cc(567)] Check failed.\n')
        self.assertEqual(crash_site, 'example.cc(567)')

    def test_default_expectations_files(self):
        port = self.make_port()
        self.assertEqual(list(port.default_expectations_files()), [
            port.path_to_generic_test_expectations_file(),
            port.host.filesystem.join(port.web_tests_dir(), 'NeverFixTests'),
            port.host.filesystem.join(port.web_tests_dir(),
                                      'StaleTestExpectations'),
            port.host.filesystem.join(port.web_tests_dir(), 'SlowTests'),
        ])

    def test_default_expectations_ordering(self):
        port = self.make_port()
        for path in port.default_expectations_files():
            port.host.filesystem.write_text_file(path, '')
        ordered_dict = port.expectations_dict()
        self.assertEqual(port.path_to_generic_test_expectations_file(),
                         list(ordered_dict)[0])

        options = optparse.Values(
            dict(additional_expectations=['/tmp/foo', '/tmp/bar']))
        port = self.make_port(options=options)
        for path in port.default_expectations_files():
            port.host.filesystem.write_text_file(path, '')
        port.host.filesystem.write_text_file('/tmp/foo', 'foo')
        port.host.filesystem.write_text_file('/tmp/bar', 'bar')
        ordered_dict = port.expectations_dict()
        self.assertEqual(
            list(ordered_dict)[-2:], options.additional_expectations)
        self.assertEqual(list(ordered_dict.values())[-2:], ['foo', 'bar'])

    def test_used_expectations_files(self):
        options = optparse.Values({
            'additional_expectations': ['/tmp/foo'],
            'additional_driver_flag': ['--flag-not-affecting'],
            'flag_specific':
            'a',
        })
        port = self.make_port(options=options)
        port.host.filesystem.write_text_file(
            port.host.filesystem.join(port.web_tests_dir(),
                                      'FlagSpecificConfig'),
            '[{"name": "a", "args": ["--aa"]}]')
        self.assertEqual(list(port.used_expectations_files()), [
            port.path_to_generic_test_expectations_file(),
            port.host.filesystem.join(port.web_tests_dir(), 'NeverFixTests'),
            port.host.filesystem.join(port.web_tests_dir(),
                                      'StaleTestExpectations'),
            port.host.filesystem.join(port.web_tests_dir(), 'SlowTests'),
            port.host.filesystem.join(port.web_tests_dir(), 'FlagExpectations',
                                      'a'),
            '/tmp/foo',
        ])

    def test_path_to_apache_config_file(self):
        # Specific behavior may vary by port, so unit test sub-classes may override this.
        port = self.make_port()

        port.host.environ[
            'WEBKIT_HTTP_SERVER_CONF_PATH'] = '/path/to/httpd.conf'
        with self.assertRaises(IOError):
            port.path_to_apache_config_file()
        port.host.filesystem.write_text_file('/existing/httpd.conf',
                                             'Hello, world!')
        port.host.environ[
            'WEBKIT_HTTP_SERVER_CONF_PATH'] = '/existing/httpd.conf'
        self.assertEqual(port.path_to_apache_config_file(),
                         '/existing/httpd.conf')

        # Mock out _apache_config_file_name_for_platform to avoid mocking platform info.
        port._apache_config_file_name_for_platform = lambda: 'httpd.conf'
        del port.host.environ['WEBKIT_HTTP_SERVER_CONF_PATH']
        self.assertEqual(
            port.path_to_apache_config_file(),
            port.host.filesystem.join(port.apache_config_directory(),
                                      'httpd.conf'))

        # Check that even if we mock out _apache_config_file_name, the environment variable takes precedence.
        port.host.environ[
            'WEBKIT_HTTP_SERVER_CONF_PATH'] = '/existing/httpd.conf'
        self.assertEqual(port.path_to_apache_config_file(),
                         '/existing/httpd.conf')

    def test_additional_platform_directory(self):
        port = self.make_port(
            options=optparse.Values(
                dict(additional_platform_directory=['/tmp/foo'])))
        self.assertEqual(port.baseline_search_path()[0], '/tmp/foo')

    def test_virtual_test_suites(self):
        # We test that we can load the real web_tests/VirtualTestSuites file properly, so we
        # use a real SystemHost(). We don't care what virtual_test_suites() returns as long
        # as it is iterable.
        port = self.make_port(host=SystemHost(), port_name=self.full_port_name)
        port.operating_system = lambda: 'linux'
        self.assertTrue(
            isinstance(port.virtual_test_suites(), collections.abc.Iterable))
