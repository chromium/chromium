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

from __future__ import print_function

import logging

from blinkpy.web_tests.layout_package.bot_test_expectations import BotTestExpectationsFactory
from blinkpy.web_tests.models.typ_types import Expectation
from blinkpy.web_tests.port.base import Port
from blinkpy.tool.commands.command import Command

_log = logging.getLogger(__name__)


class FlakyTests(Command):
    name = 'print-flaky-tests'
    help_text = 'Print out flaky tests based on results from the flakiness dashboard'
    show_in_main_help = True

    FLAKINESS_DASHBOARD_URL = (
        'https://test-results.appspot.com/dashboards/flakiness_dashboard.html'
        '#testType=blink_web_tests&tests=%s')

    BUG_TEMPLATE = (
        'https://code.google.com/p/chromium/issues/entry?owner=FILL_ME_IN&status=Assigned&'
        'labels=Pri-1,Cr-Blink,FlakyLayoutTest&summary=XXXXXXX%20is%20flaky&'
        'comment=XXXXXXX%20is%20flaky.%0A%0AIt%20failed%20twice%20and%20then'
        '%20passed%20on%20the%203rd%20or%204th%20retry.%20This%20is%20too%20'
        'flaky.%20The%20test%20will%20be%20skipped%20until%20it%27s%20fixed.'
        '%20If%20not%20fixed%20in%203%20months,%20it%20will%20be%20deleted%20'
        'or%20perma-skipped.%0A%0AIn%20the%20flakiness%20dashboard,%20the%20'
        'turquoise%20boxes%20are%20runs%20where%20the%20test%20failed%20and%20'
        'then%20passed%20on%20retry.%0A%0Ahttps://test-results.appspot.com'
        '/dashboards/flakiness_dashboard.html%23tests=XXXXXXX')

    HEADER = (
        'Manually add bug numbers for these and then put the lines in web_tests/TestExpectations.\n'
        'Look up the test in the flakiness dashboard first to see if the the platform\n'
        'specifiers should be made more general.\n\n'
        'Bug template:\n%s\n') % BUG_TEMPLATE

    OUTPUT = '%s\n%s\n\nFlakiness dashboard: %s\n'

    def __init__(self):
        super(FlakyTests, self).__init__()
        # This is sorta silly, but allows for unit testing:
        self.expectations_factory = BotTestExpectationsFactory

    def _filter_build_type_specifiers(self, specifiers):
        filtered = []
        for specifier in specifiers:
            if specifier.lower() not in Port.ALL_BUILD_TYPES:
                filtered.append(specifier)
        return filtered

    def _collect_expectation_lines(self, builder_names, factory):
        exps = []
        for builder_name in builder_names:

            expectations = factory.expectations_for_builder(builder_name)

            # TODO(ojan): We should also skip bots that haven't uploaded recently,
            # e.g. if they're >24h stale.
            if not expectations:
                _log.error("Can't load flakiness data for builder: %s",
                           builder_name)
                continue

            for line in expectations.expectation_lines(
                    only_consider_very_flaky=True):
                # TODO(ojan): Find a way to merge specifiers instead of removing build types.
                # We can't just union because some specifiers will change the meaning of others.
                # For example, it's not clear how to merge [ Mac Release ] with [ Linux Debug ].
                # But, in theory we should be able to merge [ Mac Release ] and [ Mac Debug ].
                tags = self._filter_build_type_specifiers(line.tags)
                exps.append(
                    Expectation(
                        tags=tags, results=line.results, test=line.test))
        return exps

    def execute(self, options, args, tool):
        factory = self.expectations_factory(tool.builders)
        lines = self._collect_expectation_lines(
            tool.builders.all_continuous_builder_names(), factory)
        lines.sort(key=lambda line: line.test)

        port = tool.port_factory.get()
        # Skip any tests which are mentioned in the dashboard but not in our checkout:
        fs = tool.filesystem
        lines = [
            line for line in lines
            if fs.exists(fs.join(port.web_tests_dir(), line.test))
        ]

        test_names = [line.test for line in lines]
        flakiness_dashboard_url = self.FLAKINESS_DASHBOARD_URL % \
            ','.join(test_names)
        expectations_string = '\n'.join(line.to_string() for line in lines)

        print(self.OUTPUT %
              (self.HEADER, expectations_string, flakiness_dashboard_url))
