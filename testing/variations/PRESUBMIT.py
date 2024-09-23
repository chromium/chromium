# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit script validating field trial configs.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into depot_tools.
"""

import copy
import io
import json
import re
import sys

# TODO(b/365662411): Upgrade to PRESUBMIT_VERSION 2.0.0.

from collections import OrderedDict

VALID_EXPERIMENT_KEYS = [
    'name', 'forcing_flag', 'params', 'enable_features', 'disable_features',
    'min_os_version', 'hardware_classes', 'exclude_hardware_classes', '//0',
    '//1', '//2', '//3', '//4', '//5', '//6', '//7', '//8', '//9'
]

FIELDTRIAL_CONFIG_FILE_NAME = 'fieldtrial_testing_config.json'

BASE_FEATURE_PATTERN = r'BASE_FEATURE\((.*?),(.*?),(.*?)\);'
BASE_FEATURE_RE = re.compile(BASE_FEATURE_PATTERN,
                             flags=re.MULTILINE + re.DOTALL)


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
  #                     min_os_version: "version string"
  #                     hardware_classes: [sorted classes]
  #                     exclude_hardware_classes: [sorted classes]
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
        for index in range(0, 10):
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
        if 'min_os_version' in experiment:
          ordered_experiment['min_os_version'] = experiment['min_os_version']
        if 'hardware_classes' in experiment:
          ordered_experiment['hardware_classes'] = \
              sorted(experiment['hardware_classes'])
        if 'exclude_hardware_classes' in experiment:
          ordered_experiment['exclude_hardware_classes'] = \
              sorted(experiment['exclude_hardware_classes'])
        ordered_experiment_config['experiments'].append(ordered_experiment)
      ordered_study.append(ordered_experiment_config)
    ordered_config[key] = ordered_study
  return json.dumps(
      ordered_config, sort_keys=False, indent=4, separators=(',', ': ')) + '\n'


def ValidateData(json_data, file_path, message_type):
  """Validates the format of a fieldtrial configuration.

  Args:
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

  if not isinstance(json_data, dict):
    return _CreateMessage('Expecting dict')
  for (study, experiment_configs) in iter(json_data.items()):
    warnings = _ValidateEntry(study, experiment_configs, _CreateMessage)
    if warnings:
      return warnings

  return []


def _ValidateEntry(study, experiment_configs, create_message_fn):
  """Validates one entry of the field trial configuration."""
  if not isinstance(study, str):
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
      'chromeos_lacros', 'fuchsia', 'ios', 'linux', 'mac', 'windows'
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
  if not name or not isinstance(name, str):
    return create_message_fn('Missing valid name for experiment')

  # Add context to other messages.
  def _CreateGroupMessage(message_format, *args):
    suffix = ' in Group[%s]' % name
    return create_message_fn(message_format + suffix, *args)

  if 'params' in experiment_group:
    params = experiment_group['params']
    if not isinstance(params, dict):
      return _CreateGroupMessage('Expected dict for params')
    for (key, value) in iter(params.items()):
      if not isinstance(key, str) or not isinstance(value, str):
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
  format_args = (file_path, ) + args
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
                     'python3 testing/variations/PRESUBMIT.py %s' % file_path)
    ]
  return []


def _GetStudyConfigFeatures(study_config):
  """Gets the set of features overridden in a study config."""
  features = set()
  for experiment in study_config.get('experiments', []):
    features.update(experiment.get('enable_features', []))
    features.update(experiment.get('disable_features', []))
  return features


def _GetDuplicatedFeatures(study1, study2):
  """Gets the set of features that are overridden in two overlapping studies."""
  duplicated_features = set()
  for study_config1 in study1:
    features = _GetStudyConfigFeatures(study_config1)
    platforms = set(study_config1.get('platforms', []))
    for study_config2 in study2:
      # If the study configs do not specify any common platform, they do not
      # overlap, so we can skip them.
      if platforms.isdisjoint(set(study_config2.get('platforms', []))):
        continue

      common_features = features & _GetStudyConfigFeatures(study_config2)
      duplicated_features.update(common_features)

  return duplicated_features


def CheckDuplicatedFeatures(new_json_data, old_json_data, message_type):
  """Validates that features are not specified in multiple studies.

  Note that a feature may be specified in different studies that do not overlap.
  For example, if they specify different platforms. In such a case, this will
  not give a warning/error. However, it is possible that this incorrectly
  gives an error, as it is possible for studies to have complex filters (e.g.,
  if they make use of additional filters such as form_factors,
  is_low_end_device, etc.). In those cases, the PRESUBMIT check can be bypassed.
  Since this will only check for studies that were changed in this particular
  commit, bypassing the PRESUBMIT check will not block future commits.

  Args:
    new_json_data: Parsed JSON object representing the new fieldtrial config.
    old_json_data: Parsed JSON object representing the old fieldtrial config.
    message_type: Type of message from |output_api| to return in the case of
      errors/warnings.

  Returns:
    A list of |message_type| messages. In the case of all tests passing with no
    warnings/errors, this will return [].
  """
  # Get list of studies that changed.
  changed_studies = []
  for study_name in new_json_data:
    if (study_name not in old_json_data
        or new_json_data[study_name] != old_json_data[study_name]):
      changed_studies.append(study_name)

  # A map between a feature name and the name of studies that use it. E.g.,
  # duplicated_features_to_studies_map["FeatureA"] = {"StudyA", "StudyB"}.
  # Only features that are defined in multiple studies are added to this map.
  duplicated_features_to_studies_map = dict()

  # Compare the changed studies against all studies defined.
  for changed_study_name in changed_studies:
    for study_name in new_json_data:
      if changed_study_name == study_name:
        continue

      duplicated_features = _GetDuplicatedFeatures(
          new_json_data[changed_study_name], new_json_data[study_name])

      for feature in duplicated_features:
        if feature not in duplicated_features_to_studies_map:
          duplicated_features_to_studies_map[feature] = set()
        duplicated_features_to_studies_map[feature].update(
            [changed_study_name, study_name])

  if len(duplicated_features_to_studies_map) == 0:
    return []

  duplicated_features_strings = [
      '%s (in studies %s)' % (feature, ', '.join(studies))
      for feature, studies in duplicated_features_to_studies_map.items()
  ]

  return [
      message_type('The following feature(s) were specified in multiple '
                   'studies: %s' % ', '.join(duplicated_features_strings))
  ]


