# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Common argument parsing-related code for unexpected pass finders."""

import argparse
import logging
import os

from unexpected_passes_common import constants


def AddCommonArguments(parser: argparse.ArgumentParser) -> None:
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
  parser.add_argument('--narrow-semi-stale-expectation-scope',
                      action='store_true',
                      default=False,
                      help='Automatically modify or split semi-stale '
                      'expectations so they only apply to configurations that '
                      'actually need them.')
  parser.add_argument('--no-auto-close-bugs',
                      dest='auto_close_bugs',
                      action='store_false',
                      default=True,
                      help='Disables automatic closing of bugs that no longer '
                      'have active expectations once the generated CL lands. '
                      'If set, a comment will be posted to the bug when all '
                      'active expectations are gone instead.')
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
  parser.add_argument('--expectation-grace-period',
                      type=int,
                      default=7,
                      help=('How many days old an expectation needs to be in '
                            'order to be a candidate for being removed or '
                            'modified. This prevents newly added expectations '
                            'from being removed before a sufficient amount of '
                            'data has been generated with the expectation '
                            'active. Set to a negative value to disable.'))
  parser.add_argument('--result-output-file',
                      help=('Output file to store the generated results. If '
                            'not specified, will use a temporary file.'))
  parser.add_argument('--bug-output-file',
                      help=('Output file to store "Bug:"/"Fixed:" text '
                            'intended for use in CL descriptions. If not '
                            'specified, will be printed to the terminal '
                            'instead.'))
  parser.add_argument('--jobs',
                      '-j',
                      type=int,
                      help=('DEPRECATED/NO-OP. Will be removed once all uses '
                            'are removed.'))
  parser.add_argument('--keep-unmatched-results',
                      action='store_true',
                      default=False,
                      help=('Store unmatched results and include them in the '
                            'script output. Doing so can result in a '
                            'significant increase in memory usage depending on '
                            'the data being queried. This is meant for '
                            'debugging purposes and should not be needed '
                            'during normal use.'))
  internal_group = parser.add_mutually_exclusive_group()
  internal_group.add_argument('--include-internal-builders',
                              action='store_true',
                              dest='include_internal_builders',
                              default=None,
                              help=('Includes builders that are defined in '
                                    'src-internal in addition to the public '
                                    'ones. If left unset, will be '
                                    'automatically determined by the presence '
                                    'of src-internal.'))
  internal_group.add_argument('--no-include-internal-builders',
                              action='store_false',
                              dest='include_internal_builders',
                              default=None,
                              help=('Does not include builders that are '
                                    'defined in src-internal. If left unset, '
                                    'will be automatically determined by the '
                                    'presence of src-internal.'))


def PerformCommonPostParseSetup(args: argparse.Namespace) -> None:
  """Helper function to perform all common post-parse setup.

  Args:
    args: Parsed arguments from an argparse.ArgumentParser.
  """
  SetLoggingVerbosity(args)
  SetInternalBuilderInclusion(args)


def SetLoggingVerbosity(args: argparse.Namespace) -> None:
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


def SetInternalBuilderInclusion(args: argparse.Namespace) -> None:
  """Sets internal builder inclusion based on parsed arguments.

  Args:
    args: Parsed arguments from an argparse.ArgumentParser.
  """
  if args.include_internal_builders is not None:
    return

  if os.path.isdir(constants.SRC_INTERNAL_DIR):
    args.include_internal_builders = True
  else:
    args.include_internal_builders = False
