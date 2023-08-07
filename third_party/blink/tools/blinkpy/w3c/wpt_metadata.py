# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A common library for using WPT metadata in Blink."""

import collections
import contextlib
import optparse
import re
from typing import Any, Dict, Iterator, Optional, Set
from blinkpy.common import path_finder
from blinkpy.common.host import Host
from blinkpy.common.memoized import memoized
from blinkpy.common.system.filesystem import FileSystem
from blinkpy.web_tests.port.base import Port

path_finder.bootstrap_wpt_imports()
from wptrunner import manifestexpected, manifestupdate, metadata, products
from wptrunner.wptmanifest import node as wptnode
from wptrunner.manifestexpected import TestNode, SubtestNode

RunInfo = Dict[str, Any]


def fill_implied_expectations(test: TestNode,
                              extra_subtests: Optional[Set[str]] = None,
                              test_type: str = 'testharness'):
    """Populate a test result with implied OK/PASS expectations.

    This is a helper for diffing WPT results.
    """
    default_test_status = 'OK' if test_type == 'testharness' else 'PASS'
    _ensure_expectation(test, default_test_status)
    for subtest in test.subtests:
        _ensure_expectation(test.get_subtest(subtest), 'PASS')
    missing_subtests = (extra_subtests or set()) - set(test.subtests)
    for subtest in missing_subtests:
        subtest_node = SubtestNode(wptnode.DataNode(subtest))
        # Append to both the test container and the underlying AST.
        test.append(subtest_node)
        test.node.append(subtest_node.node)
        _ensure_expectation(subtest_node, 'PASS')


def _ensure_expectation(test: TestNode, default_status: str):
    if not test.has_key('expected'):
        test.set('expected', default_status)


BUG_PATTERN = re.compile(r'(crbug(\.com)?/)?(?P<bug>\d+)')


class TestConfigurations(collections.abc.Mapping):
    def __init__(self, fs: FileSystem, configs):
        self._fs = fs
        self._finder = path_finder.PathFinder(self._fs)
        self._configs = configs
        self._get_dir_manifest = memoized(manifestexpected.get_dir_manifest)

    def __getitem__(self, config: metadata.RunInfo) -> Port:
        return self._configs[config]

    def __len__(self) -> int:
        return len(self._configs)

    def __iter__(self) -> Iterator[metadata.RunInfo]:
        return iter(self._configs)

    @classmethod
    def generate(cls, host: Host) -> 'TestConfigurations':
        """Construct run info representing all Chromium test environments.

        Each property in a config represents a value that metadata keys can be
        conditioned on (e.g., 'os').
        """
        configs = {}
        wptrunner_builders = {
            builder
            for builder in host.builders.all_builder_names()
            if host.builders.uses_wptrunner(builder)
        }

        for builder in wptrunner_builders:
            port_name = host.builders.port_name_for_builder_name(builder)
            _, build_config, *_ = host.builders.specifiers_for_builder(builder)

            for step in host.builders.step_names_for_builder(builder):
                flag_specific = host.builders.flag_specific_option(
                    builder, step)
                port = host.port_factory.get(
                    port_name,
                    optparse.Values({
                        'configuration': build_config,
                        'flag_specific': flag_specific,
                    }))
                product = host.builders.product_for_build_step(builder, step)
                debug = port.get_option('configuration') == 'Debug'
                config = metadata.RunInfo({
                    'product': product,
                    'os': port.operating_system(),
                    'port': port.version(),
                    'debug': debug,
                    'flag_specific': flag_specific or '',
                })
                configs[config] = port
        return cls(host.filesystem, configs)

    def enabled_configs(self, test: manifestupdate.TestNode,
                        metadata_root: str) -> Set[metadata.RunInfo]:
        """Find configurations where the given test is enabled.

        This method also checks parent `__dir__.ini` to give a definitive
        answer.

        Arguments:
            test: Test node holding expectations in conditional form.
            test_path: Path to the test file (relative to the test root).
            metadata_root: Absolute path to where the `.ini` files are stored.
        """
        return {
            config
            for config in self._configs
            if not self._config_disabled(config, test, metadata_root)
        }

    def _config_disabled(
        self,
        config: metadata.RunInfo,
        test: manifestupdate.TestNode,
        metadata_root: str,
    ) -> bool:
        with contextlib.suppress(KeyError):
            port = self._configs[config]
            if port.default_smoke_test_only():
                test_id = test.id
                if test_id.startswith('/'):
                    test_id = test_id[1:]
                if (not self._finder.is_wpt_internal_path(test_id)
                        and not self._finder.is_wpt_path(test_id)):
                    test_id = self._finder.wpt_prefix() + test_id
                if port.skipped_due_to_smoke_tests(test_id):
                    return True
        with contextlib.suppress(KeyError):
            product = products.Product({}, config['product'])
            executor_cls = product.executor_classes.get(test.test_type)
            # This test is implicitly disabled because its type is not supported
            # by the product under test.
            if not executor_cls:
                return True
        with contextlib.suppress(KeyError):
            return test.get('disabled', config)
        test_dir = test.parent.test_path
        while test_dir:
            test_dir = self._fs.dirname(test_dir)
            abs_test_dir = self._fs.join(metadata_root, test_dir)
            disabled = self._directory_disabled(abs_test_dir, config)
            if disabled is not None:
                return disabled
        return False

    def _directory_disabled(self, dir_path: str,
                            config: metadata.RunInfo) -> Optional[bool]:
        """Check if a `__dir__.ini` in the given directory disables tests.

        Returns:
            * True if the directory disables tests.
            * False if the directory explicitly enables tests.
            * None if the key is not present (e.g., `__dir__.ini` doesn't
              exist). We may need to search other `__dir__.ini` to get a
              conclusive answer.
        """
        metadata_path = self._fs.join(dir_path, '__dir__.ini')
        manifest = self._get_dir_manifest(metadata_path, config)
        return manifest.disabled if manifest else None
