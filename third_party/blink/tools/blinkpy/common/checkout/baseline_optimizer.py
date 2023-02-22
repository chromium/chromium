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

import collections
import contextlib
import logging
from typing import (
    Collection,
    Dict,
    FrozenSet,
    Iterator,
    List,
    NamedTuple,
    Optional,
    Set,
    Tuple,
)

from blinkpy.common.host import Host
from blinkpy.common.memoized import memoized
from blinkpy.common.path_finder import PathFinder
from blinkpy.web_tests.models.testharness_results import is_all_pass_testharness_result
from blinkpy.web_tests.models.test_expectations import TestExpectationsCache
from blinkpy.web_tests.models.typ_types import ResultType
from blinkpy.web_tests.port.base import Port

_log = logging.getLogger(__name__)


class BaselineLocation(NamedTuple):
    """A representation of a baseline that may exist on disk."""
    virtual_suite: str = ''
    platform: str = ''
    flag_specific: str = ''

    @property
    def root(self) -> bool:
        # Also check that this baseline is not flag-specific. A flag-specific
        # suite implies a platform, even without `platform/*/` in its path.
        return not self.platform and not self.flag_specific

    def __str__(self) -> str:
        parts = []
        if self.virtual_suite:
            parts.append('virtual/%s' % self.virtual_suite)
        if self.platform:
            parts.append(self.platform)
        elif self.flag_specific:
            parts.append(self.flag_specific)
        if not parts:
            parts.append('(generic)')
        return ':'.join(parts)


SearchPath = List[BaselineLocation]
DigestMap = Dict[BaselineLocation, 'ResultDigest']


