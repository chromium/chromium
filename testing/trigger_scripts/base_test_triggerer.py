#!/usr/bin/env python
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Custom swarming base trigger class.

This base class consolidates custom swarming triggering logic, to allow one bot
to conceptually span multiple Swarming configurations, while lumping all trigger
calls under one logical step.  It also gives the subclasses the ability to
define their own logic for pruning the configurations they want to trigger
jobs on and what configurations to use.

See perf_device_triggerer.py for an example of how to use this base class.

"""

import copy
import json
import os
import subprocess
import sys
import tempfile
import urllib
import logging

SRC_DIR = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(
  __file__))))

# .exe on Windows.
EXECUTABLE_SUFFIX = '.exe' if sys.platform == 'win32' else ''

SWARMING_PY = os.path.join(SRC_DIR, 'tools', 'swarming_client', 'swarming.py')
SWARMING_GO = os.path.join(SRC_DIR, 'tools', 'luci-go',
                           'swarming' + EXECUTABLE_SUFFIX)


def _convert_to_go_swarming_args(args):
  go_args = []
  i = 0
  while i < len(args):
    current_arg = args[i]
    if current_arg == '--swarming':
      current_arg = '--server'
    go_args.append(current_arg)
    i += 1
    if current_arg == '--dimension':
      go_args.append('{}={}'.format(args[i], args[i + 1]))
      i += 2
  return go_args


def strip_unicode(obj):
  """Recursively re-encodes strings as utf-8 inside |obj|. Returns the result.
  """
  if isinstance(obj, unicode):
    return obj.encode('utf-8', 'replace')
  if isinstance(obj, list):
    return list(map(strip_unicode, obj))

  if isinstance(obj, dict):
    new_obj = type(obj)(
        (strip_unicode(k), strip_unicode(v)) for k, v in obj.iteritems() )
    return new_obj
  return obj


class BaseTestTriggerer(object):
  def __init__(self):
    self._bot_configs = None
    self._bot_statuses = []
    self._total_bots = 0


  def modify_args(self, all_args, bot_index, shard_index, total_shards,
                  temp_file):
    """Modifies the given argument list.

    Specifically, it does the following:
      * Adds a --dump_json argument, to read in the results of the
        individual trigger command.
      * Adds the dimensions associated with the bot config at the given index.
      * If the number of shards is greater than one, adds --env
        arguments to set the GTEST_SHARD_INDEX and GTEST_TOTAL_SHARDS
        environment variables to _shard_index_ and _total_shards_,
        respectively.

    The arguments are structured like this:
    <args to swarming trigger> -- <args to bot running isolate>
    This means we have to add arguments to specific locations in the argument
    list, to either affect the trigger command, or what the bot runs.

    """
    bot_args = ['--dump-json', temp_file]
    if total_shards > 1:
      bot_args.append('--env')
      bot_args.append('GTEST_SHARD_INDEX=%s'%shard_index)
      bot_args.append('--env')
      bot_args.append('GTEST_TOTAL_SHARDS=%s'%total_shards)
    if self._bot_configs:
      for key, val in sorted(self._bot_configs[bot_index].iteritems()):
        bot_args.append('--dimension')
        bot_args.append(key)
        bot_args.append(val)
    if '--' in all_args:
      dash_ind = all_args.index('--')
      additional_args = all_args[:dash_ind] + bot_args + all_args[dash_ind:]
    else:
      additional_args = all_args + bot_args
    return self.append_additional_args(additional_args, shard_index)

  def append_additional_args(self, args, shard_index):
    """ Gives subclasses ability to append additional args if necessary

    Base class just returns given args."""
    del shard_index # unused
    return args

  def parse_bot_configs(self, args):
    try:
      self._bot_configs = strip_unicode(json.loads(
        args.multiple_trigger_configs))
    except Exception as e:
      raise ValueError('Error while parsing JSON from bot config string %s: %s'
                       % (args.multiple_trigger_configs, str(e)))
    # Validate the input.
    if not isinstance(self._bot_configs, list):
      raise ValueError('Bot configurations must be a list, were: %s' %
                       args.multiple_trigger_configs)
    if len(self._bot_configs) < 1:
      raise ValueError('Bot configuration list must have at least one entry')
    if not all(isinstance(entry, dict) for entry in self._bot_configs):
      raise ValueError('Bot configurations must all be dictionaries')

  # TODO(eyaich): Move the stateless logic that is specific to querying
  # swarming to its own object to make trigger logic more clear.
  def query_swarming(self, api, query_args, verbose,
                     limit='0',
                     server='chromium-swarm.appspot.com'):
    try:
      temp_file = self.make_temp_file(prefix='base_trigger_dimensions',
                                      suffix='.json')
      encoded_args = urllib.urlencode(query_args)
      args =['query',
             '-S',
             server,
             '--limit',
             limit,
             '--json',
             temp_file]
      # Append the query at the end
      args.append(('%s?%s' % (api, encoded_args)))
      ret = self.run_swarming(args, verbose)
      if ret:
        raise Exception('Error running swarming.py')
      return self.read_encoded_json_from_temp_file(temp_file)
    finally:
      self.delete_temp_file(temp_file)

  def query_swarming_for_bot_configs(self, verbose):
    # Query Swarming to figure out which bots are available.
    for config in self._bot_configs:
      values = []
      for key, value in sorted(config.iteritems()):
        values.append(('dimensions', '%s:%s' % (key, value)))
      # Ignore dead and quarantined bots.
      values.append(('is_dead', 'FALSE'))
      values.append(('quarantined', 'FALSE'))

      query_result = self.query_swarming('bots/count', values, verbose)
      # Summarize number of available bots per configuration.
      count = int(query_result['count'])
      # Be robust against errors in computation.
      available = max(0, count - int(query_result['busy']))
      self._bot_statuses.append({'total': count, 'available': available})
      if verbose:
        idx = len(self._bot_statuses) - 1
        logging.info(
            'Bot config %d: %s' % (idx, str(self._bot_statuses[idx])))
    # Sum up the total count of all bots.
    self._total_bots = sum(x['total'] for x in self._bot_statuses)
    if verbose:
      logging.info(
          'Total bots: %d' % (self._total_bots))

  def remove_swarming_dimension(self, args, dimension):
    for i in xrange(len(args)):
      if args[i] == '--dimension' and args[i+1] == dimension:
        return args[:i] + args[i+3:]
    return args

  def make_temp_file(self, prefix=None, suffix=None):
    # This trick of closing the file handle is needed on Windows in order to
    # make the file writeable.
    h, temp_file = tempfile.mkstemp(prefix=prefix, suffix=suffix)
    os.close(h)
    return temp_file

  def delete_temp_file(self, temp_file):
    os.remove(temp_file)

  def read_json_from_temp_file(self, temp_file):
    with open(temp_file) as f:
      return json.load(f)

  def read_encoded_json_from_temp_file(self, temp_file):
    return strip_unicode(self.read_json_from_temp_file(temp_file))

  def write_json_to_file(self, merged_json, output_file):
    with open(output_file, 'w') as f:
      json.dump(merged_json, f)

  def run_swarming(self, args, verbose):
    if verbose:
      logging.info('Running Swarming with args: %s', args)
    return subprocess.call([sys.executable, SWARMING_PY] + args)

  def run_swarming_go(self, args, verbose, json_path, shard_index, shards,
                      merged_json=None):
    if verbose:
      logging.info('Running Go `swarming` with args: %s', args)

    if merged_json is None:
      merged_json = {}

    if 'tasks' not in merged_json:
      merged_json['tasks'] = {}

    ret = subprocess.call([SWARMING_GO] + _convert_to_go_swarming_args(args))
    result_json = self.read_json_from_temp_file(json_path)

    tasks = {}
    for task in result_json['tasks']:
      k = task['request']['task_id']
      tasks[k] = task['request']
      invocation = task.get(
          'task_result', {}).get('resultdb_info', {}).get('invocation')
      if invocation:
        tasks[k]['invocation'] = invocation

    for k, v in tasks.items():
      v['shard_index'] = shard_index
      merged_json['tasks'][k + ':%d:%d' % (shard_index, shards)] = v
    self.write_json_to_file(merged_json, json_path)
    return ret

  def prune_test_specific_configs(self, args, verbose):
    # Ability for base class to further prune configs to
    # run tests on.
    pass

  def select_config_indices(self, args, verbose):
    # Main implementation for base class to determine which bot config to
    # trigger for each shard.
    #
    # Returns a list of tuples (shard_index, bot_config_index).
    # bot_config_index is an index into self._bot_configs
    pass

  def indices_to_trigger(self, args):
    """Returns the indices of the swarming shards that should be triggered."""
    if args.shard_index is None:
      return range(args.shards)
    else:
      return [args.shard_index]

  def trigger_tasks(self, args, remaining):
    """Triggers tasks for each bot.

    Args:
      args: Parsed arguments which we need to use.
      remaining: The remainder of the arguments, which should be passed to
                 swarming.py calls.

    Returns:
      Exit code for the script.
    """
    verbose = args.multiple_dimension_script_verbose
    self.parse_bot_configs(args)
    # Prunes config list to the exact set of configurations to trigger jobs on.
    # This logic is specific to the base class if they want to prune list
    # further.
    self.prune_test_specific_configs(args, verbose)

    # In the remaining arguments, find the Swarming dimensions that are
    # specified by the bot configs and remove them, because for each shard,
    # we're going to select one of the bot configs and put all of its Swarming
    # dimensions on the command line.
    filtered_remaining_args = copy.deepcopy(remaining)
    for config in self._bot_configs:
      for k in config.iterkeys():
        filtered_remaining_args = self.remove_swarming_dimension(
          filtered_remaining_args, k)

    merged_json = {}

    # Choose selected configs for this run of the test suite.
    for shard_index, bot_index in self.select_config_indices(args, verbose):
      # For each shard that we're going to distribute, do the following:
      # 1. Pick which bot configuration to use.
      # 2. Insert that bot configuration's dimensions as command line
      #    arguments, and invoke "swarming.py trigger".
      # Holds the results of the swarming.py trigger call.
      try:
        json_temp = self.make_temp_file(prefix='base_trigger_dimensions',
                                        suffix='.json')
        args_to_pass = self.modify_args(filtered_remaining_args, bot_index,
                                        shard_index, args.shards, json_temp)
        ret = self.run_swarming_go(
          args_to_pass, verbose, json_temp, shard_index, args.shards,
          merged_json)
        if ret:
          sys.stderr.write('Failed to trigger a task, aborting\n')
          return ret
      finally:
        self.delete_temp_file(json_temp)
    self.write_json_to_file(merged_json, args.dump_json)
    return 0

  @staticmethod
  def setup_parser_contract(parser):
    parser.add_argument('--multiple-trigger-configs', type=str, required=False,
                        help='The Swarming configurations to trigger tasks on, '
                        'in the form of a JSON array of dictionaries (these are'
                        ' Swarming dimension_sets). At least one entry is'
                        'required if you dont override parse_bot_configs')
    parser.add_argument('--multiple-dimension-script-verbose', type=bool,
                        default=False, help='Turn on verbose logging')
    parser.add_argument('--dump-json', required=True,
                        help='(Swarming Trigger Script API) Where to dump the'
                        ' resulting json which indicates which tasks were'
                        ' triggered for which shards.')
    parser.add_argument('--shards', type=int, default=1,
                        help='How many shards to trigger. Duplicated from the'
                       ' `swarming.py trigger` command.')
    parser.add_argument('--shard-index', type=int, default=None,
                        help='Which shard to trigger. Duplicated from the '
                             '`swarming.py trigger` command.')
    return parser
