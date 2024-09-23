#!/usr/bin/env python
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Script which gathers power measurement test results from bots.

This is used to collect and store IPG based power measurments before they are
deleted. This provides data to better understand IPG.
"""

import argparse
import ast
import csv
import json
import logging
import re
import sys
import urllib.request as ulib_request
import urllib.parse as ulib_parse
import urllib.error

HTTPError = urllib.error.HTTPError

_TESTS = [
    'Basic', 'Video_720_MP4', 'Video_720_MP4_Fullscreen',
    'Video_720_MP4_Underlay', 'Video_720_MP4_Underlay_Fullscreen'
]
_MEASUREMENTS = ['DRAM', 'GT', 'IA']


def GetBuildData(method, request):
  # Explorable via RPC explorer:
  # https://cr-buildbucket.appspot.com/rpcexplorer/services/
  #   buildbucket.v2.Builds/{GetBuild|SearchBuilds}
  assert method in ['GetBuild', 'SearchBuilds']

  # The Python docs are wrong. It's fine for this payload to be just
  # a JSON string.
  headers = {'content-type': 'application/json', 'accept': 'application/json'}
  url = ulib_request.Request(
      'https://cr-buildbucket.appspot.com/prpc/buildbucket.v2.Builds/' + method,
      request, headers)
  conn = ulib_request.urlopen(url)
  result = conn.read()
  conn.close()
  # Result is a multi-line string the first line of which is
  # deliberate garbage and the rest of which is a JSON payload.
  return json.loads(''.join(result.splitlines()[1:]))


def GetJsonForBuildSteps(bot, build):
  request = json.dumps({
      'builder': {
          'project': 'chromium',
          'bucket': 'ci',
          'builder': bot
      },
      'buildNumber': build,
      'fields': 'steps.*.name,steps.*.logs'
  })
  return GetBuildData('GetBuild', request)


def GetLatestGreenBuild(bot):
  request = json.dumps({
      'predicate': {
          'builder': {
              'project': 'chromium',
              'bucket': 'ci',
              'builder': bot
          },
          'status': 'SUCCESS'
      },
      'fields': 'builds.*.number',
      'pageSize': 1
  })
  builds_json = GetBuildData('SearchBuilds', request)
  builds = builds_json['builds']
  assert len(builds) == 1
  return builds[0]['number']


def GetJsonForLatestNBuilds(bot, build_count):
  fields = [
      'builds.*.number',
      'builds.*.steps.*.name',
      'builds.*.steps.*.logs',
  ]
  request = json.dumps({
      'predicate': {
          'builder': {
              'project': 'chromium',
              'bucket': 'ci',
              'builder': bot
          }
      },
      'fields': ','.join(fields),
      'pageSize': build_count
  })
  builds_json = GetBuildData('SearchBuilds', request)
  builds = builds_json['builds']
  if len(builds) != build_count:
    logging.warning('Asked %d builds, got %d builds', build_count, len(builds))
  return builds


def FindStepLogURL(steps, step_name, log_name):
  # The format of this JSON-encoded protobuf is defined here:
  # https://chromium.googlesource.com/infra/luci/luci-go/+/main/
  #   buildbucket/proto/step.proto
  # It's easiest to just use the RPC explorer to fetch one and see
  # what's desired to extract.
  for step in steps:
    if 'name' not in step or 'logs' not in step:
      continue
    if step['name'].startswith(step_name):
      for log in step['logs']:
        if log.get('name') == log_name:
          return log.get('viewUrl')
  return None


# pylint: disable=too-many-branches
def ProcessStepStdout(stdout_url, entry):
  number = entry['number']
  logging.debug('[BUILD %d] stdout URL: %s', number,
                ulib_parse.unquote(stdout_url))

  # The following fails with Python 2.7.6, but succeeds with Python 2.7.14.
  conn = ulib_request.urlopen(stdout_url + '?format=raw')
  lines = conn.read().splitlines()
  conn.close()

  pattern = re.compile(r'^\[(\d+)/(\d+)\]$')
  results = None
  bot_candidates = []
  for line in lines:
    if results is not None:
      my_results = results
      results = None
      # The line after results is [test/total_test] name passed {time}s
      tokens = line.split()
      if len(tokens) != 4:
        logging.warning('Wrong format for test passed line: %s', line)
        continue
      groups = pattern.match(tokens[0]).groups()
      if len(groups) != 2:
        logging.warning('Wrong format, expected [d+/d+], got %s', tokens[0])
        continue
      test_name = tokens[1]
      if tokens[2] != 'passed':
        logging.warning('Wrong format for test passed line: %s', line)
      # pylint: disable=unsupported-assignment-operation
      my_results['name'] = test_name
      # pylint: enable=unsupported-assignment-operation
      entry['tests'].append(my_results)
    elif line.startswith('Chrome Env: '):
      chrome_env = ast.literal_eval(line[len('Chrome Env: '):])
      if 'COMPUTERNAME' in chrome_env:
        bot_candidates.append(chrome_env['COMPUTERNAME'])
    elif line.startswith('INFO:root:Chrome Env: '):
      chrome_env = ast.literal_eval(line[len('INFO:root:Chrome Env: '):])
      if 'COMPUTERNAME' in chrome_env:
        bot_candidates.append(chrome_env['COMPUTERNAME'])
    elif line.startswith('Env: '):
      chrome_env = ast.literal_eval(line[len('Env: '):])
      if 'COMPUTERNAME' in chrome_env:
        bot_candidates.append(chrome_env['COMPUTERNAME'])
    elif line.startswith(' COMPUTERNAME: '):
      bot_candidates.append(line.strip()[len('COMPUTERNAME: '):])
    elif line.startswith('Results: '):
      assert results is None
      results = ast.literal_eval(line[len('Results: '):])
  for name in bot_candidates:
    if name.startswith('BUILD'):
      entry['bot'] = name
      break
  else:
    logging.warning('[BUILD %d] Fail to locate the bot name', number)


# pylint: enable=too-many-branches


def CollectBuildData(build, data_entries):
  if 'number' not in build:
    logging.warning('Missing build number')
    return False
  if 'bot' not in build:
    logging.warning('[BUILD %d] Missing bot name', build['number'])
    return False
  if len(build['tests']) != len(_TESTS):
    logging.warning('[BUILD %d] Measured test count should be %d, got %d',
                    build['number'], len(_TESTS), len(build['tests']))
    return False
  for test in build['tests']:
    if not CollectTestData(test, data_entries):
      return False
  for ii, data_entry in enumerate(data_entries):
    data_entry['build'] = build['number']
    data_entry['bot'] = build['bot']
    data_entry['iteration'] = ii
  return True


def CollectTestData(test, data_entries):
  test_name = test['name'].split('.')[-1]
  if test_name not in _TESTS:
    logging.warning('Unexpected test name: %s', test_name)
    return False
  for measure in _MEASUREMENTS:
    measure_name = measure + ' Power_0'
    if measure_name not in test:
      logging.warning('Missing measurment: %s', measure_name)
      return False
    results = test[measure_name]
    assert results
    if isinstance(results, list):
      repeats = len(results)
    else:
      repeats = 1
      results = [results]
    while len(data_entries) < repeats:
      data_entries.append({})
    for ii in range(repeats):
      data_entries[ii][test_name + '_' + measure] = results[ii]
  return True


def SaveResultsAsCSV(results, output_filename):
  builds = results['builds']
  csv_data = []
  for build in builds:
    entries = []
    if not CollectBuildData(build, entries):
      continue
    csv_data.extend(entries)
  if len(csv_data) > 0:
    with open(output_filename, 'w') as csv_file:
      labels = sorted(csv_data[0].keys())
      w = csv.DictWriter(csv_file, fieldnames=labels)
      w.writeheader()
      w.writerows(csv_data)
    logging.debug('Data from %d tests saved to %s', len(csv_data),
                  output_filename)
  else:
    logging.warning('No valid data saved to %s', output_filename)


def main():
  rest_args = sys.argv[1:]
  parser = argparse.ArgumentParser(
      description='Gather JSON results from a run of a Swarming test.',
      formatter_class=argparse.ArgumentDefaultsHelpFormatter)
  parser.add_argument(
      '-v',
      '--verbose',
      action='store_true',
      default=False,
      help='Enable verbose output')
  parser.add_argument('--bot',
                      default='Win10 FYI x64 Release (Intel)',
                      help='Which bot to examine.')
  parser.add_argument(
      '--last-build',
      type=int,
      help='The last of a range of builds to fetch. If not '
      'specified, use the latest build.')
  parser.add_argument(
      '--build-count',
      type=int,
      default=100,
      help='How many builds to fetch. If not specified, '
      'fetch 100 builds.')
  parser.add_argument(
      '--step',
      default='power_measurement_test',
      help='Which step to fetch (treated as a prefix).')
  parser.add_argument(
      '--output-json',
      metavar='FILE',
      default='output.json',
      help='Name of output json file. Default is output.json.')
  parser.add_argument(
      '--output-csv',
      metavar='FILE',
      default='output.csv',
      help='Name of output csv file. Default is output.csv.')

  options = parser.parse_args(rest_args)
  if options.verbose:
    logging.basicConfig(level=logging.DEBUG)

  last_build = options.last_build
  if last_build is None and options.build_count <= 100:
    logging.debug('Start pulling latest %d builds', options.build_count)
    builds = GetJsonForLatestNBuilds(options.bot, options.build_count)
  else:
    if last_build is None:
      last_build = GetLatestGreenBuild(options.bot)
    first_build = last_build - options.build_count + 1
    builds = []
    for build_id in range(last_build, first_build - 1, -1):
      logging.debug('Pull data for build %d', build_id)
      try:
        build_json = GetJsonForBuildSteps(options.bot, build_id)
        build_json['number'] = build_id
        builds.append(build_json)
      except HTTPError:
        logging.warning('HTTPError raised, failed to load data from build %d',
                        build_id)

  logging.debug('Start processing stdout data')
  results = {'builds': []}
  for ii, build in enumerate(builds):
    if 'number' not in build:
      logging.warning('Missing number in build #%d', ii)
      continue
    number = build['number']
    if 'steps' not in build:
      logging.warning('[BUILD %d] Missing steps', number)
      continue
    stdout_url = FindStepLogURL(build['steps'], options.step, 'stdout')
    if not stdout_url:
      logging.warning('[BUILD %d] Unable to find stdout from step %s*', number,
                      options.step)
      continue
    results['builds'].append({'number': number, 'tests': []})
    ProcessStepStdout(stdout_url, results['builds'][-1])

  logging.debug('Saving output to %s', options.output_json)
  with open(options.output_json, 'w') as f:
    json.dump(results, f, sort_keys=True, indent=2, separators=(',', ': '))

  logging.debug('Saving output to %s', options.output_csv)
  SaveResultsAsCSV(results, options.output_csv)

  return 0


if __name__ == '__main__':
  sys.exit(main())
