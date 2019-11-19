luci.console_view(
    name = 'chromium.android.fyi',
    header = '//consoles/chromium-header.textpb',
    repo = 'https://chromium.googlesource.com/chromium/src',
    entries = [
        luci.console_view_entry(
            builder = 'ci/android-bfcache-debug',
            category = 'android',
        ),
        luci.console_view_entry(
            builder = 'ci/Memory Infra Tester',
            category = 'Memory',
        ),
        luci.console_view_entry(
            builder = 'ci/Android WebView P FYI (rel)',
            category = 'webview',
            short_name = 'p-rel',
        ),
        luci.console_view_entry(
            builder = 'ci/Android WebView P OOR-CORS FYI (rel)',
            category = 'webview',
            short_name = 'cors',
        ),
        luci.console_view_entry(
            builder = 'ci/android-marshmallow-x86-fyi-rel',
            category = 'emulator|M|x86',
            short_name = 'rel',
        ),
        luci.console_view_entry(
            builder = 'ci/android-pie-x86-fyi-rel',
            category = 'emulator|P|x86',
            short_name = 'rel',
        ),
    ],
)
