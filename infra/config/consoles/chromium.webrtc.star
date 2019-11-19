luci.console_view(
    name = 'chromium.webrtc',
    header = '//consoles/chromium-header.textpb',
    repo = 'https://chromium.googlesource.com/chromium/src',
    entries = [
        luci.console_view_entry(
            builder = 'webrtc/WebRTC Chromium Android Builder',
            category = 'android',
            short_name = 'bld',
        ),
        luci.console_view_entry(
            builder = 'webrtc/WebRTC Chromium Android Tester',
            category = 'android',
            short_name = 'tst',
        ),
        luci.console_view_entry(
            builder = 'webrtc/WebRTC Chromium Linux Builder',
            category = 'linux',
            short_name = 'bld',
        ),
        luci.console_view_entry(
            builder = 'webrtc/WebRTC Chromium Linux Tester',
            category = 'linux',
            short_name = 'tst',
        ),
        luci.console_view_entry(
            builder = 'webrtc/WebRTC Chromium Mac Builder',
            category = 'mac',
            short_name = 'bld',
        ),
        luci.console_view_entry(
            builder = 'webrtc/WebRTC Chromium Mac Tester',
            category = 'mac',
            short_name = 'tst',
        ),
        luci.console_view_entry(
            builder = 'webrtc/WebRTC Chromium Win Builder',
            category = 'win',
            short_name = 'bld',
        ),
        luci.console_view_entry(
            builder = 'webrtc/WebRTC Chromium Win10 Tester',
            category = 'win',
            short_name = '10',
        ),
        luci.console_view_entry(
            builder = 'webrtc/WebRTC Chromium Win7 Tester',
            category = 'win',
            short_name = '7',
        ),
        luci.console_view_entry(
            builder = 'webrtc/WebRTC Chromium Win8 Tester',
            category = 'win',
            short_name = '8',
        ),
    ],
)
