# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for content/.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

_WARN_AGAINST_DCHECK_PREFIXES = (
    'content/browser/renderer_host/navigation_request.h',
    'content/browser/renderer_host/navigation_request.cc',
    'content/browser/renderer_host/render_frame_host_android.cc',
    'content/browser/renderer_host/render_frame_host_csp_context.cc',
    'content/browser/renderer_host/render_frame_host_factory.cc',
    'content/browser/renderer_host/render_frame_host_impl.h',
    'content/browser/renderer_host/render_frame_host_impl.cc',
    'content/browser/renderer_host/render_frame_host_manager.h',
    'content/browser/renderer_host/render_frame_host_manager.cc',

    # TODO(crbug.com/497761255): Enable this for:
    # - content/browser/ (root files)
    # - content/browser/renderer_host/
    # - content/browser/loader/
    # - content/browser/network/
    # - content/browser/security/
)

def _WarnAgainstDCHECK(input_api, output_api):
  """
  Warn against adding new DCHECKs. CHECKS are preferred in these files, because
  they are cheap and cause an immediate crash, as opposed to a potential
  vulnerability that could be exploited in the wild.

  See go/check-content-navigation for details.
  """
  problems = []

  def FilterFile(affected_file):
    path = affected_file.LocalPath()
    for prefix in _WARN_AGAINST_DCHECK_PREFIXES:
      if path.startswith(prefix):
        return True
    return False

  dcheck_re = input_api.re.compile(r'\bDCHECK(?!_IS_ON\b)')
  for f in input_api.AffectedSourceFiles(FilterFile):
    for line_num, line in f.ChangedContents():
      if dcheck_re.search(line):
        problems.append(f'{f.LocalPath()}:{line_num}: {line.strip()}')

  if problems:
    return [output_api.PresubmitPromptWarning(
        'DCHECK is discouraged in this file. CHECKs are cheap and are '
        'preferred when possible.',
        problems)]
  return []

def CheckChangeOnUpload(input_api, output_api):
  return _WarnAgainstDCHECK(input_api, output_api)

def CheckChangeOnCommit(input_api, output_api):
  return _WarnAgainstDCHECK(input_api, output_api)
