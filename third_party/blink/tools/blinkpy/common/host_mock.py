# Copyright (C) 2011 Google Inc. All rights reserved.
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

from blinkpy.common.checkout.git_mock import MockGit
from blinkpy.common.net.results_fetcher_mock import MockTestResultsFetcher
from blinkpy.common.net.web_mock import MockWeb
from blinkpy.common.path_finder import PathFinder
from blinkpy.common.system.system_host_mock import MockSystemHost

# New-style ports need to move down into blinkpy.common.
from blinkpy.web_tests.builder_list import BuilderList
from blinkpy.web_tests.port.factory import PortFactory
from blinkpy.web_tests.port.test import add_unit_tests_to_mock_filesystem
from blinkpy.w3c.wpt_manifest import BASE_MANIFEST_NAME


class MockHost(MockSystemHost):
    def __init__(self,
                 log_executive=False,
                 web=None,
                 git=None,
                 os_name=None,
                 os_version=None,
                 time_return_val=123):
        super(MockHost, self).__init__(
            log_executive=log_executive,
            os_name=os_name,
            os_version=os_version,
            time_return_val=time_return_val)

        add_unit_tests_to_mock_filesystem(self.filesystem)
        self._add_base_manifest_to_mock_filesystem(self.filesystem)
        self.web = web or MockWeb()
        self._git = git

        self.results_fetcher = MockTestResultsFetcher()

        # Note: We're using a real PortFactory here. Tests which don't wish to depend
        # on the list of known ports should override this with a MockPortFactory.
        self.port_factory = PortFactory(self)

        self.builders = BuilderList({
            'Fake Test Win10': {
                'port_name': 'win-win10',
                'specifiers': ['Win10', 'Release']
            },
            'Fake Test Linux': {
                'port_name': 'linux-trusty',
                'specifiers': ['Trusty', 'Release']
            },
            'Fake Test Linux (dbg)': {
                'port_name': 'linux-trusty',
                'specifiers': ['Trusty', 'Debug']
            },
            'Fake Test Mac10.12': {
                'port_name': 'mac-mac10.12',
                'specifiers': ['Mac10.12', 'Release'],
                'is_try_builder': True,
            },
            'fake_blink_try_linux': {
                'port_name': 'linux-trusty',
                'specifiers': ['Trusty', 'Release'],
                'is_try_builder': True,
            },
            'fake_blink_try_win': {
                'port_name': 'win-win10',
                'specifiers': ['Win10', 'Release'],
                'is_try_builder': True,
            },
            'android_blink_rel': {
                'bucket': 'luci.chromium.android',
                'port_name': 'android-kitkat',
                'specifiers': ['KitKat', 'Release'],
                'is_try_builder': True,
            },
        })

    def git(self, path=None):
        if path:
            return MockGit(
                cwd=path,
                filesystem=self.filesystem,
                executive=self.executive,
                platform=self.platform)
        if not self._git:
            self._git = MockGit(
                filesystem=self.filesystem,
                executive=self.executive,
                platform=self.platform)
        # Various pieces of code (wrongly) call filesystem.chdir(checkout_root).
        # Making the checkout_root exist in the mock filesystem makes that chdir not raise.
        self.filesystem.maybe_make_directory(self._git.checkout_root)
        return self._git

    def _add_base_manifest_to_mock_filesystem(self, filesystem):
        path_finder = PathFinder(filesystem)

        external_dir = path_finder.path_from_web_tests('external')
        filesystem.maybe_make_directory(filesystem.join(external_dir, 'wpt'))

        manifest_base_path = filesystem.join(external_dir, BASE_MANIFEST_NAME)
        filesystem.files[manifest_base_path] = '{"manifest": "base"}'
