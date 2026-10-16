# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit checks for //ios/chrome/test/data/policy

See https://www.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

import importlib.util
import sys

PRESUBMIT_VERSION = '2.0.0'


def CheckChange(input_api, output_api):
    old_sys_path = sys.path[:]
    results = []
    try:
        sys.path.append(input_api.change.RepositoryRoot())
        from build.ios import presubmit_support

        results += presubmit_support.CheckBundleData(
            input_api, output_api, 'policy_test_bundle_data'
        )
    finally:
        sys.path = old_sys_path
    return results


def CheckPolicyTestCasePlatforms(input_api, output_api):
    '''Runs CheckPolicyTestCasePlatforms from components/policy/PRESUBMIT.py,
    which only runs for changes that touch components/policy.'''
    policy_dir = input_api.os_path.join('components', 'policy')
    if any(
        f.LocalPath().startswith(policy_dir + input_api.os_path.sep)
        for f in input_api.change.AffectedFiles()
    ):
        return []

    root = input_api.change.RepositoryRoot()
    old_sys_path = sys.path[:]
    try:
        # components/policy/PRESUBMIT.py extends sys.path relative to its own
        # directory, so add the paths that it needs from here.
        sys.path += [
            input_api.os_path.join(root, policy_dir, 'resources'),
            input_api.os_path.join(root, 'third_party'),
            input_api.os_path.join(root, 'third_party', 'pyjson5', 'src'),
        ]
        spec = importlib.util.spec_from_file_location(
            'components_policy_presubmit',
            input_api.os_path.join(root, policy_dir, 'PRESUBMIT.py'),
        )
        presubmit = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(presubmit)
        return presubmit.CheckPolicyTestCasePlatforms(input_api, output_api)
    finally:
        sys.path = old_sys_path
