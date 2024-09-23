# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Web test-specific impl of the unexpected passes' builders module."""

from typing import Any, Dict, Set

from unexpected_passes_common import builders
from unexpected_passes_common import constants
from unexpected_passes_common import data_types


class WebTestBuilders(builders.Builders):
    def __init__(self, include_internal_builders: bool):
        super(WebTestBuilders, self).__init__(None, include_internal_builders)
        self._fake_ci_builders = None
        self._non_chromium_builders = None

    def _BuilderRunsTestOfInterest(self, test_map: Dict[str, Any]) -> bool:
        tests = test_map.get('isolated_scripts', [])
        for t in tests:
            if t.get('test') in self.GetIsolateNames():
                return True
        return False

    def GetIsolateNames(self) -> Set[str]:
        return {
            'blink_web_tests',
            # We would like to support the WebGPU tests, but they currently
            # report a temporary file instead of the WebGPU expectation file
            # and the names in ResultDB do not currently match what is in
            # the expectation file due to subtests not being exposed to
            # ResultDB.
            # 'webgpu_blink_web_tests',
        }

    def GetFakeCiBuilders(self) -> builders.FakeBuildersDict:
        # Some of these are weird in that they're explicitly defined trybots
        # instead of a mirror of a CI bot.
        if self._fake_ci_builders is None:
            fake_try_builders = {
                # chromium.fyi
                'linux-blink-rel-dummy': {
                    'linux-blink-rel',
                    'v8_linux_blink_rel',
                },
                'mac11.0-blink-rel-dummy': {
                    'mac11.0-blink-rel',
                },
                'mac11.0.arm64-blink-rel-dummy': {
                    'mac11.0.arm64-blink-rel',
                },
                'win10.20h2-blink-rel-dummy': {
                    'win10.20h2-blink-rel',
                },
                'win11-blink-rel-dummy': {
                    'win11-blink-rel',
                },
                # tryserver.chromium.linux
                # Explicit trybot.
                'linux-layout-tests-edit-ng': {
                    'linux-layout-tests-edit-ng',
                },
            }
            self._fake_ci_builders = {}
            for ci_builder, try_builders in fake_try_builders.items():
                ci_entry = data_types.BuilderEntry(ci_builder,
                                                   constants.BuilderTypes.CI,
                                                   False)
                try_entries = {
                    data_types.BuilderEntry(b, constants.BuilderTypes.TRY,
                                            False)
                    for b in try_builders
                }
                self._fake_ci_builders[ci_entry] = try_entries
        return self._fake_ci_builders

    def GetNonChromiumBuilders(self) -> Set[data_types.BuilderEntry]:
        if self._non_chromium_builders is None:
            str_builders = {
                # These builders do not use the Chromium recipe.
                'devtools_frontend_linux_blink_light_rel',
                'devtools_frontend_linux_blink_light_rel_fastbuild',
                'devtools_frontend_linux_blink_rel',
                'DevTools Linux',
                'DevTools Linux Fastbuild',
                # Could be used in the future, but has never run any builds.
                'linux-exp-code-coverage',
                'ToTMacOfficial',
                'V8 Blink Linux',
                'V8 Blink Linux Debug',
                'V8 Blink Linux Future',
                'V8 Blink Mac',
                'V8 Blink Win',
                # These do use the Chromium recipe, but are in the "build"
                # bucket instead of the "ci" bucket, which breaks some
                # assumptions we have.
                'Mac13 Tests Siso FYI',
                'Mac Tests Siso FYI',
            }
            self._non_chromium_builders = {
                data_types.BuilderEntry(b, constants.BuilderTypes.CI, False)
                for b in str_builders
            }
        return self._non_chromium_builders
