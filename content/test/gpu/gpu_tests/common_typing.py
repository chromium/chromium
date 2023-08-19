# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Helper module to store class-independent, common type hinting."""

import optparse
from typing import Any, Generator, Tuple

import dataclasses  # Built-in, but pylint gives an ordering false positive.

from telemetry.internal.browser import tab
from telemetry.internal.browser import browser

TestArgs = list
GeneratedTest = Tuple[str, str, TestArgs]
TestGenerator = Generator[GeneratedTest, None, None]

# Will hopefully eventually be replaced by argparses' equivalents once Telemetry
# finally switches off optparse.
CmdArgParser = optparse.OptionParser
ParsedCmdArgs = optparse.Values

# Telemetry screenshot type. Can be changed to union of specific types if/when
# Telemetry exposes those types.
Screenshot = Any
Tab = tab.Tab
Browser = browser.Browser

# Struct-like classes defined using dataclasses can't use [] or other mutable
# for default values. The use of lambdas is required since re-using the same
# object is also problematic.
EmptyDict = lambda: dataclasses.field(default_factory=dict)
EmptyList = lambda: dataclasses.field(default_factory=list)
EmptySet = lambda: dataclasses.field(default_factory=set)
