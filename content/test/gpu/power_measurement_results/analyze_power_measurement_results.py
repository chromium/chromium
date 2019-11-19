#!/usr/bin/env python
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Script which analyze power measurement test results from bots.

Such analysis provides insights into power data to better understand
Intel Power Gadget.

Related design doc:
https://docs.google.com/document/d/1s3L2IYguQmPHInsKkbHh06hXCXo8ggo5iPIhOaCNwVw
"""

import enum
import json
import logging
import math
import os
import sets
import sys

_TESTS = [
    'Basic', 'Video_720_MP4', 'Video_720_MP4_Fullscreen',
    'Video_720_MP4_Underlay', 'Video_720_MP4_Underlay_Fullscreen'
]
_MEASUREMENTS = ['DRAM', 'Processor']

_RESULTS_PATH = os.path.join(
    os.path.dirname(os.path.abspath(__file__)), 'win10_intel_hd_630')

_RESULTS_JSON_FILES = [
    'build_4370_4425_repeat3.json',
    'build_4426_4759_repeat3.json',
    'build_4760_5047_repeat3.json',
]


class RepeatStrategy(enum.Enum):
  COUNT_EACH = 1  # count each run individually
  COUNT_MINIMUM = 2  # count the run with minimum power
  COUNT_AVERAGE = 3  # count the average power of all runs
  COUNT_MEDIAN = 4  # count the median_low (power wise) of all runs
  COUNT_MINIMUM_FIRST_TWO = 5  # for the first 2, count the run with less power

  @classmethod
  def ToString(cls, strategy):
    if strategy == cls.COUNT_EACH:
      return "each"
    elif strategy == cls.COUNT_MINIMUM:
      return "minimum"
    elif strategy == cls.COUNT_AVERAGE:
      return "average"
    elif strategy == cls.COUNT_MEDIAN:
      return "median"
    else:
      assert strategy == cls.COUNT_MINIMUM_FIRST_TWO
      return "minimum (first two)"


def LoadResultsJsonFiles():
  jsons = []
  for json_filename in _RESULTS_JSON_FILES:
    json_path = os.path.join(_RESULTS_PATH, json_filename)
    with open(json_path, "r") as json_file:
      logging.debug('Loading %s', json_path)
      jsons.append(json.load(json_file))
  return jsons


def DetermineResultsFromMultipleRuns(measurements, repeat_strategy):
  if repeat_strategy == RepeatStrategy.COUNT_EACH:
    return measurements
  elif repeat_strategy == RepeatStrategy.COUNT_MINIMUM:
    measurements.sort()
    return [measurements[0]]
  elif repeat_strategy == RepeatStrategy.COUNT_AVERAGE:
    return [Mean(measurements)]
  elif repeat_strategy == RepeatStrategy.COUNT_MEDIAN:
    return [MedianLow(measurements)]
  elif repeat_strategy == RepeatStrategy.COUNT_MINIMUM_FIRST_TWO:
    assert len(measurements) >= 2
    first_two = measurements[0:2]
    first_two.sort()
    return [first_two[0]]
  else:
    assert False
    return []


def ProcessJsonData(jsons,
                    measurement_names=_MEASUREMENTS,
                    per_bot=False,
                    repeat_strategy=RepeatStrategy.COUNT_MINIMUM,
                    bot_whitelist=[],
                    bot_blacklist=[]):
  min_build = None
  max_build = None
  results = {}
  bots = []
  for json in jsons:
    builds = json.get('builds', [])
    for build in builds:
      build_number = build.get('number', -1)
      if build_number > 0:
        if min_build is None or build_number < min_build:
          min_build = build_number
        if max_build is None or build_number > max_build:
          max_build = build_number

      bot = build['bot']
      if bot_whitelist and bot not in bot_whitelist:
        continue
      if bot_blacklist and bot in bot_blacklist:
        continue

      if bot not in bots:
        bots.append(bot)

      tests = build['tests']
      for test in tests:
        name = test['name']
        parts = name.split('.')
        name = parts[-1]
        assert name in _TESTS
        if results.get(name, None) is None:
          if per_bot:
            results[name] = {}
          else:
            results[name] = []
        test_data = results[name]
        if per_bot:
          if test_data.get(bot, None) is None:
            test_data[bot] = []
          test_data = test_data[bot]

        measurements = [0]
        for measurement_name in measurement_names:
          actual_measurement_name = measurement_name + ' Power_0'
          data = test[actual_measurement_name]
          count = len(data)
          while len(measurements) < count:
            measurements.append(0)
          for ii in range(count):
            measurements[ii] = measurements[ii] + data[ii]
        assert measurements
        test_data.extend(
            DetermineResultsFromMultipleRuns(measurements, repeat_strategy))

  return {
      'min_build': min_build,
      'max_build': max_build,
      'bots': bots,
      'results': results,
  }


def Mean(data):
  assert len(data) > 0
  total = 0
  for num in data:
    total = total + num
  return total / len(data)


def Stdev(data):
  assert len(data) > 0
  mean = Mean(data)
  total = 0
  for num in data:
    total = total + (num - mean) * (num - mean)
  return math.sqrt(total / len(data))


def MedianLow(data):
  # Assume list is sorted.
  assert len(data) > 0
  index = int((len(data) - 1) / 2)
  return data[index]


def MarkSection():
  print ''


def MarkExperiment(description):
  print ''
  print '**************************************************************'
  print description
  print '**************************************************************'
  print ''


def GetBotBuilds(jsons, bot_name):
  build_numbers = []
  for json in jsons:
    builds = json.get('builds', [])
    for build in builds:
      build_number = build.get('number', -1)
      if build_number > 0:
        bot = build['bot']
        if bot == bot_name:
          build_numbers.append(build_number)
  return build_numbers


def GetOutliers(data, variation_threshold):
  mean = Mean(data)
  max_value = mean + mean * variation_threshold
  min_value = mean - mean * variation_threshold
  outliers = []
  for value in data:
    if value > max_value or value < min_value:
      outliers.append(value)
  return outliers


def FindBuild(jsons, bot_whitelist, test_name, result):
  for json in jsons:
    builds = json.get('builds', [])
    for build in builds:
      build_number = build.get('number', -1)
      if build_number < 0:
        continue
      bot = build['bot']
      if bot not in bot_whitelist:
        continue

      tests = build['tests']
      for test in tests:
        name = test['name']
        parts = name.split('.')
        name = parts[-1]
        assert name in _TESTS
        if name != test_name:
          continue

        # Use RepeatStrategy.COUNT_MINIMUM
        measurements = [0]
        for measurement_name in _MEASUREMENTS:
          actual_measurement_name = measurement_name + ' Power_0'
          data = test[actual_measurement_name]
          count = len(data)
          while len(measurements) < count:
            measurements.append(0)
          for ii in range(count):
            measurements[ii] = measurements[ii] + data[ii]
        assert measurements
        measurements.sort()
        if measurements[0] == result:
          return {'bot': bot, 'build': build_number}
  return None


def RunExperiment_BadBots(jsons,
                          stdev_threshold,
                          repeat_strategy=RepeatStrategy.COUNT_MINIMUM):
  MIN_RUNS_PER_BOT = 8
  MarkExperiment('Locate potential bad bots: thresh=%0.2f, repeat=%s' %
                 (stdev_threshold, RepeatStrategy.ToString(repeat_strategy)))
  outcome = ProcessJsonData(
      jsons, per_bot=True, repeat_strategy=repeat_strategy)
  logging.debug('Processed builds: [%d, %d]', outcome['min_build'],
                outcome['max_build'])
  logging.debug('Total number of bots: %d', len(outcome['bots']))
  results = outcome['results']
  total_bad_bots = sets.Set([])
  for test_name, test_results in results.items():
    if test_name == 'Basic':
      # Ignore Basic test results. They seem more unstable.
      continue
    MarkSection()
    logging.debug('Results for test: %s', test_name)
    bots_considered = 0
    bad_bots = []
    for bot_name, bot_results in test_results.items():
      if len(bot_results) < MIN_RUNS_PER_BOT:
        continue
      bots_considered = bots_considered + 1
      stdev = Stdev(bot_results)
      bot_results.sort()
      if stdev > stdev_threshold:
        bad_bots.append(bot_name)
        logging.debug('Potential bad bot %s: stdev = %f', bot_name, stdev)
    total_bad_bots = total_bad_bots | sets.Set(bad_bots)
    logging.debug("Total bots considered: %d", bots_considered)
    logging.debug("Bad bots: %d", len(bad_bots))
    logging.debug("%s", bad_bots)
  MarkSection()
  total_bad_bots = list(total_bad_bots)
  total_bad_bots.sort()
  logging.debug("All potential bad bots: %d", len(total_bad_bots))
  logging.debug("%s", total_bad_bots)
  MarkSection()
  for bot in total_bad_bots:
    build_numbers = GetBotBuilds(jsons, bot)
    build_numbers.sort()
    logging.debug("Bad bot %s builds: %s", bot, build_numbers)
  return total_bad_bots


def RunExperiment_GoodBots(jsons,
                           bad_bots=[],
                           repeat_strategy=RepeatStrategy.COUNT_MINIMUM):
  MIN_RUNS_PER_BOT = 8
  STDEV_GOOD_BOT_THRESHOLD = 0.2
  GOOD_BOT_RANGE_PERC = 0.08
  REGULAR_BOT_RANGE_PERC = 0.15
  MarkExperiment(
      'Locate potential good bots: thresh=%0.2f, repeat=%s' %
      (STDEV_GOOD_BOT_THRESHOLD, RepeatStrategy.ToString(repeat_strategy)))
  outcome = ProcessJsonData(
      jsons, per_bot=True, repeat_strategy=repeat_strategy)
  logging.debug('Processed builds: [%d, %d]', outcome['min_build'],
                outcome['max_build'])
  logging.debug('Total number of bots: %d', len(outcome['bots']))
  results = outcome['results']
  total_good_bots = sets.Set(outcome['bots'])
  for test_name, test_results in results.items():
    if test_name == 'Basic':
      # Ignore Basic test results. They seem more unstable.
      continue
    MarkSection()
    logging.debug('Results for test: %s', test_name)
    bots_considered = 0
    stdev_list = []
    good_bots = []
    for bot_name, bot_results in test_results.items():
      if len(bot_results) < MIN_RUNS_PER_BOT:
        continue
      stdev = Stdev(bot_results)
      stdev_list.append(stdev)
      if bot_name in bad_bots:
        continue
      bot_results.sort()
      bots_considered = bots_considered + 1
      mean = Mean(bot_results)
      if stdev < STDEV_GOOD_BOT_THRESHOLD:
        good_bots.append(bot_name)
        logging.debug('Potential good bot %s: mean = %f, stdev = %f', bot_name,
                      mean, stdev)
        outliers = GetOutliers(bot_results, GOOD_BOT_RANGE_PERC)
        if outliers:
          logging.debug('Good bot %s: %d runs out of %d%% range', bot_name,
                        len(outliers), GOOD_BOT_RANGE_PERC * 100)
      else:
        outliers = GetOutliers(bot_results, REGULAR_BOT_RANGE_PERC)
        if outliers:
          logging.debug('Regular bot %s: %d runs out of %d%% range', bot_name,
                        len(outliers), REGULAR_BOT_RANGE_PERC * 100)
    total_good_bots = total_good_bots & sets.Set(good_bots)
    logging.debug('Total bots considered: %d', bots_considered)
    logging.debug('Good bots: %d', len(good_bots))
    logging.debug('%s', good_bots)
    logging.debug('Average per bot stdev: %f', Mean(stdev_list))
  MarkSection()
  total_good_bots = list(total_good_bots)
  total_good_bots.sort()
  logging.debug('All potential good bots: %d', len(total_good_bots))
  logging.debug('%s', total_good_bots)
  MarkSection()
  for bot in total_good_bots:
    build_numbers = GetBotBuilds(jsons, bot)
    build_numbers.sort()
    logging.debug('Good bot %s builds: %s', bot, build_numbers)
  return total_good_bots


def RunExperiment_BestVariations(jsons, find_m_bots, variation_threshold):
  MIN_RUNS_PER_BOT = 8
  GET_RID_OF_N_BOTS_WITH_WORST_STDEV = 10

  MarkExperiment('Find %d bots with best variations, threshold = %0.2f%%' %
                 (find_m_bots, (variation_threshold * 100)))

  outcome = ProcessJsonData(
      jsons, per_bot=True, repeat_strategy=RepeatStrategy.COUNT_MINIMUM)
  results = outcome['results']
  candidates_per_test = {}
  candidate_bots_per_test = {}
  for test_name, test_results in results.items():
    if test_name == 'Basic':
      # Ignore Basic test results. They seem more unstable.
      continue
    bots_considered = 0
    candidates = []
    stdev_list = []
    # Remove N bots with worst stdev
    for bot_name, bot_results in test_results.items():
      if len(bot_results) < MIN_RUNS_PER_BOT:
        continue
      bots_considered = bots_considered + 1
      mean = Mean(bot_results)
      stdev = Stdev(bot_results)
      candidates.append({
          'bot': bot_name,
          'mean': mean,
          'stdev': stdev,
          'data': bot_results,
      })
      stdev_list.append(stdev)
    stdev_list.sort()
    guard_stdev = stdev_list[-GET_RID_OF_N_BOTS_WITH_WORST_STDEV]
    candidates_with_good_stdev = []
    mean_list = []
    for candidate in candidates:
      if candidate['stdev'] < guard_stdev:
        candidates_with_good_stdev.append(candidate)
        mean_list.append(candidate['mean'])
    assert (len(candidates) - GET_RID_OF_N_BOTS_WITH_WORST_STDEV == len(
        candidates_with_good_stdev))

    assert len(candidates_with_good_stdev) > find_m_bots

    # Find M bots with minimum range of means
    mean_list.sort()
    min_range = mean_list[-1] - mean_list[0]
    candidate_index = 0
    for low_index in range(len(candidates_with_good_stdev) - find_m_bots + 1):
      high_index = low_index + find_m_bots - 1
      mean_range = mean_list[high_index] - mean_list[low_index]
      if mean_range < min_range:
        min_range = mean_range
        candidate_index = low_index
    min_mean = mean_list[candidate_index]
    max_mean = mean_list[candidate_index + find_m_bots - 1]
    candidates = []
    candidate_bots = []
    for candidate in candidates_with_good_stdev:
      if candidate['mean'] >= min_mean and candidate['mean'] <= max_mean:
        candidates.append(candidate)
        candidate_bots.append(candidate['bot'])
    assert len(candidates) == find_m_bots
    candidate_bots_per_test[test_name] = sets.Set(candidate_bots)
    candidates_per_test[test_name] = candidates

  # Now find the list of bots that work well for all tests
  selected_bots = None
  for test_name, bots in candidate_bots_per_test.items():
    if selected_bots is None:
      selected_bots = bots
    else:
      selected_bots = selected_bots & bots
  logging.debug('Intended to find %d bots, actually found %d', find_m_bots,
                len(selected_bots))
  selected_bots = list(selected_bots)
  selected_bots.sort()
  logging.debug(selected_bots)

  # Validate: check variations are within a range
  for test_name, candidates in candidates_per_test.items():
    MarkSection()
    results = []
    for candidate in candidates:
      if candidate['bot'] in selected_bots:
        results.extend(candidate['data'])
    mean = Mean(results)
    stdev = Stdev(results)
    logging.debug('Validate test %s: mean = %f, stdev = %f', test_name, mean,
                  stdev)
    outliers = GetOutliers(results, variation_threshold)
    if outliers:
      # Find corresponding builds
      builds = []
      for outlier in outliers:
        build = FindBuild(jsons, selected_bots, test_name, outlier)
        assert build is not None
        builds.append(build)
      logging.debug('%d runs out of %d are not within %0.2f%% range: %s',
                    len(outliers), len(results), (variation_threshold * 100),
                    outliers)
      logging.debug(builds)


def main():
  logging.basicConfig(level=logging.DEBUG)

  jsons = LoadResultsJsonFiles()

  bad_bots = RunExperiment_BadBots(jsons, 0.5, RepeatStrategy.COUNT_EACH)
  RunExperiment_GoodBots(jsons, bad_bots, RepeatStrategy.COUNT_EACH)

  bad_bots = RunExperiment_BadBots(jsons, 0.5, RepeatStrategy.COUNT_AVERAGE)
  RunExperiment_GoodBots(jsons, bad_bots, RepeatStrategy.COUNT_AVERAGE)

  bad_bots = RunExperiment_BadBots(jsons, 0.5, RepeatStrategy.COUNT_MEDIAN)
  RunExperiment_GoodBots(jsons, bad_bots, RepeatStrategy.COUNT_MEDIAN)

  bad_bots = RunExperiment_BadBots(jsons, 0.5,
                                   RepeatStrategy.COUNT_MINIMUM_FIRST_TWO)
  RunExperiment_GoodBots(jsons, bad_bots,
                         RepeatStrategy.COUNT_MINIMUM_FIRST_TWO)

  bad_bots = RunExperiment_BadBots(jsons, 0.5, RepeatStrategy.COUNT_MINIMUM)
  RunExperiment_GoodBots(jsons, bad_bots, RepeatStrategy.COUNT_MINIMUM)

  RunExperiment_BestVariations(jsons, 25, 0.12)

  return 0


if __name__ == '__main__':
  sys.exit(main())
