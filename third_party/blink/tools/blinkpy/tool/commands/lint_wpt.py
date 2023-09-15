# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Lint WPT files and metadata."""

import argparse
import collections
import contextlib
import enum
import functools
import inspect
import io
import logging
import multiprocessing
import optparse
import pathlib
import random
import re
import textwrap
import typing
import urllib.parse
from typing import (
    Dict,
    FrozenSet,
    Hashable,
    List,
    Optional,
    Set,
    Tuple,
    Type,
    Union,
)

from blinkpy.common import path_finder
from blinkpy.common.host import Host
from blinkpy.common.system import command_line
from blinkpy.common.system.filesystem import FileSystem
from blinkpy.tool.commands.command import Command
from blinkpy.w3c import wpt_metadata
from blinkpy.w3c.common import is_basename_skipped
from blinkpy.w3c.wpt_manifest import WPTManifest
from blinkpy.web_tests.port.base import Port
from blinkpy.web_tests.port.factory import add_common_wpt_options

path_finder.bootstrap_wpt_imports()
from tools.lint import lint as wptlint
from tools.lint import rules
from wptrunner import manifestupdate, metadata, wptmanifest, wpttest
from wptrunner.manifestexpected import fuzzy_prop
from wptrunner.wptmanifest import node as wptnode
from wptrunner.wptmanifest.backends import conditional, static

_log = logging.getLogger(__name__)


class MetadataRule(rules.Rule):
    """Base type for metadata-related rules."""


class SectionType(enum.Enum):
    DIRECTORY = enum.auto()
    ROOT = enum.auto()
    TEST = enum.auto()
    SUBTEST = enum.auto()


class MetadataBadSyntax(MetadataRule):
    name = 'META-BAD-SYNTAX'
    description = 'WPT metadata file could not be parsed: %(detail)s'
    to_fix = r"""
    Check that the file contents conform to:
    https://web-platform-tests.org/tools/wptrunner/docs/expectation.html#metadata-format

    A common pitfall is an unescaped ']' in the section heading, which you can
    escape with a backslash '\'.
    """


class MetadataUnsortedSection(MetadataRule):
    name = 'META-UNSORTED-SECTION'
    description = ('Section contains unsorted keys or subsection headings: '
                   '%(predecessor)r should precede %(successor)r')
    to_fix = """
    Within a block (indentation level), all keys must precede all headings, and
    keys must be sorted lexographically amongst themselves (and likewise for
    headings).
    """


class MetadataEmptySection(MetadataRule):
    name = 'META-EMPTY-SECTION'
    description = 'Empty section should be removed:%(heading)s'
    to_fix = """
    A section without keys or subsections has no effect and should be removed.
    The (sub)tests represented by empty sections default to enabled and
    all-pass.
    """


class MetadataUnknownTest(MetadataRule):
    name = 'META-UNKNOWN-TEST'
    description = 'Test ID does not exist: %(test)r'
    to_fix = """
    Check that the top-level section headings are not misspelled.
    """


class MetadataSectionTooDeep(MetadataRule):
    name = 'META-SECTION-TOO-DEEP'
    description = ('%(section_type)s section%(heading)s '
                   'should not contain subheadings')
    to_fix = """
    Check that sections are indented correctly for the metadata and test type.
    In particular:
      * `__dir__.ini` should not contain sections.
      * Only metadata for `testharness` tests may contain subtest sections.
    """


class MetadataUnknownKey(MetadataRule):
    name = 'META-UNKNOWN-KEY'
    description = '%(section_type)s section%(heading)s should not have key %(key)r'
    to_fix = """
    Check that all keys are spelled and indented correctly.
    """
    valid_keys = {
        SectionType.DIRECTORY:
        frozenset({
            'disabled',
            'restart-after',
            'fuzzy',
            'implementation-status',
            'tags',
            'bug',
        }),
        SectionType.ROOT:
        frozenset({
            'disabled',
            'restart-after',
            'fuzzy',
            'implementation-status',
            'tags',
            'bug',
        }),
        SectionType.TEST:
        frozenset({
            'expected',
            'disabled',
            'restart-after',
            'fuzzy',
            'implementation-status',
            'tags',
            'bug',
        }),
        SectionType.SUBTEST:
        frozenset({
            'expected',
            'disabled',
        }),
    }


