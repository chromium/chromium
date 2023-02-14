# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//console-header.star", "HEADER")

luci.console_view(
    name = "chromium.webrtc",
    repo = "https://chromium.googlesource.com/chromium/src",
    header = HEADER,
    entries = [
        luci.console_view_entry(
            builder = "webrtc/WebRTC Chromium Android Builder",
            category = "android",
            short_name = "bld",
        ),
        luci.console_view_entry(
            builder = "webrtc/WebRTC Chromium Android Tester",
            category = "android",
            short_name = "tst",
        ),
        luci.console_view_entry(
            builder = "webrtc/WebRTC Chromium Linux Builder",
            category = "linux",
            short_name = "bld",
        ),
        luci.console_view_entry(
            builder = "webrtc/WebRTC Chromium Linux Tester",
            category = "linux",
            short_name = "tst",
        ),
        luci.console_view_entry(
            builder = "webrtc/WebRTC Chromium Mac Builder",
            category = "mac",
            short_name = "bld",
        ),
        luci.console_view_entry(
            builder = "webrtc/WebRTC Chromium Mac Tester",
            category = "mac",
            short_name = "tst",
        ),
        luci.console_view_entry(
            builder = "webrtc/WebRTC Chromium Win Builder",
            category = "win",
            short_name = "bld",
        ),
        luci.console_view_entry(
            builder = "webrtc/WebRTC Chromium Win10 Tester",
            category = "win",
            short_name = "10",
        ),
    ],
)
