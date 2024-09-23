#!/usr/bin/env vpython3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
""" Execute Audio / Video performance tests against a smart display device.
    This script needs to be executed from the build output folder, e.g.
    out/fuchsia/."""

import json
import logging
import os
import subprocess
import sys

TEST_SCRIPTS_ROOT = os.path.join(os.path.dirname(__file__), '..', '..',
                                 'build', 'fuchsia', 'test')
sys.path.append(TEST_SCRIPTS_ROOT)

from browser_runner import BrowserRunner
from chrome_driver_wrapper import ChromeDriverWrapper
from common import get_ffx_isolate_dir
from compatible_utils import running_unattended
from isolate_daemon import IsolateDaemon
from run_webpage_test import WebpageTestRunner, capture_devtools_addr


def run_test(proc: subprocess.Popen) -> None:
    host, port = capture_devtools_addr(proc, '/tmp')
    logging.warning('DevTools is now running on %s:%s', host, port)
    with ChromeDriverWrapper((host, port)) as driver:
        driver.get('http://www.google.com')
        if not running_unattended():
            input('Enter any message to stop:')


# TODO(crbug.com/40935291): Implement the tests.
def main() -> int:
    proc = subprocess.Popen([
        os.path.join(TEST_SCRIPTS_ROOT,
                     'run_test.py'), 'webpage', '--out-dir=.',
        '--browser=web-engine-shell', '--device', '--logs-dir=/tmp'
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
    # TODO(crbug.com/40935291): Currently the machine is not running a fuchsia
    # managed docker image, the FUCHSIA_NODENAME environment is not set.
    if 'FUCHSIA_NODENAME' not in os.environ:
        os.environ['FUCHSIA_NODENAME'] = 'fuchsia-ac67-8475-ee82'
    # Setting a temporary isolate daemon dir and share it with the webpage
    # runner.
    with IsolateDaemon.IsolateDir():
        logging.warning('ffx daemon is running in %s', get_ffx_isolate_dir())
        sys.exit(main())
