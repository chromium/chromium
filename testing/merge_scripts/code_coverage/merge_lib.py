# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Functions for interacting with llvm-profdata"""

import logging
import multiprocessing
import os
import re
import subprocess
import sys

_DIR_SOURCE_ROOT = os.path.normpath(
    os.path.join(os.path.dirname(__file__), '..', '..', '..'))

_JAVA_PATH = os.path.join(_DIR_SOURCE_ROOT, 'third_party', 'jdk', 'current',
                          'bin', 'java')

logging.basicConfig(format='[%(asctime)s %(levelname)s] %(message)s',
                    level=logging.DEBUG)


def _call_profdata_tool(profile_input_file_paths,
                        profile_output_file_path,
                        profdata_tool_path,
                        sparse=False,
                        timeout=3600,
                        show_profdata=True,
                        weights=None):
  """Calls the llvm-profdata tool.

  Args:
    profile_input_file_paths: A list of relative paths to the files that
        are to be merged.
    profile_output_file_path: The path to the merged file to write.
    profdata_tool_path: The path to the llvm-profdata executable.
    sparse (bool): flag to indicate whether to run llvm-profdata with --sparse.
      Doc: https://llvm.org/docs/CommandGuide/llvm-profdata.html#profdata-merge
    timeout (int): timeout (sec) for the call to merge profiles. This should
      not take > 1 hr, and so defaults to 3600 seconds.
    show_profdata (bool): flag on whether the merged output information should
    be shown for debugging purposes.
    weights (dict): maps from benchmark name to weight.

  Raises:
    CalledProcessError: An error occurred merging profiles.
  """
  # There might be too many files in input and argument limit might be
  # violated, so make the tool read a list of paths from a file.
  output_dir = os.path.dirname(profile_output_file_path)
  # Normalize to POSIX style paths for consistent results.
  input_file = os.path.join(output_dir,
                            'input-profdata-files.txt').replace('\\', '/')
  input_files_with_weights = []
  for file_path in profile_input_file_paths:
    weight = 1
    if weights:
      for benchmark, w in weights.items():
        if file_path.endswith(benchmark):
          weight = w
          break
    input_file_with_weight = file_path
    if weight != 1:
      input_file_with_weight = weight + ',' + file_path
    input_files_with_weights.append(input_file_with_weight)

  with open(input_file, 'w') as fd:
    for f in input_files_with_weights:
      fd.write('%s\n' % f)

  logging.info('Contents of input-profdata-files.txt %s',
               input_files_with_weights)

  try:
    subprocess_cmd = [
        profdata_tool_path,
        'merge',
        '-o',
        profile_output_file_path,
    ]
    if sparse:
      subprocess_cmd += [
          '-sparse=true',
      ]
    subprocess_cmd.extend(['-f', input_file])
    logging.info('profdata command: %r', subprocess_cmd)

    # Redirecting stderr is required because when error happens, llvm-profdata
    # writes the error output to stderr and our error handling logic relies on
    # that output. stdout=None should print to console.
    # Timeout in seconds, set to 1 hr (60*60)
    p = subprocess.run(subprocess_cmd,
                       capture_output=True,
                       text=True,
                       timeout=timeout,
                       check=True)
    logging.info(p.stdout)
  except subprocess.CalledProcessError as error:
    logging.info('stdout: %s', error.output)
    logging.error('Failed to merge profiles, return code (%d), error: %r',
                  error.returncode, error.stderr)
    raise error
  except subprocess.TimeoutExpired as e:
    logging.info('stdout: %s', e.output)
    raise e

  if show_profdata:
    _call_profdata_show(profile_output_file_path, profdata_tool_path)

  logging.info('Profile data is created as: "%r".', profile_output_file_path)


