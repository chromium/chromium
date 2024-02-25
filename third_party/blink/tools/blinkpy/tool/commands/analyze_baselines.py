# Copyright (c) 2016 Google Inc. All rights reserved.
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
# (INCLUDING NEGLIGENCE OR/ OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

from __future__ import print_function

import logging
import optparse
from typing import get_args

from blinkpy.common.checkout.baseline_optimizer import BaselineOptimizer
from blinkpy.common.net.web_test_results import BaselineSuffix
from blinkpy.tool.commands.rebaseline import AbstractRebaseliningCommand

_log = logging.getLogger(__name__)


class AnalyzeBaselines(AbstractRebaseliningCommand):
    name = 'analyze-baselines'
    help_text = 'Analyzes the baselines for the given tests and prints results that are identical.'
    show_in_main_help = True
    argument_names = 'TEST_NAMES'

    def __init__(self):
        super(AnalyzeBaselines, self).__init__(options=[
            self.suffixes_option,
            optparse.make_option(
                '--missing',
                action='store_true',
                default=False,
                help='Show missing baselines as well.'),
        ] + self.platform_options)
        self._baseline_suffix_list = get_args(BaselineSuffix)
        self._optimizer_class = BaselineOptimizer  # overridable for testing
        self._baseline_optimizer = None
        self._port = None
        self._tool = None

    def _write(self, msg):
        print(msg)

    def _analyze_baseline(self, options, test_name):
        # TODO(robertma): Investigate changing the CLI to take extensions with leading '.'.
        for suffix in self._baseline_suffix_list:
            extension = '.' + suffix
            name = self._port.output_filename(
                test_name, self._port.BASELINE_SUFFIX, extension)
            results_by_directory = self._baseline_optimizer.read_results_by_directory(
                test_name, name)
            if results_by_directory:
                self._write('%s:' % name)
                self._baseline_optimizer.write_by_directory(
                    results_by_directory, self._write, '  ')
            elif options.missing:
                self._write('%s: (no baselines found)' % name)

    def execute(self, options, args, tool):
        self._tool = tool
        self._baseline_suffix_list = options.suffixes.split(',')
        port_names = tool.port_factory.all_port_names(options.platform)
        if not port_names:
            _log.error("No port names match '%s'", options.platform)
            return
        self._port = tool.port_factory.get(port_names[0])
        self._baseline_optimizer = self._optimizer_class(
            tool, self._port, port_names)
        for test_name in self._port.tests(args):
            self._analyze_baseline(options, test_name)
