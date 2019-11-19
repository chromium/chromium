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

"""Chromium Mac implementation of the Port interface."""

import logging

from blinkpy.web_tests.port import base


_log = logging.getLogger(__name__)


class MacPort(base.Port):
    SUPPORTED_VERSIONS = ('mac10.10', 'mac10.11', 'mac10.12', 'mac10.13', 'mac10.14', 'mac10.15', 'retina')
    port_name = 'mac'

    # FIXME: We treat Retina (High-DPI) devices as if they are running a
    # different operating system version. This is lame and should be fixed.
    # Note that the retina versions fallback to the non-retina versions and so
    # no baselines are shared between retina versions; this keeps the fallback
    # graph as a tree and maximizes the number of baselines we can share that
    # way. We also currently only support Retina on 10.13.

    FALLBACK_PATHS = {}

    FALLBACK_PATHS['mac10.15'] = ['mac']
    FALLBACK_PATHS['mac10.14'] = ['mac']
    FALLBACK_PATHS['mac10.13'] = ['mac']
    FALLBACK_PATHS['mac10.12'] = ['mac-mac10.12'] + FALLBACK_PATHS['mac10.13']
    FALLBACK_PATHS['mac10.11'] = ['mac-mac10.11'] + FALLBACK_PATHS['mac10.12']
    FALLBACK_PATHS['mac10.10'] = ['mac-mac10.10'] + FALLBACK_PATHS['mac10.11']
    FALLBACK_PATHS['retina'] = ['mac-retina'] + FALLBACK_PATHS['mac10.13']

    CONTENT_SHELL_NAME = 'Content Shell'

    BUILD_REQUIREMENTS_URL = 'https://chromium.googlesource.com/chromium/src/+/master/docs/mac_build_instructions.md'

    @classmethod
    def determine_full_port_name(cls, host, options, port_name):
        if port_name.endswith('mac'):
            version = host.platform.os_version
            if host.platform.is_highdpi():
                version = 'retina'
            return port_name + '-' + version
        return port_name

    def __init__(self, host, port_name, **kwargs):
        super(MacPort, self).__init__(host, port_name, **kwargs)
        self._version = port_name[port_name.index('mac-') + len('mac-'):]
        assert self._version in self.SUPPORTED_VERSIONS

    def check_build(self, needs_http, printer):
        result = super(MacPort, self).check_build(needs_http, printer)
        if result:
            _log.error('For complete Mac build requirements, please see:')
            _log.error('')
            _log.error('    https://chromium.googlesource.com/chromium/src/+/master/docs/mac_build_instructions.md')

        return result

    def operating_system(self):
        return 'mac'

    #
    # PROTECTED METHODS
    #

    def path_to_apache(self):
        return self._path_from_chromium_base(
            'third_party', 'apache-mac', 'bin', 'httpd')

    def path_to_apache_config_file(self):
        config_file_basename = 'apache2-httpd-%s-php7.conf' % (self._apache_version(),)
        return self._filesystem.join(self.apache_config_directory(), config_file_basename)

    def _path_to_driver(self, target=None):
        return self._build_path_with_target(target, self.driver_name() + '.app', 'Contents', 'MacOS', self.driver_name())
