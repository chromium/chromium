# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Common argument parsing-related code for unexpected pass finders."""

import logging


def AddCommonArguments(parser):
  """Adds arguments that are common to all unexpected pass finders.

  Args:
    parser: An argparse.ArgumentParser instance to add arguments to.
  """
  parser.add_argument('--project',
                      required=True,
                      help='The billing project to use for BigQuery queries. '
                      'Must have access to the ResultDB BQ tables, e.g. '
                      '"chrome-luci-data.chromium.gpu_ci_test_results".')
  parser.add_argument('--num-samples',
                      type=int,
                      default=100,
                      help='The number of recent builds to query.')
  parser.add_argument('--output-format',
                      choices=[
                          'html',
                          'print',
                      ],
                      default='html',
                      help='How to output script results.')
  parser.add_argument('--remove-stale-expectations',
                      action='store_true',
                      default=False,
                      help='Automatically remove any expectations that are '
                      'determined to be stale from the expectation file.')
  parser.add_argument('--modify-semi-stale-expectations',
                      action='store_true',
                      default=False,
                      help='If any semi-stale expectations are found, prompt '
                      'the user about the modification of each one.')
  parser.add_argument('-v',
                      '--verbose',
                      action='count',
                      default=0,
                      help='Increase logging verbosity, can be passed multiple '
                      'times.')
  parser.add_argument('-q',
                      '--quiet',
                      action='store_true',
                      default=False,
                      help='Disable logging for non-errors.')
  parser.add_argument('--large-query-mode',
                      action='store_true',
                      default=False,
                      help='Run the script in large query mode. This incurs '
                      'a significant performance hit, but allows the use of '
                      'larger sample sizes on large test suites by partially '
                      'working around a hard memory limit in BigQuery.')


def SetLoggingVerbosity(args):
  """Sets logging verbosity based on parsed arguments.

  Args:
    args: Parsed arguments from an argparse.ArgumentParser.
  """
  if args.quiet:
    args.verbose = -1
  verbosity_level = args.verbose
  if verbosity_level == -1:
    level = logging.ERROR
  elif verbosity_level == 0:
    level = logging.WARNING
  elif verbosity_level == 1:
    level = logging.INFO
  else:
    level = logging.DEBUG
  logging.getLogger().setLevel(level)
