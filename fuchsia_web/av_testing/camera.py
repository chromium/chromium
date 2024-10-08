# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
""" Starts an av_sync_record process to record the video from the camera. """

import logging
import os.path
import subprocess

from dataclasses import dataclass
from typing import Optional


@dataclass
class Parameters:
    fps: Optional[int] = None
    output_path: str = '/tmp'
    duration_sec: Optional[int] = None
    max_frames: Optional[int] = None
    serial_number: Optional[str] = None


def start(parameters: Parameters) -> None:
    """Starts an av_sync_record process to record the video from the camera.
    Executing of this function shouldn't be terminated as it would create a
    bad constructed mp4 file. If the recorder binary does not exist, the
    function returns immediately."""
    assert parameters.output_path
    BINARY = '/usr/local/cipd/av_sync_record/av_sync_record'
    if not os.path.isfile(BINARY):
        logging.warning(
            '%s is not found, camera capturing will be ignored. This should ' +
            'only happen on the dev environment without the camera.', BINARY)
        return
    args = [
        BINARY, '--gid=', '--uid=',
        f'--camera_info_path={parameters.output_path}/info.csv',
        f'--video_output={parameters.output_path}/video.mp4'
    ]
    if parameters.fps:
        args.append(f'--camera_fps={parameters.fps}')
    if parameters.duration_sec:
        args.append(f'--duration={parameters.duration_sec}s')
    if parameters.max_frames:
        args.append(f'--max_frames={parameters.max_frames}')
    if parameters.serial_number:
        args.append(f'--serial_number={parameters.serial_number}')
    logging.warning('Camera starts recording to %s', parameters.output_path)
    subprocess.run(args, check=True)
    logging.warning('Camera finishes recording.')
