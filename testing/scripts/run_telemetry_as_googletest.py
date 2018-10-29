#!/usr/bin/env vpython
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs isolate bundled Telemetry unittests.

This script attempts to emulate the contract of gtest-style tests
invoked via recipes. The main contract is that the caller passes the
argument:

  --isolated-script-test-output=[FILENAME]

json is written to that file in the format produced by
common.parse_common_test_results.

Optional argument:

  --isolated-script-test-filter=[TEST_NAMES]

is a double-colon-separated ("::") list of test names, to run just that subset
of tests. This list is parsed by this harness and remapped to multiple arguments
passed to the target script.

This script is intended to be the base command invoked by the isolate,
followed by a subsequent Python script. It could be generalized to
invoke an arbitrary executable.

"""

import argparse
import json
import os
import sys

import common


class TelemetryUnittestAdapter(common.BaseIsolatedScriptArgsAdapter):

  def generate_test_output_args(self, output):
    return ['--write-full-results-to', output]

  def generate_test_also_run_disabled_tests_args(self):
    return ['--also-run-disabled-tests']

  def generate_test_filter_args(self, test_filter_str):
    return ['--test-filter', test_filter_str]

  def generate_sharding_args(self, total_shards, shard_index):
    return ['--total-shards=%d' % total_shards,
            '--shard-index=%d' % shard_index]

  def generate_test_launcher_retry_limit_args(self, retry_limit):
    return ['--retry-limit=%d' % retry_limit]

  def generate_test_repeat_args(self, repeat_count):
    return ['--repeat=%d' % repeat_count]


def main():
  adapter = TelemetryUnittestAdapter()
  return adapter.run_test()


# This is not really a "script test" so does not need to manually add
# any additional compile targets.
def main_compile_targets(args):
  json.dump([], args.output)


if __name__ == '__main__':
  # Conform minimally to the protocol defined by ScriptTest.
  if 'compile_targets' in sys.argv:
    funcs = {
      'run': None,
      'compile_targets': main_compile_targets,
    }
    sys.exit(common.run_script(sys.argv[1:], funcs))
  sys.exit(main())
