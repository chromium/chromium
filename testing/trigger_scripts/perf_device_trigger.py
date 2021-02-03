#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Custom swarming triggering script.

This script does custom swarming triggering logic, to enable device affinity
for our bots, while lumping all trigger calls under one logical step.

For the perf use case of device affinity, this script now enables soft device
affinity.  This means that it tries to smartly allocate jobs to bots based
on what is currently alive and what bot the task was last triggered on,
preferring that last triggered bot if available.  If the
--multiple-trigger-configs flag is specified than this script overrides
the soft device affinity functionality in favor of the provided ids.

The algorithm is roughly the following:

Find eligible bots, healthy or not.
  * Query swarming for eligible bots based on the dimensions passed in
    on the swarming call.  Determine their health status based on
    is not quarantied and is not is_dead

Of the eligible bots determine what bot id to run the shard on.
(Implementation in _select_config_indices_with_soft_affinity)
  * First query swarming for the last task that ran that shard with
    given dimensions.  Assuming they are returned with most recent first.
  * Check if the bot id that ran that task is alive, if so trigger
    on that bot again.
  * If that bot isn't alive, allocate to another alive bot or if no
    other alive bots exist, trigger on the same dead one.

Scripts inheriting must have roughly the same command line interface as
swarming.py trigger. It modifies it in the following ways:

 * Intercepts the dump-json argument, and creates its own by combining the
   results from each trigger call.
 * Intercepts the dimensions from the swarming call and determines what bots
   are healthy based on the above device affinity algorithm, and triggers
 * Adds a tag to the swarming trigger job with the shard so we know the last
   bot that ran this shard.

This script is normally called from the swarming recipe module in tools/build.

