# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import tempfile
from typing import Optional


def lint_wpt_root(input_api, output_api, repo_root: Optional[str] = None):
    """Run `blink_tool.py lint-wpt` against the specified directory."""
    repo_root = repo_root or input_api.PresubmitLocalPath()
    tool_path = input_api.os_path.join(input_api.os_path.dirname(__file__),
                                       input_api.os_path.pardir,
                                       input_api.os_path.pardir,
                                       'blink_tool.py')

    # TODO(crbug.com/1406669): After switching to wptrunner, changing a test
    # file should also lint its corresponding metadata file, if any, because the
    # test-side change may invalidate the metadata file's contents. For example,
    # removing a test variant will orphan its expectations, which the linter
    # should flag for cleanup.
    paths = []
    for abs_path in input_api.AbsoluteLocalPaths():
        # For now, skip checking metadata files in `presubmit --{files,all}` for
        # the invalidation reason mentioned above.
        if input_api.no_diffs and abs_path.endswith('.ini'):
            continue
        if abs_path.endswith(input_api.os_path.relpath(abs_path, repo_root)):
            paths.append(abs_path)

    # Without an explicit file list, `lint-wpt` will lint all files in the root,
    # which is slow.
    if not paths:
        return []

    # We have to set delete=False and then let the object go out of scope so
    # that the file can be opened by name on Windows.
    with tempfile.NamedTemporaryFile('w+', newline='', delete=False) as f:
        for path in paths:
            f.write('%s\n' % path)
        paths_name = f.name
    args = [
        input_api.python3_executable,
        tool_path,
        'lint-wpt',
        '--repo-root=%s' % repo_root,
        # To avoid false positives, do not lint files not upstreamed from
        # Chromium.
        '--ignore-glob=*-expected.txt',
        '--ignore-glob=*DIR_METADATA',
        '--ignore-glob=*OWNERS',
        '--ignore-glob=config.json',
        '--paths-file=%s' % paths_name,
    ]

    proc = input_api.subprocess.Popen(args,
                                      stdout=input_api.subprocess.PIPE,
                                      stderr=input_api.subprocess.PIPE)
    stdout, stderr = proc.communicate()
    os.remove(paths_name)

    if proc.returncode != 0:
        return [
            output_api.PresubmitError('`blink_tool.py lint-wpt` failed:',
                                      long_text=(stdout + stderr).decode())
        ]
    return []
