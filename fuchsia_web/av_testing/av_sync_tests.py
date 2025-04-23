#!/usr/bin/env vpython3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
""" Executes Audio / Video performance tests against a smart display device.
    This script needs to be executed from the build output folder, e.g.
    out/fuchsia/."""

import logging
import multiprocessing
import os
import random
import shutil
import subprocess
import sys
import time

from contextlib import AbstractContextManager
from pathlib import Path

import camera
import server
import video_analyzer

TEST_SCRIPTS_ROOT = os.path.join(os.path.dirname(__file__), '..', '..',
                                 'build', 'fuchsia', 'test')
sys.path.append(TEST_SCRIPTS_ROOT)

import monitors
import perf_trace
import version
from chrome_driver_wrapper import ChromeDriverWrapper
from common import get_build_info, get_free_local_port, get_ip_address, ssh_run
from repeating_log import RepeatingLog


HTTP_SERVER_PORT = get_free_local_port()
LOG_DIR = os.environ.get('ISOLATED_OUTDIR', '/tmp')
TEMP_DIR = os.environ.get('TMPDIR', '/tmp')

# Note, AV1 is not supported on smart display.
# Physical capacity:
#   Sherlock: 1280x800 60fps
#   Astro and Nelson: 1024x600 60fps
# Target video quality:
#   Sherlock: 720p60fps or upper.
#   Astro and Nelson: 720p30fps or upper.

# Keep names alphabetical order.
VIDEOS = [
    '480p30fpsH264_foodmarket_sync.mp4',
    '480p30fpsVP9_foodmarket_sync.webm',
    '480p60fpsH264_boat_sync.mp4',
    '480p60fpsHEVC_boat_sync.mp4',
    '480p60fpsVP9_boat_sync.mp4',
    '720p24fpsH264_gangnam_sync.mp4',
    '720p24fpsVP9_gangnam_sync.webm',
    '720p60fpsH264_boat_yt_sync.mp4',
    '720p60fpsHEVC_boat_sync.mp4',
    '720p60fpsVP9_boat_yt_sync.webm',
]

# These videos are stretch goal of sherlock, worth trying.
HIGH_PERF_VIDEOS = [
    '1080p60fpsH264_boat_yt_sync.mp4',
    '1080p60fpsHEVC_boat_sync.mp4',
    '1080p60fpsVP9_boat_yt_sync.webm',
]


class StartProcess(AbstractContextManager):
    """Starts a multiprocessing.Process."""

    def __init__(self, target, args, terminate: bool):
        self._proc = multiprocessing.Process(target=target, args=args)
        self._terminate = terminate

    def __enter__(self):
        self._proc.start()

    def __exit__(self, exc_type, exc_value, traceback):
        if self._terminate:
            self._proc.terminate()
        self._proc.join()
        if not self._terminate:
            assert self._proc.exitcode == 0


def parameters_of(file: str) -> camera.Parameters:
    result = camera.Parameters()
    result.file = file
    # Recorded videos are huge, instead of placing them into the LOG_DIR
    # which will be uploaded to CAS output, use TEMP_DIR provided by
    # luci-swarming to be cleaned up automatically after the test run.
    result.output_path = TEMP_DIR
    # max_frames controls the maximum number of umcompressed frames in the
    # memory. And if the number of uncompressed frames reaches the max_frames,
    # the basler camera recorder will fail. The camera being used may use up to
    # 388,800 bytes per frame, or ~467MB for 1200 frames, setting the max_frames
    # to 1200 would avoid OOM. Also limit the fps to 120 to ensure the processor
    # of the host machine is capable to compress the camera stream on time
    # without exhausting the in-memory frame queue.
    # TODO(crbug.com/40935291): These two values need to be adjusted to reach
    # 300fps for a more accurate analysis result when the test is running on
    # more performant host machines.
    result.max_frames = 1200
    result.fps = 120
    # All the videos now being used are 30s long.
    result.duration_sec = 30
    # Expect the camera serial number to be set, but now the hosts running in
    # media lab have only one camera per host and need no serial number.
    # TODO(crbug.com/391663618): Remove the condition once all the hosts are
    # migrated into chrome lab.
    if os.environ.get('CAMERA_SERIAL_NUMBER'):
        result.serial_number = os.environ['CAMERA_SERIAL_NUMBER']
    return result


def _wait_js_condition(driver: ChromeDriverWrapper, element,
                       condition: str) -> bool:
    """Waits a condition on the element once a second for at most 30 seconds,
       returns True if the condition met."""
    start = time.time()
    while not driver.execute_script(f'return arguments[0].{condition};',
                                    element):
        if time.time() - start >= 30:
            return False
        time.sleep(1)
    return True


