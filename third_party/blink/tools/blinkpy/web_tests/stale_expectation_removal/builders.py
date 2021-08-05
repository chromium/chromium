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
            'webgpu_blink_web_tests',
        }

    def GetFakeCiBuilders(self):
        return {
            # chromium.fyi
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
            'WebKit Linux composite_after_paint Dummy Builder': {
                'linux_layout_tests_composite_after_paint',
            },
            'WebKit Linux layout_ng_disabled Builder': {
                'linux_layout_tests_layout_ng_disabled',
            },
            'win7-blink-rel-dummy': {
                'win7-blink-rel',
            },
            'win10-blink-rel-dummy': {
                'win10-blink-rel',
            },
            'win10.20h2-blink-rel-dummy': {
                'win10.20h2-blink-rel',
            },
        }

    def GetNonChromiumBuilders(self):
        return {
            'DevTools Linux (chromium)',
        }
