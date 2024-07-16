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
import hashlib
import logging
import uuid
from pathlib import PurePosixPath
from typing import (
    Collection,
    Dict,
    FrozenSet,
    Iterator,
    List,
    Optional,
    Set,
    Tuple,
)
from urllib.parse import urlparse

from blinkpy.common.host import Host
from blinkpy.common.memoized import memoized
from blinkpy.common.path_finder import PathFinder
from blinkpy.web_tests.models.testharness_results import (
    is_all_pass_test_result,
    is_testharness_output,
    is_wdspec_output,
)
from blinkpy.web_tests.models.test_expectations import TestExpectationsCache
from blinkpy.web_tests.models.typ_types import ResultType
from blinkpy.web_tests.port.base import BaselineLocation, Port

_log = logging.getLogger(__name__)


# Sentinel node to force removal of all-pass nonvirtual baselines without
# implementing a special case.
BaselineLocation.ALL_PASS = BaselineLocation(platform='<all-pass>')
# Sentinel node to block optimization between a virtual and nonvirtual tree.
# Used for not deduplicating `virtual/stable/**/webexposed/` with their
# nonvirtual counterparts.
BaselineLocation.BLOCK = BaselineLocation(platform='<block>')

SearchPath = List[BaselineLocation]
DigestMap = Dict[BaselineLocation, 'ResultDigest']
# An adjacency list.
PredecessorMap = Dict[BaselineLocation, Set[BaselineLocation]]


