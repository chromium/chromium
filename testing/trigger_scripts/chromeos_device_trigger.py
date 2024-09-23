#!/usr/bin/env python3
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Custom swarming trigger script for ChromeOS device tests.

CrOS device tests are unique in that the device OS they prefer to run on is
continuously changing. The LKGM file, checked into src at
//chromeos/CHROMEOS_LKGM, represents the ChromeOS version Chrome's ToT aims
to be compatible with. Therefore, a CrOS test for Chrome ideally targets a
device running the LKGM.

Since the LKGM file gets updated frequently (~daily), we can't reasonably
hardcode the LKGM in the test specs. So this special trigger script will read
the current LKGM (at the time of trigger) and append that to the task's
dimensions. If such a device isn't available in time, the task will fallback
to one running any OS.
"""

import argparse
import os
import re
import sys

import base_test_triggerer

SRC_DIR = os.path.dirname(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
LKGM_FILE_PATH = os.path.join(SRC_DIR, 'chromeos', 'CHROMEOS_LKGM')
# Should match something that looks like "12345.0.0".
LKGM_RE = re.compile(r'\d+\.\d+\.\d+')
PRIMARY_SLICE_EXPIRATION_S = 300


def read_current_lkgm():
    if not os.path.exists(LKGM_FILE_PATH):
        sys.stderr.write('LKGM file not present at %s\n' % LKGM_FILE_PATH)
        return None

    with open(LKGM_FILE_PATH) as f:
        lkgm = f.read().strip()

    if not LKGM_RE.match(lkgm):
        sys.stderr.write('Unknown format of LKGM: %s\n' % lkgm)
        return None

    # Just the major version should be sufficient.
    return lkgm.split('.')[0]


def parse_args(triggerer):
    # This script will do nothing but inspect and tweak the dimension args to
    # `swarming.py trigger`. So let's pull just those out.
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        '-d',
        '--dimension',
        default=[],
        action='append',
        nargs=2,
        dest='dimensions',
        help='Dimensions to filter on. Duplicated from the `swarming.py '
        'trigger` command. Parsed here to ensure `device_os` is not added.')
    parser.add_argument(
        '--optional-dimension',
        default=[],
        action='append',
        nargs=3,
        dest='optional_dimensions',
        help='Optional dimensions which will result in additional task slices. '
        'Duplicated from the `swarming.py trigger` command.')
    base_test_triggerer.BaseTestTriggerer.setup_parser_contract(parser)
    args, additional_args = parser.parse_known_args()
    additional_args = triggerer.modify_args(additional_args, 0,
                                            args.shard_index, args.shards,
                                            args.dump_json)

    if additional_args[0] != 'trigger':
        parser.error('This script is only supported for `swarming.py trigger`'
                     ' invocations.')

    for k, _ in args.dimensions:
        if k == 'device_os':
            parser.error(
                'Must not specify the device_os dimension when using this'
                ' script. (It will be added automatically.)')

    # It might be a valid use-case to include optional-dimensions in the initial
    # invocation. But it'd be difficult to integrate them into what we're doing
    # here. So let's just ensure there aren't any.
    if args.optional_dimensions:
        parser.error(
            'Must not specify optional dimensions when using this script.')

    return args, additional_args


def main():
    triggerer = base_test_triggerer.BaseTestTriggerer()
    args, additional_args = parse_args(triggerer)

    current_lkgm = read_current_lkgm()
    if not current_lkgm:
        return 1

    new_args = additional_args[:1]
    # Insert our modified dimension args in between the 1st and 2nd args of the
    # initial `swarming.py` invocation. This avoids the presence of the special
    # `--` arg from causing swarming.py to ignore them.
    needs_device_status = True
    for k, v in args.dimensions:
        new_args.extend(['--dimension', k, v])
        if k == 'device_status':
            needs_device_status = False

    # Only CrOS device bots with a device_status dimension of "available" should
    # run tests. So target those explicitly if we aren't already.
    if needs_device_status:
        new_args.extend(['--dimension', 'device_status', 'available'])

    new_args.extend([
        '-optional-dimension',
        'device_os=%s:%d' % (current_lkgm, PRIMARY_SLICE_EXPIRATION_S),
    ])
    new_args += additional_args[1:]

    return triggerer.run_swarming_go(new_args, args.dump_json, args.shard_index
                                     or 0, args.shards)


if __name__ == '__main__':
    sys.exit(main())
