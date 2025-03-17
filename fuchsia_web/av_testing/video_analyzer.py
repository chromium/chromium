# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
""" Starts a local_video_analyzer process to analyze the recorded video
quality. Unless otherwise noted, all the functions return immediately if the
required binaries do not exist and write the output into av_sync_tests.LOG_DIR.
"""

import json
import logging
import os
import subprocess
import sys

# Copy to avoid cycle dependency.
LOG_DIR = os.environ.get('ISOLATED_OUTDIR', '/tmp')


sys.path.append(
    os.path.join(os.path.dirname(__file__), '..', '..', 'build', 'fuchsia',
                 'test'))

from repeating_log import RepeatingLog


def from_original_video(recorded: str, original: str) -> object:
    """ Analyzes the |recorded| video file by using the |original| as the
        reference, and returns the results as an json object. """
    binary = '/usr/local/cipd/local_analyzer/local_video_analyzer.par'
    assert os.path.isfile(binary)
    _, filename = os.path.split(original)
    output_dir = os.path.join(LOG_DIR, filename)
    os.mkdir(output_dir)

    with RepeatingLog('Waiting for local_video_analyzer.'):
        subprocess.run([
            binary, '--gid=', '--uid=', '--loas_pwd_fallback_in_corp',
            f'--ref_video_file={original}', f'--test_video_file={recorded}',
            f'--output_folder={output_dir}'
        ],
                       check=True)
    try:
        with open(os.path.join(output_dir, 'results.json'), 'r') as file:
            return json.load(file)
    except FileNotFoundError:
        logging.warning('No results.json file generated in %s', output_dir)
        return {}