class BaselineOptimizer:
    """Deduplicate baselines that are redundant to `run_web_tests.py`.

    `run_web_tests.py` has a fallback mechanism to search different locations
    for a test's baseline (i.e., expected text/image dump). The optimizer tries
    to minimize checkout size by removing baselines that would be redundant.

    At a high level, the algorithm works as follows:
     1. Promote or delete (non)virtual roots.
     2. Insert each port's search path into a graph data structure, removing
        any locations where the baseline doesn't exist. This models dependencies
        between locations, even if they are separated by empty locations (which
        are effectively no-ops in the port's search).
     3. For each baseline, remove it if it equals all successors.

    See also:
        Fallback visualization: https://docs.google.com/drawings/d/13l3IUlSE99RoKjDwEWuY1O77simAhhF6Wi0fZdkSaMA/
        Details: https://chromium.googlesource.com/chromium/src/+/main/docs/testing/web_test_baseline_fallback.md
    """

    def __init__(self,
                 host: Host,
                 default_port: Port,
                 port_names,
                 exp_cache: Optional[TestExpectationsCache] = None):
        self._filesystem = host.filesystem
        self._finder = PathFinder(self._filesystem)
        self._default_port = default_port
        self._host = host
        self._ports = []
        for port_name in port_names:
            self._ports.append(host.port_factory.get(port_name))
        for flag_specific in host.builders.all_flag_specific_options():
            port_name = host.builders.port_name_for_flag_specific_option(
                flag_specific)
            if port_name in port_names:
                self._ports.append(
                    self._make_flag_spec_port(host, flag_specific))
        self._exp_cache = exp_cache or TestExpectationsCache()

    def _make_flag_spec_port(self, host: Host,
                             flag_specific_config: str) -> Port:
        port_name = host.builders.port_name_for_flag_specific_option(
            flag_specific_config)
        port = host.port_factory.get(port_name)
        port.set_option_default('flag_specific', flag_specific_config)
        return port

    def optimize(self, test_name: str, suffix: str) -> bool:
        # For CLI compatibility, "suffix" is an extension without the leading
        # dot. Yet we use dotted extension everywhere else in the codebase.
        # TODO(robertma): Investigate changing the CLI.
        _log.debug('Optimizing %s (%s).', test_name, suffix)
        assert not suffix.startswith('.')
        extension = '.' + suffix

        with _indent_log():
            nonvirtual_test, virtual_tests = self._get_tests_to_optimize(
                test_name)
            paths = list(
                self._generate_search_paths(nonvirtual_test, virtual_tests))
            baseline_name = self._default_port.output_filename(
                nonvirtual_test, self._default_port.BASELINE_SUFFIX, extension)
            digests = self._digest(frozenset().union(*paths), baseline_name,
                                   self._is_reftest(nonvirtual_test))
            if not digests:
                _log.debug('Nothing to optimize.')
                return True

            predecessors_by_root = _predecessors_by_root(paths)
            for root, predecessors in predecessors_by_root.items():
                self._maybe_promote_root(root, predecessors, digests,
                                         baseline_name)
            graph = FallbackGraph.from_paths(paths, digests)
            redundant_locations = graph.find_redundant_locations()
            for location in redundant_locations:
                self._filesystem.remove(self._path(location, baseline_name))
                _log.debug('Removed %s (redundant with %s)', location,
                           ', '.join(map(str, graph.successors[location])))

            _log.debug('Final digests:')
            with _indent_log():
                for location in sorted(digests):
                    if location != FallbackGraph.ALL_PASS:
                        _log.debug('%s -> %s', location, digests[location])
        return True

    def _get_tests_to_optimize(self, test_name: str) -> Tuple[str, List[str]]:
        """Translate a test name into all associated tests to optimize.

        For convenience, we optimize nonvirtual and virtual tests together.

        Returns:
            A pair: a nonvirtual test name and any virtual tests to optimize.
            The nonvirtual test is always optimized. The virtual tests optimized
            are either `test_name` alone (if `test_name` is virtual), or every
            valid virtual test associated with the nonvirtual `test_name`.
        """
        nonvirtual_test = self._virtual_test_base(test_name)
        virtual_tests = []
        if not nonvirtual_test:
            nonvirtual_test = test_name
            for suite in self._default_port.virtual_test_suites():
                virtual_test = suite.full_prefix + test_name
                if self._virtual_test_base(virtual_test):
                    virtual_tests.append(virtual_test)
        else:
            virtual_tests.append(test_name)
        return nonvirtual_test, virtual_tests

    def _generate_search_paths(
            self,
            nonvirtual_test: str,
            virtual_tests: List[str],
    ) -> Iterator[SearchPath]:
        """Generate search paths taken by each port on each provided test.

        Note that:
          * Most paths are prefixed by others (e.g., win -> <generic> prefixes
            linux -> win -> <generic>). For completeness, this method generates
            all of them.
          * Ports where a test is skipped will not generate corresponding paths.
        """
        # Group ports to write less verbose output.
        skipped_ports_by_test = collections.defaultdict(list)
        for port in self._ports:
            if self._skips_test(port, nonvirtual_test):
                skipped_ports_by_test[nonvirtual_test].append(port)
                continue
            search_path = self._baseline_search_path(port)
            nonvirtual_locations = [
                self._location(path) for path in search_path
            ]
            yield nonvirtual_locations
            for virtual_test in virtual_tests:
                if self._skips_test(port, virtual_test):
                    skipped_ports_by_test[virtual_test].append(port)
                    continue
                virtual_locations = [
                    self._location(self._filesystem.join(path, virtual_test))
                    for path in search_path
                ]
                yield virtual_locations + nonvirtual_locations
        for test, ports in skipped_ports_by_test.items():
            port_names = [
                port.get_option('flag_specific') or port.name()
                for port in ports
            ]
            _log.debug('Excluding ports that skip "%s": %s', test,
                       ', '.join(port_names))

    def _skips_test(self, port: Port, test: str) -> bool:
        expectations = self._exp_cache.load(port)
        results = expectations.get_expectations(test).results
        return ResultType.Skip in results or port.skips_test(test)

    def write_by_directory(self, results_by_directory, writer, indent):
        """Logs results_by_directory in a pretty format."""
        for path in sorted(results_by_directory):
            writer('%s%s: %s' % (indent, self._location(path).platform
                                 or '(generic)', results_by_directory[path]))

    def read_results_by_directory(self, test_name, baseline_name):
        """Reads the baselines with the given file name in all directories.

        Returns:
            A dict from directory names to the digest of file content.
        """
        locations = set()
        for port in self._ports.values():
            locations.update(
                map(self._location, self._baseline_search_path(port)))

        digests = self._digest(locations, baseline_name,
                               self._is_reftest(test_name))
        return {
            self._path(location, baseline_name): digest
            for location, digest in digests.items()
        }

    def _digest(
            self,
            locations: FrozenSet[BaselineLocation],
            baseline_name: str,
            is_reftest: bool = False,
    ) -> DigestMap:
        digests = {}
        for location in locations:
            path = self._path(location, baseline_name)
            if self._filesystem.exists(path):
                digests[location] = ResultDigest(self._filesystem, path,
                                                 is_reftest)
        return digests

    def _maybe_promote_root(
            self,
            root: BaselineLocation,
            predecessors: FrozenSet[BaselineLocation],
            digests: DigestMap,
            baseline_name: str,
    ) -> None:
        predecessor_digest = _value_if_same(
            [digests.get(predecessor) for predecessor in predecessors])
        root_digest = digests.get(root)
        if predecessor_digest:
            # All of the root's immediate predecessors have the same value, so
            # any predecessor can be copied into the root's position. The
            # predecessors will be removed later in the redundant baseline
            # removal phase.
            predecessor, *_ = predecessors
            source = self._path(predecessor, baseline_name)
            dest = self._path(root, baseline_name)
            self._filesystem.maybe_make_directory(
                self._filesystem.dirname(dest))
            self._filesystem.copyfile(source, dest)
            digests[root] = predecessor_digest
            _log.debug('Promoted %s from %s', root,
                       ', '.join(map(str, predecessors)))
        elif root_digest and all(
                digests.get(predecessor, root_digest) != root_digest
                for predecessor in predecessors):
            # Remove the root if it can never (and should never) be reached.
            # If at least one predecessor has the same digest, that
            # predecessor will be deleted later instead of the root.
            self._filesystem.remove(self._path(root, baseline_name))
            _log.debug('Removed %s (unreachable)', root)

    @memoized
    def _path(self, location: BaselineLocation, baseline_name: str) -> str:
        """Build an absolute path from a baseline location and name.

        This is the inverse of `_location(...)`. The path has the format:
            /path/to/web_tests/<location>/<baseline-name>

        where:
            <location> = (platform/*/|flag-specific/*/)?(virtual/*/)?
        """
        parts = []
        if location.platform:
            parts.extend(['platform', location.platform])
        elif location.flag_specific:
            parts.extend(['flag-specific', location.flag_specific])
        if location.virtual_suite:
            parts.extend(['virtual', location.virtual_suite])
        return self._finder.path_from_web_tests(*parts, baseline_name)

    def _is_reftest(self, test_name: str) -> bool:
        return bool(self._default_port.reference_files(test_name))

    @memoized
    def _location(self, filename: str) -> BaselineLocation:
        """Guess a baseline location's parameters from a path.

        Arguments:
            filename: Either a baseline or test filename. Must be an absolute
                path.
        """
        parts = self._filesystem.relpath(
            filename,
            self._default_port.web_tests_dir()).split(self._filesystem.sep)
        platform = flag_specific = virtual_suite = ''
        if len(parts) >= 2:
            if parts[0] == 'platform':
                platform, parts = parts[1], parts[2:]
            elif parts[0] == 'flag-specific':
                flag_specific, parts = parts[1], parts[2:]
        if len(parts) >= 2 and parts[0] == 'virtual':
            virtual_suite = parts[1]
        return BaselineLocation(virtual_suite, platform, flag_specific)

    def _baseline_root(self) -> str:
        """Returns the name of the root (generic) baseline directory."""
        return self._default_port.web_tests_dir()

    def _baseline_search_path(self, port: Port) -> List[str]:
        """Returns the baseline search path (a list of absolute paths) of the
        given port."""
        return port.baseline_search_path() + [self._baseline_root()]

    def _virtual_test_base(self, test_name: str) -> Optional[str]:
        """Returns the base (non-virtual) version of test_name, or None if
        test_name is not virtual."""
        # This function should only accept a test name. Use baseline won't work
        # because some bases in VirtualTestSuites are full test name which has
        # .html as extension.
        return self._default_port.lookup_virtual_test_base(test_name)


