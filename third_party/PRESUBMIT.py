# Copyright 2011 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

PRESUBMIT_VERSION = '2.0.0'

ANDROID_ALLOWED_LICENSES = [
  'A(pple )?PSL 2(\.0)?',
  'Android Software Development Kit License',
  'Apache( License)?,?( Version)? 2(\.0)?',
  '(New )?([23]-Clause )?BSD( [23]-Clause)?( with advertising clause)?',
  'GNU Lesser Public License',
  'L?GPL ?v?2(\.[01])?( or later)?( with the classpath exception)?',
  '(The )?MIT(/X11)?(-like)?( License)?',
  'MPL 1\.1 ?/ ?GPL 2(\.0)? ?/ ?LGPL 2\.1',
  'MPL 2(\.0)?',
  'Microsoft Limited Public License',
  'Microsoft Permissive License',
  'Public Domain',
  'Python',
  'SIL Open Font License, Version 1.1',
  'SGI Free Software License B',
  'Unicode, Inc. License',
  'University of Illinois\/NCSA Open Source',
  'X11',
  'Zlib',
]


def LicenseIsCompatibleWithAndroid(input_api, license):
  regex = '^(%s)$' % '|'.join(ANDROID_ALLOWED_LICENSES)
  tokens = \
    [x.strip() for x in input_api.re.split(' and |,', license) if len(x) > 0]
  has_compatible_license = False
  for token in tokens:
    if input_api.re.match(regex, token, input_api.re.IGNORECASE):
      has_compatible_license = True
      break
  return has_compatible_license


def CheckThirdPartyMetadataFiles(input_api, output_api):
  """Checks that third party metadata files are correctly formatted
  and valid.
  """
  def readme_filter(f):
    local_path = f.LocalPath()

    # Limit to README.chromium files within //third_party/.
    if (not local_path.endswith('README.chromium')
        or not local_path.startswith('third_party' + input_api.os_path.sep)):
      return False

    # Some folders are currently exempt from being checked.
    skip_dirs = (
      ('third_party', 'blink'),
      ('third_party', 'boringssl'),
      ('third_party', 'closure_compiler', 'externs'),
      ('third_party', 'closure_compiler', 'interfaces'),
      ('third_party', 'feed_library'),
      ('third_party', 'ipcz'),
      ('third_party', 'jni_zero'),
      # TODO(danakj): We should look for the README.chromium file in
      # third_party/rust/CRATE_NAME/vVERSION/.
      ('third_party', 'rust'),
      ('third_party', 'webxr_test_pages'),
    )
    for path in skip_dirs:
      prefix = ''.join([dir_name + input_api.os_path.sep for dir_name in path])
      if local_path.startswith(prefix):
        return False

    return True

  return input_api.canned_checks.CheckChromiumDependencyMetadata(
      input_api, output_api, file_filter=readme_filter)


