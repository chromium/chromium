# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse


def ArgumentParser(*args, **kwargs):
  """Creates an argument parser and adds the merge API arguments to it.

  See collect_task.collect_task for more on the merge script API.
  """
  parser = argparse.ArgumentParser(*args, **kwargs)
  parser.add_argument('--build-properties', help=argparse.SUPPRESS)
  parser.add_argument('--summary-json', help=argparse.SUPPRESS)
  parser.add_argument('--task-output-dir', help=argparse.SUPPRESS)
  parser.add_argument('-o',
                      '--output-json',
                      required=True,
                      help=argparse.SUPPRESS)
  parser.add_argument('jsons_to_merge', nargs='*', help=argparse.SUPPRESS)
  return parser
