luci.console_view(
    name = 'chromium.win',
    header = '//consoles/chromium-header.textpb',
    repo = 'https://chromium.googlesource.com/chromium/src',
    entries = [
        luci.console_view_entry(
            builder = 'ci/Win Builder',
            category = 'release|builder',
            short_name = '32',
        ),
        luci.console_view_entry(
            builder = 'ci/Win x64 Builder',
            category = 'release|builder',
            short_name = '64',
        ),
        luci.console_view_entry(
            builder = 'ci/Win7 (32) Tests',
            category = 'release|tester',
            short_name = '32',
        ),
        luci.console_view_entry(
            builder = 'ci/Win7 Tests (1)',
            category = 'release|tester',
            short_name = '32',
        ),
        luci.console_view_entry(
            builder = 'ci/Win 7 Tests x64 (1)',
            category = 'release|tester',
            short_name = '64',
        ),
        luci.console_view_entry(
            builder = 'ci/Win10 Tests x64',
            category = 'release|tester',
            short_name = 'w10',
        ),
        luci.console_view_entry(
            builder = 'ci/Win x64 Builder (dbg)',
            category = 'debug|builder',
            short_name = '64',
        ),
        luci.console_view_entry(
            builder = 'ci/Win Builder (dbg)',
            category = 'debug|builder',
            short_name = '32',
        ),
        luci.console_view_entry(
            builder = 'ci/Win7 Tests (dbg)(1)',
            category = 'debug|tester',
            short_name = '7',
        ),
        luci.console_view_entry(
            builder = 'ci/Win10 Tests x64 (dbg)',
            category = 'debug|tester',
            short_name = '10',
        ),
        luci.console_view_entry(
            builder = 'ci/Windows deterministic',
            category = 'misc',
            short_name = 'det',
        ),
        luci.console_view_entry(
            builder = 'ci/WebKit Win10',
            category = 'misc',
            short_name = 'wbk',
        ),
    ],
)
