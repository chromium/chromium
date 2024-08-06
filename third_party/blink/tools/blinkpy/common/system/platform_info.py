# Copyright (c) 2011 Google Inc. All rights reserved.
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

import re
import sys

from blinkpy.common.memoized import memoized
from blinkpy.common.system.executive import ScriptError


class PlatformInfo:
    """This class provides a consistent (and mockable) interpretation of
    system-specific values (like sys.platform and platform.mac_ver())
    to be used by the rest of the blinkpy code base.

    Public (static) properties:
    -- os_name
    -- os_version

    Note that 'future' is returned for os_version if the operating system is
    newer than one known to the code.
    """

    def __init__(self, sys_module, platform_module, filesystem_module,
                 executive):
        self._executive = executive
        self._filesystem = filesystem_module
        self._platform_module = platform_module
        self.os_name = self._determine_os_name(sys_module.platform)
        if self.os_name == 'linux':
            self.os_version = self._determine_linux_version(platform_module)
        if self.os_name == 'freebsd':
            self.os_version = platform_module.release()
        if self.os_name.startswith('mac'):
            self.os_version = self._determine_mac_version(
                self._raw_mac_version(platform_module))
        if self.os_name.startswith('win'):
            self.os_version = self._determine_win_version(
                self._win_version_tuple())
        self.interactive = sys_module.stdin.isatty()
        self._processor = platform_module.processor() or ""

        assert sys.platform != 'cygwin', 'Cygwin is not supported.'

    def is_mac(self):
        return self.os_name == 'mac'

    def is_win(self):
        return self.os_name == 'win'

    def is_linux(self):
        return self.os_name == 'linux'

    def is_freebsd(self):
        return self.os_name == 'freebsd'

    def processor(self):
        return self._processor

    @memoized
    def is_highdpi(self):
        if self.is_mac():
            output = self._executive.run_command(
                ['system_profiler', 'SPDisplaysDataType'],
                error_handler=self._executive.ignore_error)
            if output and re.search(r'Resolution:.*Retina$', output,
                                    re.MULTILINE):
                return True
        return False

    @memoized
    def is_running_rosetta(self):
        if self.is_mac():
            # If we are running under Rosetta, platform.machine() is
            # 'x86_64'; we need to use a sysctl to see if we're being
            # translated.
            import ctypes
            libSystem = ctypes.CDLL("libSystem.dylib")
            ret = ctypes.c_int(0)
            size = ctypes.c_size_t(4)
            e = libSystem.sysctlbyname(ctypes.c_char_p(b'sysctl.proc_translated'),
                                       ctypes.byref(ret), ctypes.byref(size), None, 0)
            return e == 0 and ret.value == 1
        return False

    def display_name(self):
        # platform.platform() returns Darwin information for Mac, which is just confusing.
        if self.is_mac():
            return 'Mac OS X %s' % self._platform_module.mac_ver()[0]

        # Returns strings like:
        # Linux-2.6.18-194.3.1.el5-i686-with-redhat-5.5-Final
        # Windows-2008ServerR2-6.1.7600
        return self._platform_module.platform()

    @memoized
    def total_bytes_memory(self):
        if self.is_mac():
            return int(
                self._executive.run_command(['sysctl', '-n', 'hw.memsize']))
        return None

    def terminal_width(self):
        """Returns sys.maxint if the width cannot be determined."""
        try:
            if self.is_win():
                # From http://code.activestate.com/recipes/440694-determine-size-of-console-window-on-windows/
                from ctypes import windll, create_string_buffer
                handle = windll.kernel32.GetStdHandle(-12)  # -12 == stderr
                # 22 == sizeof(console_screen_buffer_info)
                console_screen_buffer_info = create_string_buffer(22)
                if windll.kernel32.GetConsoleScreenBufferInfo(
                        handle, console_screen_buffer_info):
                    import struct
                    _, _, _, _, _, left, _, right, _, _, _ = struct.unpack(
                        'hhhhHhhhhhh', console_screen_buffer_info.raw)
                    # Note that we return 1 less than the width since writing into the rightmost column
                    # automatically performs a line feed.
                    return right - left
                return sys.maxsize
            else:
                import fcntl
                import struct
                import termios
                packed = fcntl.ioctl(sys.stderr.fileno(), termios.TIOCGWINSZ,
                                     '\0' * 8)
                _, columns, _, _ = struct.unpack('HHHH', packed)
                return columns
        except Exception:  # pylint: disable=broad-except
            return sys.maxsize

    def get_machine(self):
        return self._platform_module.machine()

    def linux_distribution(self):
        if not self.is_linux():
            return None

        # Fedora also has /etc/redhat-release, this check must go first.
        if self._filesystem.exists('/etc/fedora-release'):
            return 'fedora'
        if self._filesystem.exists('/etc/redhat-release'):
            return 'redhat'
        if self._filesystem.exists('/etc/debian_version'):
            return 'debian'
        if self._filesystem.exists('/etc/arch-release'):
            return 'arch'

        return 'unknown'

    @memoized
    def _raw_mac_version(self, platform_module):
        """Read this Mac's version string (starts with "<major>.<minor>")."""
        try:
            # crbug/1294954: Python's `platform.mac_ver()` can be unreliable.
            command = ['sw_vers', '-productVersion']
            output = self._executive.run_command(command).strip()
            if re.match(r'\d+\.\d+', output):
                return output
        except (OSError, SystemError, ScriptError):
            pass
        return platform_module.mac_ver()[0]

    def _determine_os_name(self, sys_platform):
        if sys_platform == 'darwin':
            return 'mac'
        if sys_platform.startswith('linux'):
            return 'linux'
        if sys_platform == 'win32':
            return 'win'
        if sys_platform.startswith('freebsd'):
            return 'freebsd'
        raise AssertionError(
            'unrecognized platform string "%s"' % sys_platform)

    def _determine_mac_version(self, mac_version_string):
        major_release = int(mac_version_string.split('.')[0])
        minor_release = int(mac_version_string.split('.')[1])
        assert 11 <= major_release, 'Unsupported mac OS version: %s' % mac_version_string
        return 'mac{major_release}'.format(
            major_release=min(15, major_release))

    def _determine_linux_version(self, _):
        # Assume we only test against one Linux version at a time (see
        # crbug.com/1468322). Therefore, there's no need to name that version.
        return 'linux'

    def _determine_win_version(self, win_version_tuple):
        if win_version_tuple[:2] == (10, 0):
            # For win11 platform.win32_ver() returns (10, 0, 22000)
            if win_version_tuple[2] >= 22000:
                return '11'
            else:
                return '10.20h2'
        if win_version_tuple[:2] == (6, 3):
            return '8.1'
        if win_version_tuple[:2] == (6, 2):
            return '8'
        if win_version_tuple[:3] == (6, 1, 7601):
            return '7sp1'
        if win_version_tuple[:3] == (6, 1, 7600):
            return '7sp0'
        if win_version_tuple[:2] == (6, 0):
            return 'vista'
        if win_version_tuple[:2] == (5, 1):
            return 'xp'
        assert (win_version_tuple[0] > 10
                or win_version_tuple[0] == 10 and win_version_tuple[1] > 0), (
                    'Unrecognized Windows version tuple: "%s"' %
                    (win_version_tuple, ))
        return 'future'

    def _win_version_tuple(self):
        version_str = self._platform_module.win32_ver()[1]
        if version_str:
            return tuple(map(int, version_str.split('.')))

        return self._win_version_tuple_from_cmd()

    @memoized
    def _win_version_tuple_from_cmd(self):
        # Note that this should only ever be called on windows, so this should always work.
        ver_output = self._executive.run_command(['cmd', '/c', 'ver'],
                                                 decode_output=False)
        match_object = re.search(
            r'(?P<major>\d+)\.(?P<minor>\d)\.(?P<build>\d+)', ver_output)
        assert match_object, 'cmd returned an unexpected version string: ' + ver_output
        return tuple(map(int, match_object.groups()))
