luci.console_view(
    name = 'chromium.fuzz',
    header = '//consoles/chromium-header.textpb',
    repo = 'https://chromium.googlesource.com/chromium/src',
    entries = [
        luci.console_view_entry(
            builder = 'ci/Afl Upload Linux ASan',
            category = 'afl',
            short_name = 'afl',
        ),
        luci.console_view_entry(
            builder = 'ci/Win ASan Release',
            category = 'win asan',
            short_name = 'rel',
        ),
        luci.console_view_entry(
            builder = 'ci/Win ASan Release Media',
            category = 'win asan',
            short_name = 'med',
        ),
        luci.console_view_entry(
            builder = 'ci/Mac ASAN Release',
            category = 'mac asan',
            short_name = 'rel',
        ),
        luci.console_view_entry(
            builder = 'ci/Mac ASAN Release Media',
            category = 'mac asan',
            short_name = 'med',
        ),
        luci.console_view_entry(
            builder = 'ci/ChromiumOS ASAN Release',
            short_name = 'cro',
        ),
        luci.console_view_entry(
            builder = 'ci/ASAN Debug',
            category = 'linux asan',
            short_name = 'dbg',
        ),
        luci.console_view_entry(
            builder = 'ci/ASAN Release',
            category = 'linux asan',
            short_name = 'rel',
        ),
        luci.console_view_entry(
            builder = 'ci/ASAN Release Media',
            category = 'linux asan',
            short_name = 'med',
        ),
        luci.console_view_entry(
            builder = 'ci/ASan Debug (32-bit x86 with V8-ARM)',
            category = 'linux asan|x64 v8-ARM',
            short_name = 'dbg',
        ),
        luci.console_view_entry(
            builder = 'ci/ASan Release (32-bit x86 with V8-ARM)',
            category = 'linux asan|x64 v8-ARM',
            short_name = 'rel',
        ),
        luci.console_view_entry(
            builder = 'ci/ASan Release Media (32-bit x86 with V8-ARM)',
            category = 'linux asan|x64 v8-ARM',
            short_name = 'med',
        ),
        luci.console_view_entry(
            builder = 'ci/Libfuzzer Upload Chrome OS ASan',
            category = 'libfuzz',
            short_name = 'chromeos-asan',
        ),
        luci.console_view_entry(
            builder = 'ci/Libfuzzer Upload Linux32 ASan',
            category = 'libfuzz',
            short_name = 'linux32',
        ),
        luci.console_view_entry(
            builder = 'ci/Libfuzzer Upload Linux32 ASan Debug',
            category = 'libfuzz',
            short_name = 'linux32-dbg',
        ),
        luci.console_view_entry(
            builder = 'ci/Libfuzzer Upload Linux ASan',
            category = 'libfuzz',
            short_name = 'linux',
        ),
        luci.console_view_entry(
            builder = 'ci/Libfuzzer Upload Linux ASan Debug',
            category = 'libfuzz',
            short_name = 'linux-dbg',
        ),
        luci.console_view_entry(
            builder = 'ci/Libfuzzer Upload Linux MSan',
            category = 'libfuzz',
            short_name = 'linux-msan',
        ),
        luci.console_view_entry(
            builder = 'ci/Libfuzzer Upload Linux UBSan',
            category = 'libfuzz',
            short_name = 'linux-ubsan',
        ),
        luci.console_view_entry(
            builder = 'ci/Libfuzzer Upload Mac ASan',
            category = 'libfuzz',
            short_name = 'mac-asan',
        ),
        luci.console_view_entry(
            builder = 'ci/Libfuzzer Upload Windows ASan',
            category = 'libfuzz',
            short_name = 'win-asan',
        ),
        luci.console_view_entry(
            builder = 'ci/Libfuzzer Upload Linux32 V8-ARM ASan',
            category = 'libfuzz',
            short_name = 'arm',
        ),
        luci.console_view_entry(
            builder = 'ci/Libfuzzer Upload Linux32 V8-ARM ASan Debug',
            category = 'libfuzz',
            short_name = 'arm-dbg',
        ),
        luci.console_view_entry(
            builder = 'ci/Libfuzzer Upload Linux V8-ARM64 ASan',
            category = 'libfuzz',
            short_name = 'arm64',
        ),
        luci.console_view_entry(
            builder = 'ci/Libfuzzer Upload Linux V8-ARM64 ASan Debug',
            category = 'libfuzz',
            short_name = 'arm64-dbg',
        ),
        luci.console_view_entry(
            builder = 'ci/MSAN Release (chained origins)',
            category = 'linux msan',
            short_name = 'org',
        ),
        luci.console_view_entry(
            builder = 'ci/MSAN Release (no origins)',
            category = 'linux msan',
            short_name = 'rel',
        ),
        luci.console_view_entry(
            builder = 'ci/TSAN Debug',
            category = 'linux tsan',
            short_name = 'dbg',
        ),
        luci.console_view_entry(
            builder = 'ci/TSAN Release',
            category = 'linux tsan',
            short_name = 'rel',
        ),
        luci.console_view_entry(
            builder = 'ci/UBSan Release',
            category = 'linux UBSan',
            short_name = 'rel',
        ),
        luci.console_view_entry(
            builder = 'ci/UBSan vptr Release',
            category = 'linux UBSan',
            short_name = 'vpt',
        ),
    ],
)
