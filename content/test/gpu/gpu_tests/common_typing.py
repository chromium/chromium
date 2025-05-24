# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Helper module to store class-independent, common type hinting."""

from collections.abc import Callable, Generator
import dataclasses
from typing import Any

from telemetry.internal.browser import tab
from telemetry.internal.browser import browser

TestArgs = list
GeneratedTest = tuple[str, str, TestArgs]
TestGenerator = Generator[GeneratedTest, None, None]

TagConflictChecker = Callable[[str, str], bool] | None

# Will hopefully eventually be replaced by argparses' equivalents once Telemetry
# finally switches off optparse.
# TODO(crbug.com/40807291): Change these to argparse.ArgumentParser and
# argparse.Namespace respectively once the optparse -> argparse migration is
# complete.
CmdArgParser = Any
ParsedCmdArgs = Any

# Telemetry screenshot type. Can be changed to union of specific types if/when
# Telemetry exposes those types.
Screenshot = Any
Tab = tab.Tab
Browser = browser.Browser


# Struct-like classes defined using dataclasses can't use [] or other mutable
# for default values. The use of callables is required since reusing the same
# object is also problematic.
# invalid-field-call disabled since this is just more readable shorthand of a
# valid field call.
# pylint: disable=invalid-field-call
def EmptyDict():
  return dataclasses.field(default_factory=dict)


def EmptyList():
  return dataclasses.field(default_factory=list)


def EmptySet():
  return dataclasses.field(default_factory=set)
# pylint: enable=invalid-field-call
