#!/usr/bin/python
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for perf_device_trigger_unittest.py."""

import unittest

import perf_device_trigger

class Args(object):
  def __init__(self):
    self.shards = 1
    self.shard_index = None
    self.dump_json = ''
    self.multiple_trigger_configs = None
    self.multiple_dimension_script_verbose = False


class FakeTriggerer(perf_device_trigger.PerfDeviceTriggerer):
  def __init__(self, args, swarming_args, files):
    self._bot_statuses = []
    self._swarming_runs = []
    self._files = files
    self._temp_file_id = 0
    super(FakeTriggerer, self).__init__(args, swarming_args)


  def set_files(self, files):
    self._files = files

  def make_temp_file(self, prefix=None, suffix=None):
    result = prefix + str(self._temp_file_id) + suffix
    self._temp_file_id += 1
    return result

  def delete_temp_file(self, temp_file):
    pass

  def read_json_from_temp_file(self, temp_file):
    return self._files[temp_file]

  def read_encoded_json_from_temp_file(self, temp_file):
    return self._files[temp_file]

  def write_json_to_file(self, merged_json, output_file):
    self._files[output_file] = merged_json

  def run_swarming(self, args, verbose):
    del verbose #unused
    self._swarming_runs.append(args)


class UnitTest(unittest.TestCase):
  def setup_and_trigger(
      self, previous_task_assignment_map, alive_bots, dead_bots):
    args = Args()
    args.shards = len(previous_task_assignment_map)
    args.dump_json = 'output.json'
    args.multiple_dimension_script_verbose = True
    swarming_args = [
        'trigger',
        '--swarming',
        'http://foo_server',
        '--auth-service-account-json',
        '/creds/test_service_account',
        '--dimension',
        'pool',
        'chrome-perf-fyi',
        '--dimension',
        'os',
        'windows',
        '--',
        'benchmark1',
      ]

    triggerer = FakeTriggerer(args, swarming_args,
        self.get_files(args.shards, previous_task_assignment_map,
                       alive_bots, dead_bots))
    triggerer.trigger_tasks(
      args,
      swarming_args)
    return triggerer

  def get_files(self, num_shards, previous_task_assignment_map,
                alive_bots, dead_bots):
    files = {}
    file_index = 0
    files['base_trigger_dimensions%d.json' % file_index] = (
        self.generate_list_of_eligible_bots_query_response(
            alive_bots, dead_bots))
    file_index = file_index + 1
    # Perf device trigger will call swarming n times:
    #   1. Once for all eligible bots
    #   2. once per shard to determine last bot run
    # Shard builders is a list of build ids that represents
    # the last build that ran the shard that corresponds to that
    # index.  If that shard hasn't been run before the entry
    # should be an empty string.
    for i in xrange(num_shards):
      bot_id = previous_task_assignment_map.get(i)
      files['base_trigger_dimensions%d.json' % file_index] = (
          self.generate_last_task_to_shard_query_response(i, bot_id))
      file_index = file_index + 1
    for i in xrange(num_shards):
      task = {
        'base_task_name': 'webgl_conformance_tests',
        'request': {
          'expiration_secs': 3600,
          'properties': {
            'execution_timeout_secs': 3600,
          },
        },
        'tasks': {
          'webgl_conformance_tests on NVIDIA GPU on Windows': {
            'task_id': 'f%d' % i,
          },
        },
      }
      files['base_trigger_dimensions%d.json' % file_index] = task
      file_index = file_index + 1
    return files

  def generate_last_task_to_shard_query_response(self, shard, bot_id):
    if len(bot_id):
      # Test both cases where bot_id is present and you have to parse
      # out of the tags.
      if shard % 2:
        return {'items': [{'bot_id': bot_id}]}
      else:
        return {'items': [{'tags': [('id:%s' % bot_id)]}]}
    return {}

  def generate_list_of_eligible_bots_query_response(
      self, alive_bots, dead_bots):
    if len(alive_bots) == 0 and len(dead_bots) == 0:
      return {}
    items = {'items': []}
    for bot_id in alive_bots:
      items['items'].append(
          { 'bot_id': ('%s' % bot_id), 'is_dead': False, 'quarantined': False })
    is_dead = True
    for bot_id in dead_bots:
      is_quarantined = (not is_dead)
      items['items'].append({
          'bot_id': ('%s' % bot_id),
          'is_dead': is_dead,
          'quarantined': is_quarantined
      })
      is_dead = (not is_dead)
    return items


  def list_contains_sublist(self, main_list, sub_list):
    return any(sub_list == main_list[offset:offset + len(sub_list)]
               for offset in xrange(len(main_list) - (len(sub_list) - 1)))

  def assert_query_swarming_args(self, triggerer, num_shards):
    # Assert the calls to query swarming send the right args
    # First call is to get eligible bots and then one query
    # per shard
    for i in range(num_shards + 1):
      self.assertTrue('query' in triggerer._swarming_runs[i])
      self.assertTrue(self.list_contains_sublist(
        triggerer._swarming_runs[i], ['-S', 'foo_server']))
      self.assertTrue(self.list_contains_sublist(
        triggerer._swarming_runs[i], ['--auth-service-account-json',
                                      '/creds/test_service_account']))

  def get_triggered_shard_to_bot(self, triggerer, num_shards):
    self.assert_query_swarming_args(triggerer, num_shards)
    triggered_map = {}
    for run in triggerer._swarming_runs:
      if not 'trigger' in run:
        continue
      bot_id = run[(run.index('id') + 1)]
      shard = int(run[(run.index('GTEST_SHARD_INDEX') + 1)])
      triggered_map[shard] = bot_id
    return triggered_map

  def test_all_healthy_shards(self):
    triggerer = self.setup_and_trigger(
        previous_task_assignment_map={0: 'build3', 1: 'build4', 2: 'build5'},
        alive_bots=['build3', 'build4', 'build5'],
        dead_bots=['build1', 'build2'])
    expected_task_assignment = self.get_triggered_shard_to_bot(
        triggerer, num_shards=3)
    self.assertEquals(len(set(expected_task_assignment.values())), 3)

    # All three bots were healthy so we should expect the task assignment to
    # stay the same
    self.assertEquals(expected_task_assignment.get(0), 'build3')
    self.assertEquals(expected_task_assignment.get(1), 'build4')
    self.assertEquals(expected_task_assignment.get(2), 'build5')

  def test_no_bot_returned(self):
    with self.assertRaises(ValueError) as context:
      self.setup_and_trigger(
          previous_task_assignment_map={0: 'build1'},
          alive_bots=[],
          dead_bots=[])
    err_msg = 'Not enough available machines exist in swarming pool'
    self.assertTrue(err_msg in context.exception.message)

  def test_previously_healthy_now_dead(self):
    # Test that it swaps out build1 and build2 that are dead
    # for two healthy bots
    triggerer = self.setup_and_trigger(
        previous_task_assignment_map={0: 'build1', 1: 'build2', 2: 'build3'},
        alive_bots=['build3', 'build4', 'build5'],
        dead_bots=['build1', 'build2'])
    expected_task_assignment = self.get_triggered_shard_to_bot(
        triggerer, num_shards=3)
    self.assertEquals(len(set(expected_task_assignment.values())), 3)

    # The first two should be assigned to one of the unassigned healthy bots
    new_healthy_bots = ['build4', 'build5']
    self.assertIn(expected_task_assignment.get(0), new_healthy_bots)
    self.assertIn(expected_task_assignment.get(1), new_healthy_bots)
    self.assertEquals(expected_task_assignment.get(2), 'build3')

  def test_not_enough_healthy_bots(self):
    triggerer = self.setup_and_trigger(
        previous_task_assignment_map= {0: 'build1', 1: 'build2',
                                       2: 'build3', 3: 'build4', 4: 'build5'},
        alive_bots=['build3', 'build4', 'build5'],
        dead_bots=['build1', 'build2'])
    expected_task_assignment = self.get_triggered_shard_to_bot(
        triggerer, num_shards=5)
    self.assertEquals(len(set(expected_task_assignment.values())), 5)

    # We have 5 shards and 5 bots that ran them, but two
    # are now dead and there aren't any other healthy bots
    # to swap out to.  Make sure they still assign to the
    # same shards.
    self.assertEquals(expected_task_assignment.get(0), 'build1')
    self.assertEquals(expected_task_assignment.get(1), 'build2')
    self.assertEquals(expected_task_assignment.get(2), 'build3')
    self.assertEquals(expected_task_assignment.get(3), 'build4')
    self.assertEquals(expected_task_assignment.get(4), 'build5')

  def test_not_enough_healthy_bots_shard_not_seen(self):
    triggerer = self.setup_and_trigger(
        previous_task_assignment_map= {0: 'build1', 1: '',
                                       2: 'build3', 3: 'build4', 4: 'build5'},
        alive_bots=['build3', 'build4', 'build5'],
        dead_bots=['build1', 'build2'])
    expected_task_assignment = self.get_triggered_shard_to_bot(
        triggerer, num_shards=5)
    self.assertEquals(len(set(expected_task_assignment.values())), 5)

    # Not enough healthy bots so make sure shard 0 is still assigned to its
    # same dead bot.
    self.assertEquals(expected_task_assignment.get(0), 'build1')
    # Shard 1 had not been triggered yet, but there weren't enough
    # healthy bots.  Make sure it got assigned to the other dead bot.
    self.assertEquals(expected_task_assignment.get(1), 'build2')
    # The rest of the assignments should stay the same.
    self.assertEquals(expected_task_assignment.get(2), 'build3')
    self.assertEquals(expected_task_assignment.get(3), 'build4')
    self.assertEquals(expected_task_assignment.get(4), 'build5')

  def test_shards_not_triggered_yet(self):
    # First time this configuration has been seen.  Choose three
    # healthy shards to trigger jobs on
    triggerer = self.setup_and_trigger(
        previous_task_assignment_map= {0: '', 1: '', 2: ''},
        alive_bots=['build3', 'build4', 'build5'],
        dead_bots=['build1', 'build2'])
    expected_task_assignment = self.get_triggered_shard_to_bot(
        triggerer, num_shards=3)
    self.assertEquals(len(set(expected_task_assignment.values())), 3)
    new_healthy_bots = ['build3', 'build4', 'build5']
    self.assertIn(expected_task_assignment.get(0), new_healthy_bots)
    self.assertIn(expected_task_assignment.get(1), new_healthy_bots)
    self.assertIn(expected_task_assignment.get(2), new_healthy_bots)

  def test_previously_duplicate_task_assignments(self):
    triggerer = self.setup_and_trigger(
        previous_task_assignment_map={0: 'build3', 1: 'build3', 2: 'build5',
                                      3: 'build6'},
        alive_bots=['build3', 'build4', 'build5', 'build7'],
        dead_bots=['build1', 'build6'])
    expected_task_assignment = self.get_triggered_shard_to_bot(
        triggerer, num_shards=3)

    # Test that the new assignment will add a new bot to avoid
    # assign 'build3' to both shard 0 & shard 1 as before.
    # It also replaces the dead 'build6' bot.
    self.assertEquals(set(expected_task_assignment.values()),
        {'build3', 'build4', 'build5', 'build7'})


if __name__ == '__main__':
  unittest.main()
