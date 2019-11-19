luci.console_view(
    name = 'chromium.chromiumos',
    header = '//consoles/chromium-header.textpb',
    repo = 'https://chromium.googlesource.com/chromium/src',
    entries = [
        luci.console_view_entry(
            builder = 'ci/Linux ChromiumOS Full',
            category = 'default',
            short_name = 'ful',
        ),
        luci.console_view_entry(
            builder = 'ci/linux-chromeos-rel',
            category = 'default',
            short_name = 'rel',
        ),
        luci.console_view_entry(
            builder = 'ci/linux-chromeos-dbg',
            category = 'default',
            short_name = 'dbg',
        ),
        luci.console_view_entry(
            builder = 'ci/chromeos-amd64-generic-asan-rel',
            category = 'simple|release|x64',
            short_name = 'asn',
        ),
        luci.console_view_entry(
            builder = 'ci/chromeos-amd64-generic-cfi-thin-lto-rel',
            category = 'simple|release|x64',
            short_name = 'cfi',
        ),
        luci.console_view_entry(
            builder = 'ci/chromeos-amd64-generic-dbg',
            category = 'simple|debug|x64',
            short_name = 'dbg',
        ),
        luci.console_view_entry(
            builder = 'ci/chromeos-amd64-generic-rel',
            category = 'simple|release|x64',
            short_name = 'rel',
        ),
        luci.console_view_entry(
            builder = 'ci/chromeos-arm-generic-dbg',
            category = 'simple|debug',
            short_name = 'arm',
        ),
        luci.console_view_entry(
            builder = 'ci/chromeos-arm-generic-rel',
            category = 'simple|release',
            short_name = 'arm',
        ),
        luci.console_view_entry(
            builder = 'ci/chromeos-kevin-rel',
            category = 'simple|release',
            short_name = 'kvn',
        ),
    ],
)