class ResultDigest:
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
        # Note: This is not transitive (i.e., two extra results that are both
        # not implicit extra results are not necessarily equal).
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


class FallbackGraph:
    """A graph representing locations that baseline search can pass through."""
    # Sentinel node to track starting locations of baseline searches.
    _START = BaselineLocation(platform='<start>')
    # Sentinel node to force removal of all-pass nonvirtual baselines without
    # implementing a special case.
    ALL_PASS = BaselineLocation(platform='<all-pass>')

    def __init__(self, digests: DigestMap):
        # Contains digests of baselines that exist on disk.
        self._digests = digests
        self._digests.setdefault(self.ALL_PASS, ResultDigest(None, None))
        # This is an adjacency list.
        self.successors: Dict[BaselineLocation, Set[BaselineLocation]]
        self.successors = collections.defaultdict(set)

    @classmethod
    def from_paths(cls, paths: List[SearchPath],
                   digests: DigestMap) -> 'FallbackGraph':
        graph = cls(digests)
        for path in paths:
            graph.add_search_path(path)
        return graph

    def _add_edge(self, predecessor: BaselineLocation,
                  successor: BaselineLocation) -> None:
        self.successors[predecessor].add(successor)

    def add_search_path(self, path: SearchPath) -> None:
        # Filter out locations that do not correspond to a file on disk. Such
        # locations are irrelevant to baseline resolution, so the shortened path
        # should be equivalent.
        path = [location for location in path if location in self._digests]
        if not path:
            return
        path = [self._START, *path, self.ALL_PASS]
        if len(set(path)) < len(path):
            raise ValueError('path will create a cycle')
        for predecessor, successor in zip(path[:-1], path[1:]):
            self._add_edge(predecessor, successor)

    def find_redundant_locations(
            self,
            current: BaselineLocation = _START,
            visited: Optional[Set[BaselineLocation]] = None,
    ) -> Set[BaselineLocation]:
        """Find baseline locations that are redundant with depth-first search.

        A location is considered redundant when it has the same digest as all
        possible successors. All predecessors that fall back to a redundant
        location will still receive an equivalent baseline from one of the
        redundant location's successors, so redundant locations are safe to
        remove.
        """
        visited = visited or set()
        if current in visited or current == self.ALL_PASS:
            return frozenset()
        visited.add(current)
        redundant = set()
        if current != self._START:
            locations = [current, *self.successors[current]]
            if _value_if_same(map(self._digests.get, locations)):
                redundant.add(current)
        for successor in self.successors[current]:
            redundant.update(self.find_redundant_locations(successor, visited))
        return redundant


