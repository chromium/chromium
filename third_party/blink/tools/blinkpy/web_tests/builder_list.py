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
from typing import Set

from blinkpy.common.path_finder import PathFinder


class BuilderList:
    def __init__(self, builders_dict):
        """Creates and validates a builders list.

        The given dictionary maps builder names to dicts with the keys:
            "port_name": A fully qualified port name.
            "specifiers": A list of specifiers used to describe a builder.
                The specifiers list will at the very least have a valid
                port version specifier like "Mac10.15" and and a valid build
                type specifier like "Release".
            "is_try_builder": Whether the builder is a trybot.
            "main": The main name of the builder. It is deprecated, but still required
                by test-results.appspot.com API."
            "has_webdriver_tests": Whether webdriver_tests_suite runs on this builder.

        Possible refactoring note: Potentially, it might make sense to use
        blinkpy.common.net.results_fetcher.Builder and add port_name and
        specifiers properties to that class.
        """
        self._builders = builders_dict
        for builder in builders_dict:
            specifiers = {
                s.lower() for s in builders_dict[builder].get('specifiers', {})}
            assert 'port_name' in builders_dict[builder]
            assert ('android' in specifiers or
                    len(builders_dict[builder]['specifiers']) == 2)
        self._flag_spec_to_port = self._find_ports_for_flag_specific_options()

    def __repr__(self):
        return 'BuilderList(%s)' % self._builders

    def _find_ports_for_flag_specific_options(self):
        flag_spec_to_port = {}
        for builder_name, builder in self._builders.items():
            port_name = self.port_name_for_builder_name(builder_name)
            for step_name in self.step_names_for_builder(builder_name):
                option = self.flag_specific_option(builder_name, step_name)
                if not option:
                    continue
                maybe_port_name = flag_spec_to_port.get(option)
                if maybe_port_name and maybe_port_name != port_name:
                    raise ValueError(
                        'Flag-specific suite %r can only run on one port, got: '
                        '%r, %r' % (option, maybe_port_name, port_name))
                flag_spec_to_port[option] = port_name
        return flag_spec_to_port

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
        return self.filter_builders(is_try=True)

    def all_cq_try_builder_names(self):
        return self.filter_builders(is_cq=True)

    def all_flag_specific_try_builder_names(self, flag_specific):
        return self.filter_builders(is_try=True, flag_specific=flag_specific)

    def builders_for_rebaselining(self) -> Set[str]:
        try_builders = {
            builder
            for builder in self.filter_builders(is_try=True,
                                                exclude_specifiers={'android'})
            if not self.uses_wptrunner(builder)
        }
        # Remove CQ builders whose port is a duplicate of a *-blink-rel builder
        # to avoid wasting resources.
        for blink_builder, cq_builder in self.try_bots_with_cq_mirror():
            if blink_builder in try_builders and cq_builder in try_builders:
                try_builders.remove(cq_builder)
        return try_builders

    def try_bots_with_cq_mirror(self):
        """Returns a sorted list of (try_builder_names, cq_mirror_builder_names).

        When all steps in a blink-rel trybot exist in a cq trybot and the port
        name matches, we say that blink-rel trybot has a cq mirror, and thus
        there is no need to trigger both the blink-rel trybot and its cq mirror.

        As of today, this should return:
        [("linux-blink-rel", "linux-rel"),
         ("mac12.0-blink-rel", "mac-rel"),
         ("win10.20h2-blink-rel", "win-rel")]
        """
        rv = []
        all_blink_rel_trybots = sorted(
            set(self.all_try_builder_names()) -
            set(self.all_cq_try_builder_names()))
        for builder_name in all_blink_rel_trybots:
            step_names = set(self.step_names_for_builder(builder_name))
            for cq_builder_name in self.all_cq_try_builder_names():
                if (self.port_name_for_builder_name(cq_builder_name) !=
                        self.port_name_for_builder_name(builder_name)):
                    continue
                cq_step_names = self.step_names_for_builder(cq_builder_name)
                if step_names.issubset(cq_step_names):
                    rv.append((builder_name, cq_builder_name))
                    break
        return rv

    def all_continuous_builder_names(self):
        return self.filter_builders(is_try=False)

    def filter_builders(self,
                        exclude_specifiers=None,
                        include_specifiers=None,
                        is_try=False,
                        is_cq=False,
                        flag_specific=None):
        _lower_specifiers = lambda specifiers: {s.lower() for s in specifiers}
        exclude_specifiers = _lower_specifiers(exclude_specifiers or {})
        include_specifiers = _lower_specifiers(include_specifiers or {})
        builders = []
        for b, builder in self._builders.items():
            builder_specifiers = _lower_specifiers(
                builder.get('specifiers', {}))
            flag_specific_suites = {
                step.get('flag_specific')
                for step in builder.get('steps', {}).values()
            }
            if flag_specific:
                if flag_specific == '*' and not any(flag_specific_suites):
                    # Skip non flag_specific builders
                    continue
                if (flag_specific != '*'
                        and flag_specific not in flag_specific_suites):
                    # Skip if none of the steps has an exact match
                    continue
            if is_try and builder.get('is_try_builder', False) != is_try:
                continue
            if is_cq and builder.get('is_cq_builder', False) != is_cq:
                continue
            if ((not is_cq and not is_try)
                    and builder.get('is_try_builder', False)):
                continue
            if builder_specifiers & exclude_specifiers:
                continue
            if  (include_specifiers and
                     not include_specifiers & builder_specifiers):
                continue
            builders.append(b)
        return sorted(builders)

    def all_port_names(self):
        return sorted({b['port_name'] for b in self._builders.values()})

    def bucket_for_builder(self, builder_name):
        return self._builders[builder_name].get('bucket', '')

    def main_for_builder(self, builder_name):
        return self._builders[builder_name].get('main', '')

    def has_webdriver_tests_for_builder(self, builder_name):
        return self._builders[builder_name].get('has_webdriver_tests')

    def port_name_for_builder_name(self, builder_name):
        return self._builders[builder_name]['port_name']

    def port_name_for_flag_specific_option(self, option):
        return self._flag_spec_to_port[option]

    def all_flag_specific_options(self) -> Set[str]:
        return set(self._flag_spec_to_port)

    def specifiers_for_builder(self, builder_name):
        return self._builders[builder_name]['specifiers']

    def _steps(self, builder_name):
        return self._builders[builder_name].get('steps', {})

    def step_names_for_builder(self, builder_name):
        return sorted(self._steps(builder_name))

    def is_try_server_builder(self, builder_name):
        return self._builders[builder_name].get('is_try_builder', False)

    def uses_wptrunner(self, builder_name: str) -> bool:
        return any(
            step.get('uses_wptrunner', 'wpt_tests_suite' in step_name)
            for step_name, step in self._steps(builder_name).items())

    def product_for_build_step(self, builder_name: str, step_name: str) -> str:
        steps = self._steps(builder_name)
        return steps[step_name].get('product', 'content_shell')

    def has_experimental_steps(self, builder_name):
        steps = self.step_names_for_builder(builder_name)
        return any(['experimental' in step for step in steps])

    def flag_specific_option(self, builder_name, step_name):
        steps = self._steps(builder_name)
        # TODO(crbug/1291020): We cannot validate the step name here because
        # some steps are retrieved from the results server instead of read from
        # 'builders.json'. Once all the steps are in the config, we can allow
        # bad step names to raise an exception.
        return steps.get(step_name, {}).get('flag_specific')

    def flag_specific_options_for_port_name(self, port_name):
        return {
            option
            for option, port in self._flag_spec_to_port.items()
            if port == port_name
        }

    def platform_specifier_for_builder(self, builder_name):
        return self.specifiers_for_builder(builder_name)[0]

    def builder_name_for_port_name(self, target_port_name):
        """Returns a builder name for the given port name.

        Multiple builders can have the same port name; this function only
        returns builder names for non-try-bot builders, and it gives preference
        to non-debug builders. If no builder is found, None is returned.
        """
        debug_builder_name = None
        for builder_name, builder_info in list(self._builders.items()):
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
        for _, builder_info in sorted(self._builders.items()):
            if builder_info['port_name'] == target_port_name:
                return builder_info['specifiers'][0]
        return None

    def builder_name_for_specifiers(self, version, build_type, is_try_builder):
        """Returns the builder name for a give version and build type.

        Args:
            version: A string with the OS version specifier. e.g. "Trusty", "Win10".
            build_type: A string with the build type. e.g. "Debug" or "Release".

        Returns:
            The builder name if found, or an empty string if no match was found.
        """
        for builder_name, info in sorted(self._builders.items()):
            specifiers = set(spec.lower() for spec in info['specifiers'])
            is_try_builder_info = info.get('is_try_builder', False)
            if (version.lower() in specifiers
                    and build_type.lower() in specifiers
                    and is_try_builder_info == is_try_builder):
                return builder_name
        return ''
