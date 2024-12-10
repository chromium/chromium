# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import common


class GpuIntegrationTestAdapater(common.BaseIsolatedScriptArgsAdapter):

  # Overriding parent implementation.
  # pylint: disable=no-self-use
  def generate_test_output_args(self, output):
    return ['--write-full-results-to', output]
  # pylint: enable=no-self-use

  # Overriding parent implementation.
  # pylint: disable=no-self-use
  def generate_test_also_run_disabled_tests_args(self):
    return ['--all']
  # pylint: enable=no-self-use

  # Overriding parent implementation.
  # pylint: disable=no-self-use
  def generate_test_filter_args(self, test_filter_str):
    # isolated_script_test_filter comes in like:
    # WebglExtension_WEBGL_depth_texture::conformance/textures/misc/copytexsubimage2d-subrects.html # pylint: disable=line-too-long
    return ['--test-filter=%s' % test_filter_str]
  # pylint: enable=no-self-use

  # Overriding parent implementation.
  # pylint: disable=no-self-use
  def generate_sharding_args(self, total_shards, shard_index):
    return [
        '--total-shards=%d' % total_shards,
        '--shard-index=%d' % shard_index
    ]
  # pylint: enable=no-self-use

  # Overriding parent implementation.
  # pylint: disable=no-self-use
  def generate_test_launcher_retry_limit_args(self, retry_limit):
    return ['--retry-limit=%d' % retry_limit]
  # pylint: enable=no-self-use

  # Overriding parent implementation.
  # pylint: disable=no-self-use
  def generate_test_repeat_args(self, repeat_count):
    return ['--repeat=%d' % repeat_count]
  # pylint: enable=no-self-use
