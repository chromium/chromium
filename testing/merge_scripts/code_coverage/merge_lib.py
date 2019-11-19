# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Functions for interacting with llvm-profdata"""

import logging
import multiprocessing
import os
import shutil
import subprocess

logging.basicConfig(
    format='[%(asctime)s %(levelname)s] %(message)s', level=logging.DEBUG)


def _call_profdata_tool(profile_input_file_paths,
                        profile_output_file_path,
                        profdata_tool_path,
                        retries=3):
  """Calls the llvm-profdata tool.

  Args:
    profile_input_file_paths: A list of relative paths to the files that
        are to be merged.
    profile_output_file_path: The path to the merged file to write.
    profdata_tool_path: The path to the llvm-profdata executable.

  Returns:
    A list of paths to profiles that had to be excluded to get the merge to
    succeed, suspected of being corrupted or malformed.

  Raises:
    CalledProcessError: An error occurred merging profiles.
  """
  logging.info('Merging profiles.')

  try:
    subprocess_cmd = [
        profdata_tool_path, 'merge', '-o', profile_output_file_path,
        '-sparse=true'
    ]
    subprocess_cmd.extend(profile_input_file_paths)

    # Redirecting stderr is required because when error happens, llvm-profdata
    # writes the error output to stderr and our error handling logic relies on
    # that output.
    output = subprocess.check_output(subprocess_cmd, stderr=subprocess.STDOUT)
    logging.info('Merge succeeded with output: %r', output)
  except subprocess.CalledProcessError as error:
    if len(profile_input_file_paths) > 1 and retries >= 0:
      logging.warning('Merge failed with error output: %r', error.output)

      # The output of the llvm-profdata command will include the path of
      # malformed files, such as
      # `error: /.../default.profraw: Malformed instrumentation profile data`
      invalid_profiles = [
          f for f in profile_input_file_paths if f in error.output
      ]

      if not invalid_profiles:
        logging.info(
            'Merge failed, but wasn\'t able to figure out the culprit invalid '
            'profiles from the output, so skip retry and bail out.')
        raise error

      valid_profiles = list(
          set(profile_input_file_paths) - set(invalid_profiles))
      if valid_profiles:
        logging.warning(
            'Following invalid profiles are removed as they were mentioned in '
            'the merge error output: %r', invalid_profiles)
        logging.info('Retry merging with the remaining profiles: %r',
                     valid_profiles)
        return invalid_profiles + _call_profdata_tool(
            valid_profiles, profile_output_file_path, profdata_tool_path,
            retries - 1)

    logging.error('Failed to merge profiles, return code (%d), output: %r' %
                  (error.returncode, error.output))
    raise error

  logging.info('Profile data is created as: "%r".', profile_output_file_path)
  return []


def _get_profile_paths(input_dir, input_extension):
  """Finds all the profiles in the given directory (recursively)."""
  paths = []
  for dir_path, _sub_dirs, file_names in os.walk(input_dir):
    paths.extend([
        os.path.join(dir_path, fn)
        for fn in file_names
        if fn.endswith(input_extension)
    ])
  return paths


def _validate_and_convert_profraws(profraw_files, profdata_tool_path):
  """Validates and converts profraws to profdatas.

  For each given .profraw file in the input, this method first validates it by
  trying to convert it to an indexed .profdata file, and if the validation and
  conversion succeeds, the generated .profdata file will be included in the
  output, otherwise, won't.

  This method is mainly used to filter out invalid profraw files.

  Args:
    profraw_files: A list of .profraw paths.
    profdata_tool_path: The path to the llvm-profdata executable.

  Returns:
    A tulple:
      A list of converted .profdata files of *valid* profraw files.
      A list of *invalid* profraw files.
      A list of profraw files that have counter overflows.
  """
  logging.info('Validating and converting .profraw files.')

  for profraw_file in profraw_files:
    if not profraw_file.endswith('.profraw'):
      raise RuntimeError('%r is expected to be a .profraw file.' % profraw_file)

  cpu_count = multiprocessing.cpu_count()
  counts = max(10, cpu_count - 5)  # Use 10+ processes, but leave 5 cpu cores.
  pool = multiprocessing.Pool(counts)
  output_profdata_files = multiprocessing.Manager().list()
  invalid_profraw_files = multiprocessing.Manager().list()
  counter_overflows = multiprocessing.Manager().list()

  for profraw_file in profraw_files:
    pool.apply_async(
        _validate_and_convert_profraw,
        (profraw_file, output_profdata_files, invalid_profraw_files,
         counter_overflows, profdata_tool_path))

  pool.close()
  pool.join()

  # Remove inputs, as they won't be needed and they can be pretty large.
  for input_file in profraw_files:
    os.remove(input_file)

  return list(output_profdata_files), list(invalid_profraw_files), list(
      counter_overflows)


