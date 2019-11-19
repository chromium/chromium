luci.console_view(
    name = 'chromium.mac',
    header = '//consoles/chromium-header.textpb',
    repo = 'https://chromium.googlesource.com/chromium/src',
    entries = [
        luci.console_view_entry(
            builder = 'ci/Mac Builder',
            category = 'release',
            short_name = 'bld',
        ),
        luci.console_view_entry(
            builder = 'ci/Mac10.10 Tests',
            category = 'release',
            short_name = '10',
        ),
        luci.console_view_entry(
            builder = 'ci/Mac10.11 Tests',
            category = 'release',
            short_name = '11',
        ),
        luci.console_view_entry(
            builder = 'ci/Mac10.12 Tests',
            category = 'release',
            short_name = '12',
        ),
        luci.console_view_entry(
            builder = 'ci/Mac10.13 Tests',
            category = 'release',
            short_name = '13',
        ),
        luci.console_view_entry(
            builder = 'ci/WebKit Mac10.13 (retina)',
            category = 'release',
            short_name = 'ret',
        ),
        luci.console_view_entry(
            builder = 'ci/Mac Builder (dbg)',
            category = 'debug',
            short_name = 'bld',
        ),
        luci.console_view_entry(
            builder = 'ci/Mac10.13 Tests (dbg)',
            category = 'debug',
            short_name = '13',
        ),
        luci.console_view_entry(
            builder = 'ci/ios-device',
            category = 'ios|default',
            short_name = 'dev',
        ),
        luci.console_view_entry(
            builder = 'ci/ios-simulator',
            category = 'ios|default',
            short_name = 'sim',
        ),
        luci.console_view_entry(
            builder = 'ci/ios-simulator-full-configs',
            category = 'ios|default',
            short_name = 'ful',
        ),
        luci.console_view_entry(
            builder = 'ci/ios-simulator-noncq',
            category = 'ios|default',
            short_name = 'non',
        ),
        luci.console_view_entry(
            builder = 'ci/ios-device-xcode-clang',
            category = 'ios|xcode',
            short_name = 'dev',
        ),
        luci.console_view_entry(
            builder = 'ci/ios-simulator-xcode-clang',
            category = 'ios|xcode',
            short_name = 'sim',
        ),
        luci.console_view_entry(
            builder = 'ci/ios-slimnav',
            category = 'ios|misc',
            short_name = 'slim',
        ),
    ],
)
