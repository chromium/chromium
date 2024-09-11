#!/usr/bin/env vpython3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
""" Execute Audio / Video performance tests against a smart display device.
    This script needs to be executed from the build output folder, e.g.
    out/fuchsia/."""

import os
import subprocess
import sys
import time

TEST_SCRIPTS_ROOT = os.path.join(os.path.dirname(__file__), '..', '..',
                                 'build', 'fuchsia', 'test')
sys.path.append(TEST_SCRIPTS_ROOT)

from browser_runner import BrowserRunner
from compatible_utils import running_unattended
from run_webpage_test import WebpageTestRunner, capture_devtools_port

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
    port = capture_devtools_port(proc, '/tmp')
    print('DevTools is now running on ' + str(port))
    if not running_unattended():
        print('Sleep 60 seconds before shutting it down')
        time.sleep(60)
    proc.terminate()
    proc.wait()
    return 0


if __name__ == '__main__':
    # TODO(crbug.com/40935291): Currently the machine is not running a fuchsia
    # managed docker image, the FUCHSIA_NODENAME environment is not set.
    if 'FUCHSIA_NODENAME' not in os.environ:
        os.environ['FUCHSIA_NODENAME'] = 'fuchsia-ac67-8475-ee82'
    sys.exit(main())