def CheckThirdPartyReadmesUpdated(input_api, output_api):
  """
  Checks to make sure that README.chromium files are properly updated
  when dependencies in third_party are modified.
  """
  readmes = []
  files = []
  errors = []
  for f in input_api.AffectedFiles():
    local_path = f.LocalPath()
    if input_api.os_path.dirname(local_path) == 'third_party':
      continue
    exclusions = [
      'third_party/android_deps/',
      'third_party/blink/',
      'third_party/boringssl/',
      'third_party/closure_compiler/externs/',
      'third_party/closure_compiler/interfaces/',
      'third_party/feed_library/',
      'third_party/ipcz/',
      'third_party/jni_zero/',
      # TODO(danakj): We should look for the README.chromium file in
      # third_party/rust/CRATE_NAME/vVERSION/.
      'third_party/rust/',
      'third_party/webxr_test_pages/',
    ]
    exclusions = [e.replace('/', input_api.os_path.sep) for e in exclusions]
    if (local_path.startswith('third_party' + input_api.os_path.sep) and
        not any(local_path.startswith(prefix) for prefix in exclusions)):
      files.append(f)
      if local_path.endswith("README.chromium"):
        readmes.append(f)
  if files and not readmes:
    errors.append(output_api.PresubmitPromptWarning(
       'When updating or adding third party code the appropriate\n'
       '\'README.chromium\' file should also be updated with the correct\n'
       'version and package information.', files))
  if not readmes:
    return errors

  name_pattern = input_api.re.compile(
    r'^Name: [a-zA-Z0-9_\-\. \(\)]+\r?$',
    input_api.re.IGNORECASE | input_api.re.MULTILINE)
  shortname_pattern = input_api.re.compile(
    r'^Short Name: [a-zA-Z0-9_\-\.]+\r?$',
    input_api.re.IGNORECASE | input_api.re.MULTILINE)
  version_pattern = input_api.re.compile(
    r'^Version: [a-zA-Z0-9_\-\+\.:/]+\r?$',
    input_api.re.IGNORECASE | input_api.re.MULTILINE)
  release_pattern = input_api.re.compile(
    r'^Security Critical: (yes|no)\r?$',
    input_api.re.IGNORECASE | input_api.re.MULTILINE)
  license_pattern = input_api.re.compile(
    r'^License: (.+)\r?$',
    input_api.re.IGNORECASE | input_api.re.MULTILINE)
  # Using NOT_SHIPPED in a License File field is deprecated.
  # Please use the 'Shipped' field instead.
  old_shipped_pattern = input_api.re.compile(
    r'^License File: NOT_SHIPPED\r?$',
    input_api.re.IGNORECASE | input_api.re.MULTILINE)
  license_file_pattern = input_api.re.compile(
    r'^License File: (.+)\r?$',
    input_api.re.IGNORECASE | input_api.re.MULTILINE)
  # Matches both 'Shipped' (preferred) and
  # 'Shipped in Chromium' (for deps that are also standalone projects).
  shipped_pattern = input_api.re.compile(
    r'^Shipped(?: in Chromium)?: (yes|no)\r?$',
    input_api.re.IGNORECASE | input_api.re.MULTILINE)
  for f in readmes:
    if 'D' in f.Action():
      _IgnoreIfDeleting(input_api, output_api, f, errors)
      continue

    # TODO(aredulla): Delete this rudimentary README.chromium checking
    # once full metadata validation is enforced. The presubmit check
    # above (CheckThirdPartyMetadataFiles) will return only warnings
    # until all existing issues have been addressed. To prevent third
    # party metadata degrading while the data quality is being uplifted,
    # the below content checking for README.chromium files that return
    # presubmit errors will be retained.
    #
    # Note: This may result in a presubmit error from below being
    # repeated as a presubmit warning.
    contents = input_api.ReadFile(f)
    if (not shortname_pattern.search(contents)
        and not name_pattern.search(contents)):
      errors.append(output_api.PresubmitError(
        'Third party README files should contain either a \'Short Name\' or\n'
        'a \'Name\' which is the name under which the package is\n'
        'distributed. Check README.chromium.template for details.',
        [f]))
    if not version_pattern.search(contents):
      errors.append(output_api.PresubmitError(
        'Third party README files should contain a \'Version\' field.\n'
        'If the package is not versioned, list the version as \'N/A\'\n'
        'and provide at least the revision or package update date.\n'
        'Check README.chromium.template for details.',
        [f]))
    if not release_pattern.search(contents):
      errors.append(output_api.PresubmitError(
        'Third party README files should contain a \'Security Critical\'\n'
        'field. This field specifies the security impact of vulnerabilities.\n'
        'Check README.chromium.template for details.',
        [f]))
    license_match = license_pattern.search(contents)
    if not license_match:
      errors.append(output_api.PresubmitError(
        'Third party README files should contain a \'License\' field.\n'
        'This field specifies the license used by the package. Check\n'
        'README.chromium.template for details.',
        [f]))

    shipped_match = shipped_pattern.search(contents)
    # Default to Shipped if not specified. This will flag a license check or
    # be overwritten if the README is still using the deprecated NOT_SHIPPED
    # value.
    is_shipped = (shipped_match is None) or ("yes" in shipped_match.group(1))
    # Check for dependencies using the old NOT_SHIPPED pattern and issue a warning
    deprecated_not_shipped = old_shipped_pattern.search(contents)
    if deprecated_not_shipped:
        is_shipped = False
    license_file_match = license_file_pattern.search(contents)
    if is_shipped and not license_file_match:
      errors.append(output_api.PresubmitError(
        'Packages marked as shipped must provide a path to a license file.\n'
        'Check README.chromium.template for details.',
        [f]))
  return errors


def _IgnoreIfDeleting(input_api, output_api, affected_file, errors):
  third_party_dir = input_api.os_path.dirname(affected_file.LocalPath()) + \
    os.path.sep
  for f in input_api.AffectedFiles():
    if f.LocalPath().startswith(third_party_dir):
      if 'D' not in f.Action():
        errors.append(output_api.PresubmitError(
          'Third party README should only be removed when the whole\n'
          'directory is being removed.\n', [f, affected_file]))
