# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Helper module to store class-independent, common type hinting."""

import optparse
import typing

from telemetry.internal.browser import tab
from telemetry.internal.browser import browser

TestArgs = list
GeneratedTest = typing.Tuple[str, str, TestArgs]
TestGenerator = typing.Generator[GeneratedTest, None, None]

# Will hopefully eventually be replaced by argparses' equivalents once Telemetry
# finally switches off optparse.
CmdArgParser = optparse.OptionParser
ParsedCmdArgs = optparse.Values

# Telemetry screenshot type. Can be changed to union of specific types if/when
# Telemetry exposes those types.
Screenshot = typing.Any
Tab = tab.Tab
Browser = browser.Browser
