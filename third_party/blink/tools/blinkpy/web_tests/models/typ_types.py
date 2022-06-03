# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from blinkpy.common import path_finder

path_finder.add_typ_dir_to_sys_path()

from typ import json_results, expectations_parser, artifacts

# Adds classes from typ that are used in blinkpy
ResultType = json_results.ResultType
Expectation = expectations_parser.Expectation
TestExpectations = expectations_parser.TestExpectations
Artifacts = artifacts.Artifacts
