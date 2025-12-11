# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import tempfile
from typing import Optional


def lint_wpt_root(input_api, output_api, repo_root: Optional[str] = None):
    """Run `wpt lint` against the specified directory."""
    repo_root = repo_root or input_api.PresubmitLocalPath()
    wpt_root = input_api.os_path.join(input_api.change.RepositoryRoot(),
                                      'third_party', 'wpt_tools', 'wpt')
    wpt_executable = input_api.os_path.join(wpt_root, 'wpt')

    paths = []
    for abs_path in input_api.AbsoluteLocalPaths():
        if abs_path.endswith(input_api.os_path.relpath(abs_path, repo_root)):
            paths.append(abs_path)
    if not paths:
        return []

    args = [
        input_api.python3_executable,
        wpt_executable,
        # Third-party packages are vended through vpython instead of plain
        # virtualenv.
        f'--venv={wpt_root}',
        '--skip-venv-setup',
        'lint',
        f'--repo-root={repo_root}',
        # To avoid false positives, do not lint files not upstreamed from
        # Chromium.
        '--ignore-glob=*-expected.txt',
        '--ignore-glob=*.ini',
        '--ignore-glob=*DIR_METADATA',
        '--ignore-glob=*OWNERS',
        '--ignore-glob=config.tmpl.json',
    ]
    # When uploading, only check changed files to keep linting fast. However,
    # when it's time to commit the CL, check all WPT files to catch dangling
    # references to deleted/renamed files.
    paths_name = None
    if input_api.is_committing:
        args.append('--all')
    else:
        # We have to set delete=False and then let the object go out of scope
        # so that the file can be opened by name on Windows.
        with tempfile.NamedTemporaryFile('w+', newline='', delete=False) as f:
            paths_name = f.name
            args.append(f'--paths-file={paths_name}')
            for path in paths:
                f.write(f'{path}\n')

    try:
        proc = input_api.subprocess.Popen(args,
                                          stdout=input_api.subprocess.PIPE,
                                          stderr=input_api.subprocess.PIPE)
        stdout, stderr = proc.communicate()
    finally:
        if paths_name:
            os.remove(paths_name)

    if proc.returncode != 0:
        return [
            output_api.PresubmitError('`wpt lint` failed:',
                                      long_text=(stdout + stderr).decode())
        ]
    return []
