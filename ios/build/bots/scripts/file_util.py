# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Utility functions operating with files."""

import glob
import os
import shutil

SIMULATORS_FOLDER = os.path.expanduser(
    '~/Library/Developer/CoreSimulator/Devices')


def move_raw_coverage_data(udid, isolated_output_dir):
  """Moves raw coverage data files(.profraw) from simulator shared resources
     directory to isolated_output/profraw.

     Args:
       udid: (str) UDID of the simulator that just run the tests.
       isolated_out_dir: (str) Isolated output directory of current isolated
       shard.
  """
  profraw_origin_dir = os.path.join(SIMULATORS_FOLDER, udid, "data")
  profraw_destination_dir = os.path.join(isolated_output_dir, "profraw")
  if not os.path.exists(profraw_destination_dir):
    os.mkdir(profraw_destination_dir)
  for profraw_file in glob.glob(os.path.join(profraw_origin_dir, '*.profraw')):
    shutil.move(profraw_file, profraw_destination_dir)


def zip_and_remove_folder(dir_path):
  """Zips folder storing in the parent folder and then removes original folder.

  Args:
    dir_path: (str) An absolute path to directory.
  """
  shutil.make_archive(
      os.path.join(os.path.dirname(dir_path), os.path.basename(dir_path)),
      'zip', dir_path)
  shutil.rmtree(dir_path)
