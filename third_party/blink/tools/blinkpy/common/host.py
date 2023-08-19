# Copyright (c) 2010 Google Inc. All rights reserved.
# Copyright (c) 2009 Apple Inc. All rights reserved.
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

import logging

from blinkpy.common.checkout.git import Git
from blinkpy.common.net import web
from blinkpy.common.net.bb_agent import BBAgent
from blinkpy.common.net.results_fetcher import TestResultsFetcher
from blinkpy.common.system.system_host import SystemHost
from blinkpy.web_tests.builder_list import BuilderList
from blinkpy.web_tests.port.factory import PortFactory
from blinkpy.w3c.chromium_configs import ChromiumWPTConfig

_log = logging.getLogger(__name__)


class Host(SystemHost):
    def __init__(self, project_config_factory=ChromiumWPTConfig):
        SystemHost.__init__(self)
        self.web = web.Web()

        self._git = None

        self.project_config = project_config_factory(self.filesystem)

        # FIXME: Unfortunately Port objects are currently the central-dispatch objects of the NRWT world.
        # In order to instantiate a port correctly, we have to pass it at least an executive, user, git, and filesystem
        # so for now we just pass along the whole Host object.
        # FIXME: PortFactory doesn't belong on this Host object if Port is going to have a Host (circular dependency).
        self.port_factory = PortFactory(self)

        self.bb_agent = BBAgent(self)
        self.builders = BuilderList.load_default_builder_list(self.filesystem)
        self.results_fetcher = TestResultsFetcher.from_host(self)

    def git(self, path=None):
        if path:
            return Git(
                cwd=path,
                executive=self.executive,
                filesystem=self.filesystem,
                platform=self.platform)
        if not self._git:
            self._git = Git(
                filesystem=self.filesystem,
                executive=self.executive,
                platform=self.platform)
        return self._git
