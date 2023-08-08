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
    if (local_path.startswith('third_party' + input_api.os_path.sep) and
        not local_path.startswith('third_party' + input_api.os_path.sep +
                                  'blink' + input_api.os_path.sep) and
        not local_path.startswith('third_party' + input_api.os_path.sep +
                                  'boringssl' + input_api.os_path.sep) and
        not local_path.startswith('third_party' + input_api.os_path.sep +
                                  'closure_compiler' + input_api.os_path.sep +
                                  'externs' + input_api.os_path.sep) and
        not local_path.startswith('third_party' + input_api.os_path.sep +
                                  'closure_compiler' + input_api.os_path.sep +
                                  'interfaces' + input_api.os_path.sep) and
        not local_path.startswith('third_party' + input_api.os_path.sep +
                                  'feed_library' + input_api.os_path.sep) and
        not local_path.startswith('third_party' + input_api.os_path.sep +
                                  'ipcz' + input_api.os_path.sep) and
        # TODO(danakj): We should look for the README.chromium file in
        # third_party/rust/CRATE_NAME/vVERSION/.
        not local_path.startswith('third_party' + input_api.os_path.sep +
                                  'rust' + input_api.os_path.sep) and
        not local_path.startswith('third_party' + input_api.os_path.sep +
                                  'webxr_test_pages' + input_api.os_path.sep)):
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
  not_shipped_pattern = input_api.re.compile(
    r'^License File: NOT_SHIPPED\r?$',
    input_api.re.IGNORECASE | input_api.re.MULTILINE)
  license_android_compatible_pattern = input_api.re.compile(
    r'^License Android Compatible: (yes|no)\r?$',
    input_api.re.IGNORECASE | input_api.re.MULTILINE)

  for f in readmes:
    if 'D' in f.Action():
      _IgnoreIfDeleting(input_api, output_api, f, errors)
      continue

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
        'If the package is not versioned or the version is not known\n'
        'list the version as \'unknown\'.\n'
        'Check README.chromium.template for details.',
        [f]))
    if not release_pattern.search(contents):
      errors.append(output_api.PresubmitError(
        'Third party README files should contain a \'Security Critical\'\n'
        'field. This field specifies whether the package is built with\n'
        'Chromium. Check README.chromium.template for details.',
        [f]))
    license_match = license_pattern.search(contents)
    if not license_match:
      errors.append(output_api.PresubmitError(
        'Third party README files should contain a \'License\' field.\n'
        'This field specifies the license used by the package. Check\n'
        'README.chromium.template for details.',
        [f]))
    not_shipped_match = not_shipped_pattern.search(contents)
    android_compatible_match = (
        license_android_compatible_pattern.search(contents))
    if (not not_shipped_match and not android_compatible_match and
        not LicenseIsCompatibleWithAndroid(input_api, license_match.group(1))):
      errors.append(output_api.PresubmitPromptWarning(
        'Cannot determine whether specified license is compatible with\n' +
        'the Android licensing requirements. Please check that the license\n' +
        'name is spelled according to third_party/PRESUBMIT.py. Please see\n' +
        'README.chromium.template for details.',
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
