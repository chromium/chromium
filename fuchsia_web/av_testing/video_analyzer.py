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

# Copy to avoid cycle dependency.
LOG_DIR = os.environ.get('ISOLATED_OUTDIR', '/tmp')


def from_original_video(recorded: str, original: str) -> object:
    """ Analyzes the |recorded| video file by using the |original| as the
        reference, and returns the results as an json object. """
    BINARY = '/usr/local/cipd/local_analyzer/local_video_analyzer.par'
    if not os.path.isfile(BINARY):
        logging.warning(
            '%s is not found, no video analysis result would be ' +
            'generated.', BINARY)
        return {}
    subprocess.run([
        BINARY, '--gid=', '--uid=', f'--ref_video_file={original}',
        f'--test_video_file={recorded}', f'--output_folder={LOG_DIR}'
    ],
                   check=True)
    with open(os.path.join(LOG_DIR, 'results.json'), 'r') as file:
        return json.load(file)
