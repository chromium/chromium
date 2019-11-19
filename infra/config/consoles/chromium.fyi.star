luci.console_view(
    name = 'chromium.fyi',
    header = '//consoles/chromium-header.textpb',
    repo = 'https://chromium.googlesource.com/chromium/src',
    entries = [
        luci.console_view_entry(
            builder = 'ci/Closure Compilation Linux',
            category = 'closure_compilation',
        ),
        luci.console_view_entry(
            builder = 'ci/android-code-coverage',
            category = 'code_coverage',
            short_name = 'and',
        ),
        luci.console_view_entry(
            builder = 'ci/android-code-coverage-native',
            category = 'code_coverage',
            short_name = 'ann',
        ),
        luci.console_view_entry(
            builder = 'ci/linux-code-coverage',
            category = 'code_coverage',
            short_name = 'lnx',
        ),
        luci.console_view_entry(
            builder = 'ci/chromeos-vm-code-coverage',
            category = 'code_coverage',
            short_name = 'vm',
        ),
        luci.console_view_entry(
            builder = 'ci/linux-chromeos-code-coverage',
            category = 'code_coverage',
            short_name = 'lcr',
        ),
        luci.console_view_entry(
            builder = 'ci/mac-code-coverage',
            category = 'code_coverage',
            short_name = 'mac',
        ),
        luci.console_view_entry(
            builder = 'ci/ios-simulator-code-coverage',
            category = 'code_coverage',
            short_name = 'ios',
        ),
        luci.console_view_entry(
            builder = 'ci/win10-code-coverage',
            category = 'code_coverage',
            short_name = 'win',
        ),
        luci.console_view_entry(
            builder = 'ci/ios-simulator-cronet',
            category = 'cronet',
        ),
        luci.console_view_entry(
            builder = 'ci/Mac Builder Next',
            category = 'mac',
            short_name = 'bld',
        ),
        luci.console_view_entry(
            builder = 'ci/Mac10.14 Tests',
            category = 'mac',
            short_name = '14',
        ),
        luci.console_view_entry(
            builder = 'ci/mac-hermetic-upgrade-rel',
            category = 'mac',
            short_name = 'herm',
        ),
        luci.console_view_entry(
            builder = 'ci/mac-osxbeta-rel',
            category = 'mac',
            short_name = 'beta',
        ),
        luci.console_view_entry(
            builder = 'ci/Mac deterministic',
            category = 'deterministic|mac',
            short_name = 'rel',
        ),
        luci.console_view_entry(
            builder = 'ci/Mac deterministic (dbg)',
            category = 'deterministic|mac',
            short_name = 'dbg',
        ),
        luci.console_view_entry(
            builder = 'ci/fuchsia-fyi-arm64-rel',
            category = 'fuchsia',
        ),
        luci.console_view_entry(
            builder = 'ci/fuchsia-fyi-x64-dbg',
            category = 'fuchsia',
        ),
        luci.console_view_entry(
            builder = 'ci/fuchsia-fyi-x64-rel',
            category = 'fuchsia',
        ),
        luci.console_view_entry(
            builder = 'ci/chromeos-amd64-generic-rel-vm-tests',
            category = 'chromeos',
        ),
        luci.console_view_entry(
            builder = 'ci/chromeos-kevin-rel-hw-tests',
            category = 'chromos',
        ),
        luci.console_view_entry(
            builder = 'ci/ios-simulator-cr-recipe',
            category = 'iOS',
            short_name = 'chr',
        ),
        luci.console_view_entry(
            builder = 'ci/ios-webkit-tot',
            category = 'iOS',
            short_name = 'wk',
        ),
        luci.console_view_entry(
            builder = 'ci/ios13-sdk-device',
            category = 'iOS|iOS13',
            short_name = 'dev',
        ),
        luci.console_view_entry(
            builder = 'ci/ios13-sdk-simulator',
            category = 'iOS|iOS13',
            short_name = 'sim',
        ),
        luci.console_view_entry(
            builder = 'ci/ios13-beta-simulator',
            category = 'iOS|iOS13',
            short_name = 'ios13',
        ),
        luci.console_view_entry(
            builder = 'ci/linux-blink-animation-use-time-delta',
            category = 'linux|blink',
            short_name = 'TD',
        ),
        luci.console_view_entry(
            builder = 'ci/linux-blink-heap-concurrent-marking-tsan-rel',
            category = 'linux|blink',
            short_name = 'CM',
        ),
        luci.console_view_entry(
            builder = 'ci/linux-blink-heap-verification',
            category = 'linux|blink',
            short_name = 'VF',
        ),
        luci.console_view_entry(
            builder = 'ci/VR Linux',
            category = 'linux',
        ),
        luci.console_view_entry(
            builder = 'ci/linux-bfcache-debug',
            category = 'linux',
        ),
        luci.console_view_entry(
            builder = 'ci/linux-oor-cors-rel',
            category = 'linux',
        ),
        luci.console_view_entry(
            builder = 'ci/linux-fieldtrial-rel',
            category = 'linux',
        ),
        luci.console_view_entry(
            builder = 'ci/linux-wpt-fyi-rel',
            category = 'linux',
        ),
        luci.console_view_entry(
            builder = 'ci/Mojo Android',
            category = 'mojo',
            short_name = 'and',
        ),
        luci.console_view_entry(
            builder = 'ci/android-mojo-webview-rel',
            category = 'mojo',
            short_name = 'aw',
        ),
        luci.console_view_entry(
            builder = 'ci/Mojo ChromiumOS',
            category = 'mojo',
            short_name = 'cr',
        ),
        luci.console_view_entry(
            builder = 'ci/Mojo Linux',
            category = 'mojo',
            short_name = 'lnx',
        ),
        luci.console_view_entry(
            builder = 'ci/mac-mojo-rel',
            category = 'mojo',
            short_name = 'mac',
        ),
        luci.console_view_entry(
            builder = 'ci/Mojo Windows',
            category = 'mojo',
            short_name = 'win',
        ),
        luci.console_view_entry(
            builder = 'ci/linux-chromium-tests-staging-builder',
            category = 'recipe|staging|linux',
            short_name = 'bld',
        ),
        luci.console_view_entry(
            builder = 'ci/linux-chromium-tests-staging-tests',
            category = 'recipe|staging|linux',
            short_name = 'tst',
        ),
        luci.console_view_entry(
            builder = 'ci/Linux remote_run Builder',
            category = 'remote_run',
        ),
        luci.console_view_entry(
            builder = 'ci/Linux remote_run Tester',
            category = 'remote_run',
        ),
        luci.console_view_entry(
            builder = 'ci/Site Isolation Android',
            category = 'site_isolation',
        ),
        luci.console_view_entry(
            builder = 'ci/linux-annotator-rel',
            category = 'network|traffic|annotations',
            short_name = 'lnx',
        ),
        luci.console_view_entry(
            builder = 'ci/win-annotator-rel',
            category = 'network|traffic|annotations',
            short_name = 'win',
        ),
        luci.console_view_entry(
            builder = 'ci/Linux Viz',
            category = 'viz',
        ),
        luci.console_view_entry(
            builder = 'ci/Win 10 Fast Ring',
            category = 'win10',
        ),
        luci.console_view_entry(
            builder = 'ci/win-pixel-builder-rel',
            category = 'win10',
        ),
        luci.console_view_entry(
            builder = 'ci/win-pixel-tester-rel',
            category = 'win10',
        ),
        luci.console_view_entry(
            builder = 'ci/Win10 Tests x64 1803',
            category = 'win10|1803',
        ),
        luci.console_view_entry(
            builder = 'ci/win32-arm64-rel',
            category = 'win32|arm64',
        ),
        luci.console_view_entry(
            builder = 'ci/win-celab-builder-rel',
            category = 'celab',
        ),
        luci.console_view_entry(
            builder = 'ci/win-celab-tester-rel',
            category = 'celab',
        ),
    ],
)