"""

from __future__ import print_function

import argparse
import copy
import json
import os
import subprocess
import sys
import tempfile
import urllib
import logging

import base_test_triggerer

class Bot(object):
  """Eligible bots to run the task."""
  def __init__(self, bot_id, is_alive):
    self._bot_id = bot_id
    self._is_alive = is_alive

  def id(self):
    return self._bot_id

  def is_alive(self):
    return self._is_alive

  def as_json_config(self):
    return {'id': self._bot_id}

class PerfDeviceTriggerer(base_test_triggerer.BaseTestTriggerer):
  def __init__(self, args, swarming_args):
    super(PerfDeviceTriggerer, self).__init__()
    if not args.multiple_trigger_configs:
      # Represents the list of current dimensions requested
      # by the parent swarming job.
      self._dimensions = self._get_swarming_dimensions(swarming_args)

      # Store what swarming server we need and whether or not we need
      # to send down authentication with it
      self._swarming_server = self._get_swarming_server(swarming_args)

      # Map of all existing bots in swarming that satisfy the current
      # set of dimensions indexed by bot id.
      # Note: this assumes perf bot dimensions are unique between
      # configurations.
      self._eligible_bots_by_ids = (
          self._query_swarming_for_eligible_bot_configs(self._dimensions))

  def append_additional_args(self, args, shard_index):
    # Append a tag to the swarming task with the shard number
    # so we can query for the last bot that ran a specific shard.
    tag = 'shard:%d' % shard_index
    shard_tag = ['--tag', tag]
    # Need to append this before the dash if present so it gets fed to
    # the swarming task itself.
    if '--' in args:
      dash_ind = args.index('--')
      return args[:dash_ind] + shard_tag + args[dash_ind:]
    else:
      return args + shard_tag

  def parse_bot_configs(self, args):
    if args.multiple_trigger_configs:
      super(PerfDeviceTriggerer, self).parse_bot_configs(args)
    else:
      self._bot_configs = []
      # For each eligible bot, append the dimension
      # to the eligible bot_configs
      for  _, bot in self._eligible_bots_by_ids.iteritems():
        self._bot_configs.append(bot.as_json_config())

  def select_config_indices(self, args, verbose):
    if args.multiple_trigger_configs:
      configs = []
      # If specific bot ids were passed in, we want to trigger a job for
      # every valid config regardless of health status since
      # each config represents exactly one bot in the perf swarming pool.
      for index in range(len(self.indices_to_trigger(args))):
        configs.append((index, index))
    return self._select_config_indices_with_soft_affinity(args, verbose)

  def _select_config_indices_with_soft_affinity(self, args, verbose):
    trigger_count = len(self.indices_to_trigger(args))
    # First make sure the number of shards doesn't exceed the
    # number of eligible bots.  This means there is a config error somewhere.
    if trigger_count > len(self._eligible_bots_by_ids):
      if verbose:
        self._print_device_affinity_info({}, {},
          self._eligible_bots_by_ids, trigger_count)
      raise ValueError('Not enough available machines exist in swarming '
                       'pool.  Shards requested (%d) exceeds available bots '
                       '(%d).' % (
                           trigger_count, len(self._eligible_bots_by_ids)))

    shard_to_bot_assignment_map = {}
    unallocated_bots_by_ids = copy.deepcopy(self._eligible_bots_by_ids)
    for shard_index in self.indices_to_trigger(args):
      bot_id = self._query_swarming_for_last_shard_id(shard_index)
      if bot_id and bot_id in unallocated_bots_by_ids:
        bot = unallocated_bots_by_ids[bot_id]
        shard_to_bot_assignment_map[shard_index] = bot
        unallocated_bots_by_ids.pop(bot_id)
      else:
        shard_to_bot_assignment_map[shard_index] = None

    # Maintain the current map for debugging purposes
    existing_shard_bot_to_shard_map = copy.deepcopy(shard_to_bot_assignment_map)
    # Now create sets of remaining healthy and bad bots
    unallocated_healthy_bots = {
        b for b in unallocated_bots_by_ids.values() if b.is_alive()}
    unallocated_bad_bots = {
        b for b in unallocated_bots_by_ids.values() if not b.is_alive()}

    # Try assigning healthy bots for new shards first.
    for shard_index, bot in sorted(shard_to_bot_assignment_map.iteritems()):
      if not bot and unallocated_healthy_bots:
        shard_to_bot_assignment_map[shard_index] = \
            unallocated_healthy_bots.pop()
        if verbose:
          print('First time shard %d has been triggered' % shard_index)
      elif not bot:
        shard_to_bot_assignment_map[shard_index] = unallocated_bad_bots.pop()

    # Handle the rest of shards that were assigned dead bots:
    for shard_index, bot in sorted(shard_to_bot_assignment_map.iteritems()):
      if not bot.is_alive() and unallocated_healthy_bots:
        dead_bot = bot
        healthy_bot = unallocated_healthy_bots.pop()
        shard_to_bot_assignment_map[shard_index] = healthy_bot
        if verbose:
          print('Device affinity broken for shard #%d. bot %s is dead, new '
                'mapping to bot %s' % (
                    shard_index, dead_bot.id(), healthy_bot.id()))

    # Now populate the indices into the bot_configs array
    selected_configs = []
    for shard_index in self.indices_to_trigger(args):
      selected_configs.append((shard_index, self._find_bot_config_index(
          shard_to_bot_assignment_map[shard_index].id())))
    if verbose:
      self._print_device_affinity_info(
        shard_to_bot_assignment_map,
        existing_shard_bot_to_shard_map,
        self._eligible_bots_by_ids, trigger_count)
    return selected_configs


  def _print_device_affinity_info(
      self, new_map, existing_map, health_map, num_shards):
    print()
    for shard_index in xrange(num_shards):
      existing = existing_map.get(shard_index, None)
      new = new_map.get(shard_index, None)
      existing_id = ''
      if existing:
        existing_id = existing.id()
      new_id = ''
      if new:
        new_id = new.id()
      print('Shard %d\n\tprevious: %s\n\tnew: %s' % (
          shard_index, existing_id, new_id))

    healthy_bots = []
    dead_bots = []
    for _, b in health_map.iteritems():
      if b.is_alive():
        healthy_bots.append(b.id())
      else:
        dead_bots.append(b.id())
    print('Shards needed: %d' % num_shards)
    print('Total bots (dead + healthy): %d' % (
        len(dead_bots) + len(healthy_bots)))
    print('Healthy bots, %d: %s' % (len(healthy_bots), healthy_bots))
    print('Dead Bots, %d: %s' % (len(dead_bots), dead_bots))
    print()


  def _query_swarming_for_eligible_bot_configs(self, dimensions):
    """Query Swarming to figure out which bots are available.

      Returns: a dictionary in which the keys are the bot id and
      the values are Bot object that indicate the health status
      of the bots.
    """
    values = []
    for key, value in sorted(dimensions.iteritems()):
      values.append(('dimensions', '%s:%s' % (key, value)))

    query_result = self.query_swarming(
        'bots/list', values, True, server=self._swarming_server)
    if 'items' not in query_result:
      return {}
    perf_bots = {}
    for bot in query_result['items']:
      # Device maintenance is usually quick, and we can wait for it to finish.
      # However, if the device is too hot, it can take a long time for it to
      # cool down, so check for 'Device temperature' in maintenance_msg.
      alive = (not bot['is_dead'] and not bot['quarantined'] and
               'Device temperature' not in bot.get('maintenance_msg', ''))
      perf_bots[bot['bot_id']] = Bot(bot['bot_id'], alive)
    return perf_bots

  def _find_bot_config_index(self, bot_id):
    # Find the index into the bot_config map that
    # maps to the bot id in question
    for i, dimensions in enumerate(self._bot_configs):
      if dimensions['id'] == bot_id:
        return i
    return None

  def _query_swarming_for_last_shard_id(self, shard_index):
    # Per shard, query swarming for the last bot that ran the task
    # Example: swarming.py query -S server-url.com --limit 1 \\
    #  'tasks/list?tags=os:Windows&tags=pool:chrome.tests.perf&tags=shard:12'
    values = [
      ('tags', '%s:%s' % (k, v)) for k, v in self._dimensions.iteritems()
    ]
    # Append the shard as a tag
    values.append(('tags', '%s:%s' % ('shard', str(shard_index))))
    values.sort()
    # TODO(eyaich): For now we are ignoring the state of the returned
    # task (ie completed, timed_out, bot_died, etc) as we are just
    # answering the question "What bot did we last trigger this shard on?"
    # Evaluate if this is the right decision going forward.

    # Query for the last task that ran with these dimensions and this shard
    query_result = self.query_swarming(
          'tasks/list', values, True, limit='1', server=self._swarming_server)
    tasks = query_result.get('items')
    if tasks:
      # We queried with a limit of 1 so we could only get back
      # the most recent which is what we care about.
      task = tasks[0]
      if 'bot_id' in task:
        return task['bot_id']
      for tag in task['tags']:
        if tag.startswith('id:'):
          return tag[len('id:'):]
    # No eligible shard for this bot
    return None

  def _get_swarming_dimensions(self, args):
    dimensions = {}
    for i in xrange(len(args) - 2):
      if args[i] == '--dimension':
        dimensions[args[i+1]] = args[i+2]
    return dimensions

  def _get_swarming_server(self, args):
    for i in xrange(len(args)):
      if '--swarming' in args[i]:
        server = args[i+1]
        slashes_index = server.index('//') + 2
        # Strip out the protocol
        return server[slashes_index:]


def main():
  logging.basicConfig(
      level=logging.INFO,
      format='(%(levelname)s) %(asctime)s pid=%(process)d'
             '  %(module)s.%(funcName)s:%(lineno)d  %(message)s')
  # Setup args for common contract of base class
  parser = base_test_triggerer.BaseTestTriggerer.setup_parser_contract(
      argparse.ArgumentParser(description=__doc__))
  args, remaining = parser.parse_known_args()

  triggerer = PerfDeviceTriggerer(args, remaining)
  return triggerer.trigger_tasks(args, remaining)

if __name__ == '__main__':
  sys.exit(main())
