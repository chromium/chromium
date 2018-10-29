#!/usr/bin/env python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs an isolate bundled Telemetry GPU integration test.

This script attempts to emulate the contract of gtest-style tests
invoked via recipes. The main contract is that the caller passes the
argument:

  --isolated-script-test-output=[FILENAME]

json is written to that file in the format produced by
common.parse_common_test_results.

Optional argument:

  --isolated-script-test-filter=[TEST_NAMES]

is a double-colon-separated ("::") list of test names, to run just that subset
of tests. This list is parsed by this harness and sent down via the
--test-filter argument.

This script is intended to be the base command invoked by the isolate,
followed by a subsequent Python script. It could be generalized to
invoke an arbitrary executable.
"""

import json
import os
import shutil
import sys
import tempfile
import traceback

import common

class GpuIntegrationTestAdapater(common.BaseIsolatedScriptArgsAdapter):

  def generate_test_output_args(self, output):
    return ['--write-full-results-to', output]

  def generate_test_filter_args(self, test_filter_str):
    filter_list = common.extract_filter_list(test_filter_str)
    # isolated_script_test_filter comes in like:
    #   gpu_tests.webgl_conformance_integration_test.WebGLConformanceIntegrationTest.WebglExtension_WEBGL_depth_texture  # pylint: disable=line-too-long
    # but we need to pass it to --test-filter like this:
    #   WebglExtension_WEBGL_depth_texture
    filter_list = [f.split('.')[-1] for f in filter_list]
    # Need to convert this to a valid regex.
    filter_regex = '(' + '|'.join(filter_list) + ')'
    return ['--test-filter=%s' % filter_regex]

  def generate_sharding_args(self, total_shards, shard_index):
    return ['--total-shards=%d' % total_shards,
            '--shard-index=%d' % shard_index]

def main():
  adapter = GpuIntegrationTestAdapater()
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
