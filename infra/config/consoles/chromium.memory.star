luci.console_view(
    name = 'chromium.memory',
    header = '//consoles/chromium-header.textpb',
    repo = 'https://chromium.googlesource.com/chromium/src',
    entries = [
        luci.console_view_entry(
            builder = 'ci/win-asan',
            category = 'win',
            short_name = 'asn',
        ),
        luci.console_view_entry(
            builder = 'ci/Mac ASan 64 Builder',
            category = 'mac',
            short_name = 'bld',
        ),
        luci.console_view_entry(
            builder = 'ci/Mac ASan 64 Tests (1)',
            category = 'mac',
            short_name = 'tst',
        ),
        luci.console_view_entry(
            builder = 'ci/Linux TSan Builder',
            category = 'linux|TSan v2',
            short_name = 'bld',
        ),
        luci.console_view_entry(
            builder = 'ci/Linux TSan Tests',
            category = 'linux|TSan v2',
            short_name = 'tst',
        ),
        luci.console_view_entry(
            builder = 'ci/Linux ASan LSan Builder',
            category = 'linux|asan lsan',
            short_name = 'bld',
        ),
        luci.console_view_entry(
            builder = 'ci/Linux ASan LSan Tests (1)',
            category = 'linux|asan lsan',
            short_name = 'tst',
        ),
        luci.console_view_entry(
            builder = 'ci/Linux ASan Tests (sandboxed)',
            category = 'linux|asan lsan',
            short_name = 'sbx',
        ),
        luci.console_view_entry(
            builder = 'ci/Linux MSan Builder',
            category = 'linux|msan',
            short_name = 'bld',
        ),
        luci.console_view_entry(
            builder = 'ci/Linux MSan Tests',
            category = 'linux|msan',
            short_name = 'tst',
        ),
        luci.console_view_entry(
            builder = 'ci/WebKit Linux ASAN',
            category = 'linux|webkit',
            short_name = 'asn',
        ),
        luci.console_view_entry(
            builder = 'ci/WebKit Linux MSAN',
            category = 'linux|webkit',
            short_name = 'msn',
        ),
        luci.console_view_entry(
            builder = 'ci/WebKit Linux Leak',
            category = 'linux|webkit',
            short_name = 'lk',
        ),
        luci.console_view_entry(
            builder = 'ci/Linux Chromium OS ASan LSan Builder',
            category = 'cros|asan',
            short_name = 'bld',
        ),
        luci.console_view_entry(
            builder = 'ci/Linux Chromium OS ASan LSan Tests (1)',
            category = 'cros|asan',
            short_name = 'tst',
        ),
        luci.console_view_entry(
            builder = 'ci/Linux ChromiumOS MSan Builder',
            category = 'cros|msan',
            short_name = 'bld',
        ),
        luci.console_view_entry(
            builder = 'ci/Linux ChromiumOS MSan Tests',
            category = 'cros|msan',
            short_name = 'tst',
        ),
        luci.console_view_entry(
            builder = 'ci/android-asan',
            category = 'android',
            short_name = 'asn',
        ),
        luci.console_view_entry(
            builder = 'ci/Android CFI',
            category = 'cfi',
            short_name = 'and',
        ),
        luci.console_view_entry(
            builder = 'ci/Linux CFI',
            category = 'cfi',
            short_name = 'lnx',
        ),
    ],
)
