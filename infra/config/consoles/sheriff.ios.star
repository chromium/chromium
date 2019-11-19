luci.console_view(
    name = 'sheriff.ios',
    header = '//consoles/chromium-header.textpb',
    repo = 'https://chromium.googlesource.com/chromium/src',
    title = 'iOS Sheriff Console',
    entries = [

        luci.console_view_entry(
            builder = 'ci/ios-device',
            category = 'chromium.mac',
            short_name = 'dev',
        ),
        luci.console_view_entry(
            builder = 'ci/ios-simulator',
            category = 'chromium.mac',
            short_name = 'sim',
        ),
        luci.console_view_entry(
            builder = 'ci/ios-simulator-full-configs',
            category = 'chromium.mac',
            short_name = 'ful',
        ),
        luci.console_view_entry(
            builder = 'ci/ios-simulator-noncq',
            category = 'chromium.mac',
            short_name = 'non',
        ),
        luci.console_view_entry(
            builder = 'ci/ios-device-xcode-clang',
            category = 'chromium.mac|xcode',
            short_name = 'dev',
        ),
        luci.console_view_entry(
            builder = 'ci/ios-simulator-xcode-clang',
            category = 'chromium.mac|xcode',
            short_name = 'sim',
        ),
        luci.console_view_entry(
            builder = 'ci/ios-slimnav',
            category = 'chromium.mac',
            short_name = 'slim',
        ),
        luci.console_view_entry(
            builder = 'ci/ios13-sdk-device',
            category = 'chromium.fyi|13',
            short_name = 'dev',
        ),
        luci.console_view_entry(
            builder = 'ci/ios13-sdk-simulator',
            category = 'chromium.fyi|13',
            short_name = 'sim',
        ),
        luci.console_view_entry(
            builder = 'ci/ios13-beta-simulator',
            category = 'chromium.fyi|13',
            short_name = 'ios13',
        ),
    ],
)
