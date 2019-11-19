luci.console_view(
    name = 'main',
    header = '//consoles/chromium-header.textpb',
    repo = 'https://chromium.googlesource.com/chromium/src',
    title = 'Chromium Main Console',
    entries = [
        luci.console_view_entry(
            builder = 'ci/android-archive-dbg',
            category = 'chromium|android',
            short_name = 'dbg',
        ),
        luci.console_view_entry(
            builder = 'ci/android-archive-rel',
            category = 'chromium|android',
            short_name = 'rel',
        ),
        luci.console_view_entry(
            builder = 'ci/linux-archive-dbg',
            category = 'chromium|linux',
            short_name = 'dbg',
        ),
        luci.console_view_entry(
            builder = 'ci/linux-archive-rel',
            category = 'chromium|linux',
            short_name = 'rel',
        ),
        luci.console_view_entry(
            builder = 'ci/mac-archive-dbg',
            category = 'chromium|mac',
            short_name = 'dbg',
        ),
        luci.console_view_entry(
            builder = 'ci/mac-archive-rel',
            category = 'chromium|mac',
            short_name = 'rel',
        ),
        luci.console_view_entry(
            builder = 'ci/win32-archive-rel',
            category = 'chromium|win|rel',
            short_name = '32',
        ),
        luci.console_view_entry(
            builder = 'ci/win-archive-rel',
            category = 'chromium|win|rel',
            short_name = '64',
        ),
        luci.console_view_entry(
            builder = 'ci/win32-archive-dbg',
            category = 'chromium|win|dbg',
            short_name = '32',
        ),
        luci.console_view_entry(
            builder = 'ci/win-archive-dbg',
            category = 'chromium|win|dbg',
            short_name = '64',
        ),
        luci.console_view_entry(
            builder = 'ci/Win Builder',
            category = 'chromium.win|release|builder',
            short_name = '32',
        ),
        luci.console_view_entry(
            builder = 'ci/Win x64 Builder',
            category = 'chromium.win|release|builder',
            short_name = '64',
        ),
        luci.console_view_entry(
            builder = 'ci/Win7 (32) Tests',
            category = 'chromium.win|release|tester',
            short_name = '32',
        ),
        luci.console_view_entry(
            builder = 'ci/Win7 Tests (1)',
            category = 'chromium.win|release|tester',
            short_name = '32',
        ),
        luci.console_view_entry(
            builder = 'ci/Win 7 Tests x64 (1)',
            category = 'chromium.win|release|tester',
            short_name = '64',
        ),
        luci.console_view_entry(
            builder = 'ci/Win10 Tests x64',
            category = 'chromium.win|release|tester',
            short_name = 'w10',
        ),
        luci.console_view_entry(
            builder = 'ci/Win x64 Builder (dbg)',
            category = 'chromium.win|debug|builder',
            short_name = '64',
        ),
        luci.console_view_entry(
            builder = 'ci/Win Builder (dbg)',
            category = 'chromium.win|debug|builder',
            short_name = '32',
        ),
        luci.console_view_entry(
            builder = 'ci/Win7 Tests (dbg)(1)',
            category = 'chromium.win|debug|tester',
            short_name = '7',
        ),
        luci.console_view_entry(
            builder = 'ci/Win10 Tests x64 (dbg)',
            category = 'chromium.win|debug|tester',
            short_name = '10',
        ),
        luci.console_view_entry(
            builder = 'ci/Windows deterministic',
            category = 'chromium.win|misc',
            short_name = 'det',
        ),
        luci.console_view_entry(
            builder = 'ci/WebKit Win10',
            category = 'chromium.win|misc',
            short_name = 'wbk',
        ),
        luci.console_view_entry(
            builder = 'ci/Mac Builder',
            category = 'chromium.mac|release',
            short_name = 'bld',
        ),
        luci.console_view_entry(
            builder = 'ci/Mac10.10 Tests',
            category = 'chromium.mac|release',
            short_name = '10',
        ),
        luci.console_view_entry(
            builder = 'ci/Mac10.11 Tests',
            category = 'chromium.mac|release',
            short_name = '11',
        ),
        luci.console_view_entry(
            builder = 'ci/Mac10.12 Tests',
            category = 'chromium.mac|release',
            short_name = '12',
        ),
        luci.console_view_entry(
            builder = 'ci/Mac10.13 Tests',
            category = 'chromium.mac|release',
            short_name = '13',
        ),
        luci.console_view_entry(
            builder = 'ci/WebKit Mac10.13 (retina)',
            category = 'chromium.mac|release',
            short_name = 'ret',
        ),
        luci.console_view_entry(
            builder = 'ci/Mac Builder (dbg)',
            category = 'chromium.mac|debug',
            short_name = 'bld',
        ),
        luci.console_view_entry(
            builder = 'ci/Mac10.13 Tests (dbg)',
            category = 'chromium.mac|debug',
            short_name = '13',
        ),
        luci.console_view_entry(
            builder = 'ci/ios-device',
            category = 'chromium.mac|ios|default',
            short_name = 'dev',
        ),
        luci.console_view_entry(
            builder = 'ci/ios-simulator',
            category = 'chromium.mac|ios|default',
            short_name = 'sim',
        ),
        luci.console_view_entry(
            builder = 'ci/ios-simulator-full-configs',
            category = 'chromium.mac|ios|default',
            short_name = 'ful',
        ),
        luci.console_view_entry(
            builder = 'ci/ios-simulator-noncq',
            category = 'chromium.mac|ios|default',
            short_name = 'non',
        ),
        luci.console_view_entry(
            builder = 'ci/ios-device-xcode-clang',
            category = 'chromium.mac|ios|xcode',
            short_name = 'dev',
        ),
        luci.console_view_entry(
            builder = 'ci/ios-simulator-xcode-clang',
            category = 'chromium.mac|ios|xcode',
            short_name = 'sim',
        ),
        luci.console_view_entry(
            builder = 'ci/ios-slimnav',
            category = 'chromium.mac|ios|misc',
            short_name = 'slim',
        ),
        luci.console_view_entry(
            builder = 'ci/Linux Builder',
            category = 'chromium.linux|release',
            short_name = 'bld',
        ),
        luci.console_view_entry(
            builder = 'ci/Linux Tests',
            category = 'chromium.linux|release',
            short_name = 'tst',
        ),
        luci.console_view_entry(
            builder = 'ci/linux-gcc-rel',
            category = 'chromium.linux|release',
            short_name = 'gcc',
        ),
        luci.console_view_entry(
            builder = 'ci/Deterministic Linux',
            category = 'chromium.linux|release',
            short_name = 'det',
        ),
        luci.console_view_entry(
            builder = 'ci/Leak Detection Linux',
            category = 'chromium.linux|release',
            short_name = 'lk',
        ),
        luci.console_view_entry(
            builder = 'ci/linux-ozone-rel',
            category = 'chromium.linux|release',
            short_name = 'ozo',
        ),
        luci.console_view_entry(
            builder = 'ci/linux-trusty-rel',
            category = 'chromium.linux|release',
            short_name = 'tru',
        ),
        luci.console_view_entry(
            builder = 'ci/Linux Builder (dbg)(32)',
            category = 'chromium.linux|debug|builder',
            short_name = '32',
        ),
        luci.console_view_entry(
            builder = 'ci/Linux Builder (dbg)',
            category = 'chromium.linux|debug|builder',
            short_name = '64',
        ),
        luci.console_view_entry(
            builder = 'ci/Deterministic Linux (dbg)',
            category = 'chromium.linux|debug|builder',
            short_name = 'det',
        ),
        luci.console_view_entry(
            builder = 'ci/Linux Tests (dbg)(1)',
            category = 'chromium.linux|debug|tester',
            short_name = '64',
        ),
        luci.console_view_entry(
            builder = 'ci/Cast Linux',
            category = 'chromium.linux|cast',
            short_name = 'vid',
        ),
        luci.console_view_entry(
            builder = 'ci/Cast Audio Linux',
            category = 'chromium.linux|cast',
            short_name = 'aud',
        ),
        luci.console_view_entry(
            builder = 'ci/Fuchsia ARM64',
            category = 'chromium.linux|fuchsia|a64',
        ),
        luci.console_view_entry(
            builder = 'ci/fuchsia-arm64-cast',
            category = 'chromium.linux|fuchsia|cast',
            short_name = 'a64',
        ),
        luci.console_view_entry(
            builder = 'ci/fuchsia-x64-cast',
            category = 'chromium.linux|fuchsia|cast',
            short_name = 'x64',
        ),
        luci.console_view_entry(
            builder = 'ci/fuchsia-x64-dbg',
            category = 'chromium.linux|fuchsia|x64',
            short_name = 'dbg',
        ),
        luci.console_view_entry(
            builder = 'ci/Deterministic Fuchsia (dbg)',
            category = 'chromium.linux|fuchsia|x64',
            short_name = 'det',
        ),
        luci.console_view_entry(
            builder = 'ci/Fuchsia x64',
            category = 'chromium.linux|fuchsia|x64',
            short_name = 'rel',
        ),
        luci.console_view_entry(
            builder = 'ci/Linux ChromiumOS Full',
            category = 'chromium.chromiumos|default',
            short_name = 'ful',
        ),
        luci.console_view_entry(
            builder = 'ci/linux-chromeos-rel',
            category = 'chromium.chromiumos|default',
            short_name = 'rel',
        ),
        luci.console_view_entry(
            builder = 'ci/linux-chromeos-dbg',
            category = 'chromium.chromiumos|default',
            short_name = 'dbg',
        ),
        luci.console_view_entry(
            builder = 'ci/chromeos-amd64-generic-asan-rel',
            category = 'chromium.chromiumos|simple|release|x64',
            short_name = 'asn',
        ),
        luci.console_view_entry(
            builder = 'ci/chromeos-amd64-generic-cfi-thin-lto-rel',
            category = 'chromium.chromiumos|simple|release|x64',
            short_name = 'cfi',
        ),
        luci.console_view_entry(
            builder = 'ci/chromeos-amd64-generic-dbg',
            category = 'chromium.chromiumos|simple|debug|x64',
            short_name = 'dbg',
        ),
        luci.console_view_entry(
            builder = 'ci/chromeos-amd64-generic-rel',
            category = 'chromium.chromiumos|simple|release|x64',
            short_name = 'rel',
        ),
        luci.console_view_entry(
            builder = 'ci/chromeos-arm-generic-dbg',
            category = 'chromium.chromiumos|simple|debug',
            short_name = 'arm',
        ),
        luci.console_view_entry(
            builder = 'ci/chromeos-arm-generic-rel',
            category = 'chromium.chromiumos|simple|release',
            short_name = 'arm',
        ),
        luci.console_view_entry(
            builder = 'ci/chromeos-kevin-rel',
            category = 'chromium.chromiumos|simple|release',
            short_name = 'kvn',
        ),
        luci.console_view_entry(
            builder = 'chrome:ci/linux-chromeos-google-rel',
            category = 'chrome',
            short_name = 'cro',
        ),
        luci.console_view_entry(
            builder = 'chrome:ci/linux-google-rel',
            category = 'chrome',
            short_name = 'lnx',
        ),
        luci.console_view_entry(
            builder = 'chrome:ci/mac-google-rel',
            category = 'chrome',
            short_name = 'mac',
        ),
        luci.console_view_entry(
            builder = 'chrome:ci/win-google-rel',
            category = 'chrome',
            short_name = 'win',
        ),
        luci.console_view_entry(
            builder = 'ci/win-asan',
            category = 'chromium.memory|win',
            short_name = 'asn',
        ),
        luci.console_view_entry(
            builder = 'ci/Mac ASan 64 Builder',
            category = 'chromium.memory|mac',
            short_name = 'bld',
        ),
        luci.console_view_entry(
            builder = 'ci/Mac ASan 64 Tests (1)',
            category = 'chromium.memory|mac',
            short_name = 'tst',
        ),
        luci.console_view_entry(
            builder = 'ci/Linux TSan Builder',
            category = 'chromium.memory|linux|TSan v2',
            short_name = 'bld',
        ),
        luci.console_view_entry(
            builder = 'ci/Linux TSan Tests',
            category = 'chromium.memory|linux|TSan v2',
            short_name = 'tst',
        ),
        luci.console_view_entry(
            builder = 'ci/Linux ASan LSan Builder',
            category = 'chromium.memory|linux|asan lsan',
            short_name = 'bld',
        ),
        luci.console_view_entry(
            builder = 'ci/Linux ASan LSan Tests (1)',
            category = 'chromium.memory|linux|asan lsan',
            short_name = 'tst',
        ),
        luci.console_view_entry(
            builder = 'ci/Linux ASan Tests (sandboxed)',
            category = 'chromium.memory|linux|asan lsan',
            short_name = 'sbx',
        ),
        luci.console_view_entry(
            builder = 'ci/Linux MSan Builder',
            category = 'chromium.memory|linux|msan',
            short_name = 'bld',
        ),
        luci.console_view_entry(
            builder = 'ci/Linux MSan Tests',
            category = 'chromium.memory|linux|msan',
            short_name = 'tst',
        ),
        luci.console_view_entry(
            builder = 'ci/WebKit Linux ASAN',
            category = 'chromium.memory|linux|webkit',
            short_name = 'asn',
        ),
        luci.console_view_entry(
            builder = 'ci/WebKit Linux MSAN',
            category = 'chromium.memory|linux|webkit',
            short_name = 'msn',
        ),
        luci.console_view_entry(
            builder = 'ci/WebKit Linux Leak',
            category = 'chromium.memory|linux|webkit',
            short_name = 'lk',
        ),
        luci.console_view_entry(
            builder = 'ci/Linux Chromium OS ASan LSan Builder',
            category = 'chromium.memory|cros|asan',
            short_name = 'bld',
        ),
        luci.console_view_entry(
            builder = 'ci/Linux Chromium OS ASan LSan Tests (1)',
            category = 'chromium.memory|cros|asan',
            short_name = 'tst',
        ),
        luci.console_view_entry(
            builder = 'ci/Linux ChromiumOS MSan Builder',
            category = 'chromium.memory|cros|msan',
            short_name = 'bld',
        ),
        luci.console_view_entry(
            builder = 'ci/Linux ChromiumOS MSan Tests',
            category = 'chromium.memory|cros|msan',
            short_name = 'tst',
        ),
        luci.console_view_entry(
            builder = 'ci/android-asan',
            category = 'chromium.memory|android',
            short_name = 'asn',
        ),
        luci.console_view_entry(
            builder = 'ci/Android CFI',
            category = 'chromium.memory|cfi',
            short_name = 'and',
        ),
        luci.console_view_entry(
            builder = 'ci/Linux CFI',
            category = 'chromium.memory|cfi',
            short_name = 'lnx',
        ),
    ],
)
