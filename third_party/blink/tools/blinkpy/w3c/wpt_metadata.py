# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A common library for using WPT metadata in Blink."""

import collections
import contextlib
import optparse
import re
from typing import (
    Any,
    Dict,
    FrozenSet,
    Iterator,
    Literal,
    Optional,
    Set,
    Tuple,
)
from blinkpy.common import path_finder
from blinkpy.common.host import Host
from blinkpy.common.memoized import memoized
from blinkpy.common.system.filesystem import FileSystem
from blinkpy.web_tests.port.base import Port

path_finder.bootstrap_wpt_imports()
from wptrunner import (
    manifestexpected,
    manifestupdate,
    metadata,
    products,
    wpttest,
)
from wptrunner.wptmanifest import node as wptnode
from wptrunner.wptmanifest.backends import static
from wptrunner.manifestexpected import TestNode, SubtestNode

RunInfo = Dict[str, Any]
METADATA_EXTENSION: str = '.ini'


def make_empty_test(other: TestNode) -> TestNode:
    test_ast = wptnode.DataNode()
    test_ast.append(wptnode.DataNode(other.id))
    exp = static.compile_ast(test_ast,
                             expr_data={},
                             data_cls_getter=manifestexpected.data_cls_getter,
                             test_path=other.root.test_path)
    with contextlib.suppress(KeyError):
        exp.set('type', other.test_type)
    return exp.get_test(other.id)


def fill_implied_expectations(test: TestNode,
                              extra_subtests: Optional[Set[str]] = None):
    """Populate a test result with implied OK/PASS expectations.

    This is a helper for diffing WPT results.
    """
    default_expected = default_expected_by_type()
    _ensure_expectation(test, default_expected[test.test_type, False])
    for subtest in test.subtests:
        _ensure_expectation(test.get_subtest(subtest),
                            default_expected[test.test_type, True])
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
        # Do not memoize with `self` as an argument, which is unhashable.
        self.possible_values = memoized(self._possible_values)

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
            if host.builders.has_wptrunner_steps(builder)
        }

        for builder in wptrunner_builders:
            port_name = host.builders.port_name_for_builder_name(builder)
            _, build_config, *_ = host.builders.specifiers_for_builder(builder)

            for step in host.builders.step_names_for_builder(builder):
                if not host.builders.uses_wptrunner(builder, step):
                    continue
                flag_specific = host.builders.flag_specific_option(
                    builder, step) or ''
                port = host.port_factory.get(
                    port_name,
                    optparse.Values({
                        'configuration': build_config,
                        'flag_specific': flag_specific,
                    }))
                product = host.builders.product_for_build_step(builder, step)
                debug = port.get_option('configuration') == 'Debug'
                virtual_suites = {''}
                # Only `content_shell` runs virtual tests, currently.
                if product == Port.CONTENT_SHELL_NAME:
                    virtual_suites.update(
                        suite.full_prefix.split('/')[1]
                        for suite in port.virtual_test_suites())
                for virtual_suite in virtual_suites:
                    config = metadata.RunInfo({
                        'product': product,
                        'os': port.operating_system(),
                        'port': port.version(),
                        'debug': debug,
                        'flag_specific': flag_specific,
                        'virtual_suite': virtual_suite,
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
            test_name = wpt_url_to_exp_test(test.id)
            virtual_suite = config['virtual_suite']
            if virtual_suite:
                test_name = f'virtual/{virtual_suite}/{test_name}'
                # Check whether this is a valid virtual test.
                if not port.lookup_virtual_test_base(test_name):
                    return True
            if port.skips_test(test_name):
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

    def _possible_values(self, prop: str) -> FrozenSet[Any]:
        return frozenset(config[prop] for config in self)


TestType = Literal[tuple(wpttest.manifest_test_cls)]


@memoized
def default_expected_by_type() -> Dict[Tuple[TestType, bool], str]:
    """Make a registry of default expected statuses.

    The registry's key is composed of a test type and a flag that is `True` for
    subtests, and `False` for test-level statuses.
    """
    default_expected = {}
    for test_type, test_cls in wpttest.manifest_test_cls.items():
        if test_cls.result_cls:
            expected = test_cls.result_cls.default_expected
            default_expected[test_type, False] = expected
        if test_cls.subtest_result_cls:
            expected = test_cls.subtest_result_cls.default_expected
            default_expected[test_type, True] = expected
    return default_expected


@memoized
def can_have_subtests(test_type: TestType) -> bool:
    return (test_type, True) in default_expected_by_type()


def exp_test_to_wpt_url(test: str) -> Optional[str]:
    for wpt_dir, url_prefix in Port.WPT_DIRS.items():
        if test.startswith(wpt_dir):
            test = test.replace(wpt_dir + '/', url_prefix, 1)
            # Directory globs in TestExpectations resolve to a "pseudo-test ID".
            # Do not give an ID for non-directory globs.
            if test.endswith('/*'):
                test = test[:-len('*')] + '__dir__'
            elif test.endswith('*'):
                return None
            return test
    return None


def wpt_url_to_exp_test(test: str) -> str:
    for wpt_dir, url_prefix in Port.WPT_DIRS.items():
        if test.startswith(url_prefix):
            return test.replace(url_prefix, wpt_dir + '/', 1)
    raise ValueError('no matching WPT roots found')
