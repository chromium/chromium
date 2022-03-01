#!/usr/bin/env vpython
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for perf_device_trigger_unittest.py."""

import unittest

import perf_device_trigger


class Args(object): # pylint: disable=useless-object-inheritance
    def __init__(self):
        self.shards = 1
        self.shard_index = None
        self.dump_json = ''
        self.multiple_trigger_configs = None
        self.multiple_dimension_script_verbose = False
        self.use_dynamic_shards = False


class FakeTriggerer(perf_device_trigger.PerfDeviceTriggerer):
    def __init__(self, args, swarming_args, files, list_bots_result,
                 list_tasks_results):
        self._bot_statuses = []
        self._swarming_runs = []
        self._files = files
        self._temp_file_id = 0
        self._triggered_with_swarming_go = 0
        self._list_bots_result = list_bots_result
        self._list_tasks_results = list_tasks_results
        # pylint: disable=super-with-arguments
        super(FakeTriggerer, self).__init__(args, swarming_args)
        # pylint: enable=super-with-arguments

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

    def list_bots(self,
                  dimensions,
                  server='chromium-swarm.appspot.com'):
        return self._list_bots_result

    def list_tasks(self, tags, limit=None,
                   server='chromium-swarm.appspot.com'):
        res, self._list_tasks_results = self._list_tasks_results[
            0], self._list_tasks_results[1:]
        return res

    def run_swarming(self, args):
        self._swarming_runs.append(args)

    def run_swarming_go(self,
                        args,
                        _json_path,
                        _shard_index,
                        _shard,
                        _merged_json=None):
        self._triggered_with_swarming_go += 1
        self.run_swarming(args)


class UnitTest(unittest.TestCase):
    def setup_and_trigger(self,
                          previous_task_assignment_map,
                          alive_bots,
                          dead_bots,
                          use_dynamic_shards=False):
        args = Args()
        args.shards = len(previous_task_assignment_map)
        args.dump_json = 'output.json'
        args.multiple_dimension_script_verbose = True
        if use_dynamic_shards:
            args.use_dynamic_shards = True
        swarming_args = [
            'trigger',
            '--swarming',
            'http://foo_server',
            '--dimension',
            'pool',
            'chrome-perf-fyi',
            '--dimension',
            'os',
            'windows',
            '--',
            'benchmark1',
        ]

        triggerer = FakeTriggerer(
            args, swarming_args, self.get_files(args.shards),
            self.generate_list_of_eligible_bots_query_response(
                alive_bots, dead_bots), [
                    self.generate_last_task_to_shard_query_response(
                        i, previous_task_assignment_map.get(i))
                    for i in range(args.shards)
                ])
        triggerer.trigger_tasks(args, swarming_args)
        return triggerer

    def get_files(self, num_shards):
        files = {}
        file_index = 0
        file_index = file_index + 1
        # Perf device trigger will call swarming n times:
        #   1. Once for all eligible bots
        #   2. once per shard to determine last bot run
        # Shard builders is a list of build ids that represents
        # the last build that ran the shard that corresponds to that
        # index.  If that shard hasn't been run before the entry
        # should be an empty string.
        for i in range(num_shards):
            task = {
                'tasks': [{
                    'request': {
                        'task_id': 'f%d' % i,
                    },
                }],
            }
            files['base_trigger_dimensions%d.json' % file_index] = task
            file_index = file_index + 1
        return files

    def generate_last_task_to_shard_query_response(self, shard, bot_id):
        if len(bot_id):
            # Test both cases where bot_id is present and you have to parse
            # out of the tags.
            if shard % 2:
                return [{'bot_id': bot_id}]
            return [{'tags': ['id:%s' % bot_id]}]
        return []

    def generate_list_of_eligible_bots_query_response(self, alive_bots,
                                                      dead_bots):
        if len(alive_bots) == 0 and len(dead_bots) == 0:
            return {}
        bots = []
        for bot_id in alive_bots:
            bots.append({
                'bot_id': ('%s' % bot_id),
                'is_dead': False,
                'quarantined': False
            })
        is_dead = True
        for bot_id in dead_bots:
            is_quarantined = (not is_dead)
            bots.append({
                'bot_id': ('%s' % bot_id),
                'is_dead': is_dead,
                'quarantined': is_quarantined
            })
            is_dead = (not is_dead)
        return bots

    def list_contains_sublist(self, main_list, sub_list):
        return any(sub_list == main_list[offset:offset + len(sub_list)]
                   for offset in range(len(main_list) - (len(sub_list) - 1)))

    def get_triggered_shard_to_bot(self, triggerer):
        triggered_map = {}
        for run in triggerer._swarming_runs:
            if not 'trigger' in run:
                continue
            bot_id = run[(run.index('id') + 1)]

            g = 'GTEST_SHARD_INDEX='
            shard = [int(r[len(g):]) for r in run if r.startswith(g)][0]

            triggered_map[shard] = bot_id
        return triggered_map

    def test_all_healthy_shards(self):
        triggerer = self.setup_and_trigger(
            previous_task_assignment_map={
                0: 'build3',
                1: 'build4',
                2: 'build5'
            },
            alive_bots=['build3', 'build4', 'build5'],
            dead_bots=['build1', 'build2'])
        expected_task_assignment = self.get_triggered_shard_to_bot(triggerer)
        self.assertEquals(len(set(expected_task_assignment.values())), 3)

        # All three bots were healthy so we should expect the task assignment to
        # stay the same
        self.assertEquals(expected_task_assignment.get(0), 'build3')
        self.assertEquals(expected_task_assignment.get(1), 'build4')
        self.assertEquals(expected_task_assignment.get(2), 'build5')

    def test_no_bot_returned(self):
        with self.assertRaises(ValueError) as context:
            self.setup_and_trigger(previous_task_assignment_map={0: 'build1'},
                                   alive_bots=[],
                                   dead_bots=[])
        err_msg = 'Not enough available machines exist in swarming pool'
        self.assertTrue(err_msg in str(context.exception))

    def test_previously_healthy_now_dead(self):
        # Test that it swaps out build1 and build2 that are dead
        # for two healthy bots
        triggerer = self.setup_and_trigger(
            previous_task_assignment_map={
                0: 'build1',
                1: 'build2',
                2: 'build3'
            },
            alive_bots=['build3', 'build4', 'build5'],
            dead_bots=['build1', 'build2'])
        expected_task_assignment = self.get_triggered_shard_to_bot(triggerer)
        self.assertEquals(len(set(expected_task_assignment.values())), 3)

        # The first two should be assigned to one of the unassigned healthy bots
        new_healthy_bots = ['build4', 'build5']
        self.assertIn(expected_task_assignment.get(0), new_healthy_bots)
        self.assertIn(expected_task_assignment.get(1), new_healthy_bots)
        self.assertEquals(expected_task_assignment.get(2), 'build3')

    def test_not_enough_healthy_bots(self):
        triggerer = self.setup_and_trigger(
            previous_task_assignment_map={
                0: 'build1',
                1: 'build2',
                2: 'build3',
                3: 'build4',
                4: 'build5'
            },
            alive_bots=['build3', 'build4', 'build5'],
            dead_bots=['build1', 'build2'])
        expected_task_assignment = self.get_triggered_shard_to_bot(triggerer)
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
            previous_task_assignment_map={
                0: 'build1',
                1: '',
                2: 'build3',
                3: 'build4',
                4: 'build5'
            },
            alive_bots=['build3', 'build4', 'build5'],
            dead_bots=['build1', 'build2'])
        expected_task_assignment = self.get_triggered_shard_to_bot(triggerer)
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
            previous_task_assignment_map={
                0: '',
                1: '',
                2: ''
            },
            alive_bots=['build3', 'build4', 'build5'],
            dead_bots=['build1', 'build2'])
        expected_task_assignment = self.get_triggered_shard_to_bot(triggerer)
        self.assertEquals(len(set(expected_task_assignment.values())), 3)
        new_healthy_bots = ['build3', 'build4', 'build5']
        self.assertIn(expected_task_assignment.get(0), new_healthy_bots)
        self.assertIn(expected_task_assignment.get(1), new_healthy_bots)
        self.assertIn(expected_task_assignment.get(2), new_healthy_bots)

    def test_previously_duplicate_task_assignments(self):
        triggerer = self.setup_and_trigger(
            previous_task_assignment_map={
                0: 'build3',
                1: 'build3',
                2: 'build5',
                3: 'build6'
            },
            alive_bots=['build3', 'build4', 'build5', 'build7'],
            dead_bots=['build1', 'build6'])
        expected_task_assignment = self.get_triggered_shard_to_bot(triggerer)

        # Test that the new assignment will add a new bot to avoid
        # assign 'build3' to both shard 0 & shard 1 as before.
        # It also replaces the dead 'build6' bot.
        self.assertEquals(set(expected_task_assignment.values()),
                          {'build3', 'build4', 'build5', 'build7'})

    def test_dynamic_sharding(self):
        triggerer = self.setup_and_trigger(
            # The previous map should not matter.
            previous_task_assignment_map={
                0: 'build301',
                1: 'build1--',
                2: 'build-blah'
            },
            alive_bots=['build1', 'build2', 'build3', 'build4', 'build5'],
            dead_bots=[],
            use_dynamic_shards=True)
        expected_task_assignment = self.get_triggered_shard_to_bot(triggerer)

        self.assertEquals(set(expected_task_assignment.values()),
                          {'build1', 'build2', 'build3', 'build4', 'build5'})

    def test_dynamic_sharding_with_dead_bots(self):
        triggerer = self.setup_and_trigger(
            # The previous map should not matter.
            previous_task_assignment_map={
                0: 'build301',
                1: 'build1--',
                2: 'build-blah'
            },
            alive_bots=['build2', 'build5', 'build3'],
            dead_bots=['build1', 'build4'],
            use_dynamic_shards=True)
        expected_task_assignment = self.get_triggered_shard_to_bot(triggerer)

        self.assertEquals(set(expected_task_assignment.values()),
                          {'build2', 'build3', 'build5'})


if __name__ == '__main__':
    unittest.main()