def _call_profdata_show(profile_path,
                        profdata_tool_path,
                        topn=1000,
                        timeout=60):
  """Calls the llvm-profdata show command.

  Args:
    profile_path: The path to the profdata file to show.
    profdata_tool_path: The path to the llvm-profdata executable.
    topn: Only show functions with the topn hottest basic blocks.
    timeout (int): timeout (sec) for the call to show profiles.
  """

  try:
    subprocess_cmd = [
        profdata_tool_path,
        'show',
        '-topn',
        str(topn),
        profile_path,
    ]
    logging.info('profdata command: %r', subprocess_cmd)

    p = subprocess.run(subprocess_cmd,
                       capture_output=True,
                       text=True,
                       timeout=timeout,
                       check=True)
    logging.info(p.stdout)
  except subprocess.CalledProcessError as error:
    logging.info('stdout: %s', error.output)
    logging.error('Failed to show profile, return code (%d), error: %r',
                  error.returncode, error.stderr)
  except subprocess.TimeoutExpired as e:
    logging.info('stdout: %s', e.output)


def _get_profile_paths(input_dir, input_extension, input_filename_pattern='.*'):
  """Finds all the profiles in the given directory (recursively)."""
  paths = []
  for dir_path, _sub_dirs, file_names in os.walk(input_dir):
    paths.extend([
        # Normalize to POSIX style paths for consistent results.
        os.path.join(dir_path, fn).replace('\\', '/') for fn in file_names if
        fn.endswith(input_extension) and re.search(input_filename_pattern, fn)
    ])
  return paths


def _validate_and_convert_profraws(profraw_files,
                                   profdata_tool_path,
                                   sparse=False):
  """Validates and converts profraws to profdatas.

  For each given .profraw file in the input, this method first validates it by
  trying to convert it to an indexed .profdata file, and if the validation and
  conversion succeeds, the generated .profdata file will be included in the
  output, otherwise, won't.

  This method is mainly used to filter out invalid profraw files.

  Args:
    profraw_files: A list of .profraw paths.
    profdata_tool_path: The path to the llvm-profdata executable.
    sparse (bool): flag to indicate whether to run llvm-profdata with --sparse.
      Doc: https://llvm.org/docs/CommandGuide/llvm-profdata.html#profdata-merge

  Returns:
    A tuple:
      A list of converted .profdata files of *valid* profraw files.
      A list of *invalid* profraw files.
      A list of profraw files that have counter overflows.
  """
  for profraw_file in profraw_files:
    if not profraw_file.endswith('.profraw'):
      raise RuntimeError('%r is expected to be a .profraw file.' % profraw_file)

  cpu_count = multiprocessing.cpu_count()
  counts = max(10, cpu_count - 5)  # Use 10+ processes, but leave 5 cpu cores.
  if sys.platform == 'win32':
    # TODO(crbug.com/40755900) - we can't use more than 56 child processes on
    # Windows or Python3 may hang.
    counts = min(counts, 56)
  pool = multiprocessing.Pool(counts)
  output_profdata_files = multiprocessing.Manager().list()
  invalid_profraw_files = multiprocessing.Manager().list()
  counter_overflows = multiprocessing.Manager().list()

  results = []
  for profraw_file in profraw_files:
    results.append(
        pool.apply_async(
            _validate_and_convert_profraw,
            (profraw_file, output_profdata_files, invalid_profraw_files,
             counter_overflows, profdata_tool_path, sparse)))

  pool.close()
  pool.join()

  for x in results:
    x.get()

  # Remove inputs, as they won't be needed and they can be pretty large.
  for input_file in profraw_files:
    os.remove(input_file)

  return list(output_profdata_files), list(invalid_profraw_files), list(
      counter_overflows)