def _predecessors_by_root(paths: List[SearchPath]
                          ) -> Dict[BaselineLocation, Set[BaselineLocation]]:
    """Map baseline roots (virtual or nonvirtual) to their predecessors."""
    predecessors_by_root = collections.defaultdict(set)
    for path in paths:
        for predecessor, maybe_root in zip(path[:-1], path[1:]):
            if maybe_root.root:
                predecessors_by_root[maybe_root].add(predecessor)
    return predecessors_by_root


def _value_if_same(digests: Collection[Optional[ResultDigest]]
                   ) -> Optional[ResultDigest]:
    """Get the value of a collection of digests if they are the same."""
    if digests:
        first, *digests = digests
        # `ResultDigest` currently doesn't support `hash(...)`, so we cannot
        # implement this with a set.
        if all(first == digest for digest in digests):
            return first
    return None


@contextlib.contextmanager
def _indent_log(prefix: str = ' ' * 2) -> Iterator[None]:
    record_factory = logging.getLogRecordFactory()

    def make_indented_record(name,
                             level,
                             fn,
                             lno,
                             msg,
                             args,
                             exc_info,
                             func=None,
                             sinfo=None,
                             **kwargs):
        return record_factory(name, level, fn, lno, prefix + msg, args,
                              exc_info, func, sinfo, **kwargs)

    try:
        logging.setLogRecordFactory(make_indented_record)
        yield
    finally:
        logging.setLogRecordFactory(record_factory)
