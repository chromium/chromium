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

import optparse
import unittest

from blinkpy.common.host_mock import MockHost
from blinkpy.common.path_finder import PathFinder
from blinkpy.web_tests.builder_list import BuilderList
from blinkpy.web_tests.port import android
from blinkpy.web_tests.port import factory
from blinkpy.web_tests.port import linux
from blinkpy.web_tests.port import mac
from blinkpy.web_tests.port import win


class FactoryTest(unittest.TestCase):
    """Test that the factory creates the proper port object for given combination of port_name, host.platform, and options."""
    # FIXME: The ports themselves should expose what options they require,
    # instead of passing generic "options".

    def setUp(self):
        self.webkit_options = optparse.Values({})

    def assert_port(self, port_name=None, os_name=None, os_version=None, options=None, cls=None):
        host = MockHost(os_name=os_name, os_version=os_version)
        port = factory.PortFactory(host).get(port_name, options=options)
        self.assertIsInstance(port, cls)

    def test_mac(self):
        self.assert_port(port_name='mac', os_name='mac', os_version='mac10.11',
                         cls=mac.MacPort)

    def test_linux(self):
        self.assert_port(port_name='linux', os_name='linux', os_version='trusty',
                         cls=linux.LinuxPort)

    def test_android(self):
        self.assert_port(port_name='android', cls=android.AndroidPort)

    def test_win(self):
        self.assert_port(port_name='win-win7', cls=win.WinPort)
        self.assert_port(port_name='win-win10', cls=win.WinPort)
        self.assert_port(port_name='win', os_name='win', os_version='win7',
                         cls=win.WinPort)

    def test_unknown_specified(self):
        with self.assertRaises(NotImplementedError):
            factory.PortFactory(MockHost()).get(port_name='unknown')

    def test_unknown_default(self):
        with self.assertRaises(NotImplementedError):
            factory.PortFactory(MockHost(os_name='vms')).get()

    def test_get_from_builder_name(self):
        host = MockHost()
        host.builders = BuilderList({
            'My Fake Mac10.12 Builder': {
                'port_name': 'mac-mac10.12',
                'specifiers': ['Mac10.12', 'Release'],
            }
        })
        self.assertEqual(factory.PortFactory(host).get_from_builder_name('My Fake Mac10.12 Builder').name(),
                         'mac-mac10.12')

    def get_port(self, target=None, configuration=None, files=None):
        host = MockHost()
        finder = PathFinder(host.filesystem)
        files = files or {}
        for path, contents in files.items():
            host.filesystem.write_text_file(finder.path_from_chromium_base(path), contents)
        options = optparse.Values({'target': target, 'configuration': configuration})
        return factory.PortFactory(host).get(options=options)

    def test_default_target_and_configuration(self):
        port = self.get_port()
        self.assertEqual(port._options.configuration, 'Release')
        self.assertEqual(port._options.target, 'Release')

    def test_debug_configuration(self):
        port = self.get_port(configuration='Debug')
        self.assertEqual(port._options.configuration, 'Debug')
        self.assertEqual(port._options.target, 'Debug')

    def test_release_configuration(self):
        port = self.get_port(configuration='Release')
        self.assertEqual(port._options.configuration, 'Release')
        self.assertEqual(port._options.target, 'Release')

    def test_debug_target(self):
        port = self.get_port(target='Debug')
        self.assertEqual(port._options.configuration, 'Debug')
        self.assertEqual(port._options.target, 'Debug')

    def test_debug_x64_target(self):
        port = self.get_port(target='Debug_x64')
        self.assertEqual(port._options.configuration, 'Debug')
        self.assertEqual(port._options.target, 'Debug_x64')

    def test_release_x64_target(self):
        port = self.get_port(target='Release_x64')
        self.assertEqual(port._options.configuration, 'Release')
        self.assertEqual(port._options.target, 'Release_x64')

    def test_release_args_gn(self):
        port = self.get_port(target='foo', files={'out/foo/args.gn': 'is_debug = false'})
        self.assertEqual(port._options.configuration, 'Release')
        self.assertEqual(port._options.target, 'foo')

        # Also test that we handle multi-line args files properly.
        port = self.get_port(target='foo', files={'out/foo/args.gn': 'is_debug = false\nfoo = bar\n'})
        self.assertEqual(port._options.configuration, 'Release')
        self.assertEqual(port._options.target, 'foo')

        port = self.get_port(target='foo', files={'out/foo/args.gn': 'foo=bar\nis_debug=false\n'})
        self.assertEqual(port._options.configuration, 'Release')
        self.assertEqual(port._options.target, 'foo')

    def test_debug_args_gn(self):
        port = self.get_port(target='foo', files={'out/foo/args.gn': 'is_debug = true'})
        self.assertEqual(port._options.configuration, 'Debug')
        self.assertEqual(port._options.target, 'foo')

    def test_default_gn_build(self):
        port = self.get_port(target='Default', files={'out/Default/toolchain.ninja': ''})
        self.assertEqual(port._options.configuration, 'Debug')
        self.assertEqual(port._options.target, 'Default')

    def test_empty_args_gn(self):
        port = self.get_port(target='foo', files={'out/foo/args.gn': ''})
        self.assertEqual(port._options.configuration, 'Debug')
        self.assertEqual(port._options.target, 'foo')

    def test_unknown_dir(self):
        with self.assertRaises(ValueError):
            self.get_port(target='unknown')

    def test_both_configuration_and_target_is_an_error(self):
        with self.assertRaises(ValueError):
            self.get_port(target='Debug', configuration='Release',
                          files={'out/Debug/toolchain.ninja': ''})