def _validate_and_convert_profraw(profraw_file, output_profdata_files,
                                  invalid_profraw_files, counter_overflows,
                                  profdata_tool_path):
  output_profdata_file = profraw_file.replace('.profraw', '.profdata')
  subprocess_cmd = [
      profdata_tool_path, 'merge', '-o', output_profdata_file, '-sparse=true',
      profraw_file
  ]
  profile_valid = False
  counter_overflow = False
  validation_output = None

  # 1. Determine if the profile is valid.
  try:
    # Redirecting stderr is required because when error happens, llvm-profdata
    # writes the error output to stderr and our error handling logic relies on
    # that output.
    validation_output = subprocess.check_output(
        subprocess_cmd, stderr=subprocess.STDOUT)
    if 'Counter overflow' in validation_output:
      counter_overflow = True
    else:
      profile_valid = True
  except subprocess.CalledProcessError as error:
    validation_output = error.output

  # 2. Add the profile to the appropriate list(s).
  if profile_valid:
    output_profdata_files.append(output_profdata_file)
  else:
    invalid_profraw_files.append(profraw_file)
    if counter_overflow:
      counter_overflows.append(profraw_file)

  # 3. Log appropriate message
  if not profile_valid:
    template = 'Bad profile: %r, output: %r'
    if counter_overflow:
      template = 'Counter overflow: %r, output: %r'
    logging.warning(template, profraw_file, validation_output)

    # 4. Delete profdata for invalid profiles if present.
    if os.path.exists(output_profdata_file):
      # The output file may be created before llvm-profdata determines the
      # input is invalid. Delete it so that it does not leak and affect other
      # merge scripts.
      os.remove(output_profdata_file)

def merge_java_exec_files(input_dir, output_path, jacococli_path):
  """Merges generated .exec files to output_path.

  Args:
    input_dir (str): The path to traverse to find input files.
    output_path (str): Where to write the merged .exec file.
    jacococli_path: The path to jacococli.jar.

  Raises:
    CalledProcessError: merge command failed.
  """
  exec_input_file_paths = _get_profile_paths(input_dir, '.exec')
  if not exec_input_file_paths:
    logging.info('No exec file found under %s', input_dir)
    return

  cmd = ['java', '-jar', jacococli_path, 'merge']
  cmd.extend(exec_input_file_paths)
  cmd.extend(['--destfile', output_path])
  output = subprocess.check_output(cmd, stderr=subprocess.STDOUT)
  logging.info('Merge succeeded with output: %r', output)


def merge_profiles(input_dir, output_file, input_extension, profdata_tool_path):
  """Merges the profiles produced by the shards using llvm-profdata.

  Args:
    input_dir (str): The path to traverse to find input profiles.
    output_file (str): Where to write the merged profile.
    input_extension (str): File extension to look for in the input_dir.
        e.g. '.profdata' or '.profraw'
    profdata_tool_path: The path to the llvm-profdata executable.
  Returns:
    The list of profiles that had to be excluded to get the merge to
    succeed and a list of profiles that had a counter overflow.
  """
  profile_input_file_paths = _get_profile_paths(input_dir, input_extension)
  invalid_profraw_files = []
  counter_overflows = []
  if input_extension == '.profraw':
    profile_input_file_paths, invalid_profraw_files, counter_overflows = (
        _validate_and_convert_profraws(profile_input_file_paths,
                                       profdata_tool_path))
    logging.info('List of converted .profdata files: %r',
                 profile_input_file_paths)
    logging.info((
        'List of invalid .profraw files that failed to validate and convert: %r'
    ), invalid_profraw_files)

    if counter_overflows:
      logging.warning('There were %d profiles with counter overflows',
                      len(counter_overflows))

  # The list of input files could be empty in the following scenarios:
  # 1. The test target is pure Python scripts test which doesn't execute any
  #    C/C++ binaries, such as devtools_type_check.
  # 2. The test target executes binary and does dumps coverage profile data
  #    files, however, all of them turned out to be invalid.
  if not profile_input_file_paths:
    logging.info('There is no valid profraw/profdata files to merge, skip '
                 'invoking profdata tools.')
    return invalid_profraw_files, counter_overflows

  invalid_profdata_files = _call_profdata_tool(
      profile_input_file_paths=profile_input_file_paths,
      profile_output_file_path=output_file,
      profdata_tool_path=profdata_tool_path)

  # Remove inputs, as they won't be needed and they can be pretty large.
  for input_file in profile_input_file_paths:
    os.remove(input_file)

  return invalid_profraw_files + invalid_profdata_files, counter_overflows

# We want to retry shards that contain one or more profiles that cannot be
# merged (typically due to corruption described in crbug.com/937521).
def get_shards_to_retry(bad_profiles):
  bad_shard_ids = set()

  def is_task_id(s):
    # Swarming task ids are 16 hex chars. The pythonic way to validate this is
    # to cast to int and catch a value error.
    try:
      assert len(s) == 16, 'Swarming task IDs are expected be of length 16'
      _int_id = int(s, 16)
      return True
    except (AssertionError, ValueError):
      return False

  for profile in bad_profiles:
    # E.g. /b/s/w/ir/tmp/t/tmpSvBRii/44b643576cf39f10/profraw/default-1.profraw
    _base_path, task_id, _profraw, _filename = os.path.normpath(profile).rsplit(
        os.path.sep, 3)
    # Since we are getting a task_id from a file path, which is less than ideal,
    # do some checking to at least verify that the snippet looks like a valid
    # task id.
    assert is_task_id(task_id)
    bad_shard_ids.add(task_id)
  return bad_shard_ids
