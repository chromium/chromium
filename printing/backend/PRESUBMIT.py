# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Presubmit script for the printing backend.

See https://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API.
"""

def _CheckForStringViewFromNullableIppApi(input_api, output_api):
  """
  Looks for all affected lines in CL where one constructs
  std::string_view from any ipp*() CUPS API call.
  Assumes over-broadly that all ipp*() calls can return NULL.
  Returns affected lines as a list of presubmit errors.
  """
  # Attempts to detect source lines like:
  # *   std::string_view foo = ippDoBar();
  # *   std::string_view foo(ippDoBar());
  string_view_re = input_api.re.compile(
      r"^.+(std::string_view)\s+\w+( = |\()ipp[A-Z].+$")
  violations = input_api.canned_checks._FindNewViolationsOfRule(
      lambda extension, line:
        not (extension in ("cc", "h") and string_view_re.search(line)),
      input_api, None)
  bulleted_violations = ["  * {}".format(entry) for entry in violations]

  if bulleted_violations:
    return [output_api.PresubmitError(
        ("Possible construction of std::string_view "
         "from CUPS IPP API (that can probably return NULL):\n{}").format(
             "\n".join(bulleted_violations))),]
  return []

def _CommonChecks(input_api, output_api):
  """Actual implementation of presubmits for the printing backend."""
  results = []
  results.extend(_CheckForStringViewFromNullableIppApi(input_api, output_api))
  return results

def CheckChangeOnUpload(input_api, output_api):
  """Mandatory presubmit entry point."""
  return _CommonChecks(input_api, output_api)

def CheckChangeOnCommit(input_api, output_api):
  """Mandatory presubmit entry point."""
  return _CommonChecks(input_api, output_api)
