#!/usr/bin/env vpython
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import optparse
import sys

from blinkpy.common.host import Host
from blinkpy.w3c.wpt_output_updater import WPTOutputUpdater
from blinkpy.web_tests.models.test_expectations import TestExpectations


def main(args):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--additional-expectations", action="append",
                        help="Paths to additional expectations files for WPT.")
    known_args, rest_args = parser.parse_known_args(args)

    host = Host()
    port = host.port_factory.get(options=optparse.Values(vars(known_args)))
    expectations = TestExpectations(port)
    output_updater = WPTOutputUpdater(expectations)
    sys.exit(output_updater.run(rest_args))

if __name__ == '__main__':
    main(sys.argv[1:])
