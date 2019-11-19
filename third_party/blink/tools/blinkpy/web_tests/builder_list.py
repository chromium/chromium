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

"""Represents a set of builder bots running web tests.

This class is used to hold a list of builder bots running web tests and their
corresponding port names and TestExpectations specifiers.
"""

import json

from blinkpy.common.path_finder import PathFinder


class BuilderList(object):

    def __init__(self, builders_dict):
        """Creates and validates a builders list.

        The given dictionary maps builder names to dicts with the keys:
            "port_name": A fully qualified port name.
            "specifiers": A two-item list: [version specifier, build type specifier].
                Valid values for the version specifier can be found in
                TestExpectationsParser._configuration_tokens_list, and valid
                values for the build type specifier include "Release" and "Debug".
            "is_try_builder": Whether the builder is a trybot.
            "master": The master name of the builder. It is deprecated, but still required
                by test-results.appspot.com API."
            "has_webdriver_tests": Whether webdriver_tests_suite runs on this builder.

        Possible refactoring note: Potentially, it might make sense to use
        blinkpy.common.net.results_fetcher.Builder and add port_name and
        specifiers properties to that class.
        """
        self._builders = builders_dict
        for builder in builders_dict:
            assert 'port_name' in builders_dict[builder]
            assert len(builders_dict[builder]['specifiers']) == 2

    @staticmethod
    def load_default_builder_list(filesystem):
        """Loads the set of builders from a JSON file and returns the BuilderList."""
        path = PathFinder(filesystem).path_from_blink_tools(
            'blinkpy', 'common', 'config', 'builders.json')
        contents = filesystem.read_text_file(path)
        return BuilderList(json.loads(contents))

    def all_builder_names(self):
        return sorted(self._builders)

    def all_try_builder_names(self):
        return sorted(b for b in self._builders if self._builders[b].get('is_try_builder'))

    def all_continuous_builder_names(self):
        return sorted(b for b in self._builders if not self._builders[b].get('is_try_builder'))

    def all_port_names(self):
        return sorted({b['port_name'] for b in self._builders.values()})

    def bucket_for_builder(self, builder_name):
        return self._builders[builder_name].get('bucket', '')

    def master_for_builder(self, builder_name):
        return self._builders[builder_name].get('master', '')

    def has_webdriver_tests_for_builder(self, builder_name):
        return self._builders[builder_name].get('has_webdriver_tests')

    def port_name_for_builder_name(self, builder_name):
        return self._builders[builder_name]['port_name']

    def specifiers_for_builder(self, builder_name):
        return self._builders[builder_name]['specifiers']

    def platform_specifier_for_builder(self, builder_name):
        return self.specifiers_for_builder(builder_name)[0]

    def builder_name_for_port_name(self, target_port_name):
        """Returns a builder name for the given port name.

        Multiple builders can have the same port name; this function only
        returns builder names for non-try-bot builders, and it gives preference
        to non-debug builders. If no builder is found, None is returned.
        """
        debug_builder_name = None
        for builder_name, builder_info in self._builders.iteritems():
            if builder_info.get('is_try_builder'):
                continue
            if builder_info['port_name'] == target_port_name:
                if 'dbg' in builder_name:
                    debug_builder_name = builder_name
                else:
                    return builder_name
        return debug_builder_name

    def version_specifier_for_port_name(self, target_port_name):
        """Returns the OS version specifier for a given port name.

        This just uses information in the builder list, and it returns
        the version specifier for the first builder that matches, even
        if it's a try bot builder.
        """
        for _, builder_info in sorted(self._builders.iteritems()):
            if builder_info['port_name'] == target_port_name:
                return builder_info['specifiers'][0]
        return None

    def builder_name_for_specifiers(self, version, build_type):
        """Returns the builder name for a give version and build type.

        Args:
            version: A string with the OS version specifier. e.g. "Trusty", "Win10".
            build_type: A string with the build type. e.g. "Debug" or "Release".

        Returns:
            The builder name if found, or an empty string if no match was found.
        """
        for builder_name, info in sorted(self._builders.items()):
            specifiers = info['specifiers']
            if specifiers[0].lower() == version.lower() and specifiers[1].lower() == build_type.lower():
                return builder_name
        return ''
