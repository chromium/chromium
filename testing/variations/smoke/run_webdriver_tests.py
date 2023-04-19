#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Finch smoke test runner script.

This is the runner script that is executed in the swarming bot. It looks up
the test cases, downloads chrome/chromedriver if needed, and sets up webdriver
environment.

This script can be invoked from the root as:
testing/scripts/run_isolated_script_test.py \
  testing/variations/smoke/run_webdriver_tests.py

**Working in progres**
"""
import pytest

import sys

# pylint: disable=redefined-outer-name
@pytest.fixture
def webdriver():
  # return nothing for now.
  return None

def test_load_seed_no_crash(webdriver):
  assert webdriver == None

if __name__ == "__main__":
  sys.exit(pytest.main(["-qq", __file__]))