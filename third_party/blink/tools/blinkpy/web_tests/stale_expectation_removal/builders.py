# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Web test-specific impl of the unexpected passes' builders module."""

from unexpected_passes_common import builders


class WebTestBuilders(builders.Builders):
    def _BuilderRunsTestOfInterest(self, test_map, _):
        tests = test_map.get('isolated_scripts', [])
        for t in tests:
            if t.get('isolate_name') in self.GetIsolateNames():
                return True
        return False

    def GetIsolateNames(self):
        return {
            'blink_web_tests',
            # We would like to support the WebGPU tests, but they currently
            # report a temporary file instead of the WebGPU expectation file
            # and the names in ResultDB do not currently match what is in
            # the expectation file due to subtests not being exposed to
            # ResultDB.
            # 'webgpu_blink_web_tests',
        }

    def GetFakeCiBuilders(self):
        # Some of these are weird in that they're explicitly defined trybots
        # instead of a mirror of a CI bot.
        return {
            # chromium.fyi
            'linux-blink-optional-highdpi-rel-dummy': {
                'linux-blink-optional-highdpi-rel',
            },
            'linux-blink-rel-dummy': {
                'linux-blink-rel',
                'v8_linux_blink_rel',
            },
            'mac10.12-blink-rel-dummy': {
                'mac10.12-blink-rel',
            },
            'mac10.13-blink-rel-dummy': {
                'mac10.13-blink-rel',
            },
            'mac10.14-blink-rel-dummy': {
                'mac10.14-blink-rel',
            },
            'mac10.15-blink-rel-dummy': {
                'mac10.15-blink-rel',
            },
            'mac11.0-blink-rel-dummy': {
                'mac11.0-blink-rel',
            },
            'mac11.0.arm64-blink-rel-dummy': {
                'mac11.0.arm64-blink-rel',
            },
            'WebKit Linux layout_ng_disabled Builder': {
                'linux_layout_tests_layout_ng_disabled',
            },
            'win7-blink-rel-dummy': {
                'win7-blink-rel',
            },
            'win10.20h2-blink-rel-dummy': {
                'win10.20h2-blink-rel',
            },
            # tryserver.chromium.linux
            # Explicit trybot.
            'linux-blink-web-tests-force-accessibility-rel': {
                'linux-blink-web-tests-force-accessibility-rel',
            },
            # Explicit trybot.
            'linux-layout-tests-edit-ng': {
                'linux-layout-tests-edit-ng',
            },
        }

    def GetNonChromiumBuilders(self):
        return {
            'devtools_frontend_linux_blink_light_rel',
            'devtools_frontend_linux_blink_rel',
            'DevTools Linux',
            'DevTools Linux (chromium)',
            # Could be used in the future, but has never run any builds.
            'linux-exp-code-coverage',
            'ToTMacOfficial',
            'V8 Blink Linux',
            'V8 Blink Linux Debug',
            'V8 Blink Linux Future',
            'V8 Blink Mac',
            'V8 Blink Win',
        }