class MetadataBadValue(MetadataRule):
    name = 'META-BAD-VALUE'
    description = '%(section_type)s key %(key)r has invalid value %(value)r'
    to_fix = """
    Check that the value satisfies any required formats:
    https://web-platform-tests.org/tools/wptrunner/docs/expectation.html#web-platform-tests-metadata
    """
    implementation_statuses = {'implementing', 'not-implementing', 'default'}


class MetadataConditionsUnnecessary(MetadataRule):
    name = 'META-CONDITIONS-UNNECESSARY'
    description = '%(section_type)s key %(key)r always has value %(value)r'
    to_fix = """
    Express the key as an unconditional expression without `if`.
    """


class MetadataUnnecessaryKey(MetadataRule):
    name = 'META-UNNECESSARY-KEY'
    description = ("%(section_type)s%(heading)s key %(key)r always resolves "
                   'to an implied %(value)r and should be removed')
    to_fix = """
    A key that only resolves to an implied default value should be removed. For
    example, `expected: (OK|PASS)` is implied by an absent `expected` key.
    """


class MetadataUnreachableValue(MetadataRule):
    name = 'META-UNREACHABLE-VALUE'
    description = '%(section_type)s key %(key)r has an unused %(condition)s'
    to_fix = """
    Check that at least one test configuration takes the condition branch.
    """


class MetadataUnknownProp(MetadataRule):
    name = 'META-UNKNOWN-PROP'
    description = ('%(section_type)s key %(key)r %(condition)s '
                   'uses unrecognized property %(prop)s')
    to_fix = """
    Check that all property names are spelled correctly:
    https://chromium.googlesource.com/chromium/src/+/HEAD/docs/testing/web_platform_tests_wptrunner.md#conditional-values
    """


class MetadataUnknownPropValue(MetadataRule):
    name = 'META-UNKNOWN-PROP-VALUE'
    description = ('%(section_type)s key %(key)r %(condition)s compares '
                   '%(prop)r against unrecognized value %(value)r')
    to_fix = """
    Check that all property values are valid and spelled correctly:
    https://chromium.googlesource.com/chromium/src/+/HEAD/docs/testing/web_platform_tests_wptrunner.md#conditional-values
    """


class MetadataSingleElementList(MetadataRule):
    name = 'META-SINGLE-ELEM-LIST'
    description = (
        '%(section_type)s%(heading)s key %(key)r has a single-element list '
        'that should be unwrapped to %(value)r')
    to_fix = """
    The list form should be replaced by the unwrapped value, which is
    semantically equivalent.
    """


class MetadataLongTimeout(MetadataRule):
    name = 'META-LONG-TIMEOUT'
    description = ('%(test)r should be disabled when it consistently times '
                   "out even with 'timeout=long'")
    to_fix = """
    To reduce resource waste, add a `disabled: ...` key that disables the test
    for configurations where there are consistent timeouts.

    Splitting a `testharness.js` test with many subtests may also help them run
    to completion.
    """


class IgnoreListInvalidRule(rules.Rule):
    name = 'IGNORELIST-BAD-RULE'
    description = 'Rule %(rule)r cannot be ignored or does not exist'
    to_fix = """
    Check that all rules are spelled correctly in `lint.ignore`. `META-*` rules
    are only defined in Chromium and cannot be ignored.
    """


LintError = Tuple[str, str, str, Optional[int]]
ValueNode = Union[wptnode.ValueNode, wptnode.AtomNode, wptnode.ListNode]
Condition = Optional[wptnode.Node]


class WebPlatformTestRegexp(rules.WebPlatformTestRegexp):
    def __init__(self, fs: FileSystem):
        super().__init__()
        self._fs = fs

    def applies(self, path: str) -> bool:
        # Skip searching for the forbidden `web-platform.test` domain if this
        # is a metadata file. Checking the metadata file is redundant because
        # its contents simply reflect the corresponding test file that needs to
        # be fixed first.
        _, extension = self._fs.splitext(path)
        return extension != '.ini' and super().applies(path)


