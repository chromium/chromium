luci.console_view(
    name = 'chromium.linux',
    header = '//consoles/chromium-header.textpb',
    repo = 'https://chromium.googlesource.com/chromium/src',
    entries = [
        luci.console_view_entry(
            builder = 'ci/Linux Builder',
            category = 'release',
            short_name = 'bld',
        ),
        luci.console_view_entry(
            builder = 'ci/Linux Tests',
            category = 'release',
            short_name = 'tst',
        ),
        luci.console_view_entry(
            builder = 'ci/linux-gcc-rel',
            category = 'release',
            short_name = 'gcc',
        ),
        luci.console_view_entry(
            builder = 'ci/Deterministic Linux',
            category = 'release',
            short_name = 'det',
        ),
        luci.console_view_entry(
            builder = 'ci/Leak Detection Linux',
            category = 'release',
            short_name = 'lk',
        ),
        luci.console_view_entry(
            builder = 'ci/linux-ozone-rel',
            category = 'release',
            short_name = 'ozo',
        ),
        luci.console_view_entry(
            builder = 'ci/linux-trusty-rel',
            category = 'release',
            short_name = 'tru',
        ),
        luci.console_view_entry(
            builder = 'ci/Linux Builder (dbg)(32)',
            category = 'debug|builder',
            short_name = '32',
        ),
        luci.console_view_entry(
            builder = 'ci/Linux Builder (dbg)',
            category = 'debug|builder',
            short_name = '64',
        ),
        luci.console_view_entry(
            builder = 'ci/Deterministic Linux (dbg)',
            category = 'debug|builder',
            short_name = 'det',
        ),
        luci.console_view_entry(
            builder = 'ci/Linux Tests (dbg)(1)',
            category = 'debug|tester',
            short_name = '64',
        ),
        luci.console_view_entry(
            builder = 'ci/Cast Linux',
            category = 'cast',
            short_name = 'vid',
        ),
        luci.console_view_entry(
            builder = 'ci/Cast Audio Linux',
            category = 'cast',
            short_name = 'aud',
        ),
        luci.console_view_entry(
            builder = 'ci/Fuchsia ARM64',
            category = 'fuchsia|a64',
        ),
        luci.console_view_entry(
            builder = 'ci/fuchsia-arm64-cast',
            category = 'fuchsia|cast',
            short_name = 'a64',
        ),
        luci.console_view_entry(
            builder = 'ci/fuchsia-x64-cast',
            category = 'fuchsia|cast',
            short_name = 'x64',
        ),
        luci.console_view_entry(
            builder = 'ci/fuchsia-x64-dbg',
            category = 'fuchsia|x64',
            short_name = 'dbg',
        ),
        luci.console_view_entry(
            builder = 'ci/Deterministic Fuchsia (dbg)',
            category = 'fuchsia|x64',
            short_name = 'det',
        ),
        luci.console_view_entry(
            builder = 'ci/Fuchsia x64',
            category = 'fuchsia|x64',
            short_name = 'rel',
        ),
    ],
)
