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
import version
from chrome_driver_wrapper import ChromeDriverWrapper
from common import get_build_info, get_ffx_isolate_dir, get_free_local_port
from isolate_daemon import IsolateDaemon
from run_webpage_test import capture_devtools_addr


HTTP_SERVER_PORT = get_free_local_port()
LOG_DIR = os.environ.get('ISOLATED_OUTDIR', '/tmp')
TEMP_DIR = os.environ.get('TMPDIR', '/tmp')

VIDEOS =[
    '720p24fpsH264_gangnam_sync.mp4',
    '720p24fpsVP9_gangnam_sync.webm',
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
    return result


def run_video_perf_test(file: str, driver: ChromeDriverWrapper,
                        host: str) -> None:
    driver.get(f'http://{host}:{HTTP_SERVER_PORT}/video.html?file={file}')
    camera_params = parameters_of(file)
    original_video = os.path.join(server.VIDEO_DIR, file)
    # Ensure the original video won't be overwritten.
    assert camera_params.video_file != original_video
    with StartProcess(camera.start, [camera_params], False):
        video = driver.find_element_by_id('video')
        video.click()
    # Video playback should finish almost within the same time as the camera
    # recording, and this check is only necessary to indicate a very heavy
    # network laggy and buffering.
    # TODO(crbug.com/40935291): May need to adjust the strategy here, the
    # final frame / barcode is considered laggy and drops the score.
    with monitors.time_consumption(file, 'video_perf', 'playback', 'laggy'):
        while not driver.execute_script('return arguments[0].ended;', video):
            time.sleep(1)
    logging.warning('Video %s finished', file)

    results = video_analyzer.from_original_video(camera_params.video_file,
                                                 original_video)

    def record(key: str) -> None:
        # If the video_analyzer does not generate any result, treat it as an
        # error and use the default value to filter them out instead of failing
        # the tests.
        # TODO(crbug.com/40935291): Revise the default value for errors.
        monitors.average(file, key).record(results.get(key, -128))

    record('smoothness')
    record('freezing')
    record('dropped_frame_count')
    record('total_frame_count')
    record('dropped_frame_percentage')
    logging.warning('Video analysis result of %s: %s', file, results)

    # Move the info csv to the cas-output for debugging purpose. Video files
    # are huge and will be ignored.
    shutil.move(camera_params.info_file, LOG_DIR)


def run_test(proc: subprocess.Popen) -> None:
    device, port = capture_devtools_addr(proc, LOG_DIR)
    logging.warning('DevTools is now running on %s:%s', device, port)
    # webpage test may update the fuchsia version, so get build_info after its
    # finish.
    logging.warning('Chrome version %s %s', version.chrome_version_str(),
                    version.git_revision())
    build_info = get_build_info()
    logging.warning('Fuchsia build info %s', build_info)
    monitors.tag(version.chrome_version_str(), build_info.version,
                 version.chrome_version_str() + '/' + build_info.version)
    # Replace the last byte to 1, by default it's the ip address of the host
    # machine being accessible on the device.
    host = '.'.join(device.split('.')[:-1] + ['1'])
    proxy_host = os.environ.get('GCS_PROXY_HOST')
    if proxy_host:
        # This is a hacky way to get the ip address of the host machine
        # being accessible on the device by the fuchsia managed docker image.
        host = proxy_host + '0'
    with ChromeDriverWrapper((device, port)) as driver:
        for file in VIDEOS:
            run_video_perf_test(file, driver, host)


def main() -> int:
    proc = subprocess.Popen([
        os.path.join(TEST_SCRIPTS_ROOT,
                     'run_test.py'), 'webpage', '--out-dir=.',
        '--browser=web-engine-shell', '--device', f'--logs-dir={LOG_DIR}'
    ],
                            env={
                                **os.environ, 'CHROME_HEADLESS': '1'
                            })
    try:
        run_test(proc)
        return 0
    except:
        # Do not dump the results unless the tests were passed successfully to
        # avoid polluting the metrics.
        monitors.clear()
        raise
    finally:
        proc.terminate()
        proc.wait()
        # May need to merge with the existing file created by run_test.py
        # webpage process.
        monitors.dump(os.path.join(LOG_DIR, 'invocations'))


if __name__ == '__main__':
    logging.warning('Running %s with env %s', sys.argv, os.environ)
    # Setting a temporary isolate daemon dir and share it with the webpage
    # runner.
    with StartProcess(server.start, [HTTP_SERVER_PORT], True), \
         IsolateDaemon.IsolateDir():
        logging.warning('ffx daemon is running in %s', get_ffx_isolate_dir())
        sys.exit(main())