def run_video_perf_test(file: str, driver: ChromeDriverWrapper,
                        host: str) -> None:
    perf_trace.start()
    driver.get(f'http://{host}:{HTTP_SERVER_PORT}/video.html?file={file}')
    camera_params = parameters_of(file)
    original_video = os.path.join(server.VIDEO_DIR, file)
    # Ensure the original video won't be overwritten.
    assert camera_params.video_file != original_video
    video = driver.find_element_by_id('video')
    with monitors.time_consumption(file, 'video_perf', 'playback', 'loading'), \
         RepeatingLog(f'Waiting for video {file} to be loaded.'):
        if not _wait_js_condition(driver, video, 'readyState >= 2'):
            logging.warning(
                '%s may never be loaded, still go ahead to play it.', file)
            monitors.average(file, 'video_perf', 'playback',
                             'failed_to_load').record(1)
    with StartProcess(camera.start, [camera_params], False):
        video.click()
    # Video playback should finish almost within the same time as the camera
    # recording, and this check is only necessary to indicate a very heavy
    # network laggy and buffering.
    # TODO(crbug.com/40935291): May need to adjust the strategy here, the
    # final frame / barcode is considered laggy and drops the score.
    with monitors.time_consumption(file, 'video_perf', 'playback', 'laggy'), \
         RepeatingLog(f'Waiting for video {file} playback to finish.'):
        if not _wait_js_condition(driver, video, 'ended'):
            logging.warning('%s may never finish', file)
            monitors.average(file, 'video_perf', 'playback',
                             'never_finish').record(1)
    logging.warning('Video %s finished', file)
    perf_trace.stop(file)

    results = video_analyzer.from_original_video(camera_params.video_file,
                                                 original_video)

    def record(key: str) -> None:
        # If the video_analyzer does not generate any result, treat it as an
        # error and use the default value to filter them out instead of failing
        # the tests.
        # TODO(crbug.com/40935291): Revise the default value for errors.
        monitors.average(file, 'video_perf', key).record(results.get(key, -128))

    record('smoothness')
    record('freezing')
    record('dropped_frame_count')
    record('total_frame_count')
    record('dropped_frame_percentage')
    logging.warning('Video analysis result of %s: %s', file, results)

    # Move the info csv to the cas-output for debugging purpose. Video files
    # are huge and will be ignored.
    shutil.move(camera_params.info_file, LOG_DIR)


def main() -> int:
    try:
        with ChromeDriverWrapper() as driver:
            # webpage test may update the fuchsia version, so get build_info
            # after its finish.
            logging.warning('Chrome version %s %s',
                            version.chrome_version_str(),
                            version.git_revision())
            build_info = get_build_info()
            logging.warning('Fuchsia build info %s', build_info)
            monitors.tag(
                version.chrome_version_str(), build_info.version,
                version.chrome_version_str() + '/' + build_info.version)
            # TODO(crbug.com/391663618): Remove the condition once all the hosts
            # are migrated into chrome lab.
            host = '.'.join(get_ip_address(os.environ.get('FUCHSIA_NODENAME'),
                                           ipv4_only=True).
                            exploded.split('.')[:-1] +
                            [os.environ.get('CAMERA_SERIAL_NUMBER') and
                             '10' or  # In chrome lab, the host is at .10.
                             '1'])    # In media lab, the host is at .1.

            # Waiting for a change like https://crrev.com/c/6063979 to loose the
            # size limitation of the invocation which triggers an upload error
            # when too many metrics are included.
            # Before that, randomly select two of the videos to test so that we
            # will have sufficient coverage after multiple runs.
            # It also helps to reduce the time cost of each swarming job and
            # ensures the devices can be restarted after several videos.
            candidates = set(VIDEOS)
            # Run extra 1080p tests on sherlock.
            if build_info.board == 'sherlock':
                candidates.update(HIGH_PERF_VIDEOS)
            for file in random.sample(list(candidates), 2):
                run_video_perf_test(file, driver, host)
        return 0
    except:
        # Do not dump the results unless the tests were passed successfully to
        # avoid polluting the metrics.
        monitors.clear()
        raise
    finally:
        # May need to merge with the existing file created by run_test.py
        # webpage process.
        monitors.dump(os.path.join(LOG_DIR, 'invocations'))


if __name__ == '__main__':
    logging.warning('Running %s with env %s', sys.argv, os.environ)
    with StartProcess(server.start, [HTTP_SERVER_PORT], True):
        sys.exit(main())
