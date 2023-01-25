# Copyright (C) 2011, Google Inc. All rights reserved.
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

import copy
import logging
from typing import Optional

from blinkpy.common.memoized import memoized
from blinkpy.web_tests.models.testharness_results import is_all_pass_testharness_result
from blinkpy.web_tests.models.test_expectations import TestExpectationsCache
from blinkpy.web_tests.models.typ_types import ResultType

_log = logging.getLogger(__name__)


class BaselineOptimizer(object):
    def __init__(self,
                 host,
                 default_port,
                 port_names,
                 exp_cache: Optional[TestExpectationsCache] = None):
        self._filesystem = host.filesystem
        self._default_port = default_port
        self._host = host
        self._flag_specific_configs = set()
        self._ports = {}
        for port_name in port_names:
            self._ports[port_name] = host.port_factory.get(port_name)
            self._flag_specific_configs.update(
                host.builders.flag_specific_options_for_port_name(port_name))

        self._exp_cache = exp_cache or TestExpectationsCache()
        self._web_tests_dir = default_port.web_tests_dir()
        self._parent_of_tests = self._filesystem.dirname(self._web_tests_dir)
        self._web_tests_dir_name = self._filesystem.relpath(
            self._web_tests_dir, self._parent_of_tests)

        # Only used by unit tests.
        self.new_results_by_directory = []

    def _make_flag_spec_port(self, host, flag_specific_config):
        if not flag_specific_config:
            return None
        port_name = host.builders.port_name_for_flag_specific_option(
            flag_specific_config)
        port = host.port_factory.get(port_name)
        port.set_option_default('flag_specific', flag_specific_config)
        return port

    def optimize(self, test_name, suffix):
        # A visualization of baseline fallback:
        # https://docs.google.com/drawings/d/13l3IUlSE99RoKjDwEWuY1O77simAhhF6Wi0fZdkSaMA/
        # The full document with more details:
        # https://chromium.googlesource.com/chromium/src/+/main/docs/testing/web_test_baseline_fallback.md
        # The virtual and non-virtual subtrees are identical, with the virtual
        # root being the special node having multiple parents and connecting the
        # two trees. We patch the virtual subtree to cut its dependencies on the
        # non-virtual one and optimze the two independently. Finally, we treat
        # the virtual subtree specially to remove any duplication between the
        # two subtrees.

        # For CLI compatibility, "suffix" is an extension without the leading
        # dot. Yet we use dotted extension everywhere else in the codebase.
        # TODO(robertma): Investigate changing the CLI.
        _log.debug('Optimizing %s(%s).' % (test_name, suffix))
        assert not suffix.startswith('.')
        extension = '.' + suffix
        succeeded = True

        non_virtual_test_name = self._virtual_test_base(test_name)
        if non_virtual_test_name:
            # Optimize a virtual test and its flag specific versions
            non_virtual_baseline_name = self._default_port.output_filename(
                non_virtual_test_name, self._default_port.BASELINE_SUFFIX,
                extension)
            succeeded = self._optimize_virtual_baseline(
                test_name, extension, non_virtual_baseline_name)
        else:
            # Optimize a real test and all derived virtual/flag specific versions.
            baseline_name = self._default_port.output_filename(
                test_name, self._default_port.BASELINE_SUFFIX, extension)

            _log.debug('Optimizing virtual fallback paths.')
            for vts in self._default_port.virtual_test_suites():
                virtual_test_name = vts.full_prefix + test_name
                if self._default_port.lookup_virtual_test_base(
                        virtual_test_name) is None:
                    # Not a valid virtual test
                    continue
                succeeded &= self._optimize_virtual_baseline(
                    virtual_test_name, extension, baseline_name)

            _log.debug('Optimizing non-virtual fallback path.')
            succeeded &= self._optimize_subtree(test_name, baseline_name)
            self._optimize_flag_specific_baselines(test_name, extension)
            self._remove_extra_result_at_root(test_name, baseline_name)

        if not succeeded:
            _log.error('Heuristics failed to optimize %s', baseline_name)
        return succeeded

    def _optimize_virtual_baseline(self, test_name, extension,
                                   non_virtual_baseline_name):
        baseline_name = self._default_port.output_filename(
            test_name, self._default_port.BASELINE_SUFFIX, extension)

        self._patch_virtual_subtree(test_name, extension, baseline_name)
        succeeded = self._optimize_subtree(test_name, baseline_name)

        self._optimize_virtual_root(test_name, extension, baseline_name,
                                    non_virtual_baseline_name)

        self._optimize_flag_specific_baselines(test_name, extension)
        return succeeded

    def _optimize_flag_specific_baselines(self, test_name, extension):
        """Optimize flag-specific baselines."""
        for flag_specific in self._flag_specific_configs:
            # To ensure the flag-specific baselines join the fallback graph in the
            # same location each time the optimizer runs, we create a separate port.
            flag_spec_port = self._make_flag_spec_port(self._host,
                                                       flag_specific)
            if not flag_spec_port:
                continue
            self._optimize_single_baseline_flag_specific(
                test_name, extension, flag_spec_port)

    def _get_baseline_paths(self, test_name, extension, port):
        """Get paths to baselines that the provided port would search.

        Returns:
            list[str]: A list of absolute paths (symbolically generated, may
                not actually exist on disk).
        """
        baselines = port.expected_baselines(test_name,
                                            extension,
                                            all_baselines=True)
        non_virtual_test_name = self._virtual_test_base(test_name)
        if non_virtual_test_name:
            baselines.extend(
                port.expected_baselines(non_virtual_test_name,
                                        extension,
                                        all_baselines=True))
        # `baseline_dir` is `None` when the search path is empty and the generic
        # baseline is also missing.
        baseline_paths = [
            self._filesystem.join(baseline_dir, baseline_filename)
            for baseline_dir, baseline_filename in baselines if baseline_dir
        ]
        return baseline_paths

    def _optimize_single_baseline_flag_specific(self, test_name, extension,
                                                port):
        """Optimize a baseline directly by simulating the fallback algorithm."""
        baseline_paths = self._get_baseline_paths(test_name, extension, port)
        if not baseline_paths:
            # The baseline for this test does not exist.
            return
        baseline_to_optimize = baseline_paths[0]
        if (("flag-specific" not in baseline_to_optimize)
                or (not self._filesystem.exists(baseline_to_optimize))):
            return
        basename = self._filesystem.basename(baseline_to_optimize)
        if len(baseline_paths) < 2:
            _log.debug('  %s: (no baselines found)', basename)
            return
        fallback_baseline = baseline_paths[1]
        is_reftest = self._is_reftest(test_name)
        target_digest = ResultDigest(self._filesystem, baseline_to_optimize,
                                     is_reftest)
        fallback_digest = ResultDigest(self._filesystem, fallback_baseline,
                                       is_reftest)
        if target_digest == fallback_digest:
            _log.debug('  %s:', basename)
            _log.debug('    Deleting (file system): %s', baseline_to_optimize)
            self._filesystem.remove(baseline_to_optimize)
        else:
            _log.debug('  %s: (already optimal)', basename)

    def write_by_directory(self, results_by_directory, writer, indent):
        """Logs results_by_directory in a pretty format."""
        for path in sorted(results_by_directory):
            writer('%s%s: %s' % (indent, self._platform(path),
                                 results_by_directory[path]))

    def read_results_by_directory(self, test_name, baseline_name):
        """Reads the baselines with the given file name in all directories.

        Returns:
            A dict from directory names to the digest of file content.
        """
        results_by_directory = {}
        directories = set()
        for port in self._ports.values():
            directories.update(set(self._relative_baseline_search_path(port)))

        for directory in directories:
            path = self._join_directory(directory, baseline_name)
            if self._filesystem.exists(path):
                results_by_directory[directory] = ResultDigest(
                    self._filesystem, path, self._is_reftest(test_name))
        return results_by_directory

    def _is_reftest(self, test_name):
        return bool(self._default_port.reference_files(test_name))

    def _optimize_subtree(self, test_name, baseline_name):
        basename = self._filesystem.basename(baseline_name)
        results_by_directory, new_results_by_directory = self._find_optimal_result_placement(
            test_name, baseline_name)

        if new_results_by_directory == results_by_directory:
            if new_results_by_directory:
                _log.debug('  %s: (already optimal)', basename)
                self.write_by_directory(results_by_directory, _log.debug,
                                        '    ')
            else:
                _log.debug('  %s: (no baselines found)', basename)
            # This is just used for unit tests.
            # Intentionally set it to the old data if we don't modify anything.
            self.new_results_by_directory.append(results_by_directory)
            return True

        # Check if the results before and after optimization are equivalent.
        if (self._results_by_port_name(results_by_directory) !=
                self._results_by_port_name(new_results_by_directory)):
            # This really should never happen. Just a sanity check to make
            # sure the script fails in the case of bugs instead of committing
            # incorrect baselines.
            _log.error('  %s: optimization failed', basename)
            self.write_by_directory(results_by_directory, _log.warning,
                                    '      ')
            return False

        _log.debug('  %s:', basename)
        _log.debug('    Before: ')
        self.write_by_directory(results_by_directory, _log.debug, '      ')
        _log.debug('    After: ')
        self.write_by_directory(new_results_by_directory, _log.debug, '      ')

        self._move_baselines(baseline_name, results_by_directory,
                             new_results_by_directory)
        return True

    def _move_baselines(self, baseline_name, results_by_directory,
                        new_results_by_directory):
        data_for_result = {}
        for directory, result in results_by_directory.items():
            if str(result) not in data_for_result:
                source = self._join_directory(directory, baseline_name)
                data_for_result[str(
                    result)] = self._filesystem.read_binary_file(source)

        fs_files = []
        for directory, result in results_by_directory.items():
            if new_results_by_directory.get(directory) != result:
                file_name = self._join_directory(directory, baseline_name)
                if self._filesystem.exists(file_name):
                    fs_files.append(file_name)

        if fs_files:
            _log.debug('    Deleting (file system):')
            for platform_dir in sorted(
                    self._platform(filename) for filename in fs_files):
                _log.debug('      ' + platform_dir)
            for filename in fs_files:
                self._filesystem.remove(filename)
        else:
            _log.debug('    (Nothing to delete)')

        file_names = []
        for directory, result in new_results_by_directory.items():
            if results_by_directory.get(directory) != result:
                destination = self._join_directory(directory, baseline_name)
                self._filesystem.maybe_make_directory(
                    self._filesystem.split(destination)[0])
                self._filesystem.write_binary_file(
                    destination, data_for_result[result.__str__()])
                file_names.append(destination)

        if file_names:
            _log.debug('    Adding:')
            for platform_dir in sorted(
                    self._platform(filename) for filename in file_names):
                _log.debug('      ' + platform_dir)
        else:
            _log.debug('    (Nothing to add)')

    def _platform(self, filename):
        """Guesses the platform from a path (absolute or relative).

        Returns:
            The platform name, or '(generic)' if unable to make a guess.
        """
        platform_dir = self._web_tests_dir_name + self._filesystem.sep + 'platform' + self._filesystem.sep
        if filename.startswith(platform_dir):
            return filename.replace(platform_dir,
                                    '').split(self._filesystem.sep)[0]
        platform_dir = self._filesystem.join(self._parent_of_tests,
                                             platform_dir)
        if filename.startswith(platform_dir):
            return filename.replace(platform_dir,
                                    '').split(self._filesystem.sep)[0]
        return '(generic)'

    def _port_from_baseline_dir(self, baseline_dir):
        """Returns a Port object from the given baseline directory."""
        baseline_dir = self._filesystem.basename(baseline_dir)
        for port in self._ports.values():
            if self._filesystem.basename(
                    port.baseline_version_dir()) == baseline_dir:
                return port
        raise Exception(
            'Failed to find port for primary baseline %s.' % baseline_dir)

    def _walk_immediate_predecessors_of_virtual_root(
            self, test_name, extension, baseline_name, worker_func):
        """Maps a function onto each immediate predecessor of the virtual root.

        For each immediate predecessor, we call
            worker_func(virtual_baseline, non_virtual_fallback)
        where the two arguments are the absolute paths to the virtual platform
        baseline and the non-virtual fallback respectively.
        """
        actual_test_name = self._virtual_test_base(test_name)
        assert actual_test_name, '%s is not a virtual test.' % test_name

        for directory in self._directories_immediately_preceding_root():
            port = self._port_from_baseline_dir(directory)
            virtual_baseline = self._join_directory(directory, baseline_name)
            # return_default=False mandates expected_filename() to return None
            # instead of a non-existing generic path when nothing is found.
            non_virtual_fallback = port.expected_filename(
                actual_test_name, extension, return_default=False)
            if not non_virtual_fallback:
                # Unable to find a non-virtual fallback baseline, skipping.
                continue
            worker_func(virtual_baseline, non_virtual_fallback)

    def _patch_virtual_subtree(self, test_name, extension, baseline_name):
        # Ensure all immediate predecessors of the root have a baseline for this
        # virtual suite so that the virtual subtree can be treated completely
        # independently. If an immediate predecessor is missing a baseline, find
        # its non-virtual fallback and copy over.
        _log.debug(
            'Copying non-virtual baselines to the virtual subtree to make it independent.'
        )
        virtual_root_baseline_path = self._filesystem.join(
            self._web_tests_dir, baseline_name)
        if self._filesystem.exists(virtual_root_baseline_path):
            return

        def patcher(virtual_baseline, non_virtual_fallback):
            if not self._filesystem.exists(virtual_baseline):
                _log.debug('    Copying (file system): %s -> %s.',
                           non_virtual_fallback, virtual_baseline)
                self._filesystem.maybe_make_directory(
                    self._filesystem.split(virtual_baseline)[0])
                self._filesystem.copyfile(non_virtual_fallback,
                                          virtual_baseline)

        self._walk_immediate_predecessors_of_virtual_root(
            test_name, extension, baseline_name, patcher)

    def _optimize_virtual_root(self, test_name, extension, baseline_name,
                               non_virtual_baseline_name):
        virtual_root_baseline_path = self._filesystem.join(
            self._web_tests_dir, baseline_name)
        if self._filesystem.exists(virtual_root_baseline_path):
            _log.debug(
                'Virtual root baseline found. Checking if we can remove it.')
            self._try_to_remove_virtual_root(test_name,
                                             non_virtual_baseline_name,
                                             virtual_root_baseline_path)
        else:
            _log.debug(
                'Virtual root baseline not found. Searching for virtual baselines redundant with non-virtual ones.'
            )
            self._unpatch_virtual_subtree(test_name, extension, baseline_name)

    def _try_to_remove_virtual_root(self, test_name, non_virtual_baseline_name,
                                    virtual_root_baseline_path):
        # See if all the successors of the virtual root (i.e. all non-virtual
        # platforms) have the same baseline as the virtual root. If so, the
        # virtual root is redundant and can be safely removed.
        virtual_root_digest = ResultDigest(self._filesystem,
                                           virtual_root_baseline_path,
                                           self._is_reftest(test_name))

        # Read the base (non-virtual) results.
        results_by_directory = self.read_results_by_directory(
            test_name, non_virtual_baseline_name)
        results_by_port_name = self._results_by_port_name(results_by_directory)

        for port_name, port in self._ports.items():
            assert port_name in results_by_port_name
            # When the virtual test is skipped on a port, the baseline for the
            # non virtual test on the same port won't matter
            full_expectations = self._exp_cache.load(port)
            if ResultType.Skip in full_expectations.get_expectations(
                    test_name).results:
                continue
            if port.skips_test(test_name):
                continue
            if results_by_port_name[port_name] != virtual_root_digest:
                return

        _log.debug('Deleting redundant virtual root baseline.')
        _log.debug('    Deleting (file system): ' + virtual_root_baseline_path)
        self._filesystem.remove(virtual_root_baseline_path)

    def _unpatch_virtual_subtree(self, test_name, extension, baseline_name):
        # Check all immediate predecessors of the virtual root and delete those
        # duplicate with their non-virtual fallback, essentially undoing some
        # of the work done in _patch_virtual_subtree.
        is_reftest = self._is_reftest(test_name)

        def unpatcher(virtual_baseline, non_virtual_fallback):
            if self._filesystem.exists(virtual_baseline) and \
                    (ResultDigest(self._filesystem, virtual_baseline, is_reftest) ==
                     ResultDigest(self._filesystem, non_virtual_fallback, is_reftest)):
                _log.debug(
                    '    Deleting (file system): %s (redundant with %s).',
                    virtual_baseline, non_virtual_fallback)
                self._filesystem.remove(virtual_baseline)

        self._walk_immediate_predecessors_of_virtual_root(
            test_name, extension, baseline_name, unpatcher)

    def _baseline_root(self):
        """Returns the name of the root (generic) baseline directory."""
        return self._web_tests_dir_name

    def _baseline_search_path(self, port):
        """Returns the baseline search path (a list of absolute paths) of the
        given port."""
        return port.baseline_search_path()

    @memoized
    def _relative_baseline_search_path(self, port):
        """Returns a list of paths to check for baselines in order.

        The generic baseline path is appended to the list. All paths are
        relative to the parent of the test directory.
        """
        baseline_search_path = self._baseline_search_path(port)
        relative_paths = [
            self._filesystem.relpath(path, self._parent_of_tests)
            for path in baseline_search_path
        ]
        relative_baseline_root = self._baseline_root()
        return relative_paths + [relative_baseline_root]

    def _virtual_test_base(self, test_name):
        """Returns the base (non-virtual) version of test_name, or None if
        test_name is not virtual."""
        # This function should only accept a test name. Use baseline won't work
        # because some bases in VirtualTestSuites are full test name which has
        # .html as extension.
        return self._default_port.lookup_virtual_test_base(test_name)

    def _join_directory(self, directory, baseline_name):
        """Returns the absolute path to the baseline in the given directory."""
        return self._filesystem.join(self._parent_of_tests, directory,
                                     baseline_name)

    def _results_by_port_name(self, results_by_directory):
        """Transforms a by-directory result dict to by-port-name.

        The method mimicks the baseline search behaviour, i.e. results[port] is
        the first baseline found on the baseline search path of the port. If no
        baseline is found on the search path, the test is assumed to be an all-
        PASS testharness.js test.

        Args:
            results_by_directory: A dictionary returned by read_results_by_directory().

        Returns:
            A dictionary mapping port names to their baselines.
        """
        results_by_port_name = {}
        for port_name, port in self._ports.items():
            for directory in self._relative_baseline_search_path(port):
                if directory in results_by_directory:
                    results_by_port_name[port_name] = results_by_directory[
                        directory]
                    break
            if port_name not in results_by_port_name:
                # Implicit extra result.
                results_by_port_name[port_name] = ResultDigest(None, None)
        return results_by_port_name

    @memoized
    def _directories_immediately_preceding_root(self):
        """Returns a list of directories immediately preceding the root on
        search paths."""
        directories = set()
        for port in self._ports.values():
            directory = self._filesystem.relpath(
                self._baseline_search_path(port)[-1], self._parent_of_tests)
            directories.add(directory)
        return frozenset(directories)

    def _optimize_result_for_root(self, new_results_by_directory):
        # The root directory (i.e. web_tests) is the only one not
        # corresponding to a specific platform. As such, baselines in
        # directories that immediately precede the root on search paths may
        # be promoted up if they are all the same.
        # Example: if win and mac have the same baselines, then they can be
        # promoted up to be the root baseline.
        # All other baselines can only be removed if they're redundant with a
        # baseline later on the search path. They can never be promoted up.
        immediately_preceding_root = self._directories_immediately_preceding_root(
        )

        shared_result = None
        root_baseline_unused = False
        for directory in immediately_preceding_root:
            this_result = new_results_by_directory.get(directory)

            # If any of these directories don't have a baseline, there's no optimization we can do.
            if not this_result:
                return

            if not shared_result:
                shared_result = this_result
            elif shared_result != this_result:
                root_baseline_unused = True

        baseline_root = self._baseline_root()

        # The root baseline is unused if all the directories immediately preceding the root
        # have a baseline, but have different baselines, so the baselines can't be promoted up.
        if root_baseline_unused:
            if baseline_root in new_results_by_directory:
                del new_results_by_directory[baseline_root]
            return

        new_results_by_directory[baseline_root] = shared_result
        for directory in immediately_preceding_root:
            del new_results_by_directory[directory]

    def _find_optimal_result_placement(self, test_name, baseline_name):
        results_by_directory = self.read_results_by_directory(
            test_name, baseline_name)
        results_by_port_name = self._results_by_port_name(results_by_directory)

        new_results_by_directory = self._remove_redundant_results(
            results_by_directory, results_by_port_name)
        self._optimize_result_for_root(new_results_by_directory)

        return results_by_directory, new_results_by_directory

    def _remove_redundant_results(self, results_by_directory,
                                  results_by_port_name):
        # For every port, traverse its search path in the fallback order (from
        # specific to generic). Remove duplicate baselines until a different
        # baseline is found (or the root is reached), i.e., keep the most
        # generic one among duplicate baselines.
        new_results_by_directory = copy.copy(results_by_directory)
        for port_name, port in self._ports.items():
            current_result = results_by_port_name.get(port_name)

            # This happens if we're missing baselines for a port.
            if not current_result:
                continue

            search_path = self._relative_baseline_search_path(port)
            current_index, current_directory = self._find_in_search_path(
                search_path, current_result, new_results_by_directory)
            found_different_result = False
            for index in range(current_index + 1, len(search_path)):
                new_directory = search_path[index]
                if new_directory not in new_results_by_directory:
                    # No baseline in this directory.
                    continue
                elif new_results_by_directory[new_directory] == current_result:
                    # The baseline in current_directory is redundant with the
                    # baseline in new_directory which is later in the search
                    # path. Remove the earlier one and point current to new.
                    if current_directory in new_results_by_directory:
                        del new_results_by_directory[current_directory]
                        current_directory = new_directory
                else:
                    # A different result is found, so stop.
                    found_different_result = True
                    break

            # If we did not find a different fallback and current_result is
            # an extra result, we can safely remove it.
            # Note that we do not remove the generic extra result here.
            # Roots (virtual and non-virtual) are treated specially later.
            if (not found_different_result and current_result.is_extra_result
                    and current_directory != self._baseline_root()
                    and current_directory in new_results_by_directory):
                del new_results_by_directory[current_directory]

        return new_results_by_directory

    def _find_in_search_path(self, search_path, current_result,
                             results_by_directory):
        """Finds the index and the directory of a result on a search path."""
        for index, directory in enumerate(search_path):
            if (directory in results_by_directory
                    and (results_by_directory[directory] == current_result)):
                return index, directory
        assert current_result.is_extra_result, (
            'result %s not found in search path %s, %s' %
            (current_result, search_path, results_by_directory))
        # Implicit extra result at the root.
        return len(search_path) - 1, search_path[-1]

    def _remove_extra_result_at_root(self, test_name,
                                     non_virtual_baseline_name):
        """Removes extra result at the non-virtual root."""
        path = self._join_directory(self._baseline_root(),
                                    non_virtual_baseline_name)
        if (self._filesystem.exists(path)
                and ResultDigest(self._filesystem, path,
                                 self._is_reftest(test_name)).is_extra_result):
            _log.debug(
                'Deleting extra baseline (empty, -expected.png for reftest, or all-PASS testharness JS result)'
            )
            _log.debug('    Deleting (file system): ' + path)
            self._filesystem.remove(path)


