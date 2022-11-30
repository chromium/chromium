#!/usr/bin/env vpython3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import logging
import sys

import gold_inexact_matching.binary_search_parameter_optimizer\
    as binary_optimizer
import gold_inexact_matching.brute_force_parameter_optimizer as brute_optimizer
import gold_inexact_matching.local_minima_parameter_optimizer\
    as local_optimizer
from gold_inexact_matching import optimizer_set

# Script to find suitable values for Skia Gold inexact matching.
#
# Inexact matching in Skia Gold has three tunable parameters:
#   1. The max number of differing pixels.
#   2. The max delta for any single pixel.
#   3. The threshold for a Sobel filter.
#
# Ideally, we use the following hierarchy of comparison approaches:
#   1. Exact matching.
#   2. Exact matching after a Sobel filter is applied.
#   3. Fuzzy matching after a Sobel filter is applied.
#
# However, there may be cases where only using a Sobel filter requires masking a
# very large amount of the image compared to Sobel + very conservative fuzzy
# matching.
#
# Even if such cases are not hit, the process of determining good values for the
# parameters is quite tedious since it requires downloading images from Gold and
# manually running multiple calls to `goldctl match`.
#
# This script attempts to remedy both issues by handling all of the trial and
# error and suggesting potential parameter values for the user to choose from.


def CreateArgumentParser():
  parser = argparse.ArgumentParser(
      formatter_class=argparse.ArgumentDefaultsHelpFormatter)
  script_parser = parser.add_argument_group('Script Arguments')
  script_parser.add_argument('-v',
                             '--verbose',
                             dest='verbose_count',
                             default=0,
                             action='count',
                             help='Verbose level (multiple times for more')

  subparsers = parser.add_subparsers(help='Optimization algorithm')

  binary_parser = subparsers.add_parser(
      'binary_search',
      formatter_class=argparse.ArgumentDefaultsHelpFormatter,
      help='Perform a binary search to optimize a single parameter. The best '
      'option if you only want to tune one parameter.')
  binary_parser.set_defaults(
      clazz=binary_optimizer.BinarySearchParameterOptimizer)
  binary_optimizer.BinarySearchParameterOptimizer.AddArguments(binary_parser)

  local_parser = subparsers.add_parser(
      'local_minima',
      formatter_class=argparse.ArgumentDefaultsHelpFormatter,
      help='Perform a BFS to find local minima using weights for each '
      'parameter. Slower than binary searching, but supports an arbitrary '
      'number of parameters.')
  local_parser.set_defaults(clazz=local_optimizer.LocalMinimaParameterOptimizer)
  local_optimizer.LocalMinimaParameterOptimizer.AddArguments(local_parser)

  brute_parser = subparsers.add_parser(
      'brute_force',
      formatter_class=argparse.ArgumentDefaultsHelpFormatter,
      help='Brute force all possible combinations. VERY, VERY slow, but can '
      'potentially find better values than local_minima.')
  brute_parser.set_defaults(clazz=brute_optimizer.BruteForceParameterOptimizer)
  brute_optimizer.BruteForceParameterOptimizer.AddArguments(brute_parser)

  return parser


def SetLoggingVerbosity(args):
  logger = logging.getLogger()
  if args.verbose_count == 0:
    logger.setLevel(logging.WARNING)
  elif args.verbose_count == 1:
    logger.setLevel(logging.INFO)
  else:
    logger.setLevel(logging.DEBUG)


def main():
  parser = CreateArgumentParser()
  args = parser.parse_args()
  SetLoggingVerbosity(args)
  optimizer = optimizer_set.OptimizerSet(args, args.clazz)
  optimizer.RunOptimization()
  return 0


if __name__ == '__main__':
  sys.exit(main())
