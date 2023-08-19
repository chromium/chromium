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
  url_pattern = input_api.re.compile(
    r'^URL: (.+)\r?$',
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
  shipped_pattern = input_api.re.compile(
    r'^Shipped: (yes|no)\r?$',
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
    if not url_pattern.search(contents):
      # TODO: This should be changed to `PresubmitError` once all existing
      # README.chromium files contain a URL field. Until then it needs to be a
      # warning to not make the linux-presubmit CI job fail.
      errors.append(output_api.PresubmitPromptWarning(
        'Third party README files should contain a \'URL\' field.\n'
        'This field specifies the URL where the package lives. Check\n'
        'README.chromium.template for details.',
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
    # TODO: The check for this field should be upgraded to PresubmitError
    # when the changes to all files transitioned away from NOT_SHIPPED
    # to the new field.
    shipped_match = shipped_pattern.search(contents)
    if not shipped_match:
      errors.append(output_api.PresubmitPromptWarning(
        'Third party README files should contain a \'Shipped\' field.\n'
        'This field specifies whether the package is shipped as part of\n'
        'a release. Check README.chromium.template for details.',
        [f]))
    # Default to Shipped if not specified. This will flag a license check or
    # be overwritten if the README is still using the deprecated NOT_SHIPPED
    # value.
    is_shipped = (shipped_match is None) or ("yes" in shipped_match.group(1))
    # Check for dependencies using the old NOT_SHIPPED pattern and issue a warning
    deprecated_not_shipped = old_shipped_pattern.search(contents)
    if deprecated_not_shipped:
        is_shipped = False
        errors.append(output_api.PresubmitPromptWarning(
          'Using NOT_SHIPPED in the \'License File:\' is deprecated\n'
          'behavior. Please use the \'Shipped\' field instead. Refer to\n'
          'README.chromium.template for more details.',
          [f]))
    android_compatible_match = (
        license_android_compatible_pattern.search(contents))
    if (is_shipped and not android_compatible_match and
        not LicenseIsCompatibleWithAndroid(input_api, license_match.group(1))):
      errors.append(output_api.PresubmitPromptWarning(
        'Cannot determine whether specified license is compatible with\n' +
        'the Android licensing requirements. Please check that the license\n' +
        'name is spelled according to third_party/PRESUBMIT.py. Please see\n' +
        'README.chromium.template for details.',
        [f]))
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
