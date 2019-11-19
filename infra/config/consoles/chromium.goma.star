luci.console_view(
    name = 'chromium.goma',
    header = '//consoles/chromium-header.textpb',
    include_experimental_builds = True,
    repo = 'https://chromium.googlesource.com/chromium/src',
    entries = [
        luci.console_view_entry(
            builder = 'goma/Chromium Linux Goma Staging',
            category = 'clients5',
            short_name = 'lnx',
        ),
        luci.console_view_entry(
            builder = 'goma/Chromium Mac Goma Staging',
            category = 'clients5',
            short_name = 'mac',
        ),
        luci.console_view_entry(
            builder = 'goma/CrWinGomaStaging',
            category = 'clients5',
            short_name = 'win',
        ),
        luci.console_view_entry(
            builder = 'goma/Chromium Linux Goma RBE ToT',
            category = 'rbe|tot|linux|rel',
        ),
        luci.console_view_entry(
            builder = 'goma/Chromium Linux Goma RBE ToT (ATS)',
            category = 'rbe|tot|linux|rel',
            short_name = 'ats',
        ),
        luci.console_view_entry(
            builder = 'goma/Chromium Mac Goma RBE ToT',
            category = 'rbe|tot|mac|rel',
        ),
        luci.console_view_entry(
            builder = 'goma/Chromium Win Goma RBE ToT',
            category = 'rbe|tot|win|rel',
        ),
        luci.console_view_entry(
            builder = 'goma/Chromium Android ARM 32-bit Goma RBE ToT',
            category = 'rbe|tot|android arm|rel',
        ),
        luci.console_view_entry(
            builder = 'goma/Chromium Android ARM 32-bit Goma RBE ToT (ATS)',
            category = 'rbe|tot|android arm|rel',
            short_name = 'ats',
        ),
        luci.console_view_entry(
            builder = 'goma/Chromium Linux Goma RBE Staging (clobber)',
            category = 'rbe|staging|linux|rel',
            short_name = 'clb',
        ),
        luci.console_view_entry(
            builder = 'goma/Chromium Linux Goma RBE Staging',
            category = 'rbe|staging|linux|rel',
        ),
        luci.console_view_entry(
            builder = 'goma/Chromium Linux Goma RBE Staging (dbg) (clobber)',
            category = 'rbe|staging|linux|debug',
            short_name = 'clb',
        ),
        luci.console_view_entry(
            builder = 'goma/Chromium Linux Goma RBE Staging (dbg)',
            category = 'rbe|staging|linux|debug',
        ),
        luci.console_view_entry(
            builder = 'goma/Chromium Mac Goma RBE Staging (clobber)',
            category = 'rbe|staging|mac|rel',
            short_name = 'clb',
        ),
        luci.console_view_entry(
            builder = 'goma/Chromium Mac Goma RBE Staging',
            category = 'rbe|staging|mac|rel',
        ),
        luci.console_view_entry(
            builder = 'goma/Chromium Mac Goma RBE Staging (dbg)',
            category = 'rbe|staging|mac|debug',
        ),
        luci.console_view_entry(
            builder = 'goma/Chromium Win Goma RBE Staging',
            category = 'rbe|staging|win|rel',
        ),
        luci.console_view_entry(
            builder = 'goma/Chromium Win Goma RBE Staging (clobber)',
            category = 'rbe|staging|win|rel',
            short_name = 'clb',
        ),
        luci.console_view_entry(
            builder = 'goma/Chromium Android ARM 32-bit Goma RBE Staging',
            category = 'rbe|staging|android arm|rel',
        ),
    ],
)
