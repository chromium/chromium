# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

from blinkpy.common.memoized import memoized
from blinkpy.tool.commands.rebaseline import AbstractRebaseliningCommand
from blinkpy.web_tests.models.test_expectations import TestExpectationsCache
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
            self.flag_specific_option,
            self.results_directory_option,
        ])
        self._exp_cache = TestExpectationsCache()

    def execute(self, options, args, tool):
        self._tool = tool
        port_name = options.port_name
        flag_specific = options.flag_specific
        for suffix in options.suffixes.split(','):
            self._copy_existing_baseline(port_name, flag_specific,
                                         options.test, suffix)

    def _log_skipped_test(self, port, test_name):
        flag_specific = port.get_option('flag_specific')
        if flag_specific:
            _log.debug('%s is skipped on %s(%s).', test_name, port.name(),
                       flag_specific)
        else:
            _log.debug('%s is skipped on %s.', test_name, port.name())

    def _copy_existing_baseline(self, port_name, flag_specific, test_name,
                                suffix):
        """Copies the baseline for the given builder to all "predecessor" directories."""

        # copy existing non virtual baseline to virtual subtree
        self._patch_virtual_subtree(port_name, flag_specific, test_name,
                                    suffix)

        if flag_specific:
            # No test will fallback to a flag-specific baseline
            return

        baseline_directory = self._tool.port_factory.get(
            port_name).baseline_version_dir()
        ports = [
            self._port_for_primary_baseline(baseline) for baseline in self.
            _immediate_predecessors_in_fallback(baseline_directory)
        ]

        # Copy baseline to any flag specific suite running on the port
        for flag_specific_config in self._tool.builders.flag_specific_options_for_port_name(
                port_name):
            port = self._tool.port_factory.get(port_name)
            port.set_option_default('flag_specific', flag_specific_config)
            ports.append(port)

        old_baselines = []
        new_baselines = []

        # Need to gather all the baseline paths before modifying the filesystem since
        # the modifications can affect the results of port.expected_filename.
        for port in ports:
            new_baseline = self._tool.filesystem.join(
                port.baseline_version_dir(),
                self._file_name_for_expected_result(test_name, suffix))

            if port.skips_test(test_name):
                self._log_skipped_test(port, test_name)
                if self._tool.filesystem.exists(new_baseline):
                    self._tool.filesystem.remove(new_baseline)
                continue

            full_expectations = self._exp_cache.load(port)
            if ResultType.Skip in full_expectations.get_expectations(
                    test_name).results:
                self._log_skipped_test(port, test_name)
                # do not delete existing baseline in this case, as this happens
                # more often, and such baselines could still be in use.
                continue

            if self._tool.filesystem.exists(new_baseline):
                _log.debug('Existing baseline at %s, not copying over it.',
                           new_baseline)
                continue

            old_baseline = port.expected_filename(test_name, '.' + suffix)
            if not self._tool.filesystem.exists(old_baseline):
                _log.debug('No existing baseline(%s) for %s.', suffix,
                           test_name)
                # TODO: downloading all passing baseline for this test harness test
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

    def _patch_virtual_subtree(self, port_name, flag_specific, test_name,
                               suffix):
        port = self._tool.port_factory.get(port_name)
        if flag_specific:
            port.set_option_default('flag_specific', flag_specific)
        full_expectations = self._exp_cache.load(port)

        if port.lookup_virtual_test_base(test_name):
            # Do nothing for virtual tests
            return

        old_baseline = port.expected_filename(test_name,
                                              '.' + suffix,
                                              return_default=False)

        for vts in port.virtual_test_suites():
            virtual_test_name = vts.full_prefix + test_name
            if port.lookup_virtual_test_base(virtual_test_name) is None:
                # Not a valid virtual test
                continue

            new_baseline = self._tool.filesystem.join(
                port.baseline_version_dir(),
                self._file_name_for_expected_result(virtual_test_name, suffix))

            if port.skips_test(test_name):
                self._log_skipped_test(port, virtual_test_name)
                if self._tool.filesystem.exists(new_baseline):
                    self._tool.filesystem.remove(new_baseline)
                continue

            if ResultType.Skip in full_expectations.get_expectations(
                    virtual_test_name).results:
                self._log_skipped_test(port, virtual_test_name)
                # same as above, do not delete existing baselines
                continue

            baseline_dir, filename = port.expected_baselines(
                virtual_test_name, '.' + suffix)[0]
            if baseline_dir is not None:
                # This virtual test does not fall back to non virtual baseline
                continue

            if old_baseline is None:
                _log.debug('No existing non virtual baseline(%s) for %s.',
                           suffix, virtual_test_name)
                # TODO: downloading all passing baseline for this virtual test harness test
                continue

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
        immediate_predecessors = set()
        for port_name in port_names:
            port = self._tool.port_factory.get(port_name)
            baseline_search_path = port.baseline_search_path()
            try:
                index = baseline_search_path.index(path_to_rebaseline)
                if index:
                    immediate_predecessors.add(
                        self._tool.filesystem.basename(
                            baseline_search_path[index - 1]))
            except ValueError:
                # baseline_search_path.index() throws a ValueError if the item isn't in the list.
                pass
        return list(immediate_predecessors)
