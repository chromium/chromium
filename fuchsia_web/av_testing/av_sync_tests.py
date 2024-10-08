#!/usr/bin/env vpython3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
""" Executes Audio / Video performance tests against a smart display device.
    This script needs to be executed from the build output folder, e.g.
    out/fuchsia/."""

import json
import logging
import multiprocessing
import os
import re
import subprocess
import sys
import time

from contextlib import AbstractContextManager

import camera
import server

TEST_SCRIPTS_ROOT = os.path.join(os.path.dirname(__file__), '..', '..',
                                 'build', 'fuchsia', 'test')
sys.path.append(TEST_SCRIPTS_ROOT)

from browser_runner import BrowserRunner
from chrome_driver_wrapper import ChromeDriverWrapper
from common import get_ffx_isolate_dir, get_free_local_port
from compatible_utils import running_unattended
from isolate_daemon import IsolateDaemon
from run_webpage_test import WebpageTestRunner, capture_devtools_addr


HTTP_SERVER_PORT = get_free_local_port()
LOG_DIR = os.environ.get('ISOLATED_OUTDIR', '/tmp')

VIDEOS = {'720p24fpsVP9_gangnam_sync.webm': {'length': 251}}


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
    result.output_path = LOG_DIR
    # Record several extra seconds to cover the video buffering.
    result.duration_sec = VIDEOS[file]['length'] + 5

    # Testing purpose now. The video will later be uploaded to the GCS bucket
    # for analysis rather than storing in the cas output.
    result.duration_sec = 10

    return result


def run_test(proc: subprocess.Popen) -> None:
    device, port = capture_devtools_addr(proc, LOG_DIR)
    logging.warning('DevTools is now running on %s:%s', device, port)
    # Replace the last byte to 1, by default it's the ip address of the host
    # machine being accessible on the device.
    host = '.'.join(device.split('.')[:-1] + ['1'])
    with ChromeDriverWrapper((device, port)) as driver:
        file = '720p24fpsVP9_gangnam_sync.webm'
        if running_unattended():
            param = f'file={file}'
            proxy_host = os.environ.get('GCS_PROXY_HOST')
            if proxy_host:
                # This is a hacky way to get the ip address of the host machine
                # being accessible on the device.
                host = proxy_host + '0'
        else:
            param = 'local'
        driver.get(f'http://{host}:{HTTP_SERVER_PORT}/video.html?{param}')
        with StartProcess(camera.start, [parameters_of(file)], False):
            video = driver.find_element_by_id('video')
            video.click()
            # Video playback should finish almost within the same time as the
            # camera recording, and this check may only be necessary to indicate
            # a very heavy network laggy and buffering.
            # TODO(crbug.com/40935291): Consider a way to record the extra time
            # over the camera recording.
            while not driver.execute_script('return arguments[0].ended;',
                                            video):
                time.sleep(1)
            logging.warning('Video finished')


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
    finally:
        proc.terminate()
        proc.wait()


if __name__ == '__main__':
    logging.warning('Running %s with env %s', sys.argv, os.environ)
    # TODO(crbug.com/40935291): Currently the machine is not running a fuchsia
    # managed docker image, the FUCHSIA_NODENAME environment is not set.
    if 'FUCHSIA_NODENAME' not in os.environ:
        os.environ['FUCHSIA_NODENAME'] = 'fuchsia-ac67-8475-ee82'
    # Setting a temporary isolate daemon dir and share it with the webpage
    # runner.
    with StartProcess(server.start, [HTTP_SERVER_PORT], True), \
         IsolateDaemon.IsolateDir():
        logging.warning('ffx daemon is running in %s', get_ffx_isolate_dir())
        sys.exit(main())
