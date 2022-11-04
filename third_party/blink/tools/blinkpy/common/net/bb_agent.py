# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import logging

from blinkpy.common.net.luci_auth import LuciAuth
from blinkpy.common.net.results_fetcher import Build
from blinkpy.common.memoized import memoized
from blinkpy.common.path_finder import PathFinder

_log = logging.getLogger(__name__)


class BBAgent(object):
    def __init__(self, host):
        self._host = host

    @property
    def bb_bin_path(self):
        return self._host.filesystem.join(
            PathFinder(self._host.filesystem).depot_tools_base(), 'bb')

    @memoized
    def _check_luci_auth(self):
        try:
            LuciAuth(self._host).get_access_token()
        except Exception as ex:
            _log.exception('Caught an exception when checking luci '
                           'authentication. Please run `luci-auth login` '
                           'before trying again.')
            raise ex

    def get_finished_build(self, builder_name, number, try_build=False):
        self._check_luci_auth()
        builder_path = ('chromium/' + ('try' if try_build else 'ci') + '/' +
                        builder_name)

        # get build from the latest run if number == 0, otherwise get the build specified
        # by number, must be within the latest 100 runs.
        count = '100' if number else '-1'
        bb_output = self._host.executive.run_command([
            self.bb_bin_path, 'ls', count, '-json', '-status', 'ended',
            builder_path
        ]).strip()
        if not bb_output:
            return

        if not number:
            json_output = json.loads(bb_output)
            return Build(builder_name, json_output['number'],
                         json_output['id'])

        for line in bb_output.splitlines():
            build = json.loads(line)
            if build['number'] == number:
                return Build(builder_name, number, build['id'])

    def get_build_test_results(self, build, step_name):
        self._check_luci_auth()
        assert build.build_id, 'ID of the build must be provided'
        bb_output = self._host.executive.run_command(
            [self.bb_bin_path, 'log', '-nocolor', build.build_id,
             step_name, 'json.output'])

        if not bb_output:
            return
        return json.loads(bb_output)
