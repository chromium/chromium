# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Top-level presubmit script for testing.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into depot_tools.
"""

PRESUBMIT_VERSION = '2.0.0'

PYLINT_PATHS_COMPONENTS = [
  ('build',),
  ('build', 'android'),
  ('build', 'util'),
  ('content', 'test', 'gpu'),
  ('testing',),
  ('testing', 'buildbot'),
  ('testing', 'scripts'),
  ('testing', 'variations', 'presubmit'),
  ('third_party',),
  ('third_party', 'blink', 'renderer', 'bindings', 'scripts'),
  ('third_party', 'blink', 'tools'),
  ('third_party', 'catapult', 'telemetry'),
  ('third_party', 'catapult', 'third_party', 'typ'),
  ('third_party', 'catapult', 'tracing'),
  ('third_party', 'domato', 'src'),
  ('third_party', 'js_code_coverage'),
  ('third_party', 'webdriver', 'pylib'),
  ('tools', 'perf'),
]


def _GetChromiumSrcPath(input_api):
  """Returns the path to the Chromium src directory."""
  return input_api.os_path.realpath(
    input_api.os_path.join(input_api.PresubmitLocalPath(), '..')
  )


def _GetTestingEnv(input_api):
  """Gets the common environment for running testing/ tests."""
  testing_env = dict(input_api.environ)
  testing_path = input_api.PresubmitLocalPath()
  # TODO(crbug.com/40237086): This is temporary till gpu code in
  # flake_suppressor_commonis moved to gpu dir.
  # Only common code will reside under /testing.
  gpu_test_path = input_api.os_path.join(
    input_api.PresubmitLocalPath(), '..', 'content', 'test', 'gpu'
  )
  typ_path = input_api.os_path.join(
    input_api.PresubmitLocalPath(),
    '..',
    'third_party',
    'catapult',
    'third_party',
    'typ',
  )
  testing_env.update(
    {
      'PYTHONPATH': input_api.os_path.pathsep.join(
        [testing_path, gpu_test_path, typ_path]
      ),
      'PYTHONDONTWRITEBYTECODE': '1',
    }
  )
  return testing_env


def _ShouldRunCommonUnittests(input_api, directories):
  """Returns True if affected files match directories or this PRESUBMIT.py."""
  if isinstance(directories, str):
    directories = [directories]
  this_presubmit = input_api.os_path.normcase(
    input_api.os_path.join(input_api.PresubmitLocalPath(), 'PRESUBMIT.py')
  )
  # Ensure trailing separator to avoid prefix-matching unrelated directories.
  prefixes = tuple(
    input_api.os_path.normcase(
      input_api.os_path.join(input_api.PresubmitLocalPath(), d, '')
    )
    for d in directories
  )
  affected_paths = (
    input_api.os_path.normcase(f.AbsoluteLocalPath())
    for f in input_api.AffectedFiles()
  )
  return any(
    p.startswith(prefixes) or p == this_presubmit for p in affected_paths
  )


def CheckFlakeSuppressorCommonUnittests(input_api, output_api):
  """Runs unittests in the testing/flake_suppressor_common/ directory."""
  # Note: flake_suppressor_common depends on unexpected_passes_common.
  if not _ShouldRunCommonUnittests(
    input_api, ['flake_suppressor_common', 'unexpected_passes_common']
  ):
    return []
  return input_api.canned_checks.RunUnitTestsInDirectory(
    input_api,
    output_api,
    input_api.os_path.join(
      input_api.PresubmitLocalPath(), 'flake_suppressor_common'
    ),
    [r'^.+_unittest\.py$'],
    env=_GetTestingEnv(input_api),
  )


def CheckUnexpectedPassesCommonUnittests(input_api, output_api):
  """Runs unittests in the testing/unexpected_passes_common/ directory."""
  if not _ShouldRunCommonUnittests(input_api, ['unexpected_passes_common']):
    return []
  return input_api.canned_checks.RunUnitTestsInDirectory(
    input_api,
    output_api,
    input_api.os_path.join(
      input_api.PresubmitLocalPath(), 'unexpected_passes_common'
    ),
    [r'^.+_unittest\.py$'],
    env=_GetTestingEnv(input_api),
  )


def CheckPresubmitUnittests(input_api, output_api):
  """Runs unittests for testing/PRESUBMIT.py."""
  return input_api.canned_checks.RunUnitTestsInDirectory(
    input_api,
    output_api,
    input_api.PresubmitLocalPath(),
    [r'^PRESUBMIT_test\.py$'],
    env=_GetTestingEnv(input_api),
  )


def _GetPylintFilesToCheck(input_api):
  """Returns regex patterns identifying Python files to lint.

  If testing/PRESUBMIT.py, any root Python file under testing/, or
  input_api.no_diffs is set, returns None to lint all Python files under
  testing/. Otherwise, scopes linting to the affected subdirectories under
  testing/, or returns [] if no Python files are affected.
  """
  if input_api.no_diffs:
    return None

  local_root = input_api.PresubmitLocalPath()
  norm_root = input_api.os_path.normcase(input_api.os_path.abspath(local_root))
  this_presubmit = input_api.os_path.normcase(
    input_api.os_path.join(norm_root, 'PRESUBMIT.py')
  )

  affected_subdirs = set()
  has_root_py = False

  for f in input_api.AffectedFiles():
    abs_path = f.AbsoluteLocalPath()
    norm_path = input_api.os_path.normcase(input_api.os_path.abspath(abs_path))
    if norm_path == this_presubmit:
      return None

    if not norm_path.endswith('.py'):
      continue

    try:
      norm_rel = input_api.os_path.relpath(norm_path, norm_root)
    except ValueError:
      continue

    if (
      norm_rel != '.'
      and not norm_rel.startswith('..' + input_api.os_path.sep)
      and norm_rel != '..'
      and not input_api.os_path.isabs(norm_rel)
    ):
      # Extract from original case path to avoid case-sensitive regex failures.
      original_rel_path = input_api.os_path.relpath(abs_path, local_root)
      parts = original_rel_path.split(input_api.os_path.sep)
      if len(parts) > 1:
        affected_subdirs.add(parts[0])
      else:
        has_root_py = True

  if not affected_subdirs and not has_root_py:
    # No affected Python files in testing/; return empty list to skip.
    return []

  # If root Python files changed, check everything to ensure module consistency.
  if has_root_py:
    return None

  patterns = []
  for subdir in sorted(affected_subdirs):
    # Match both POSIX and Windows separators across OS regex engines.
    patterns.append(r'%s(?:/|\\).*\.py$' % input_api.re.escape(subdir))
  return patterns


def CheckPylint(input_api, output_api):
  """Runs pylint on affected subdirectories or all directory content."""
  files_to_check = _GetPylintFilesToCheck(input_api)
  if files_to_check == []:
    return []
  files_to_skip = input_api.DEFAULT_FILES_TO_SKIP
  chromium_src_path = _GetChromiumSrcPath(input_api)
  pylint_extra_paths = [
    input_api.os_path.join(chromium_src_path, *component)
    for component in PYLINT_PATHS_COMPONENTS
  ]
  if input_api.is_windows:
    # These scripts don't run on Windows and should not be linted on Windows -
    # trying to do so will lead to spurious errors.
    files_to_skip += ('xvfb.py', '.*host_info.py')
  pylint_checks = input_api.canned_checks.GetPylint(
    input_api,
    output_api,
    files_to_check=files_to_check,
    extra_paths_list=pylint_extra_paths,
    files_to_skip=files_to_skip,
    # TODO(crbug.com/355016915): Remove this directory-specific pylintrc
    # file as the default one gets its disable list cleaned up.
    pylintrc='pylintrc',
    version='3.2',
  )
  return input_api.RunTests(pylint_checks)


def CheckPatchFormatted(input_api, output_api):
  return input_api.canned_checks.CheckPatchFormatted(
    input_api,
    output_api,
    result_factory=output_api.PresubmitError,
  )
