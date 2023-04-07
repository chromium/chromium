# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Lint WPT files and metadata."""

import argparse
import io
import logging
import optparse
from typing import Iterator, List, Optional, Tuple

from blinkpy.common import path_finder
from blinkpy.common.host import Host
from blinkpy.tool.commands.command import Command

path_finder.bootstrap_wpt_imports()
from tools.lint import lint as wptlint
from tools.lint import rules
from wptrunner import wptmanifest
from wptrunner.wptmanifest import node as wptnode

_log = logging.getLogger(__name__)


class MetadataRule(rules.Rule):
    """Base type for metadata-related rules."""


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
    description = 'Empty section can be removed: %(heading)r'
    to_fix = """
    A section without keys or subsections has no effect and should be removed.
    The (sub)tests represented by empty sections default to enabled and
    all-pass.
    """


LintError = Tuple[str, str, str, Optional[int]]


class LintWPT(Command):
    name = 'lint-wpt'
    show_in_main_help = False  # TODO(crbug.com/1406669): To be switched on.
    help_text = __doc__.strip().splitlines()[0]
    long_help = __doc__

    def __init__(self, tool: Host):
        super().__init__()
        self._tool = tool
        self._fs = self._tool.filesystem
        self._default_port = self._tool.port_factory.get()
        self._finder = path_finder.PathFinder(self._fs)

    def parse_args(self, args: List[str]) -> Tuple[optparse.Values, List[str]]:
        # TODO(crbug.com/1431070): Migrate `blink_tool.py` to stdlib's
        # `argparse`. `optparse` is deprecated.
        parser = argparse.ArgumentParser(description=self.long_help,
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
        parameters = parser.parse_args(args)
        # TODO(crbug.com/1406669): Find a way to lint `wpt_internal/` files too
        # so that they can be upstreamed easily.
        if not parameters.repo_root:
            parameters.repo_root = self._finder.path_from_wpt_tests()
        return optparse.Values(vars(parameters)), []

    def execute(self, options: optparse.Values, _args: List[str],
                _tool: Host) -> Optional[int]:
        # Pipe `wpt lint`'s logs into `blink_tool.py`'s formatter.
        wptlint.logger = _log
        wptlint.file_lints.append(self.check_metadata)
        return wptlint.main(**vars(options))

    def check_metadata(self, repo_root: str, path: str,
                       metadata_file: io.BytesIO) -> List[LintError]:
        if not self._is_metadata_file(repo_root, path):
            return []
        try:
            ast = wptmanifest.parse(metadata_file)
        except wptmanifest.parser.ParseError as error:
            context = {'detail': error.detail}
            return [MetadataBadSyntax.error(path, context, error.line)]
        errors = []
        for check in [
                self._check_metadata_sorted,
                self._check_metadata_nonempty_sections,
                # TODO(crbug.com/1406669): Implement remaining rules.
        ]:
            errors.extend(check(path, ast))
        return errors

    def _check_metadata_sorted(self, path: str,
                               node: wptnode.Node) -> Iterator[LintError]:
        if not isinstance(node, wptnode.DataNode):
            return
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
                context = {
                    'predecessor': _format_node(sorted_child),
                    'successor': _format_node(child),
                }
                yield MetadataUnsortedSection.error(path, context)
                # Only report one error per block to avoid spam.
                break
        for child in node.children:
            yield from self._check_metadata_sorted(path, child)

    def _check_metadata_nonempty_sections(
            self, path: str, node: wptnode.Node) -> Iterator[LintError]:
        if not isinstance(node, wptnode.DataNode):
            return
        if not node.children:
            context = {'heading': _format_node(node)}
            yield MetadataEmptySection.error(path, context)
        for child in node.children:
            yield from self._check_metadata_nonempty_sections(path, child)

    def _is_metadata_file(self, repo_root: str, path: str) -> bool:
        test_path, extension = self._fs.splitext(path)
        wpt_dir = self._fs.normpath(
            self._fs.relpath(repo_root, self._finder.path_from_web_tests()))
        manifest = self._default_port.wpt_manifest(wpt_dir)
        return extension == '.ini' and manifest.is_test_file(test_path)


def _format_node(node: wptnode.Node) -> str:
    return wptmanifest.serialize(node).splitlines()[0].strip()
