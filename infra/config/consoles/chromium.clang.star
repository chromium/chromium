luci.console_view(
    name = 'chromium.clang',
    header = '//consoles/chromium-header.textpb',
    repo = 'https://chromium.googlesource.com/chromium/src',
    entries = [
        luci.console_view_entry(
            builder = 'ci/ToTLinux',
            category = 'ToT Linux',
            short_name = 'rel',
        ),
        luci.console_view_entry(
            builder = 'chrome:ci/ToTLinuxOfficial',
            category = 'ToT Linux',
            short_name = 'ofi',
        ),
        luci.console_view_entry(
            builder = 'ci/ToTLinux (dbg)',
            category = 'ToT Linux',
            short_name = 'dbg',
        ),
        luci.console_view_entry(
            builder = 'ci/ToTLinuxASan',
            category = 'ToT Linux',
            short_name = 'asn',
        ),
        luci.console_view_entry(
            builder = 'ci/ToTLinuxASanLibfuzzer',
            category = 'ToT Linux',
            short_name = 'fuz',
        ),
        luci.console_view_entry(
            builder = 'ci/ToTLinuxMSan',
            category = 'ToT Linux',
            short_name = 'msn',
        ),
        luci.console_view_entry(
            builder = 'ci/ToTLinuxTSan',
            category = 'ToT Linux',
            short_name = 'tsn',
        ),
        luci.console_view_entry(
            builder = 'ci/ToTLinuxThinLTO',
            category = 'ToT Linux',
            short_name = 'lto',
        ),
        luci.console_view_entry(
            builder = 'ci/ToTLinuxUBSanVptr',
            category = 'ToT Linux',
            short_name = 'usn',
        ),
        luci.console_view_entry(
            builder = 'ci/ToTAndroid',
            category = 'ToT Android',
            short_name = 'rel',
        ),
        luci.console_view_entry(
            builder = 'ci/ToTAndroid (dbg)',
            category = 'ToT Android',
            short_name = 'dbg',
        ),
        luci.console_view_entry(
            builder = 'ci/ToTAndroid x64',
            category = 'ToT Android',
            short_name = 'x64',
        ),
        luci.console_view_entry(
            builder = 'ci/ToTAndroid64',
            category = 'ToT Android',
            short_name = 'a64',
        ),
        luci.console_view_entry(
            builder = 'ci/ToTAndroidASan',
            category = 'ToT Android',
            short_name = 'asn',
        ),
        luci.console_view_entry(
            builder = 'ci/ToTAndroidCFI',
            category = 'ToT Android',
            short_name = 'cfi',
        ),
        luci.console_view_entry(
            builder = 'ci/ToTAndroidOfficial',
            category = 'ToT Android',
            short_name = 'off',
        ),
        luci.console_view_entry(
            builder = 'ci/ToTMac',
            category = 'ToT Mac',
            short_name = 'rel',
        ),
        luci.console_view_entry(
            builder = 'chrome:ci/ToTMacOfficial',
            category = 'ToT Mac',
            short_name = 'ofi',
        ),
        luci.console_view_entry(
            builder = 'ci/ToTMac (dbg)',
            category = 'ToT Mac',
            short_name = 'dbg',
        ),
        luci.console_view_entry(
            builder = 'ci/ToTMacASan',
            category = 'ToT Mac',
            short_name = 'asn',
        ),
        luci.console_view_entry(
            builder = 'chrome:ci/ToTWin',
            category = 'ToT Windows',
            short_name = 'rel',
        ),
        luci.console_view_entry(
            builder = 'chrome:ci/ToTWinOfficial',
            category = 'ToT Windows',
            short_name = 'ofi',
        ),
        luci.console_view_entry(
            builder = 'ci/ToTWin(dbg)',
            category = 'ToT Windows',
            short_name = 'dbg',
        ),
        luci.console_view_entry(
            builder = 'ci/ToTWin(dll)',
            category = 'ToT Windows',
            short_name = 'dll',
        ),
        luci.console_view_entry(
            builder = 'chrome:ci/ToTWin64',
            category = 'ToT Windows|x64',
            short_name = 'rel',
        ),
        luci.console_view_entry(
            builder = 'ci/ToTWin64(dbg)',
            category = 'ToT Windows|x64',
            short_name = 'dbg',
        ),
        luci.console_view_entry(
            builder = 'ci/ToTWin64(dll)',
            category = 'ToT Windows|x64',
            short_name = 'dll',
        ),
        luci.console_view_entry(
            builder = 'chrome:ci/ToTWinThinLTO64',
            category = 'ToT Windows|x64',
            short_name = 'lto',
        ),
        luci.console_view_entry(
            builder = 'ci/ToTWinLibcxx64',
            category = 'ToT Windows|x64',
            short_name = 'cxx',
        ),
        luci.console_view_entry(
            builder = 'ci/CrWinAsan',
            category = 'ToT Windows|Asan',
            short_name = 'asn',
        ),
        luci.console_view_entry(
            builder = 'ci/CrWinAsan(dll)',
            category = 'ToT Windows|Asan',
            short_name = 'dll',
        ),
        luci.console_view_entry(
            builder = 'ci/ToTWinASanLibfuzzer',
            category = 'ToT Windows|Asan',
            short_name = 'fuz',
        ),
        luci.console_view_entry(
            builder = 'ci/linux-win_cross-rel',
            category = 'ToT Windows',
            short_name = 'lxw',
        ),
        luci.console_view_entry(
            builder = 'ci/ToTLinuxCoverage',
            category = 'ToT Code Coverage',
            short_name = 'linux',
        ),
        luci.console_view_entry(
            builder = 'ci/ToTMacCoverage',
            category = 'ToT Code Coverage',
            short_name = 'mac',
        ),
        luci.console_view_entry(
            builder = 'ci/CFI Linux CF',
            category = 'CFI|Linux',
            short_name = 'CF',
        ),
        luci.console_view_entry(
            builder = 'ci/CFI Linux ToT',
            category = 'CFI|Linux',
            short_name = 'ToT',
        ),
        luci.console_view_entry(
            builder = 'ci/ToTWinCFI',
            category = 'CFI|Win',
            short_name = 'x86',
        ),
        luci.console_view_entry(
            builder = 'ci/ToTWinCFI64',
            category = 'CFI|Win',
            short_name = 'x64',
        ),
        luci.console_view_entry(
            builder = 'ci/ToTiOS',
            category = 'iOS',
            short_name = 'sim',
        ),
        luci.console_view_entry(
            builder = 'ci/ToTiOSDevice',
            category = 'iOS',
            short_name = 'dev',
        ),
        luci.console_view_entry(
            builder = 'ci/UBSanVptr Linux',
            short_name = 'usn',
        ),
    ],
)
