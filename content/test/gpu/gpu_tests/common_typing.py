# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Helper module to store class-independent, common type hinting."""

import optparse
from typing import Any, Generator, Tuple

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
