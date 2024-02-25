# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

# if the current directory is in scripts (pwd), then we need to
# add plugin in order to import from that directory
if os.path.split(os.path.dirname(__file__))[1] != 'plugin':
  sys.path.append(
      os.path.join(os.path.abspath(os.path.dirname(__file__)), 'plugin'))
from plugin_constants import VIDEO_RECORDER_PLUGIN_OPTIONS
from test_plugins import VideoRecorderPlugin, FileCopyPlugin


def init_plugins_from_args(out_dir, **kwargs):
  plugins = []
  device_info_shared_cache = {}
  if (kwargs.get('video_plugin_option')):
    video_plugin_option = kwargs.get('video_plugin_option')
    if (video_plugin_option == VIDEO_RECORDER_PLUGIN_OPTIONS.failed_only.name):
      plugins.append(VideoRecorderPlugin(device_info_shared_cache, out_dir))
  if (kwargs.get('use_clang_coverage')):
    plugins.append(
        FileCopyPlugin('data/*.profraw',
                       os.path.join(os.path.dirname(out_dir), 'profraw'),
                       device_info_shared_cache))
  return plugins
