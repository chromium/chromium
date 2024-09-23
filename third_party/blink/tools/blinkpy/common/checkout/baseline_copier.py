# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import hashlib
from typing import Dict, Iterable, Iterator, List, Optional, Tuple

from blinkpy.common.host import Host
from blinkpy.common.checkout.baseline_optimizer import (
    BaselineOptimizer,
    DigestMap,
    ResultDigest,
    SearchPath,
    BaselineLocation,
    find_redundant_locations,
    random_digest,
)
from blinkpy.web_tests.port.base import Port

SourceMap = Dict[BaselineLocation, BaselineLocation]
CopyOperation = Tuple[Optional[str], str]


class BaselineCopier:
    def __init__(self, host: Host, default_port: Optional[Port] = None):
        self._host = host
        self._fs = host.filesystem
        self._default_port = default_port or host.port_factory.get()
        self._optimizer = BaselineOptimizer(host, self._default_port,
                                            host.port_factory.all_port_names())

    def find_baselines_to_copy(
        self,
        test_name: str,
        suffix: str,
        baseline_set: 'TestBaselineSet',
    ) -> Iterator[CopyOperation]:
        """Find the minimal set of copied baselines to preserve expectations.

        When new baselines are downloaded for only some platforms or virtual
        suites, other platforms or virtual suites that fell back to the old
        baselines may incorrectly inherit the new baselines as well.

        This method finds baselines to preserve so that rebaselining does not
        clobber any other expectations. At a high level, this is accomplished by
        simulating giving every location a copy of its resolved baseline, then
        excluding copies that would be redundant even after a "worst-case"
        rebaseline.

        Arguments:
            test_name: Either a virtual or nonvirtual test name, with behavior
                matching `BaselineOptimizer.get_tests_to_optimize`. The
                nonvirtual test's baselines are always copied. If the given test
                was virtual, only that virtual suite's baselines will be copied;
                otherwise, all virtual baselines associated with the nonvirtual
                test will be copied.
            suffix: A file extension without a leading dot '.'.
            baseline_set: Contains information about which tests should be
                rebaselined for which platforms.

        Example:

            Virtual  win            :   +------> win: a     Nonvirtual
                        \           :  /               \
                         (generic) -:-+--------+        (generic)
                        /           :  \        \      /
            mac12 -> mac            :   mac12 -> mac: b

            * All 'mac*' locations have nonvirtual 'mac' as a source. Likewise
              for 'win'.
            * Suppose we will only rebaseline 'mac'. In the simulation, we place
              random digests x, y at (non)virtual 'mac'. The resolved digests
              now look like:

            Virtual     win: a            :   +---------> win: a     Nonvirtual
                              \           :  /                  \
                               (generic) -:-+-----------+        (generic)
                              /           :  \           \      /
            mac12: b -> mac: x            :   mac12: b -> mac: y

            * Virtual 'win' is considered a redundant copy, (non)virtual 'mac12'
              are not. Therefore, 'b' is copied to the 'mac12' locations.
        """
        nonvirtual_test, virtual_tests = self._optimizer.get_tests_to_optimize(
            test_name)
        paths = list(
            self._optimizer.generate_search_paths(nonvirtual_test,
                                                  virtual_tests))
        baseline_name = self._default_port.output_filename(
            nonvirtual_test, self._default_port.BASELINE_SUFFIX, '.' + suffix)

        sources = self._resolve_sources(paths, baseline_name)
        # Simulate the optimizer's digest map if we were to give each location
        # a copy of its resolved baseline. "Filling" the baseline graph in this
        # way would ensure no location would need to fall back anywhere. This
        # brute-force strategy would clearly preserve baselines for ports not
        # rebaselined.
        digests = self._simulate_physical_copies(sources, baseline_name)

        # Here, we refine the brute-force strategy, which can create unnecessary
        # copies (e.g., two ports not rebaselined that fall back to each other
        # don't both need copies). This is accomplished by:
        #  1. Simulating a "worst-case" rebaseline where every new baseline has
        #     never been seen before. These new baselines will never be
        #     redundant with copied baselines.
        #  2. Use the optimizer as a opaque-box algorithm to identify redundant
        #     copies (because they fall back to identical copies). Because of
        #     step [1], copies that would become necessary after the
        #     "worst-case" rebaseline are not considered redundant by the
        #     optimizer.
        for test in (nonvirtual_test, *virtual_tests):
            for location in self._locations_to_rebaseline(test, baseline_set):
                # Provides its own physical file.
                sources[location] = location
                digests[location] = random_digest()
        redundant_copies = find_redundant_locations(paths, digests)

        # Find destinations to copy to. These are locations that:
        #  1. Will not provide their own baselines after rebaselining. This
        #     avoids copying files to themselves, or copying to paths that will
        #     be overwritten shortly by new baselines.
        #  2. Are not redundant copies (see above).
        for location, source in sources.items():
            if location != source and location not in redundant_copies:
                source_path = None
                if source != BaselineLocation.ALL_PASS:
                    source_path = self._optimizer.path(source, baseline_name)
                dest_path = self._optimizer.path(location, baseline_name)
                yield source_path, dest_path

    def _locations_to_rebaseline(
        self,
        test: str,
        baseline_set: 'TestBaselineSet',
    ) -> Iterator[BaselineLocation]:
        for build, step_name, port_name in baseline_set.runs_for_test(test):
            flag_specific = None
            if step_name:
                flag_specific = self._host.builders.flag_specific_option(
                    build.builder_name, step_name)
            port = self._optimizer.port(port_name, flag_specific)
            location = self._optimizer.location(
                self._fs.join(port.baseline_version_dir(), test))
            maybe_test_file_path = self._fs.join(port.web_tests_dir(), test)
            # Coerce this location to a non-virtual one for physical tests under
            # a virtual directory. See crbug.com/1450725.
            if self._fs.exists(maybe_test_file_path):
                location = BaselineLocation(
                    platform=location.platform,
                    flag_specific=location.flag_specific)
            yield location

    def _resolve_sources(
        self,
        paths: List[SearchPath],
        baseline_name: str,
    ) -> SourceMap:
        """Determine which locations fall back to each other.

        This is done by simulating the fallback algorithm.

        Returns:
            A map from a location to the location where its resolved baseline
            actually exists as a file. A location that provides its own baseline
            (i.e., does not fall back anywhere) will map to itself.
        """
        sources = {}
        for path in paths:
            source = BaselineLocation.ALL_PASS
            for location in path:
                if self._fs.exists(
                        self._optimizer.path(location, baseline_name)):
                    source = location
                    break
            sources[path[0]] = source
        return sources

    def _simulate_physical_copies(self, sources: SourceMap,
                                  baseline_name: str) -> DigestMap:
        """Simulate the digests of giving every location its own file."""
        digests = {}
        for location, source in sources.items():
            if source == BaselineLocation.ALL_PASS:
                digests[location] = ResultDigest.ALL_PASS
            else:
                # Rather than actually digesting the file's arbitrarily long
                # contents, we assign a digest that's a proxy for the path. When
                # the baselines are unoptimized, this can result in extra copied
                # baselines, which is OK.
                path = self._optimizer.path(source, baseline_name)
                digests[location] = ResultDigest(
                    hashlib.sha1(path.encode()).hexdigest(), path)
        return digests

    def write_copies(self,
                     copies: Iterable[CopyOperation],
                     placeholder: str = '') -> None:
        for source, dest in copies:
            self._fs.maybe_make_directory(self._fs.dirname(dest))
            if source:
                self._fs.copyfile(source, dest)
            else:
                self._fs.write_text_file(dest, placeholder)
