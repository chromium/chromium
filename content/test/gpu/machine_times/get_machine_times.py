# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Script for determining GPU test shard times.

Reported data should be taken as estimates rather than concrete numbers since
data will not be precise when failed, timed out, or infra-failed tasks are
present.
"""

import argparse
import collections
import json
import logging
import math
import multiprocessing
import os
import re
import subprocess
from typing import Dict, List, Optional, Tuple

from gpu_tests import gpu_integration_test

from unexpected_passes import gpu_builders
from unexpected_passes_common import data_types

# Grabs the runtime and overhead values from strings like:
#   [shard #0 (runtime (5m 8s) + overhead (13s): 5m 20s)]
SHARD_TIME_REGEX = re.compile(r'\[shard #\d+ \(runtime \((\d+m \d+s|\d+s)\) '
                              r'\+ overhead \((\d+m \d+s|\d+s)\)')
# Grabs the values from strings like:
#  [shard #0 (failed) (44s)]
# The reported value is runtime + overhead, so we'll end up dropping the data,
# but we can at least avoid raising a warning if we detect this case.
FAILURE_SHARD_TIME_REGEX = re.compile(
    r'\[shard #\d+ \(failed\) \((\d+m \d+s|\d+s)\)')
TIMED_OUT_SHARD_TIME_REGEX = re.compile(
    r'\[shard #\d+ timed out after (\d+m \d+s|\d+s)')


def ParseArgs() -> argparse.Namespace:
  name_mapping = gpu_integration_test.GenerateTestNameMapping()
  test_suites = list(name_mapping.keys())
  test_suites.sort()

  parser = argparse.ArgumentParser(
      description=('Script for determining GPU test shard times. Reported data '
                   'should be taken as estimates rather than concrete numbers '
                   'since data will not be precise when failed, timed out, or '
                   'infra-failed tasks are present.'))
  parser.add_argument(
      '--suite',
      action='append',
      dest='suites',
      default=[],
      help=('Test suite that must run on a builder for it to be used. If not '
            'specified, will look for all instances of GPU integration tests.'))
  parser.add_argument(
      '--builder',
      action='append',
      dest='builders',
      default=[],
      help=('CI builder to check. Can be specified multiple times. If not '
            'specified, will check all GPU builders.'))
  parser.add_argument('--num-samples',
                      default=10,
                      type=int,
                      help='The number of samples per builder to use.')
  parser.add_argument('--shard-max-threshold',
                      type=int,
                      help='Omit showing results if the max shard time for a '
                      'step is less than the specified number of seconds. '
                      'Useful for finding problematic configurations if the '
                      'goal is to have shard times under some limit.')

  args = parser.parse_args()
  args.suites = args.suites or test_suites

  if args.num_samples <= 0:
    parser.error('--num-samples must be a positive integer.')
  if args.shard_max_threshold and args.shard_max_threshold < 0:
    parser.error('--shard-max-threshold must be a non-negative integer.')

  return args


def _EnsureBuildbucketAuth() -> None:
  """Ensures that the bb tool is authenticated.

  Raises an exception if auth is not detected.
  """
  # This is taken from //testing/unexpected_passes_common/builders.py, so this
  # may be able to be deduplicated with some refactoring.
  try:
    with open(os.devnull, 'w') as devnull:
      subprocess.check_call(['bb', 'auth-info'], stdout=devnull, stderr=devnull)
  except subprocess.CalledProcessError as e:
    raise RuntimeError(
        'You are not logged into bb - run `bb auth-login`') from e


def _GetTimesForBuilder(inputs: Tuple[data_types.BuilderEntry, int]):
  """Get all shard times for a builder.

  Args:
    inputs: A tuple (builder, num_samples). |builder| is a
        data_types.BuilderEntry describing the builder to work on. |num_samples|
        is the number of builds to sample.

  Returns:
    A dict in the format:
      {
        builder (str): {
          step_name (str): [
            (shard_runtime_in_seconds (int), shard_overhead_in_seconds (int)),
            ...
          ],
          ...
        },
      }
  """
  builder, num_samples = inputs
  full_builder_string = '%s/%s/%s' % (builder.project, builder.builder_type,
                                      builder.name)
  buildbucket_ids = _GetBuildbucketIdsForBuilder(builder, num_samples)
  builder_to_step = {full_builder_string: collections.defaultdict(list)}
  if not buildbucket_ids:
    logging.warning('Did not find Buildbucket IDs for %s', full_builder_string)
    return builder_to_step

  for build_id in buildbucket_ids:
    step_output = _GetStepOutputForBuild(build_id)
    steps_to_shard_times = _GetShardTimesFromStepOutput(step_output)
    for step, shard_times in steps_to_shard_times.items():
      builder_to_step[full_builder_string][step].extend(shard_times)
  return builder_to_step


def _GetBuildbucketIdsForBuilder(builder: data_types.BuilderEntry,
                                 num_samples: int) -> List[str]:
  """Get the Buildbucket IDs for the most recent N completed builds.

  Args:
    builder: A data_types.BuilderEntry describing the builder to get IDs from.
    num_samples: The number of build IDs to get.

  Returns:
    A list of Buildbucket IDs as strings.
  """
  # Get the N most recent build IDs.
  cmd = [
      'bb',
      'ls',
      '-id',
      '-%d' % num_samples,
      '-status',
      'ended',
      '%s/%s/%s' % (builder.project, builder.builder_type, builder.name),
  ]
  completed_process = subprocess.run(cmd,
                                     text=True,
                                     check=True,
                                     stdout=subprocess.PIPE)
  return completed_process.stdout.splitlines()


def _GetStepOutputForBuild(build_id: str) -> str:
  """Gets Buildbucket step output for the given build.

  Args:
    build_id: A string containing the Buildbucket ID to query.

  Returns:
    The JSON string result of querying the build with ID |build_id|.
  """
  cmd = ['bb', 'get', '-json', '-steps', build_id]
  completed_process = subprocess.run(cmd,
                                     text=True,
                                     check=True,
                                     stdout=subprocess.PIPE)
  return completed_process.stdout


def _GetShardTimesFromStepOutput(
    step_output: str) -> Dict[str, List[Tuple[int, int]]]:
  """Extract shard time information from Buildbucket step output.

  Args:
    step_output: A JSON string containing the Buildbucket step output for a
        single build.

  Returns:
    A dict in the form:
      {
        step_name (str): [
          (runtime (int), overhead (int)),
          ...
        ],
        ...
      }
    |step_name| is a string containing a test step name from the build. It maps
    to a list of tuples, where each tuple represents the runtime/overhead
    information for a single shard of |step_name|. |runtime| is an int
    containing how much time in seconds was spent actually running the test on
    that shard. |overhead| is an int containing how much time in seconds was
    spent on swarming overhead before/after the test started. The total amount
    of machine time used on a shard should be approximately equal to |runtime|
    + |overhead|.
  """
  step_output = json.loads(step_output)
  steps = step_output['steps']
  shard_times = {}
  for s in steps:
    if 'summaryMarkdown' not in s:
      continue
    summary = s['summaryMarkdown']
    if not any(
        substr in summary
        for substr in ('Max pending time', 'Pending time', 'Shard runtime')):
      continue
    matches = SHARD_TIME_REGEX.findall(summary)
    # Failed and timed out shards report combined runtime + overhead. Since
    # runtime will typically be much greater than overhead anyways, treat the
    # combined value as runtime with 0 overhead.
    failure_matches = FAILURE_SHARD_TIME_REGEX.findall(summary)
    if failure_matches:
      matches.extend([(runtime, '0s') for runtime in failure_matches])
    timeout_matches = TIMED_OUT_SHARD_TIME_REGEX.findall(summary)
    if timeout_matches:
      matches.extend([(runtime, '0s') for runtime in timeout_matches])
    if not matches:
      if 'had an internal swarming failure' in summary:
        # Assume all shards had these infra failures, so ignore this data
        # point.
        continue
      logging.warning('Unable to find shard runtimes from summary "%s"',
                      summary)
      continue

    suite_name = s['name']
    assert suite_name not in shard_times, (
        'Found duplicate suite %s in build %s' %
        (suite_name, step_output['id']))
    shard_times[suite_name] = []
    for runtime_str, overhead_str in matches:
      runtime = _ConvertSummaryRuntimeToSeconds(runtime_str)
      overhead = _ConvertSummaryRuntimeToSeconds(overhead_str)
      shard_times[suite_name].append((runtime, overhead))
  return shard_times


def _ConvertSummaryRuntimeToSeconds(summary_runtime: str) -> int:
  """Converts string representations of runtimes to ints.

  Args:
    summary_runtime: A string representing a single shard's runtime or overhead
        time in either "Xm Ys" or "Ys" format.

  Returns:
    An int representing the same amount of time as |summary_runtime| in seconds.
  """
  if 'm' in summary_runtime:
    minutes, seconds = summary_runtime.split()
  else:
    seconds = summary_runtime
    minutes = '0m'
  minutes = int(minutes[:-1])
  seconds = int(seconds[:-1])
  return 60 * minutes + seconds


def _OutputBuilderInformation(builders_to_steps: Dict[str,
                                                      Dict[str,
                                                           List[Tuple[int,
                                                                      int]]]],
                              num_samples: int,
                              shard_max_threshold: Optional[int]) -> None:
  """Print out collected runtime information.

  Args:
    builders_to_step: A dict in the form:
        {
          builder_name (str): {
            step_name(str): [
              (runtime (int), overhead (int)),
              ...
            ],
            ...
          },
          ...
        }
        This dict contains all the collected shard runtime information for
        every relevant test step on every queried builder.
    num_samples: An int containing the number of builds used from each builder.
    shard_max_threshold: An int for a max shard time in seconds. If a step's max
        shard time is under this value, the stats for the step will not be
        output. If None, all stats will be output regardless of max shard time.
  """
  def _OutputListStats(l, output_lines):
    # Here and lower down when we do the shard calculations, we can potentially
    # understate the values since we aren't guaranteed to get |num_samples|
    # datapoints if a test is new or recently renamed. However, this should be
    # quite rare, and the script can simply be run again in the near future to
    # work around this.
    output_lines.append('      Average per build %f' %
                        (float(sum(l)) / num_samples))
    output_lines.append('      Average per shard %f' % (float(sum(l)) / len(l)))
    output_lines.append('      Median per shard %d' % l[len(l) // 2])
    output_lines.append('      Min shard %d' % l[0])
    output_lines.append('      Max shard %d' % l[-1])

  # Re-create the mapping now with sorted keys so that builder output is
  # consistent.
  builders_to_steps = {
      k: builders_to_steps[k]
      for k in sorted(list(builders_to_steps.keys()))
  }
  for builder, steps_to_times in builders_to_steps.items():
    output_lines = [builder]
    for step, times in steps_to_times.items():
      runtimes = [t[0] for t in times]
      overheads = [t[1] for t in times]
      runtimes.sort()
      overheads.sort()
      # Skip any steps that are under the requested max shard time.
      if shard_max_threshold is not None and runtimes[-1] < shard_max_threshold:
        continue
      output_lines.append('  %s (~%d shards)' %
                          (step, math.ceil(float(len(runtimes)) / num_samples)))
      output_lines.append('    Runtime')
      _OutputListStats(runtimes, output_lines)
      output_lines.append('    Overhead')
      _OutputListStats(overheads, output_lines)

    if len(output_lines) == 1:
      output_lines.append('  No steps over requested max shard time.')
    print('\n'.join(output_lines))


def main() -> None:
  args = ParseArgs()

  _EnsureBuildbucketAuth()

  ci_builders = set()
  for suite in args.suites:
    builders_instance = gpu_builders.GpuBuilders(suite, False)
    ci_builders |= builders_instance.GetCiBuilders()
  ci_builders -= builders_instance.GetNonChromiumBuilders()

  if args.builders:
    valid_builders = set()
    for b in args.builders:
      valid_builders.add(data_types.BuilderEntry(b, 'ci', False))
    ci_builders &= valid_builders

  with multiprocessing.Pool() as p:
    builder_times = p.map(_GetTimesForBuilder,
                          [(b, args.num_samples) for b in ci_builders])
  p.join()

  builders_to_steps = {}
  for bt in builder_times:
    builders_to_steps.update(bt)

  _OutputBuilderInformation(builders_to_steps, args.num_samples,
                            args.shard_max_threshold)
