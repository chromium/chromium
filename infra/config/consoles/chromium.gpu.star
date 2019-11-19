luci.console_view(
    name = 'chromium.gpu',
    header = '//consoles/chromium-header.textpb',
    repo = 'https://chromium.googlesource.com/chromium/src',
    entries = [
        luci.console_view_entry(
            builder = 'ci/GPU Win x64 Builder',
            category = 'Windows',
        ),
        luci.console_view_entry(
            builder = 'ci/GPU Win x64 Builder (dbg)',
            category = 'Windows',
        ),
        luci.console_view_entry(
            builder = 'ci/Win10 x64 Debug (NVIDIA)',
            category = 'Windows',
        ),
        luci.console_view_entry(
            builder = 'ci/Win10 x64 Release (NVIDIA)',
            category = 'Windows',
        ),
        luci.console_view_entry(
            builder = 'ci/GPU Mac Builder',
            category = 'Mac',
        ),
        luci.console_view_entry(
            builder = 'ci/GPU Mac Builder (dbg)',
            category = 'Mac',
        ),
        luci.console_view_entry(
            builder = 'ci/Mac Debug (Intel)',
            category = 'Mac',
        ),
        luci.console_view_entry(
            builder = 'ci/Mac Release (Intel)',
            category = 'Mac',
        ),
        luci.console_view_entry(
            builder = 'ci/Mac Retina Debug (AMD)',
            category = 'Mac',
        ),
        luci.console_view_entry(
            builder = 'ci/Mac Retina Release (AMD)',
            category = 'Mac',
        ),
        luci.console_view_entry(
            builder = 'ci/GPU Linux Builder',
            category = 'Linux',
        ),
        luci.console_view_entry(
            builder = 'ci/GPU Linux Builder (dbg)',
            category = 'Linux',
        ),
        luci.console_view_entry(
            builder = 'ci/Linux Debug (NVIDIA)',
            category = 'Linux',
        ),
        luci.console_view_entry(
            builder = 'ci/Linux Release (NVIDIA)',
            category = 'Linux',
        ),
        luci.console_view_entry(
            builder = 'ci/Android Release (Nexus 5X)',
            category = 'Android',
        ),
    ],
)