class ResultDigest(object):
    """Digest of a result file for fast comparison.

    A result file can be any actual or expected output from a web test,
    including text and image. SHA1 is used internally to digest the file.
    """

    # A baseline is extra in the following cases:
    # 1. if the result is an all-PASS testharness result,
    # 2. if the result is empty,
    # 3. if the test is a reftest and the baseline is an -expected-png file.
    #
    # An extra baseline should be deleted if doesn't override the fallback
    # baseline. To check that, we compare the ResultDigests of the baseline and
    # the fallback baseline. If the baseline is not the root baseline and the
    # fallback baseline doesn't exist, we assume that the fallback baseline is
    # an *implicit extra result* which equals to any extra baselines, so that
    # the extra baseline will be treated as not overriding the fallback baseline
    # thus will be removed.
    _IMPLICIT_EXTRA_RESULT = '<EXTRA>'

    def __init__(self, fs, path, is_reftest=False):
        """Constructs the digest for a result.

        Args:
            fs: An instance of common.system.FileSystem.
            path: The path to a result file. If None is provided, the result is
                an *implicit* extra result.
            is_reftest: Whether the test is a reftest.
        """
        self.path = path
        if path is None:
            self.sha = self._IMPLICIT_EXTRA_RESULT
            self.is_extra_result = True
            return

        assert fs.exists(path), path + " does not exist"
        if path.endswith('.txt'):
            try:
                content = fs.read_text_file(path)
                self.is_extra_result = not content or is_all_pass_testharness_result(
                    content)
            except UnicodeDecodeError as e:
                self.is_extra_result = False
            # Unfortunately, we may read the file twice, once in text mode
            # and once in binary mode.
            self.sha = fs.sha1(path)
            return

        if path.endswith('.png') and is_reftest:
            self.is_extra_result = True
            self.sha = ''
            return

        self.is_extra_result = not fs.read_binary_file(path)
        self.sha = fs.sha1(path)

    def __eq__(self, other):
        if other is None:
            return False
        # Implicit extra result is equal to any extra results.
        if self.sha == self._IMPLICIT_EXTRA_RESULT or other.sha == self._IMPLICIT_EXTRA_RESULT:
            return self.is_extra_result and other.is_extra_result
        return self.sha == other.sha

    # Python 2 does not automatically delegate __ne__ to not __eq__.
    def __ne__(self, other):
        return not self.__eq__(other)

    def __str__(self):
        return self.sha[0:7]

    def __repr__(self):
        is_extra_result = ' EXTRA' if self.is_extra_result else ''
        return '<ResultDigest %s%s %s>' % (self.sha, is_extra_result,
                                           self.path)
