luci.console_view(
    name = 'chromium.android',
    header = '//consoles/chromium-header.textpb',
    repo = 'https://chromium.googlesource.com/chromium/src',
    entries = [
        luci.console_view_entry(
            builder = 'ci/android-cronet-arm-dbg',
            category = 'cronet|arm',
            short_name = 'dbg',
        ),
        luci.console_view_entry(
            builder = 'ci/android-cronet-arm-rel',
            category = 'cronet|arm',
            short_name = 'rel',
        ),
        luci.console_view_entry(
            builder = 'ci/android-cronet-arm64-dbg',
            category = 'cronet|arm64',
            short_name = 'dbg',
        ),
        luci.console_view_entry(
            builder = 'ci/android-cronet-arm64-rel',
            category = 'cronet|arm64',
            short_name = 'rel',
        ),
        luci.console_view_entry(
            builder = 'ci/android-cronet-x86-dbg',
            category = 'cronet|x86',
            short_name = 'dbg',
        ),
        luci.console_view_entry(
            builder = 'ci/android-cronet-x86-rel',
            category = 'cronet|x86',
            short_name = 'rel',
        ),
        luci.console_view_entry(
            builder = 'ci/android-cronet-asan-arm-rel',
            category = 'cronet|asan',
        ),
        luci.console_view_entry(
            builder = 'ci/android-cronet-kitkat-arm-rel',
            category = 'cronet|test',
            short_name = 'k',
        ),
        luci.console_view_entry(
            builder = 'ci/android-cronet-lollipop-arm-rel',
            category = 'cronet|test',
            short_name = 'l',
        ),
        luci.console_view_entry(
            builder = 'ci/android-cronet-marshmallow-arm64-rel',
            category = 'cronet|test',
            short_name = 'm',
        ),
        luci.console_view_entry(
            builder = 'ci/android-cronet-marshmallow-arm64-perf-rel',
            category = 'cronet|test|perf',
            short_name = 'm',
        ),
        luci.console_view_entry(
            builder = 'ci/Android arm Builder (dbg)',
            category = 'builder|arm',
            short_name = '32',
        ),
        luci.console_view_entry(
            builder = 'ci/Android arm64 Builder (dbg)',
            category = 'builder|arm',
            short_name = '64',
        ),
        luci.console_view_entry(
            builder = 'ci/Android x86 Builder (dbg)',
            category = 'builder|x86',
            short_name = '32',
        ),
        luci.console_view_entry(
            builder = 'ci/Android x64 Builder (dbg)',
            category = 'builder|x86',
            short_name = '64',
        ),
        luci.console_view_entry(
            builder = 'ci/Deterministic Android',
            category = 'builder|det',
            short_name = 'rel',
        ),
        luci.console_view_entry(
            builder = 'ci/Deterministic Android (dbg)',
            category = 'builder|det',
            short_name = 'dbg',
        ),
        luci.console_view_entry(
            builder = 'ci/KitKat Phone Tester (dbg)',
            category = 'tester|phone',
            short_name = 'K',
        ),
        luci.console_view_entry(
            builder = 'ci/Lollipop Phone Tester',
            category = 'tester|phone',
            short_name = 'L',
        ),
        luci.console_view_entry(
            builder = 'ci/Marshmallow 64 bit Tester',
            category = 'tester|phone',
            short_name = 'M',
        ),
        luci.console_view_entry(
            builder = 'ci/Nougat Phone Tester',
            category = 'tester|phone',
            short_name = 'N',
        ),
        luci.console_view_entry(
            builder = 'ci/Oreo Phone Tester',
            category = 'tester|phone',
            short_name = 'O',
        ),
        luci.console_view_entry(
            builder = 'ci/android-pie-arm64-dbg',
            category = 'tester|phone',
            short_name = 'P',
        ),
        luci.console_view_entry(
            builder = 'ci/KitKat Tablet Tester',
            category = 'tester|tablet',
            short_name = 'K',
        ),
        luci.console_view_entry(
            builder = 'ci/Lollipop Tablet Tester',
            category = 'tester|tablet',
            short_name = 'L',
        ),
        luci.console_view_entry(
            builder = 'ci/Marshmallow Tablet Tester',
            category = 'tester|tablet',
            short_name = 'M',
        ),
        luci.console_view_entry(
            builder = 'ci/android-incremental-dbg',
            category = 'tester|incremental',
        ),
        luci.console_view_entry(
            builder = 'ci/Android WebView L (dbg)',
            category = 'tester|webview',
            short_name = 'L',
        ),
        luci.console_view_entry(
            builder = 'ci/Android WebView M (dbg)',
            category = 'tester|webview',
            short_name = 'M',
        ),
        luci.console_view_entry(
            builder = 'ci/Android WebView N (dbg)',
            category = 'tester|webview',
            short_name = 'N',
        ),
        luci.console_view_entry(
            builder = 'ci/Android WebView O (dbg)',
            category = 'tester|webview',
            short_name = 'O',
        ),
        luci.console_view_entry(
            builder = 'ci/Android WebView P (dbg)',
            category = 'tester|webview',
            short_name = 'P',
        ),
        luci.console_view_entry(
            builder = 'ci/android-kitkat-arm-rel',
            category = 'on_cq',
            short_name = 'K',
        ),
        luci.console_view_entry(
            builder = 'ci/android-marshmallow-arm64-rel',
            category = 'on_cq',
            short_name = 'M',
        ),
        luci.console_view_entry(
            builder = 'ci/Cast Android (dbg)',
            category = 'on_cq',
            short_name = 'cst',
        ),
        luci.console_view_entry(
            builder = 'ci/Android ASAN (dbg)',
            category = 'on_cq',
            short_name = 'san',
        ),
        luci.console_view_entry(
            builder = 'ci/android-pie-arm64-rel',
            category = 'on_cq|future',
            short_name = 'P',
        ),
    ],
)