class BaselineOptimizer:
    """Deduplicate baselines that are redundant to `run_web_tests.py`.

    `run_web_tests.py` has a fallback mechanism to search different locations
    for a test's baseline (i.e., expected text/image dump). The optimizer tries
    to minimize checkout size by removing baselines that would be redundant.

    See also:
        Fallback visualization: https://docs.google.com/drawings/d/13l3IUlSE99RoKjDwEWuY1O77simAhhF6Wi0fZdkSaMA/
        Details: https://chromium.googlesource.com/chromium/src/+/main/docs/testing/web_test_baseline_fallback.md
    """

    def __init__(self,
                 host: Host,
                 default_port: Port,
                 port_names,
                 exp_cache: Optional[TestExpectationsCache] = None,
                 check: bool = False):
        self._filesystem = host.filesystem
        self._finder = PathFinder(self._filesystem)
        self._default_port = default_port
        self._host = host
        self._ports = []
        for port_name in port_names:
            self._ports.append(self.port(port_name))
            flag_spec_options = (
                host.builders.flag_specific_options_for_port_name(port_name))
            self._ports.extend(
                self.port(port_name, flag_specific)
                for flag_specific in flag_spec_options)
        self._exp_cache = exp_cache or TestExpectationsCache()
        self._check = check

    @memoized
    def port(self,
             port_name: str,
             flag_specific: Optional[str] = None) -> Port:
        port = self._host.port_factory.get(port_name)
        if flag_specific:
            port.set_option_default('flag_specific', flag_specific)
        return port

    def optimize(self, test_name: str, suffix: str) -> bool:
        # For CLI compatibility, "suffix" is an extension without the leading
        # dot. Yet we use dotted extension everywhere else in the codebase.
        # TODO(robertma): Investigate changing the CLI.
        if self._check:
            _log.info('Checking %s (%s)', test_name, suffix)
        else:
            _log.info('Optimizing %s (%s)', test_name, suffix)
        assert not suffix.startswith('.')
        extension = '.' + suffix

        with _indent_log():
            nonvirtual_test, virtual_tests = self.get_tests_to_optimize(
                test_name)
            paths = list(
                self.generate_search_paths(nonvirtual_test, virtual_tests))
            baseline_name = self._default_port.output_filename(
                nonvirtual_test, self._default_port.BASELINE_SUFFIX, extension)
            digests = self._digest(frozenset().union(*paths), baseline_name,
                                   self._is_reftest(nonvirtual_test))
            if not digests:
                _log.debug('Nothing to optimize.')
                return True

            predecessors_by_root = _predecessors_by_root(paths)
            can_optimize = any([
                self._handle_root(root, predecessors, digests, baseline_name)
                for root, predecessors in predecessors_by_root.items()
            ])
            redundant_locations = find_redundant_locations(paths, digests)
            can_optimize = can_optimize or len(redundant_locations) > 0
            for location in redundant_locations:
                self._remove(location, baseline_name, 'redundant')

            _log.debug('Digests:')
            with _indent_log():
                for location in sorted(digests):
                    if location != BaselineLocation.ALL_PASS:
                        _log.debug('%s -> %s', location, digests[location])
        return not self._check or not can_optimize

    def get_tests_to_optimize(self, test_name: str) -> Tuple[str, List[str]]:
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

    def generate_search_paths(
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
        is_webexposed = 'webexposed' in _test_path(nonvirtual_test).parts
        # Group ports to write less verbose output.
        skipped_ports_by_test = collections.defaultdict(list)
        for port in self._ports:
            search_path = self._baseline_search_path(port)
            nonvirtual_locations = [
                self.location(path) for path in search_path
            ]
            if not self.skips_test(port, nonvirtual_test):
                yield nonvirtual_locations
            else:
                skipped_ports_by_test[nonvirtual_test].append(port)
            for virtual_test in virtual_tests:
                if self.skips_test(port, virtual_test):
                    skipped_ports_by_test[virtual_test].append(port)
                    continue
                virtual_locations = [
                    self.location(self._filesystem.join(path, virtual_test))
                    for path in search_path
                ]
                test_abs_path = self._finder.path_from_web_tests(virtual_test)
                virtual_suite = self.location(test_abs_path).virtual_suite
                # Virtual suite was parsed correctly from all baseline/test
                # paths.
                assert {
                    location.virtual_suite
                    for location in virtual_locations
                } == {virtual_suite}, virtual_locations
                if virtual_suite == 'stable' and is_webexposed:
                    virtual_locations.append(BaselineLocation.BLOCK)
                yield virtual_locations + nonvirtual_locations
        for test, ports in skipped_ports_by_test.items():
            port_names = [
                port.get_option('flag_specific') or port.name()
                for port in ports
            ]
            _log.debug('Excluding ports that skip "%s": %s', test,
                       ', '.join(port_names))

    def skips_test(self, port: Port, test: str) -> bool:
        expectations = self._exp_cache.load(port)
        results = expectations.get_expectations(test).results
        return ResultType.Skip in results or port.skips_test(test)

    def write_by_directory(self, results_by_directory, writer, indent):
        """Logs results_by_directory in a pretty format."""
        for path in sorted(results_by_directory):
            writer('%s%s: %s' % (indent, self.location(path).platform
                                 or '(generic)', results_by_directory[path]))

    def read_results_by_directory(self, test_name, baseline_name):
        """Reads the baselines with the given file name in all directories.

        Returns:
            A dict from directory names to the digest of file content.
        """
        locations = set()
        for port in self._ports.values():
            locations.update(
                map(self.location, self._baseline_search_path(port)))

        digests = self._digest(locations, baseline_name,
                               self._is_reftest(test_name))
        return {
            self.path(location, baseline_name): digest
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
            path = self.path(location, baseline_name)
            if location == BaselineLocation.BLOCK:
                digests[location] = random_digest()
            elif self._filesystem.exists(path):
                digests[location] = ResultDigest.from_file(
                    self._filesystem, path, is_reftest)
        return digests

    def _handle_root(
        self,
        root: BaselineLocation,
        predecessors: FrozenSet[BaselineLocation],
        digests: DigestMap,
        baseline_name: str,
    ) -> bool:
        """Determine whether to create or remove a generic baseline.

        Returns:
            Whether a change could be made on disk.
        """
        predecessor_digest = _value_if_same(
            [digests.get(predecessor) for predecessor in predecessors])
        root_digest = digests.get(root)
        if predecessor_digest:
            # All of the root's immediate predecessors have the same value, so
            # any predecessor can be copied into the root's position. The
            # predecessors will be removed later in the redundant baseline
            # removal phase.
            self._promote(root, predecessors, baseline_name)
            digests[root] = predecessor_digest
            return True
        elif root_digest and all(
                digests.get(predecessor, root_digest) != root_digest
                for predecessor in predecessors):
            # Remove the root if it can never (and should never) be reached.
            # If a predecessor has the same digest as the root, that predecessor
            # will be deleted later instead of the root.
            self._remove(root, baseline_name, 'unreachable')
            return True
        return False

    def _promote(
        self,
        root: BaselineLocation,
        predecessors: FrozenSet[BaselineLocation],
        baseline_name: str,
    ) -> None:
        predecessor, *_ = predecessors
        source = self.path(predecessor, baseline_name)
        dest = self.path(root, baseline_name)
        if self._check:
            # Show the full path instead of the abbreviated representation so
            # that the recommendation is actionable.
            _log.info('Can promote %s from %s', dest,
                      ', '.join(map(str, sorted(predecessors))))
        else:
            self._filesystem.maybe_make_directory(
                self._filesystem.dirname(dest))
            self._filesystem.copyfile(source, dest)
            _log.debug('Promoted %s from %s', root,
                       ', '.join(map(str, sorted(predecessors))))

    def _remove(
        self,
        location: BaselineLocation,
        baseline_name: str,
        explanation: str,
    ) -> None:
        path = self.path(location, baseline_name)
        if self._check:
            # As with `_promote(...)`, show the full path.
            _log.info('Can remove %s (%s)', path, explanation)
        else:
            self._filesystem.remove(path)
            _log.debug('Removed %s (%s)', location, explanation)

    @memoized
    def path(self, location: BaselineLocation, baseline_name: str) -> str:
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
    def location(self, filename: str) -> BaselineLocation:
        """Guess a baseline location's parameters from a path."""
        location, _ = self._default_port.parse_output_filename(filename)
        return location

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

    def __init__(self,
                 sha: str = _IMPLICIT_EXTRA_RESULT,
                 path: Optional[str] = None,
                 is_extra_result: bool = False):
        self.sha = sha
        self.path = path
        self.is_extra_result = is_extra_result

    @classmethod
    def from_file(cls, fs, path, is_reftest=False) -> 'ResultDigest':
        """Constructs the digest for a result.

        Args:
            fs: An instance of common.system.FileSystem.
            path: The path to a result file. If None is provided, the result is
                an *implicit* extra result.
            is_reftest: Whether the test is a reftest.
        """
        if path is None:
            return cls(cls._IMPLICIT_EXTRA_RESULT, path, is_extra_result=True)
        assert fs.exists(path), f'{path!r} does not exist'
        if path.endswith(f'.png') and is_reftest:
            return cls('', path, is_extra_result=True)

        with fs.open_binary_file_for_reading(path) as baseline_file:
            contents = baseline_file.read()

        is_extra_result = not contents
        if path.endswith('.txt'):
            try:
                contents_text = contents.decode()
                if is_testharness_output(contents_text) or is_wdspec_output(
                        contents_text):
                    # Canonicalize the representation of a testharness/wdspec
                    # baselines with insignificant whitespace.
                    #
                    # TODO(crbug.com/1482887): Digest the parsed testharness
                    # results to be fully independent of formatting.
                    #
                    # TODO(crbug.com/1482887): Consider making the serialized
                    # representation between `run_web_tests.py`'s
                    # `testharnessreport.js` and `format_testharness_baseline()`
                    # consistent.
                    contents_text = contents_text.strip()
                if is_all_pass_test_result(contents_text):
                    is_extra_result = True
                contents = contents_text.encode()
            except UnicodeDecodeError:
                is_extra_result = False

        digest = hashlib.sha1(contents).hexdigest()
        return cls(digest, path, is_extra_result)

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


ResultDigest.ALL_PASS = ResultDigest(is_extra_result=True)


def find_redundant_locations(paths: List[SearchPath],
                             digests: DigestMap) -> Set[BaselineLocation]:
    """Find baseline locations that are redundant and can be safely removed.

    At a high level, this is done by checking for baselines that, if deleted,
    would not affect the resolution of any path (see Example 1). This procedure
    repeats for as long as there were new baselines to delete because deleting a
    baseline can enable further deletions (see Example 2).

    Arguments:
        paths: A list of paths to check against.
        digests: Maps baseline locations that exist as files to their values.

    Example 1:

        (generic): a -:-> linux: a -> win: a -> (generic): a
                 \    :  /           /
        Virtual   +---:-+-----------+    Nonvirtual

        * Virtual root has nonvirtual 'linux' and 'win' as successors. All have
          value 'a', so the virtual root is removed. Virtual 'linux' and 'win'
          still resolve to 'a'.
        * Nonvirtual 'linux' and 'win' are removed similarly, leaving only the
          nonvirtual root.

    Example 2:

        linux: a -> win: b -:-> linux: a -> win: b
                       \    :              /
        Virtual         +---:-------------+    Nonvirtual

        * Virtual 'win' is removed because it only has nonvirtual 'win' as a
          successor (virtual 'linux' provides its own baseline).
        * Virtual 'linux' is removed on the next iteration because it now has
          nonvirtual 'linux' as a successor.
    """
    predecessors = _predecessors(paths)
    # The deletion order is significant because, on each deletion, the deleted
    # baseline's predecessors may become "critical" (i.e., should no longer be
    # deleted), and the number of those predecessors can vary based on past
    # deletions.
    #
    # Try two orders: DFS post- and pre-order deletions from the all-pass node.
    # In the event both orders remove the same number of files, favor
    # postorder, which generally favors deleting older OS or virtual baselines
    # first. See crbug.com/1512264 for an example where preorder deletion would
    # be better.
    return max(
        _find_redundant_locations_with_order(
            paths, digests, list(_visit_postorder(predecessors))),
        _find_redundant_locations_with_order(
            paths, digests, list(_visit_preorder(predecessors))),
        key=len,
    )


def _find_redundant_locations_with_order(
    paths: List[SearchPath],
    digests: DigestMap,
    removal_order: List[BaselineLocation],
) -> Set[BaselineLocation]:
    redundant_locations, digests = set(), dict(digests)
    # Because `_find_new_redundant_location(...)` returns a member of `digests`,
    # and that member is removed from `digests` to simulate file removal,
    # `digests` can only shrink in each iteration. At some point, the map will
    # stop shrinking, possibly becoming empty, which guarantees termination.
    while digests:
        new_redundant_location = _find_new_redundant_location(
            paths, digests, removal_order)
        if new_redundant_location:
            digests.pop(new_redundant_location)
            redundant_locations.add(new_redundant_location)
        else:
            break
    return redundant_locations


def _find_new_redundant_location(
    paths: List[SearchPath],
    digests: DigestMap,
    removal_order: List[BaselineLocation],
) -> Optional[BaselineLocation]:
    """Find a baseline location to remove, if available.

    Baselines are removed one at a time to avoid incompatible removals.
    Consider:

        linux: a -> win: a -:-> linux: b -> win: a
                            :
        Virtual             :   Nonvirtual

    Either virtual baseline may be removed, but not both. As soon as one is
    removed, the other one will no longer be considered redundant on the next
    iteration.

    When multiple baselines can be removed, as in this example, remove those
    for older OSes before newer OSes and the generic baseline.
    """
    digests.setdefault(BaselineLocation.ALL_PASS, ResultDigest.ALL_PASS)
    # Maps a location that exists as a file to successor files it can fall back
    # to if that location were removed.
    dependencies: Dict[BaselineLocation, Set[BaselineLocation]]
    dependencies = collections.defaultdict(set)
    for path in paths:
        path = [*path, BaselineLocation.ALL_PASS]
        assert len(set(path)) == len(path), ('duplicate location in path %s' %
                                             path)
        with contextlib.suppress(ValueError):
            # Get the resolved location (i.e., `source`), and the file the
            # path would fall back to next if `source` were deleted. When only
            # one source is present, this path resolves to the implicit
            # all-pass, in which case there is no file removal to contemplate.
            source, next_source, *_ = filter(digests.get, path)
            dependencies[source].add(next_source)

    # A location is considered redundant when, for all paths that resolve to it,
    # the next file in each search has the same digest. All predecessors that
    # fall back to a redundant location will still receive an equivalent
    # baseline from one of the redundant location's successors.
    for location in removal_order:
        successors = dependencies.get(location)
        if successors and _value_if_same(
                map(digests.get, [location, *successors])):
            return location
    return None


def _visit_preorder(
    predecessors: PredecessorMap,
    current: BaselineLocation = BaselineLocation.ALL_PASS,
    visited: Optional[Set[BaselineLocation]] = None,
) -> Iterator[BaselineLocation]:
    visited = visited or set()
    if current in visited:
        return
    visited.add(current)
    yield current
    for predecessor in predecessors[current]:
        yield from _visit_preorder(predecessors, predecessor, visited)


def _visit_postorder(
    predecessors: PredecessorMap,
    current: BaselineLocation = BaselineLocation.ALL_PASS,
    visited: Optional[Set[BaselineLocation]] = None,
) -> Iterator[BaselineLocation]:
    visited = visited or set()
    if current in visited:
        return
    visited.add(current)
    for predecessor in predecessors[current]:
        yield from _visit_postorder(predecessors, predecessor, visited)
    yield current


def _predecessors(paths: List[SearchPath]) -> PredecessorMap:
    """Map each location to its immediate predecessors."""
    predecessors = collections.defaultdict(set)
    for path in paths:
        path = [*path, BaselineLocation.ALL_PASS]
        for predecessor, successor in zip(path[:-1], path[1:]):
            predecessors[successor].add(predecessor)
    return predecessors


def _predecessors_by_root(paths: List[SearchPath]) -> PredecessorMap:
    """Map baseline roots (virtual or not) to their immediate predecessors."""
    return {
        maybe_root: predecessors
        for maybe_root, predecessors in _predecessors(paths).items()
        if maybe_root.root
    }


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


def _test_path(test: str) -> PurePosixPath:
    return PurePosixPath(urlparse(test).path)


def random_digest() -> ResultDigest:
    """Synthesize a digest that is guaranteed to not equal any other.

    The purpose of this digest is to simulate a baseline that prevents
    predecessors from being removed.
    """
    return ResultDigest(uuid.uuid4().hex)
