# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Common argument parsing library for `blinkpy`."""

import argparse
import functools
import shutil
from typing import ClassVar, Optional


class ArgumentParser(argparse.ArgumentParser):
    """Argument parser with neater default formatting."""

    MAX_WIDTH: ClassVar[int] = 100

    def __init__(self, *args, width: Optional[int] = None, **kwargs):
        if not width:
            fallback = self.MAX_WIDTH, 1
            width = shutil.get_terminal_size(fallback).columns
        width = min(width, self.MAX_WIDTH)
        formatter_class = functools.partial(
            argparse.HelpFormatter,
            width=width,
            # `max_help_position` measures the left-side margin before the help
            # message begins:
            #   --flag                 Flag help
            #   <- max_help_position ->
            #
            # Pick a relatively large default `max_help_position` to make enough
            # space for the flag and try to fit the entry on one line, instead
            # of breaking like:
            #   --flag
            #        Flag help
            #
            # This also keeps the help message narrow.
            max_help_position=int(0.4 * width))
        kwargs.setdefault('formatter_class', formatter_class)
        super().__init__(*args, **kwargs)