def CheckUndeclaredFeatures(input_api, output_api, json_data, changed_lines):
  """Checks that feature names are all valid declared features.

  There have been more than one instance of developers accidentally mistyping
  a feature name in the fieldtrial_testing_config.json file, which leads
  to the config silently doing nothing.

  This check aims to catch these errors by validating that the feature name
  is defined somewhere in the Chrome source code.

  Args:
    input_api: Presubmit InputApi
    output_api: Presubmit OutputApi
    json_data: The parsed fieldtrial_testing_config.json
    changed_lines: The AffectedFile.ChangedContents() of the json file

  Returns:
    List of validation messages - empty if there are no errors.
  """

  declared_features = set()
  # I was unable to figure out how to do a proper top-level include that did
  # not depend on getting the path from input_api. I found this pattern
  # elsewhere in the code base. Please change to a top-level include if you
  # know how.
  old_sys_path = sys.path[:]
  try:
    sys.path.append(
        input_api.os_path.join(input_api.PresubmitLocalPath(), 'presubmit'))
    # pylint: disable=import-outside-toplevel
    import find_features
    # pylint: enable=import-outside-toplevel
    declared_features = find_features.FindDeclaredFeatures(input_api)
  finally:
    sys.path = old_sys_path

  if not declared_features:
    return [
        output_api.PresubmitError(
            'Presubmit unable to find any declared flags in source. Please '
            'check PRESUBMIT.py for errors.')
    ]

  messages = []
  # Join all changed lines into a single string. This will be used to check
  # if feature names are present in the changed lines by substring search.
  changed_contents = ' '.join([x[1].strip() for x in changed_lines])
  for study_name in json_data:
    study = json_data[study_name]
    for config in study:
      features = set(_GetStudyConfigFeatures(config))
      # Determine if a study has been touched by the current change by checking
      # if any of the features are part of the changed lines of the file.
      # This limits the noise from old configs that are no longer valid.
      probably_affected = False
      for feature in features:
        if feature in changed_contents:
          probably_affected = True
          break

      if probably_affected and not declared_features.issuperset(features):
        missing_features = features - declared_features
        # CrOS has external feature declarations starting with this prefix
        # (checked by build tools in base/BUILD.gn).
        # Warn, but don't break, if they are present in the CL
        cros_late_boot_features = {
            s
            for s in missing_features if s.startswith('CrOSLateBoot')
        }
        missing_features = missing_features - cros_late_boot_features
        if cros_late_boot_features:
          msg = ('CrOSLateBoot features added to '
                 'study %s are not checked by presubmit.'
                 '\nPlease manually check that they exist in the code base.'
                 ) % study_name
          messages.append(
              output_api.PresubmitResult(msg, cros_late_boot_features))

        if missing_features:
          msg = ('Presubmit was unable to verify existence of features in '
                 'study %s.\nThis happens most commonly if the feature is '
                 'defined by code generation.\n'
                 'Please verify that the feature names have been spelled '
                 'correctly before submitting. The affected features are:'
                 ) % study_name
          messages.append(output_api.PresubmitResult(msg, missing_features))

  return messages


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
              'validation. ' % FIELDTRIAL_CONFIG_FILE_NAME)
      ]
    contents = input_api.ReadFile(f)
    try:
      json_data = input_api.json.loads(contents)
      result = ValidateData(json_data, f.AbsoluteLocalPath(),
                            output_api.PresubmitError)
      if result:
        return result
      result = CheckPretty(contents, f.LocalPath(), output_api.PresubmitError)
      if result:
        return result
      result = CheckDuplicatedFeatures(
          json_data, input_api.json.loads('\n'.join(f.OldContents())),
          output_api.PresubmitError)
      if result:
        return result
      if input_api.is_committing:
        result = CheckUndeclaredFeatures(input_api, output_api, json_data,
                                         f.ChangedContents())
        if result:
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
  with io.open(argv[1], encoding='utf-8') as f:
    content = f.read()
  pretty = PrettyPrint(content)
  io.open(argv[1], 'wb').write(pretty.encode('utf-8'))


if __name__ == '__main__':
  sys.exit(main(sys.argv))
