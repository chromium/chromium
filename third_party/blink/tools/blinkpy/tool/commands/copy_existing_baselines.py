# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

from blinkpy.common.memoized import memoized
from blinkpy.tool.commands.rebaseline import AbstractRebaseliningCommand
from blinkpy.web_tests.models.test_expectations import TestExpectations
from blinkpy.web_tests.models.typ_types import ResultType

_log = logging.getLogger(__name__)


class CopyExistingBaselines(AbstractRebaseliningCommand):
    name = 'copy-existing-baselines-internal'
    help_text = ('Copy existing baselines down one level in the baseline '
                 'order to ensure new baselines don\'t break existing passing '
                 'platforms.')

    def __init__(self):
        super(CopyExistingBaselines, self).__init__(options=[
            self.test_option,
            self.suffixes_option,
            self.port_name_option,
            self.results_directory_option,
        ])

    def execute(self, options, args, tool):
        self._tool = tool
        port_name = options.port_name
        for suffix in options.suffixes.split(','):
            self._copy_existing_baseline(port_name, options.test, suffix)

    def _copy_existing_baseline(self, port_name, test_name, suffix):
        """Copies the baseline for the given builder to all "predecessor" directories."""
        baseline_directory = self._tool.port_factory.get(
            port_name).baseline_version_dir()
        ports = [
            self._port_for_primary_baseline(baseline) for baseline in self.
            _immediate_predecessors_in_fallback(baseline_directory)
        ]

        old_baselines = []
        new_baselines = []

        # Need to gather all the baseline paths before modifying the filesystem since
        # the modifications can affect the results of port.expected_filename.
        for port in ports:
            old_baseline = port.expected_filename(test_name, '.' + suffix)
            if not self._tool.filesystem.exists(old_baseline):
                _log.debug('No existing baseline for %s.', test_name)
                continue

            new_baseline = self._tool.filesystem.join(
                port.baseline_version_dir(),
                self._file_name_for_expected_result(test_name, suffix))
            if self._tool.filesystem.exists(new_baseline):
                _log.debug('Existing baseline at %s, not copying over it.',
                           new_baseline)
                continue

            full_expectations = TestExpectations(port)
            if ResultType.Skip in full_expectations.get_expectations(
                    test_name).results:
                _log.debug('%s is skipped on %s.', test_name, port.name())
                continue
            if port.skipped_due_to_smoke_tests(test_name):
                _log.debug('%s is skipped on %s.', test_name, port.name())
                continue

            old_baselines.append(old_baseline)
            new_baselines.append(new_baseline)

        for i in range(len(old_baselines)):
            old_baseline = old_baselines[i]
            new_baseline = new_baselines[i]

            _log.debug('Copying baseline from %s to %s.', old_baseline,
                       new_baseline)
            self._tool.filesystem.maybe_make_directory(
                self._tool.filesystem.dirname(new_baseline))
            self._tool.filesystem.copyfile(old_baseline, new_baseline)

    def _port_for_primary_baseline(self, baseline):
        """Returns a Port object for the given baseline directory base name."""
        for port in [
                self._tool.port_factory.get(port_name)
                for port_name in self._tool.port_factory.all_port_names()
        ]:
            if self._tool.filesystem.basename(
                    port.baseline_version_dir()) == baseline:
                return port
        raise Exception(
            'Failed to find port for primary baseline %s.' % baseline)

    @memoized
    def _immediate_predecessors_in_fallback(self, path_to_rebaseline):
        """Returns the predecessor directories in the baseline fall-back graph.

        The platform-specific fall-back baseline directories form a tree, where
        when we search for baselines we normally fall back to parents nodes in
        the tree. The "immediate predecessors" are the children nodes of the
        given node.

        For example, if the baseline fall-back graph includes:
            "mac10.9" -> "mac10.10/"
            "mac10.10/" -> "mac/"
            "retina/" -> "mac/"
        Then, the "immediate predecessors" are:
            "mac/": ["mac10.10/", "retina/"]
            "mac10.10/": ["mac10.9/"]
            "mac10.9/", "retina/": []

        Args:
            path_to_rebaseline: The absolute path to a baseline directory.

        Returns:
            A list of directory names (not full paths) of directories that are
            "immediate predecessors" of the given baseline path.
        """
        port_names = self._tool.port_factory.all_port_names()
        immediate_predecessors = []
        for port_name in port_names:
            port = self._tool.port_factory.get(port_name)
            baseline_search_path = port.baseline_search_path()
            try:
                index = baseline_search_path.index(path_to_rebaseline)
                if index:
                    immediate_predecessors.append(
                        self._tool.filesystem.basename(
                            baseline_search_path[index - 1]))
            except ValueError:
                # baseline_search_path.index() throws a ValueError if the item isn't in the list.
                pass
        return immediate_predecessors
