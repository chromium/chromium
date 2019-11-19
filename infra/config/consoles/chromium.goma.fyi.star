luci.console_view(
    name = 'chromium.goma.fyi',
    header = '//consoles/chromium-header.textpb',
    include_experimental_builds = True,
    repo = 'https://chromium.googlesource.com/chromium/src',
    entries = [
        luci.console_view_entry(
            builder = 'goma/Chromium Linux Goma RBE Prod',
            category = 'prod|linux|rel',
        ),
        luci.console_view_entry(
            builder = 'goma/Chromium Linux Goma RBE Prod (clobber)',
            category = 'prod|linux|rel',
            short_name = 'clb',
        ),
        luci.console_view_entry(
            builder = 'goma/Chromium Linux Goma RBE Prod (dbg)',
            category = 'prod|linux|dbg',
        ),
        luci.console_view_entry(
            builder = 'goma/Chromium Linux Goma RBE Prod (dbg) (clobber)',
            category = 'prod|linux|dbg',
            short_name = 'clb',
        ),
        luci.console_view_entry(
            builder = 'goma/Chromium Mac Goma RBE Prod',
            category = 'prod|mac|rel',
        ),
        luci.console_view_entry(
            builder = 'goma/Chromium Win Goma RBE Prod',
            category = 'prod|win|rel',
        ),
        luci.console_view_entry(
            builder = 'goma/Chromium Win Goma RBE Prod (clobber)',
            category = 'prod|win|rel',
            short_name = 'clb',
        ),
        luci.console_view_entry(
            builder = 'goma/Chromium Win Goma RBE Prod (dbg)',
            category = 'prod|win|dbg',
        ),
        luci.console_view_entry(
            builder = 'goma/Chromium Win Goma RBE Prod (dbg) (clobber)',
            category = 'prod|win|dbg',
            short_name = 'clb',
        ),
        luci.console_view_entry(
            builder = 'goma/Chromium Android ARM 32-bit Goma RBE Prod',
            category = 'prod|android arm|rel',
        ),
        luci.console_view_entry(
            builder = 'goma/Chromium Android ARM 32-bit Goma RBE Prod (clobber)',
            category = 'prod|android arm|rel',
            short_name = 'clb',
        ),
        luci.console_view_entry(
            builder = 'goma/Chromium Android ARM 32-bit Goma RBE Prod (dbg)',
            category = 'prod|android arm|dbg',
        ),
        luci.console_view_entry(
            builder = 'goma/Chromium Android ARM 32-bit Goma RBE Prod (dbg) (clobber)',
            category = 'prod|android arm|dbg',
            short_name = 'clb',
        ),
        luci.console_view_entry(
            builder = 'goma/fuchsia-fyi-arm64-rel (Goma RBE FYI)',
            category = 'prod|chromium.linux|fuchsia|misc',
            short_name = 'a64',
        ),
        luci.console_view_entry(
            builder = 'goma/fuchsia-fyi-x64-rel (Goma RBE FYI)',
            category = 'prod|chromium.linux|fuchsia|misc',
            short_name = 'x64',
        ),
        luci.console_view_entry(
            builder = 'goma/chromeos-amd64-generic-rel (Goma RBE FYI)',
            category = 'prod|chromium.chromiumos|simple|release|x64',
            short_name = 'rel',
        ),
    ],
)
