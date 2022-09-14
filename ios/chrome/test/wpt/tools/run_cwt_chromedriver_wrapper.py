#!/usr/bin/env vpython3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This is a wrapper that invokes run_cwt_chromedriver but uses vpython3
# rather than python3. This wrapper is needed by WPT scripts, which work
# inside a vpython3 environment.
import run_cwt_chromedriver
