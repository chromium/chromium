# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

from blinkpy.common.checkout.baseline_optimizer import BaselineOptimizer
from blinkpy.tool.commands.rebaseline import AbstractRebaseliningCommand

_log = logging.getLogger(__name__)


class OptimizeBaselines(AbstractRebaseliningCommand):
    name = 'optimize-baselines'
    help_text = 'Reshuffles the baselines for the given tests to use as little space on disk as possible.'
    show_in_main_help = True
    argument_names = 'TEST_NAMES'

    def __init__(self):
        super(OptimizeBaselines, self).__init__(options=[
            self.suffixes_option,
            self.port_name_option,
        ] + self.platform_options + self.wpt_options)

    def _optimize_baseline(self, optimizer, test_name):
        for suffix in self._baseline_suffix_list:
            optimizer.optimize(test_name, suffix)

    def execute(self, options, args, tool):
        self._tool = tool
        self._baseline_suffix_list = options.suffixes.split(',')
        port_names = tool.port_factory.all_port_names(options.platform)
        if not port_names:
            _log.error("No port names match '%s'", options.platform)
            return
        port = tool.port_factory.get(port_names[0], options)
        optimizer = BaselineOptimizer(tool, port, port_names)
        tests = port.tests(args)
        for test_name in tests:
            self._optimize_baseline(optimizer, test_name)
