# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import json
from collections import defaultdict

from blinkpy.common.host import Host
from blinkpy.common.path_finder import RELATIVE_WEB_TESTS
from blinkpy.w3c.wpt_manifest import WPTManifest
from blinkpy.web_tests.port.base import ALL_TESTS_BY_DIRECTORIES
from blinkpy.web_tests.port.factory import PortFactory


class WebTestsCollector(object):
    def __init__(self, host):
        self.host = host
        self.fs = host.filesystem
        self.port = PortFactory(host).get()

    def main(self, argv=None):
        usage = 'Precollects web tests and save the result to AllTestsByDirectories.json.'
        parser = argparse.ArgumentParser()
        parser.description = usage
        parser.add_argument('--out', '-o', required=False, default=None,
                            help='Path for the output list file')
        parser.add_argument('paths', nargs='*',
                            help='Paths for which to update test cases')
        options = parser.parse_args(argv)
        all_tests_by_dir_path = self.fs.join(self.port.web_tests_dir(),
                                             ALL_TESTS_BY_DIRECTORIES)
        return self.collect_tests(options.out if options.out else all_tests_by_dir_path,
                                  options.paths)

    def collect_tests(self, out, paths):
        all_tests_by_dir_path = self.fs.join(self.port.web_tests_dir(),
                                             ALL_TESTS_BY_DIRECTORIES)

        if self.fs.exists(all_tests_by_dir_path):
            old_data = self.fs.read_text_file(all_tests_by_dir_path)
        else:
            old_data = '{}'

        if paths and self.fs.exists(all_tests_by_dir_path):
            # do a partial update for this case
            tests = self.port.real_tests(paths)
            if 'external/wpt' in paths:
                WPTManifest.ensure_manifest(self.port)
                tests.extend([
                    'external/wpt/' + test
                    for test in self.port.wpt_manifest('external/wpt').all_urls()
                ])
            if 'wpt_internal' in paths:
                WPTManifest.ensure_manifest(self.port, 'wpt_internal')
                tests.extend([
                    'wpt_internal/' + test
                    for test in self.port.wpt_manifest('wpt_internal').all_urls()
                ])
            tests_by_dir = self.port.read_all_tests_by_directories()
        else:
            tests = self.port.real_tests(None)
            WPTManifest.ensure_manifest(self.port)
            WPTManifest.ensure_manifest(self.port, 'wpt_internal')
            tests.extend([
                wpt_path + '/' + test for wpt_path in self.port.WPT_DIRS
                for test in self.port.wpt_manifest(wpt_path).all_urls()
            ])
            tests_by_dir = {}

        for key in list(tests_by_dir):
            if any(key.startswith(path + '/') for path in paths):
                tests_by_dir.pop(key, None)

        newdict = defaultdict(list)
        for test in tests:
            dirname = self.fs.dirname(test) + '/'
            newdict[dirname].append(test)

        for _, v in newdict.items():
            v.sort()

        tests_by_dir.update(newdict)
        new_data = json.dumps(tests_by_dir, indent=2, sort_keys=True)
        self.fs.write_text_file(out, new_data)

        return 0 if old_data == new_data else 1