class LintWPT(Command):
    name = 'lint-wpt'
    show_in_main_help = True
    help_text = __doc__.strip().splitlines()[0]
    long_help = __doc__
    ignorelist_filename: str = 'lint.ignore'

    def __init__(self,
                 tool: Host,
                 configs: Optional[wpt_metadata.TestConfigurations] = None):
        super().__init__()
        self._tool = tool
        self._fs = self._tool.filesystem
        self._default_port = self._tool.port_factory.get()
        # Ensure that `self._default_port`, which is shared among child
        # processes, never updates the manifest, as doing so could race.
        self._default_port.set_option_default('manifest_update', False)
        self._default_port.set_option_default(
            'test_types', typing.get_args(wpt_metadata.TestType))
        self._finder = path_finder.PathFinder(self._fs)
        self._configs = configs or wpt_metadata.TestConfigurations.generate(
            self._tool)

    def parse_args(self, args: List[str]) -> Tuple[optparse.Values, List[str]]:
        # TODO(crbug.com/1431070): Migrate `blink_tool.py` to stdlib's
        # `argparse`. `optparse` is deprecated. Also, consider making our own
        # command subparser [1] instead of using the `wpt lint` one.
        #
        # [1]: https://docs.python.org/3/library/argparse.html#argparse.ArgumentParser.add_subparsers
        parser = command_line.ArgumentParser(description=self.long_help,
                                             parents=[wptlint.create_parser()],
                                             conflict_handler='resolve')
        # Hide formatting parameters that won't be used.
        parser.add_argument('--markdown',
                            action='store_true',
                            help=argparse.SUPPRESS)
        parser.add_argument('--json',
                            action='store_true',
                            help=argparse.SUPPRESS)
        parser.add_argument('--github-checks-text-file',
                            help=argparse.SUPPRESS)
        add_common_wpt_options(parser)
        parameters = parser.parse_args(args)
        if not parameters.repo_root:
            parameters.repo_root = self._finder.path_from_wpt_tests()
        return optparse.Values(vars(parameters)), []

    def execute(self, options: optparse.Values, _args: List[str],
                _tool: Host) -> Optional[int]:
        if options.manifest_update:
            for path in Port.WPT_DIRS:
                WPTManifest.ensure_manifest(self._default_port, path)
        # Pipe `wpt lint`'s logs into `blink_tool.py`'s formatter.
        wptlint.logger = _log
        # Repurpose the `json` format to collect all lint errors, including
        # non-metadata ones, so that `lint-wpt` can customize the logs.
        errors = self.check_ignorelist(options.repo_root)
        options.json = True
        wptlint.output_errors_json = (
            lambda _log, worker_errors: errors.extend(worker_errors))
        # This is ugly, but it works around crbug.com/1470511 while still
        # allowing for parallelism.
        self._initialize_rule_registry()
        wptlint.multiprocessing.Pool = functools.partial(
            multiprocessing.Pool, initializer=self._initialize_rule_registry)
        exit_code = wptlint.main(**vars(options))
        self._log_errors(errors, options.repo_root)
        return exit_code

    def _initialize_rule_registry(self):
        """Add custom rules to the linter rule registry. Must be idempotent."""
        # Replace `web-platform.test` regexp rule with a metadata-aware one.
        wptlint.regexps = [
            regexp for regexp in wptlint.regexps
            if not isinstance(regexp, rules.WebPlatformTestRegexp)
        ]
        wptlint.regexps.append(WebPlatformTestRegexp(self._fs))
        if self.check_metadata not in wptlint.file_lints:
            wptlint.file_lints.append(self.check_metadata)

    def _log_errors(self, errors: List[LintError], repo_root: str):
        if not errors:
            _log.info('All files OK.')
            return
        manifest = self._manifest(repo_root)
        wptlint.output_errors_text(_log.error, errors)
        test_file_errors, metadata_file_errors = [], []
        for error in errors:
            _, _, path, _ = error
            if self._is_dir_metadata(path) or self._test_path(manifest, path):
                metadata_file_errors.append(error)
            elif path != self.ignorelist_filename and not is_basename_skipped(
                    self._fs.basename(path)):
                test_file_errors.append(error)

        paragraphs = []
        # TODO(crbug.com/1406669): Document the supplemental `META-*` rules in
        # `//docs/testing` and add the link.
        paragraphs.append(
            'You must address all errors; for details on how to fix them, see '
            'https://web-platform-tests.org/writing-tests/lint-tool.html')
        if test_file_errors:
            error, _, path, _ = test_file_errors[-1]
            context = {'error': error, 'path': path}
            ignorelist_path = self._fs.abspath(
                self._fs.join(repo_root, 'lint.ignore'))
            paragraphs.append(
                "However, for errors in test files, it's sometimes OK to add "
                'lines to `%s` to ignore them.' % self._fs.relpath(
                    ignorelist_path, self._finder.path_from_web_tests()))
            paragraphs.append(
                "For example, to make the lint tool ignore all '%(error)s' "
                'errors in the %(path)s file, you could add the following '
                'line to the lint.ignore file:' % context)
            paragraphs.append('%(error)s: %(path)s' % context)
        if metadata_file_errors:
            paragraphs.append(
                'Errors for `*.ini` metadata files cannot be ignored and must '
                'be fixed.')
        wptlint.output_error_count(
            collections.Counter(error for error, _, _, _ in errors))
        for paragraph in paragraphs:
            _log.info('')
            for line in textwrap.wrap(paragraph):
                _log.info(line)

    def check_ignorelist(self, repo_root: str) -> List[LintError]:
        ignorelist_path = self._fs.join(repo_root, self.ignorelist_filename)
        with self._fs.open_text_file_for_reading(
                ignorelist_path) as ignorelist_file:
            ignorelist, _ = wptlint.parse_ignorelist(ignorelist_file)
        ignorable_rules = {
            maybe_rule.name: maybe_rule
            for maybe_rule in rules.__dict__.values()
            if inspect.isclass(maybe_rule)
            and issubclass(maybe_rule, (rules.Rule, rules.Regexp))
        }
        invalid_rules = set(ignorelist) - set(ignorable_rules)
        return [
            IgnoreListInvalidRule.error(self.ignorelist_filename,
                                        {'rule': rule})
            for rule in sorted(invalid_rules)
        ]

    def check_metadata(self, repo_root: str, path: str,
                       metadata_file: io.BytesIO) -> List[LintError]:
        manifest = self._manifest(repo_root)
        test_path = self._test_path(manifest, path)
        if not test_path and not self._is_dir_metadata(path):
            return []
        try:
            ast = wptmanifest.parse(metadata_file)
        except wptmanifest.parser.ParseError as error:
            context = {'detail': error.detail}
            return [MetadataBadSyntax.error(path, context, error.line)]

        test_type = manifest.get_test_type(test_path) if test_path else None
        linter = MetadataLinter(path, test_path, test_type, manifest,
                                repo_root, self._configs)
        return linter.find_errors(ast)

    def _manifest(self, repo_root: str) -> WPTManifest:
        wpt_dir = self._fs.normpath(
            self._fs.relpath(repo_root, self._finder.path_from_web_tests()))
        return self._default_port.wpt_manifest(
            pathlib.Path(wpt_dir).as_posix())

    def _is_dir_metadata(self, path: str) -> bool:
        return self._fs.basename(path) == '__dir__.ini'

    def _test_path(self, manifest: WPTManifest,
                   metadata_path: str) -> Optional[str]:
        test_path, extension = self._fs.splitext(metadata_path)
        if extension == '.ini' and manifest.is_test_file(test_path):
            return test_path
        return None


