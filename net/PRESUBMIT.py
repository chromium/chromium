# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Top-level presubmit for //net

See https://www.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

PRESUBMIT_VERSION = '2.0.0'


# This is similar to the standard runtime/int check but allows exceptions for
# platform-specific code, as platform APIs often use types that are disallowed
# by the style guide.
def CheckNoShortKeywordInNet(input_api, output_api):
    """
    Prohibit the use of the "short" keyword in platform-independent //net code.
    """
    problems = []
    platform_suffixes = ('_android', '_apple', '_chromeos', '_fuchsia', '_ios',
                         '_linux', '_mac', '_openbsd', '_posix', '_win')
    platform_subdirs = {
        'android', 'apple', 'chromeos', 'fuchsia', 'ios', 'linux', 'mac',
        'openbsd', 'posix', 'win'
    }

    for f in input_api.AffectedSourceFiles(None):
        local_path = f.LocalPath().replace('\\', '/')
        if not local_path.startswith('net/'):
            continue
        if not local_path.endswith(('.cc', '.h')):
            continue

        path_parts = local_path.split('/')
        if any(part in platform_subdirs for part in path_parts[:-1]):
            continue

        basename = input_api.os_path.splitext(path_parts[-1])[0]
        if basename.endswith(platform_suffixes):
            continue

        for line_number, line in f.ChangedContents():
            if input_api.re.search(r'\bshort\b', line):
                problems.append(
                    f'{local_path}:{line_number}\n    {line.strip()}')

    if problems:
        return [
            output_api.PresubmitError(
                'Do not use the "short" keyword in platform-independent //net '
                'code. Use int16_t or uint16_t instead.', problems)
        ]
    return []


def CheckChange(input_api, output_api):
    import sys
    old_sys_path = sys.path[:]
    results = []
    try:
        sys.path.append(input_api.change.RepositoryRoot())
        from build.ios import presubmit_support
        results += presubmit_support.CheckBundleData(
            input_api,
            output_api,
            'data/test_bundle_data',
            globroot='.')
        results += presubmit_support.CheckBundleData(
            input_api,
            output_api,
            'data/test_support_bundle_data',
            globroot='.')
        results += CheckNoShortKeywordInNet(input_api, output_api)
    finally:
        sys.path = old_sys_path
    return results
