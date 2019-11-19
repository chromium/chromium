luci.console_view(
    name = 'android.packager',
    header = '//consoles/chromium-header.textpb',
    repo = 'https://chromium.googlesource.com/chromium/src',
    entries = [
        luci.console_view_entry(
            builder = 'ci/android-avd-packager',
            short_name = 'avd',
        ),
        luci.console_view_entry(
            builder = 'ci/android-sdk-packager',
            short_name = 'sdk',
        ),
    ],
)
