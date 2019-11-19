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

"""Windows implementation of the Port interface."""

import errno
import logging
import tempfile

# The _winreg library is only available on Windows.
# https://docs.python.org/2/library/_winreg.html
try:
    import _winreg  # pylint: disable=import-error
except ImportError:
    _winreg = None  # pylint: disable=invalid-name

from blinkpy.common import exit_codes
from blinkpy.web_tests.breakpad.dump_reader_win import DumpReaderWin
from blinkpy.web_tests.models import test_run_results
from blinkpy.web_tests.port import base


_log = logging.getLogger(__name__)


class WinPort(base.Port):
    port_name = 'win'

    SUPPORTED_VERSIONS = ('win7', 'win10')

    FALLBACK_PATHS = {'win10': ['win']}
    FALLBACK_PATHS['win7'] = ['win7'] + FALLBACK_PATHS['win10']

    BUILD_REQUIREMENTS_URL = 'https://chromium.googlesource.com/chromium/src/+/master/docs/windows_build_instructions.md'

    @classmethod
    def determine_full_port_name(cls, host, options, port_name):
        if port_name.endswith('win'):
            assert host.platform.is_win()
            # We don't maintain separate baselines for vista, so we pretend it is win7.
            if host.platform.os_version in ('vista', '7sp0', '7sp1'):
                version = 'win7'
            # Same for win8, we treat it as win10.
            elif host.platform.os_version in ('8', '8.1', '10', 'future'):
                version = 'win10'
            else:
                version = host.platform.os_version
            port_name = port_name + '-' + version
        return port_name

    def __init__(self, host, port_name, **kwargs):
        super(WinPort, self).__init__(host, port_name, **kwargs)
        self._version = port_name[port_name.index('win-') + len('win-'):]
        assert self._version in self.SUPPORTED_VERSIONS, '%s is not in %s' % (self._version, self.SUPPORTED_VERSIONS)
        if self.get_option('disable_breakpad'):
            self._dump_reader = None
        else:
            self._dump_reader = DumpReaderWin(host, self._build_path())

    def additional_driver_flags(self):
        flags = super(WinPort, self).additional_driver_flags()
        if not self.get_option('disable_breakpad'):
            flags += ['--enable-crash-reporter', '--crash-dumps-dir=%s' % self._dump_reader.crash_dumps_directory()]
        return flags

    def check_httpd(self):
        res = super(WinPort, self).check_httpd()
        if self.uses_apache():
            # In order to run CGI scripts on Win32 that use unix shebang lines, we need to
            # create entries in the registry that remap the extensions (.pl and .cgi) to the
            # appropriate Win32 paths. The command line arguments must match the command
            # line arguments in the shebang line exactly.
            if _winreg:
                res = self._check_reg(r'.cgi\Shell\ExecCGI\Command') and res
                res = self._check_reg(r'.pl\Shell\ExecCGI\Command') and res
            else:
                _log.warning('Could not check the registry; http may not work correctly.')

        return res

    def _check_reg(self, sub_key):
        # see comments in check_httpd(), above, for why this routine exists and what it's doing.
        try:
            # Note that we HKCR is a union of HKLM and HKCR (with the latter
            # overriding the former), so reading from HKCR ensures that we get
            # the value if it is set in either place. See als comments below.
            hkey = _winreg.OpenKey(_winreg.HKEY_CLASSES_ROOT, sub_key)
            args = _winreg.QueryValue(hkey, '').split()
            _winreg.CloseKey(hkey)

            # In order to keep multiple checkouts from stepping on each other, we simply check that an
            # existing entry points to a valid path and has the right command line.
            if len(args) == 2 and self._filesystem.exists(args[0]) and args[0].endswith('perl.exe') and args[1] == '-wT':
                return True
        except WindowsError as error:  # WindowsError is not defined on non-Windows platforms - pylint: disable=undefined-variable
            if error.errno != errno.ENOENT:
                raise
            # The key simply probably doesn't exist.

        # Note that we write to HKCU so that we don't need privileged access
        # to the registry, and that will get reflected in HKCR when it is read, above.
        cmdline = self._path_from_chromium_base(
            'third_party', 'perl', 'perl', 'bin', 'perl.exe') + ' -wT'
        hkey = _winreg.CreateKeyEx(_winreg.HKEY_CURRENT_USER, 'Software\\Classes\\' + sub_key, 0, _winreg.KEY_WRITE)
        _winreg.SetValue(hkey, '', _winreg.REG_SZ, cmdline)
        _winreg.CloseKey(hkey)
        return True

    def setup_environ_for_server(self):
        # A few extra environment variables are required for Apache on Windows.
        if 'TEMP' not in self.host.environ:
            self.host.environ['TEMP'] = tempfile.gettempdir()
        # CGIs are run directory-relative so they need an absolute TEMP
        self.host.environ['TEMP'] = self._filesystem.abspath(self.host.environ['TEMP'])
        # Make TMP an alias for TEMP
        self.host.environ['TMP'] = self.host.environ['TEMP']
        env = super(WinPort, self).setup_environ_for_server()
        apache_envvars = ['SYSTEMDRIVE', 'SYSTEMROOT', 'TEMP', 'TMP']
        for key, value in self.host.environ.copy().items():
            if key not in env and key in apache_envvars:
                env[key] = value
        return env

    def check_build(self, needs_http, printer):
        result = super(WinPort, self).check_build(needs_http, printer)

        if result:
            _log.error('For complete Windows build requirements, please see:')
            _log.error('')
            _log.error('    https://chromium.googlesource.com/chromium/src/+/master/docs/windows_build_instructions.md')
        return result

    def operating_system(self):
        return 'win'

    def relative_test_filename(self, filename):
        path = filename[len(self.web_tests_dir()) + 1:]
        return path.replace('\\', '/')

    def uses_apache(self):
        val = self.get_option('use_apache')
        if val is None:
            return True
        return val

    def path_to_apache(self):
        return self._path_from_chromium_base(
            'third_party', 'apache-win32', 'bin', 'httpd.exe')

    def path_to_apache_config_file(self):
        return self._filesystem.join(self.apache_config_directory(), 'win-httpd.conf')

    #
    # PROTECTED ROUTINES
    #

    def _path_to_driver(self, target=None):
        binary_name = '%s.exe' % self.driver_name()
        return self._build_path_with_target(target, binary_name)

    def _path_to_image_diff(self):
        binary_name = 'image_diff.exe'
        return self._build_path(binary_name)

    def look_for_new_crash_logs(self, crashed_processes, start_time):
        if self.get_option('disable_breakpad'):
            return None
        return self._dump_reader.look_for_new_crash_logs(crashed_processes, start_time)

    def clobber_old_port_specific_results(self):
        if not self.get_option('disable_breakpad'):
            self._dump_reader.clobber_old_results()
