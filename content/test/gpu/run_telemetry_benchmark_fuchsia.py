#!/usr/bin/env vpython3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Wrapper for running Telemetry benchmarks on Fuchsia devices."""

from __future__ import print_function

import sys

import fuchsia_util


def main():
  return fuchsia_util.RunTestOnFuchsiaDevice('perf')


if __name__ == '__main__':
  sys.exit(main())
