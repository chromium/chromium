# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import functools
import logging
import optparse

from blinkpy.common.checkout.baseline_optimizer import BaselineOptimizer
from blinkpy.tool.commands.rebaseline import AbstractParallelRebaselineCommand
from blinkpy.web_tests.models.test_expectations import TestExpectationsCache

_log = logging.getLogger(__name__)


class OptimizeBaselines(AbstractParallelRebaselineCommand):
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

    def execute(self, options, args, tool):
        if not args != options.all_tests:
            _log.error('Must provide one of --all or TEST_NAMES')
            return

        self._tool = tool
        port_names = tool.port_factory.all_port_names(options.platform)
        if not port_names:
            _log.error("No port names match '%s'", options.platform)
            return

        port = tool.port_factory.get(options=options)
        test_set = self._get_test_set(port, options, args)
        if not test_set:
            _log.error('No tests to optimize. Ensure all listed tests exist.')
            return 1

        worker_factory = functools.partial(Worker,
                                           port_names=port_names,
                                           options=options)
        baseline_suffix_list = options.suffixes.split(',')
        with self._message_pool(worker_factory) as pool:
            tasks = [(self.name, test_name, suffix) for test_name in test_set
                     for suffix in baseline_suffix_list]
            pool.run(tasks)

    def _get_test_set(self, port, options, args):
        test_set = set(port.tests() if options.all_tests else port.tests(args))
        virtual_tests_to_exclude = set([
            test for test in test_set
            if port.lookup_virtual_test_base(test) in test_set
        ])
        test_set -= virtual_tests_to_exclude
        return test_set


class Worker:
    def __init__(self, connection, port_names, options):
        self._connection = connection
        self._options = options
        self._port_names = port_names

    def start(self):
        # Workers should never update the manifest, as this could cause a race.
        # The manifest should already be updated by `optimize-baselines` or
        # `rebaseline-cl`.
        self._options.manifest_update = False
        self._optimizer = BaselineOptimizer(
            self._connection.host,
            self._connection.host.port_factory.get(options=self._options),
            self._port_names)

    def handle(self, name: str, source: str, test_name: str, suffix: str):
        self._optimizer.optimize(test_name, suffix)
        self._connection.post(name)
