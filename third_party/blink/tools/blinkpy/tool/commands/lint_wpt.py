# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Lint WPT files and metadata."""

import argparse
import io
import logging
import optparse
from typing import List, Optional, Tuple

from blinkpy.common import path_finder
from blinkpy.common.host import Host
from blinkpy.tool.commands.command import Command

path_finder.bootstrap_wpt_imports()
from tools.lint import lint as wptlint
from tools.lint import rules
from wptrunner import wptmanifest

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
        # TODO(crbug.com/1406669): Implement remaining rules.
        return []

    def _is_metadata_file(self, repo_root: str, path: str) -> bool:
        test_path, extension = self._fs.splitext(path)
        wpt_dir = self._fs.normpath(
            self._fs.relpath(repo_root, self._finder.path_from_web_tests()))
        manifest = self._default_port.wpt_manifest(wpt_dir)
        return extension == '.ini' and manifest.is_test_file(test_path)
