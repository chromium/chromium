#!/usr/bin/env vpython3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Wrapper for running gpu integration tests on Fuchsia devices."""

from __future__ import print_function

import sys

import fuchsia_util


def main():
  return fuchsia_util.RunTestOnFuchsiaDevice('gpu')


if __name__ == '__main__':
  sys.exit(main())
