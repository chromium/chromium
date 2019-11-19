luci.console_view(
    name = 'goma.latest',
    header = '//consoles/chromium-header.textpb',
    repo = 'https://chromium.googlesource.com/chromium/src',
    entries = [
        luci.console_view_entry(
            builder = 'goma/Win Builder Goma Latest Client',
            category = 'win|rel',
        ),
        luci.console_view_entry(
            builder = 'goma/Win Builder (dbg) Goma Latest Client',
            category = 'win|dbg',
        ),
        luci.console_view_entry(
            builder = 'goma/win32-archive-rel-goma-latest-localoutputcache',
            category = 'win|rel',
            short_name = 'loc',
        ),
        luci.console_view_entry(
            builder = 'goma/Win cl.exe Goma Latest Client LocalOutputCache',
            category = 'cl.exe|rel',
            short_name = 'loc',
        ),
        luci.console_view_entry(
            builder = 'goma/Win7 Builder Goma Latest Client',
            category = 'win7|rel',
        ),
        luci.console_view_entry(
            builder = 'goma/Win7 Builder (dbg) Goma Latest Client',
            category = 'win7|dbg',
        ),
        luci.console_view_entry(
            builder = 'goma/WinMSVC64 Goma Latest Client',
            category = 'cl.exe|rel',
        ),
        luci.console_view_entry(
            builder = 'goma/Mac Builder Goma Latest Client',
            category = 'mac|rel',
        ),
        luci.console_view_entry(
            builder = 'goma/Mac Builder (dbg) Goma Latest Client',
            category = 'mac|dbg',
        ),
        luci.console_view_entry(
            builder = 'goma/mac-archive-rel-goma-latest',
            category = 'mac|rel',
            short_name = 'clb',
        ),
        luci.console_view_entry(
            builder = 'goma/Mac Builder (dbg) Goma Latest Client (clobber)',
            category = 'mac|dbg',
            short_name = 'clb',
        ),
        luci.console_view_entry(
            builder = 'goma/mac-archive-rel-goma-latest-localoutputcache',
            category = 'mac|rel',
            short_name = 'loc',
        ),
        luci.console_view_entry(
            builder = 'goma/chromeos-amd64-generic-rel-goma-latest',
            category = 'cros|rel',
        ),
        luci.console_view_entry(
            builder = 'goma/Linux Builder Goma Latest Client',
            category = 'linux|rel',
        ),
        luci.console_view_entry(
            builder = 'goma/linux-archive-rel-goma-latest',
            category = 'linux|rel',
            short_name = 'clb',
        ),
        luci.console_view_entry(
            builder = 'goma/linux-archive-rel-goma-latest-localoutputcache',
            category = 'linux|rel',
            short_name = 'loc',
        ),
        luci.console_view_entry(
            builder = 'goma/android-archive-dbg-goma-latest',
            category = 'android|dbg',
        ),
        luci.console_view_entry(
            builder = 'goma/ios-device-goma-latest-clobber',
            category = 'ios',
            short_name = 'clb',
        ),
  # RBE
        luci.console_view_entry(
            builder = 'goma/linux-archive-rel-goma-rbe-latest',
            category = 'rbe|linux|rel',
            short_name = 'clb',
        ),
        luci.console_view_entry(
            builder = 'goma/linux-archive-rel-goma-rbe-ats-latest',
            category = 'rbe|linux|rel',
            short_name = 'ats',
        ),
        luci.console_view_entry(
            builder = 'goma/Linux Builder Goma RBE Latest Client',
            category = 'rbe|linux|rel',
        ),
        luci.console_view_entry(
            builder = 'goma/chromeos-amd64-generic-rel-goma-rbe-latest',
            category = 'rbe|cros|rel',
        ),
        luci.console_view_entry(
            builder = 'goma/android-archive-dbg-goma-rbe-latest',
            category = 'rbe|android|dbg',
        ),
        luci.console_view_entry(
            builder = 'goma/android-archive-dbg-goma-rbe-ats-latest',
            category = 'rbe|android|dbg',
            short_name = 'ats',
        ),
        luci.console_view_entry(
            builder = 'goma/mac-archive-rel-goma-rbe-latest',
            category = 'rbe|mac|rel',
            short_name = 'clb',
        ),
        luci.console_view_entry(
            builder = 'goma/Mac Builder (dbg) Goma RBE Latest Client (clobber)',
            category = 'rbe|mac|dbg',
            short_name = 'clb',
        ),
        luci.console_view_entry(
            builder = 'goma/ios-device-goma-rbe-latest-clobber',
            category = 'rbe|ios',
            short_name = 'clb',
        ),
        luci.console_view_entry(
            builder = 'goma/Win Builder Goma RBE Latest Client',
            category = 'rbe|win|rel',
        ),
        luci.console_view_entry(
            builder = 'goma/Win Builder (dbg) Goma RBE Latest Client',
            category = 'rbe|win|dbg',
        ),
    ],
)