class MetadataLinter(static.Compiler):
    _disable_pattern = re.compile(
        r'\s*lint-wpt:\s*disable\s*=\s*(?P<rules>[^;]*)')

    def __init__(
        self,
        path: str,
        test_path: Optional[str],
        test_type: Optional[str],
        manifest: WPTManifest,
        metadata_root: str,
        configs: wpt_metadata.TestConfigurations,
    ):
        super().__init__()
        self.path = path
        self.test_path, self.test_type = test_path, test_type
        self.manifest, self.metadata_root = manifest, metadata_root
        self.configs = configs
        # `context` contains information about the current section type,
        # heading, and key as it becomes available during the traversal. It's
        # also provided to the error message formatter.
        self.context: Dict[str, str] = {}
        self.disabled_rules: Set[str] = set()
        self.errors: Set[LintError] = set()
        # Check that all configurations have the same keys.
        assert len({frozenset(config.data) for config in configs}) == 1

    @contextlib.contextmanager
    def using_context(self, **context):
        """Set some context variables that will be reset on exit."""
        prev_context = self.context
        try:
            self.context = {**prev_context, **context}
            yield
        finally:
            self.context = prev_context

    def find_errors(self, ast: wptnode.DataNode) -> List[LintError]:
        self.errors.clear()
        if self.test_type:
            initial_type = SectionType.ROOT
            url_base = Port.WPT_DIRS[self.manifest.wpt_dir]
            # Since the long timeout check requires examining both `disabled`
            # and `expected` at the same time, use the high-level expectations
            # API instead of checking during the syntax tree traversal.
            expected = conditional.compile_ast(ast,
                                               manifestupdate.data_cls_getter,
                                               test_path=self.test_path,
                                               run_info_properties=([], {}),
                                               url_base=url_base)
            self._check_for_long_timeouts(expected)
        else:
            initial_type = SectionType.DIRECTORY
        with self.using_context(next_type=initial_type):
            self.visit(ast)
        return sorted(self.errors, key=lambda error: error[:3])

    def _check_for_long_timeouts(self,
                                 expected: manifestupdate.ExpectedManifest):
        for test_id in sorted(expected.child_map):
            test = expected.get_test(test_id)
            if test_id.startswith('/'):
                test_id = test_id[1:]
            if not self.manifest.is_slow_test(test_id):
                continue
            if not any(condition.value == 'TIMEOUT'
                       for condition in test.get_conditions('expected')):
                # This is an optimization to skip evaluating the condition on
                # every config (slow) if there are no consistent timeouts in the
                # first place. The main check is still necessary in case all
                # `TIMEOUT`s are only taken by disabled configs.
                continue
            configs = self.configs.enabled_configs(test, self.metadata_root)
            for config in configs:
                with contextlib.suppress(KeyError):
                    if test.get('expected', config) == 'TIMEOUT':
                        self._error(MetadataLongTimeout, test=test_id)
                        break

    def visit(self, node: wptnode.Node):
        with self._disable_rules(node):
            try:
                return super().visit(node)
            except AttributeError:
                # When no handler is explicitly specified, default to traversing
                # the node's children.
                for child in node.children:
                    self.visit(child)

    @contextlib.contextmanager
    def _disable_rules(self, node: wptnode.Node):
        disabled_rules = set(self.disabled_rules)
        try:
            for _, comment in node.comments:
                disable_match = self._disable_pattern.match(comment)
                if disable_match:
                    rules = disable_match['rules'].split(',')
                    self.disabled_rules.update(rule.strip() for rule in rules)
            yield
        finally:
            self.disabled_rules = disabled_rules

    def visit_DataNode(self, node: wptnode.DataNode):
        section_type = self.context.get('next_type')
        if not section_type:
            self._error(MetadataSectionTooDeep)
            return
        heading = f' {_format_node(node)!r}' if node.data else ''
        with self.using_context(heading=heading, section_type=section_type):
            next_type = None
            if section_type is SectionType.ROOT:
                next_type = SectionType.TEST
            elif section_type is SectionType.TEST:
                assert node.data
                # Intentionally replaces the basename in `path`.
                test_id = urllib.parse.urljoin(
                    pathlib.Path(self.path).as_posix(), node.data)
                if not self.manifest.is_test_url(test_id):
                    self._error(MetadataUnknownTest, test=test_id)
                if wpt_metadata.can_have_subtests(self.test_type):
                    next_type = SectionType.SUBTEST
            if heading and not node.children:
                self._error(MetadataEmptySection)
            self._check_section_sorted(node)
            with self.using_context(next_type=next_type):
                for child in node.children:
                    self.visit(child)

    def visit_KeyValueNode(self, node: wptnode.KeyValueNode):
        assert node.data
        section_type = self.context['section_type']
        valid_keys = MetadataUnknownKey.valid_keys[section_type]
        if self.test_type != 'reftest':
            valid_keys -= {'fuzzy'}
        with self.using_context(key=node.data):
            if node.data not in valid_keys:
                self._error(MetadataUnknownKey)
            else:
                with self.using_context(
                        prop_comparisons=collections.defaultdict(set)):
                    self._check_conditions(node)

    def _get_conditional_values(
        self,
        key_value_node: wptnode.KeyValueNode,
    ) -> Tuple[List[Condition], List[ValueNode]]:
        conditions, values = [], []
        for i, child in enumerate(key_value_node.children):
            if isinstance(child, wptnode.ConditionalNode):
                condition, value = child.children
            else:
                assert i == len(key_value_node.children) - 1
                condition, value = None, child
            conditions.append(condition)
            values.append(value)
        return conditions, values

    def _check_conditions(self, key_value_node: wptnode.KeyValueNode):
        conditions, values = self._get_conditional_values(key_value_node)
        assert conditions and values
        # Reference conditions by index because they are not hashable.
        conditions_not_taken = set(range(len(conditions)))
        unique_values = set(map(self.visit, values))
        implicit_default = self._implicit_default_value(key_value_node.data)
        if unique_values == {implicit_default}:
            self._error(MetadataUnnecessaryKey, value=_format_node(values[0]))
            return
        if conditions == [None]:  # Early out for unconditional values
            return

        # Simulate conditional value resolution for each test configuration.
        # Randomly shuffling the configs is an optimization to try to exercise
        # every branch and take the early out as quickly as possible (sometimes
        # with >8x speedups). Most conditions are very simple:
        #   expected:
        #     if os == "win": FAIL
        #
        # However, if we rely on the default lexographical order, we may get
        # unlucky and need to iterate almost the entire `TestConfigurations`,
        # which shuffling mitigates:
        #   os=linux virtual_suite=threaded
        #   ...
        #   os=linux virtual_suite=webgpu
        #   os=mac virtual_suite=threaded
        #   ...
        #   os=mac virtual_suite=webgpu
        #   os=win virtual_suite=threaded   <= First (os == "win") config
        for config in self._shuffled_configs:
            for i, condition in enumerate(conditions):
                try:
                    if self._eval_condition_taken(condition, config):
                        # Mark this condition as having been exercised.
                        conditions_not_taken.discard(i)
                        break
                except KeyError as error:
                    self._error(MetadataUnknownProp,
                                prop=str(error),
                                condition=_format_condition(condition))
                    # The conditional expression could not be evaluated because
                    # of an unknown property. Do not show an unactionable
                    # `META-UNREACHABLE-VALUE` error for this branch, but act
                    # as if this branch were not taken.
                    conditions_not_taken.discard(i)
            else:
                unique_values.add(implicit_default)
            if not conditions_not_taken and (conditions[-1] is None
                                             or implicit_default
                                             in unique_values):
                break

        if (len([condition for condition in conditions if condition]) > 0
                and len(unique_values) == 1):
            self._error(MetadataConditionsUnnecessary,
                        value=_format_node(values[0]))
            return
        # No need to show condition-related errors if no conditions are
        # necessary in the first place.
        for i in conditions_not_taken:
            self._error(MetadataUnreachableValue,
                        condition=_format_condition(conditions[i]))
        for prop, values in self.context['prop_comparisons'].items():
            unknown_values = values - self.configs.possible_values(prop)
            for value in unknown_values:
                self._error(MetadataUnknownPropValue,
                            prop=prop,
                            value=value,
                            condition=_format_condition(condition))

    @functools.cached_property
    def _shuffled_configs(self) -> List[metadata.RunInfo]:
        configs = list(self.configs)
        random.shuffle(configs)
        return configs

    def _implicit_default_value(self, key: str) -> Hashable:
        """Return the value wptrunner infers when no conditions match."""
        if key == 'expected':
            is_subtest = self.context['section_type'] is not SectionType.TEST
            default_expected = wpt_metadata.default_expected_by_type()
            return default_expected[self.test_type, is_subtest]
        elif key == 'disabled' or key == 'restart-after':
            return False
        # Add a sentinel object to simulate no explicit default. This unique
        # value forces `META-CONDITIONS-UNNECESSARY` to pass because at least
        # one configuration falls through to the end.
        return object()

    def _eval_condition_taken(self, condition: Condition,
                              run_info: metadata.RunInfo) -> bool:
        if not condition:
            return True
        self.expr_data = run_info.data
        return self.visit(condition)

    def visit_BinaryExpressionNode(self,
                                   node: wptnode.BinaryExpressionNode) -> bool:
        # Evaluate the result first to check for unknown properties, which will
        # raise a `KeyError`.
        result = super().visit_BinaryExpressionNode(node)
        _, operand0, operand1 = node.children
        # Canonicalize operand order.
        operand0, operand1 = sorted(
            [operand0, operand1],
            key=lambda operand: isinstance(operand, wptnode.VariableNode))
        if (isinstance(operand0, (wptnode.NumberNode, wptnode.StringNode))
                and isinstance(operand1, wptnode.VariableNode)):
            value, prop = operand0.data, operand1.data
            self.context['prop_comparisons'][prop].add(value)
        return result

    def visit_ListNode(self,
                       node: wptnode.ListNode) -> Tuple[Union[bool, str]]:
        key = self.context['key']
        if key == 'implementation-status':
            self._error(MetadataBadValue, value=_format_node(node))
            # Skip checking the children when the type needs to be fixed first.
            return ()
        if key in {'bug', 'expected', 'fuzzy'} and len(node.children) == 1:
            self._error(MetadataSingleElementList,
                        value=_format_node(node.children[0]))
        return tuple(self.visit(child) for child in node.children)

    def visit_ValueNode(self, node: wptnode.ValueNode) -> str:
        assert node.data is not None
        key = self.context['key']
        if (key == 'implementation-status'
                and node.data not in MetadataBadValue.implementation_statuses):
            self._error(MetadataBadValue, value=node.data)
        if key == 'expected' and node.data not in self.allowed_statuses:
            self._error(MetadataBadValue, value=node.data)
        if key == 'fuzzy':
            try:
                fuzzy_prop({'fuzzy': node.data})
            except ValueError:
                self._error(MetadataBadValue, value=node.data)
        if key == 'bug' and not wpt_metadata.BUG_PATTERN.fullmatch(node.data):
            self._error(MetadataBadValue, value=node.data)
        return node.data

    def visit_AtomNode(self, node: wptnode.AtomNode) -> bool:
        key = self.context['key']
        if key in {'fuzzy', 'expected', 'implementation-status', 'bug'}:
            self._error(MetadataBadValue, value=_format_node(node))
        return node.data

    def _error(self, rule: Type[MetadataRule], **extra):
        context = {**self.context, **extra}
        section_type = context.get('section_type')
        if section_type:
            context['section_type'] = section_type.name.capitalize()
        error = name, description, path, _ = rule.error(self.path, context)
        if {'*', rule.name} & self.disabled_rules:
            _log.debug('Skipping rule %s in %s: %s', name, path, description)
        else:
            self.errors.add(error)

    def _check_section_sorted(self, node: wptnode.DataNode):
        sort_key = lambda child: (isinstance(child, wptnode.DataNode), child.
                                  data or '')
        sorted_children = sorted(node.children, key=sort_key)
        for child, sorted_child in zip(node.children, sorted_children):
            if child is not sorted_child:
                # The original line numbers are lost after parsing, and
                # attempting to rediscover them with diffing seems fragile and
                # potentially inaccurate. Therefore, instead of reporting a line
                # number, show the exact contents of the first pair of
                # out-of-order lines. This is probably more helpful anyway.
                self._error(MetadataUnsortedSection,
                            predecessor=_format_node(sorted_child),
                            successor=_format_node(child))
                # Only report one error per block to avoid spam.
                break

    @property
    def allowed_statuses(self) -> FrozenSet[str]:
        test_cls = wpttest.manifest_test_cls[self.test_type]
        section_type = self.context['section_type']
        if section_type is SectionType.SUBTEST:
            result_cls = test_cls.subtest_result_cls
            assert result_cls, f'{self.test_type!r} test cannot have subtests'
        else:
            result_cls = test_cls.result_cls
        return frozenset(result_cls.statuses)


def _format_condition(condition: Condition) -> str:
    if not condition:
        return 'default condition'
    formatted_expr = f'if {_format_node(condition)}'
    return f'condition {formatted_expr!r}'


def _format_node(node: wptnode.Node) -> str:
    node, _, _ = wptmanifest.serialize(node).splitlines()[0].partition('#')
    return node.strip()
