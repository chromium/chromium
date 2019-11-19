luci.console_view(
    name = 'chromium',
    header = '//consoles/chromium-header.textpb',
    include_experimental_builds = True,
    repo = 'https://chromium.googlesource.com/chromium/src',
    entries = [
        luci.console_view_entry(
            builder = 'ci/android-archive-dbg',
            category = 'android',
            short_name = 'dbg',
        ),
        luci.console_view_entry(
            builder = 'ci/android-archive-rel',
            category = 'android',
            short_name = 'rel',
        ),
        luci.console_view_entry(
            builder = 'ci/linux-archive-dbg',
            category = 'linux',
            short_name = 'dbg',
        ),
        luci.console_view_entry(
            builder = 'ci/linux-archive-rel',
            category = 'linux',
            short_name = 'rel',
        ),
        luci.console_view_entry(
            builder = 'ci/mac-archive-dbg',
            category = 'mac',
            short_name = 'dbg',
        ),
        luci.console_view_entry(
            builder = 'ci/mac-archive-rel',
            category = 'mac',
            short_name = 'rel',
        ),
        luci.console_view_entry(
            builder = 'ci/win32-archive-rel',
            category = 'win|rel',
            short_name = '32',
        ),
        luci.console_view_entry(
            builder = 'ci/win-archive-rel',
            category = 'win|rel',
            short_name = '64',
        ),
        luci.console_view_entry(
            builder = 'ci/win32-archive-dbg',
            category = 'win|dbg',
            short_name = '32',
        ),
        luci.console_view_entry(
            builder = 'ci/win-archive-dbg',
            category = 'win|dbg',
            short_name = '64',
        ),
    ],
)
