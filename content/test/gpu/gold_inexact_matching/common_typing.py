# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Helper module to store file-independent, common type hinting."""

import argparse
import typing

CmdArgParser = argparse.ArgumentParser
ParsedCmdArgs = argparse.Namespace
ArgumentGroup = 'argparse._ArgumentGroup'
ArgumentGroupTuple = typing.Tuple[ArgumentGroup, ArgumentGroup, ArgumentGroup]
