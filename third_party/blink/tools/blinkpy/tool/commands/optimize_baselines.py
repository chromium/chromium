# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import optparse

from blinkpy.common.checkout.baseline_optimizer import BaselineOptimizer
from blinkpy.tool.commands.rebaseline import AbstractRebaseliningCommand
from blinkpy.web_tests.models.test_expectations import TestExpectationsCache

_log = logging.getLogger(__name__)


class OptimizeBaselines(AbstractRebaseliningCommand):
    name = 'optimize-baselines'
    help_text = ('Reshuffles the baselines for the given tests to use '
                 'as little space on disk as possible.')
    show_in_main_help = True
    argument_names = '[TEST_NAMES]'

    all_option = optparse.make_option(
        '--all',
        dest='all_tests',
        action='store_true',
        default=False,
        help=('Optimize all tests (instead of using TEST_NAMES)'))

    def __init__(self):
        super(OptimizeBaselines, self).__init__(options=[
            self.suffixes_option,
            self.port_name_option,
            self.all_option,
        ] + self.platform_options + self.wpt_options)
        self._exp_cache = TestExpectationsCache()

    def _optimize_baseline(self, optimizer, test_name):
        for suffix in self._baseline_suffix_list:
            optimizer.optimize(test_name, suffix)

    def execute(self, options, args, tool):
        if not args != options.all_tests:
            _log.error('Must provide one of --all or TEST_NAMES')
            return

        self._tool = tool
        self._baseline_suffix_list = options.suffixes.split(',')
        port_names = tool.port_factory.all_port_names(options.platform)
        if not port_names:
            _log.error("No port names match '%s'", options.platform)
            return
        port = tool.port_factory.get(port_names[0], options)
        optimizer = BaselineOptimizer(tool, port, port_names, self._exp_cache)
        test_set = set(port.tests() if options.all_tests else port.tests(args))
        virtual_tests_to_exclude = set([
            test for test in test_set
            if port.lookup_virtual_test_base(test) in test_set
        ])
        test_set -= virtual_tests_to_exclude

        if not test_set:
            _log.error('No tests to optimize. Ensure all listed tests exist.')
            return 1
        for test_name in test_set:
            self._optimize_baseline(optimizer, test_name)