def _validate_and_convert_profraw(profraw_file,
                                  output_profdata_files,
                                  invalid_profraw_files,
                                  counter_overflows,
                                  profdata_tool_path,
                                  sparse=False,
                                  show_profdata=True):
  output_profdata_file = profraw_file.replace('.profraw', '.profdata')
  subprocess_cmd = [
      profdata_tool_path,
      'merge',
      '-o',
      output_profdata_file,
  ]
  if sparse:
    subprocess_cmd.append('--sparse')

  subprocess_cmd.append(profraw_file)
  logging.info('profdata command: %r', subprocess_cmd)

  profile_valid = False
  counter_overflow = False
  validation_output = None

  # 1. Determine if the profile is valid.
  try:
    # Redirecting stderr is required because when error happens, llvm-profdata
    # writes the error output to stderr and our error handling logic relies on
    # that output.
    validation_output = subprocess.check_output(subprocess_cmd,
                                                stderr=subprocess.STDOUT,
                                                encoding='UTF-8')
    if 'Counter overflow' in validation_output:
      counter_overflow = True
    else:
      profile_valid = True
  except subprocess.CalledProcessError as error:
    logging.warning('Validating and converting %r to %r failed with output: %r',
                    profraw_file, output_profdata_file, error.output)
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

  # 5. Show profdata information.
  if show_profdata:
    _call_profdata_show(output_profdata_file, profdata_tool_path)


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

  cmd = [_JAVA_PATH, '-jar', jacococli_path, 'merge']
  cmd.extend(exec_input_file_paths)
  cmd.extend(['--destfile', output_path])
  subprocess.check_call(cmd, stderr=subprocess.STDOUT)


def merge_profiles(input_dir,
                   output_file,
                   input_extension,
                   profdata_tool_path,
                   input_filename_pattern='.*',
                   sparse=False,
                   skip_validation=False,
                   merge_timeout=3600,
                   show_profdata=True,
                   weights=None):
  """Merges the profiles produced by the shards using llvm-profdata.

  Args:
    input_dir (str): The path to traverse to find input profiles.
    output_file (str): Where to write the merged profile.
    input_extension (str): File extension to look for in the input_dir.
        e.g. '.profdata' or '.profraw'
    profdata_tool_path: The path to the llvm-profdata executable.
    input_filename_pattern (str): The regex pattern of input filename. Should be
        a valid regex pattern if present.
    sparse (bool): flag to indicate whether to run llvm-profdata with --sparse.
      Doc: https://llvm.org/docs/CommandGuide/llvm-profdata.html#profdata-merge
    skip_validation (bool): flag to skip the _validate_and_convert_profraws
        invocation. only applicable when input_extension is .profraw.
    merge_timeout (int): timeout (sec) for the call to merge profiles. This
      should not take > 1 hr, and so defaults to 3600 seconds.
    weights (dict): maps from profdata file to weight.

  Returns:
    The list of profiles that had to be excluded to get the merge to
    succeed and a list of profiles that had a counter overflow.
  """
  profile_input_file_paths = _get_profile_paths(input_dir, input_extension,
                                                input_filename_pattern)
  invalid_profraw_files = []
  counter_overflows = []

  if skip_validation:
    logging.warning('--skip-validation has been enabled. Skipping conversion '
                    'to ensure that profiles are valid.')

  if input_extension == '.profraw' and not skip_validation:
    profile_input_file_paths, invalid_profraw_files, counter_overflows = (
        _validate_and_convert_profraws(profile_input_file_paths,
                                       profdata_tool_path,
                                       sparse=sparse))
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

  _call_profdata_tool(profile_input_file_paths=profile_input_file_paths,
                      profile_output_file_path=output_file,
                      profdata_tool_path=profdata_tool_path,
                      sparse=sparse,
                      timeout=merge_timeout,
                      show_profdata=show_profdata,
                      weights=weights)

  # Remove inputs when merging profraws as they won't be needed and they can be
  # pretty large. If the inputs are profdata files, do not remove them as they
  # might be used again for multiple test types coverage.
  if input_extension == '.profraw':
    for input_file in profile_input_file_paths:
      os.remove(input_file)

  return invalid_profraw_files, counter_overflows


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
