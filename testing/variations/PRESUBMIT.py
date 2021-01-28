# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit script validating field trial configs.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into depot_tools.
"""

import copy
import json
import sys

from collections import OrderedDict

VALID_EXPERIMENT_KEYS = [
    'name', 'forcing_flag', 'params', 'enable_features', 'disable_features',
    'min_os_version', '//0', '//1', '//2', '//3', '//4', '//5', '//6', '//7',
    '//8', '//9'
]

FIELDTRIAL_CONFIG_FILE_NAME = 'fieldtrial_testing_config.json'

FIELDTRIAL_CONFIG_TOO_LONG_ERROR_MSG = \
  "Contents of %s result in command-line flags that exceed Windows' 32767 "\
  "character limit. To add a new entry, please remove existing obsolete "\
  "entries. To check if an entry is obsolete, do a code search for its "\
  "'enable_features' and 'disable_features' strings and verify there are "\
  "no results other than the files related to the testing config. "\
  "Automating this is tracked under crbug.com/1053702." % \
  FIELDTRIAL_CONFIG_FILE_NAME


def PrettyPrint(contents):
  """Pretty prints a fieldtrial configuration.

  Args:
    contents: File contents as a string.

  Returns:
    Pretty printed file contents.
  """

  # We have a preferred ordering of the fields (e.g. platforms on top). This
  # code loads everything into OrderedDicts and then tells json to dump it out.
  # The JSON dumper will respect the dict ordering.
  #
  # The ordering is as follows:
  # {
  #     'StudyName Alphabetical': [
  #         {
  #             'platforms': [sorted platforms]
  #             'groups': [
  #                 {
  #                     name: ...
  #                     forcing_flag: "forcing flag string"
  #                     params: {sorted dict}
  #                     enable_features: [sorted features]
  #                     disable_features: [sorted features]
  #                     (Unexpected extra keys will be caught by the validator)
  #                 }
  #             ],
  #             ....
  #         },
  #         ...
  #     ]
  #     ...
  # }
  config = json.loads(contents)
  ordered_config = OrderedDict()
  for key in sorted(config.keys()):
    study = copy.deepcopy(config[key])
    ordered_study = []
    for experiment_config in study:
      ordered_experiment_config = OrderedDict([('platforms',
                                                experiment_config['platforms']),
                                               ('experiments', [])])
      for experiment in experiment_config['experiments']:
        ordered_experiment = OrderedDict()
        for index in xrange(0, 10):
          comment_key = '//' + str(index)
          if comment_key in experiment:
            ordered_experiment[comment_key] = experiment[comment_key]
        ordered_experiment['name'] = experiment['name']
        if 'forcing_flag' in experiment:
          ordered_experiment['forcing_flag'] = experiment['forcing_flag']
        if 'params' in experiment:
          ordered_experiment['params'] = OrderedDict(
              sorted(experiment['params'].items(), key=lambda t: t[0]))
        if 'enable_features' in experiment:
          ordered_experiment['enable_features'] = \
              sorted(experiment['enable_features'])
        if 'disable_features' in experiment:
          ordered_experiment['disable_features'] = \
              sorted(experiment['disable_features'])
        ordered_experiment_config['experiments'].append(ordered_experiment)
        if 'min_os_version' in experiment:
          ordered_experiment['min_os_version'] = experiment['min_os_version']
      ordered_study.append(ordered_experiment_config)
    ordered_config[key] = ordered_study
  return json.dumps(
      ordered_config, sort_keys=False, indent=4, separators=(',', ': ')) + '\n'


def ValidateData(input_api, json_data, file_path, message_type):
  """Validates the format of a fieldtrial configuration.

  Args:
    input_api: An instance passed to presubmit scripts telling info about the
      changes.
    json_data: Parsed JSON object representing the fieldtrial config.
    file_path: String representing the path to the JSON file.
    message_type: Type of message from |output_api| to return in the case of
      errors/warnings.

  Returns:
    A list of |message_type| messages. In the case of all tests passing with no
    warnings/errors, this will return [].
  """

  def _CreateMessage(message_format, *args):
    return _CreateMalformedConfigMessage(message_type, file_path,
                                         message_format, *args)

  if not _IsFieldTrialSizeBelowLimitOnWindows(input_api, file_path):
    return [message_type(FIELDTRIAL_CONFIG_TOO_LONG_ERROR_MSG)]

  if not isinstance(json_data, dict):
    return _CreateMessage('Expecting dict')
  for (study, experiment_configs) in json_data.iteritems():
    warnings = _ValidateEntry(study, experiment_configs, _CreateMessage)
    if warnings:
      return warnings

  return []


def _ValidateEntry(study, experiment_configs, create_message_fn):
  """Validates one entry of the field trial configuration."""
  if not isinstance(study, unicode):
    return create_message_fn('Expecting keys to be string, got %s', type(study))
  if not isinstance(experiment_configs, list):
    return create_message_fn('Expecting list for study %s', study)

  # Add context to other messages.
  def _CreateStudyMessage(message_format, *args):
    suffix = ' in Study[%s]' % study
    return create_message_fn(message_format + suffix, *args)

  for experiment_config in experiment_configs:
    warnings = _ValidateExperimentConfig(experiment_config, _CreateStudyMessage)
    if warnings:
      return warnings
  return []


def _ValidateExperimentConfig(experiment_config, create_message_fn):
  """Validates one config in a configuration entry."""
  if not isinstance(experiment_config, dict):
    return create_message_fn('Expecting dict for experiment config')
  if not 'experiments' in experiment_config:
    return create_message_fn('Missing valid experiments for experiment config')
  if not isinstance(experiment_config['experiments'], list):
    return create_message_fn('Expecting list for experiments')
  for experiment_group in experiment_config['experiments']:
    warnings = _ValidateExperimentGroup(experiment_group, create_message_fn)
    if warnings:
      return warnings
  if not 'platforms' in experiment_config:
    return create_message_fn('Missing valid platforms for experiment config')
  if not isinstance(experiment_config['platforms'], list):
    return create_message_fn('Expecting list for platforms')
  supported_platforms = [
      'android', 'android_weblayer', 'android_webview', 'chromeos',
      'chromeos_lacros', 'ios', 'linux', 'mac', 'windows'
  ]
  experiment_platforms = experiment_config['platforms']
  unsupported_platforms = list(
      set(experiment_platforms).difference(supported_platforms))
  if unsupported_platforms:
    return create_message_fn('Unsupported platforms %s', unsupported_platforms)
  return []


def _ValidateExperimentGroup(experiment_group, create_message_fn):
  """Validates one group of one config in a configuration entry."""
  name = experiment_group.get('name', '')
  if not name or not isinstance(name, unicode):
    return create_message_fn('Missing valid name for experiment')

  # Add context to other messages.
  def _CreateGroupMessage(message_format, *args):
    suffix = ' in Group[%s]' % name
    return create_message_fn(message_format + suffix, *args)

  if 'params' in experiment_group:
    params = experiment_group['params']
    if not isinstance(params, dict):
      return _CreateGroupMessage('Expected dict for params')
    for (key, value) in params.iteritems():
      if not isinstance(key, unicode) or not isinstance(value, unicode):
        return _CreateGroupMessage('Invalid param (%s: %s)', key, value)
  for key in experiment_group.keys():
    if key not in VALID_EXPERIMENT_KEYS:
      return _CreateGroupMessage('Key[%s] is not a valid key', key)
  return []


def _CreateMalformedConfigMessage(message_type, file_path, message_format,
                                  *args):
  """Returns a list containing one |message_type| with the error message.

  Args:
    message_type: Type of message from |output_api| to return in the case of
      errors/warnings.
    message_format: The error message format string.
    file_path: The path to the config file.
    *args: The args for message_format.

  Returns:
    A list containing a message_type with a formatted error message and
    'Malformed config file [file]: ' prepended to it.
  """
  error_message_format = 'Malformed config file %s: ' + message_format
  format_args = (file_path,) + args
  return [message_type(error_message_format % format_args)]


def CheckPretty(contents, file_path, message_type):
  """Validates the pretty printing of fieldtrial configuration.

  Args:
    contents: File contents as a string.
    file_path: String representing the path to the JSON file.
    message_type: Type of message from |output_api| to return in the case of
      errors/warnings.

  Returns:
    A list of |message_type| messages. In the case of all tests passing with no
    warnings/errors, this will return [].
  """
  pretty = PrettyPrint(contents)
  if contents != pretty:
    return [
        message_type('Pretty printing error: Run '
                     'python testing/variations/PRESUBMIT.py %s' % file_path)
    ]
  return []


def _IsFieldTrialSizeBelowLimitOnWindows(input_api, file_path):
  """Checks whether the fieldtrial parameters exceeded the Windows cmd limit.

  When launching chrome, the fieldtrial related parameters take more than
  30,000 characters. Windows has a limit of 32767 characters on command
  line and thus it raises parameter error when the parameters are too long.
  Before we have a valid fix, we need to limit the number of fieldtrias in
  fieldtrial_testing_config.json to 31,500, from the fact that the
  non-fieldtrial parameters take roughly 1450 characters.
  See crbug.com/1045530 for more details.

  Args:
    input_api: An instance passed to presubmit scripts telling info about the
      changes.
    file_path: the absolute path to the json file with the fieldtrial configs.
  """
  sys.path.append(input_api.os_path.join(
      input_api.PresubmitLocalPath(), '..', '..', 'tools', 'variations'))
  import fieldtrial_util

  args = fieldtrial_util.GenerateArgs(file_path, 'windows')
  total_length = 0
  for arg in args:
    total_length += len(arg)
  return total_length < 31500


def CommonChecks(input_api, output_api):
  affected_files = input_api.AffectedFiles(
      include_deletes=False,
      file_filter=lambda x: x.LocalPath().endswith('.json'))
  for f in affected_files:
    if not f.LocalPath().endswith(FIELDTRIAL_CONFIG_FILE_NAME):
      return [
          output_api.PresubmitError(
              '%s is the only json file expected in this folder. If new jsons '
              'are added, please update the presubmit process with proper '
              'validation. ' % FIELDTRIAL_CONFIG_FILE_NAME
          )
      ]
    contents = input_api.ReadFile(f)
    try:
      json_data = input_api.json.loads(contents)
      result = ValidateData(
          input_api,
          json_data,
          f.AbsoluteLocalPath(),
          output_api.PresubmitError)
      if len(result):
        return result
      result = CheckPretty(contents, f.LocalPath(), output_api.PresubmitError)
      if len(result):
        return result
    except ValueError:
      return [
          output_api.PresubmitError('Malformed JSON file: %s' % f.LocalPath())
      ]
  return []


def CheckChangeOnUpload(input_api, output_api):
  return CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return CommonChecks(input_api, output_api)


def main(argv):
  content = open(argv[1]).read()
  pretty = PrettyPrint(content)
  open(argv[1], 'wb').write(pretty)


if __name__ == '__main__':
  sys.exit(main(sys.argv))
