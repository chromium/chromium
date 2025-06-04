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
"""Factory method to retrieve the appropriate port implementation."""

import fnmatch
import optparse
import re
import sys
from copy import deepcopy

from blinkpy.common.path_finder import PathFinder


class PortFactory:
    PORT_CLASSES = (
        'android.AndroidPort',
        'fuchsia.FuchsiaPort',
        'ios.IOSPort',
        'linux.LinuxPort',
        'mac.MacPort',
        'mock_drt.MockDRTPort',
        'test.TestPort',
        'webview.WebviewPort',
        'win.WinPort',
    )

    def __init__(self, host):
        self._host = host

    def _default_port(self):
        platform = self._host.platform
        if platform.is_linux() or platform.is_freebsd():
            return 'linux'
        elif platform.is_mac():
            return 'mac'
        elif platform.is_win():
            return 'win'
        raise NotImplementedError('unknown platform: %s' % platform)

    def get(self, port_name=None, options=None, **kwargs):
        """Returns an object implementing the Port interface.

        If port_name is None, this routine attempts to guess at the most
        appropriate port on this platform.
        """
        port_name = port_name or self._default_port()
        port_options = deepcopy(options) or optparse.Values()

        _update_configuration_and_target(self._host.filesystem, port_options)

        port_class, class_name = self.get_port_class(port_name)
        if port_class is None:
            raise NotImplementedError('unsupported platform: "%s"' % port_name)

        full_port_name = port_class.determine_full_port_name(
            self._host, port_options, port_name)
        return port_class(self._host,
                          full_port_name,
                          options=port_options,
                          **kwargs)

    @classmethod
    def get_port_class(cls, port_name):
        """Returns a Port subclass and its name for the given port_name."""
        for port_class in cls.PORT_CLASSES:
            module_name, class_name = port_class.rsplit('.', 1)
            try:
                module = __import__(module_name, globals(), locals(), [], -1)
            except ValueError:
                # Python3 doesn't allow the level param to be -1. Setting it
                # to 1 searches for modules in 1 parent directory.
                module = __import__(module_name, globals(), locals(), [], 1)
            port_class = module.__dict__[class_name]
            if port_name.startswith(port_class.port_name):
                return port_class, class_name
        return None, None

    def all_port_names(self, platform=None):
        """Returns a list of all valid, fully-specified, "real" port names.

        This is the list of directories that are used as actual baseline_paths()
        by real ports. This does not include any "fake" names like "test"
        or "mock-mac", and it does not include any directories that are not
        port names.

        If platform is not specified, all known port names will be returned.
        """
        platform = platform or '*'
        return fnmatch.filter(self._host.builders.all_port_names(), platform)

    def get_from_builder_name(self, builder_name):
        port_name = self._host.builders.port_name_for_builder_name(
            builder_name)
        assert port_name, 'unrecognized builder name: "%s"' % builder_name
        return self.get(port_name, options=_builder_options(builder_name))


def _builder_options(builder_name):
    return optparse.Values({
        'builder_name':
        builder_name,
        'configuration':
        'Debug' if re.search(r'[d|D](ebu|b)g', builder_name) else 'Release',
        'target':
        None,
    })


def _update_configuration_and_target(host, options):
    """Updates options.configuration and options.target based on a best guess."""
    if not getattr(options, 'target', None):
        options.target = getattr(options, 'configuration', None) or 'Release'

    gn_configuration = _read_configuration_from_gn(host, options)
    if gn_configuration:
        expected_configuration = getattr(options, 'configuration', None)
        if expected_configuration not in (None, gn_configuration):
            raise ValueError('Configuration does not match the GN build args. '
                             'Expected "%s" but got "%s".' %
                             (expected_configuration, gn_configuration))
        options.configuration = gn_configuration
        return

    if getattr(options, 'configuration', None):
        return

    if options.target in ('Debug', 'Debug_x64'):
        options.configuration = 'Debug'
    elif options.target in ('Release', 'Release_x64'):
        options.configuration = 'Release'
    else:
        raise ValueError(
            'Could not determine build configuration type.\n'
            'Either switch to one of the default target directories,\n'
            'use args.gn, or specify --debug or --release explicitly.\n'
            'If the directory is out/<dir>, then pass -t <dir>.')


def _read_configuration_from_gn(fs, options):
    """Returns the configuration to used based on args.gn, if possible."""
    build_directory = getattr(options, 'build_directory', None)
    finder = PathFinder(fs)
    if not build_directory:
        build_directory = fs.join(finder.chromium_base(), 'out',
                                  options.target)
    path = fs.join(build_directory, 'args.gn')
    if not fs.exists(path):
        path = fs.join(build_directory, 'toolchain.ninja')
        if not fs.exists(path):
            # This does not appear to be a GN-based build directory, so we don't know
            # how to interpret it.
            return None

        # toolchain.ninja exists, but args.gn does not; this can happen when
        # `gn gen` is run with no --args.
        return 'Debug'

    args = fs.read_text_file(path)
    for line in args.splitlines():
        if re.match(r'^\s*is_debug\s*=\s*false(\s*$|\s*#.*$)', line):
            return 'Release'

    # If is_debug is not set, the default is based on if is_official_build
    # is set to true.
    for line in args.splitlines():
        if re.match(r'^\s*is_official_build\s*=\s*true(\s*$|\s*#.*$)', line):
            return 'Release'

    # If is_debug is set to anything other than false, or if it
    # does not exist at all, we should use the default value (True).
    return 'Debug'
