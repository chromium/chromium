luci.console_view(
    name = 'chromium.dawn',
    header = '//consoles/chromium-header.textpb',
    repo = 'https://chromium.googlesource.com/chromium/src',
    entries = [
        luci.console_view_entry(
            builder = 'ci/Dawn Linux x64 Builder',
            category = 'ToT|Linux|Builder',
            short_name = 'x64',
        ),
        luci.console_view_entry(
            builder = 'ci/Dawn Linux x64 Release (Intel HD 630)',
            category = 'ToT|Linux|Intel',
            short_name = 'x64',
        ),
        luci.console_view_entry(
            builder = 'ci/Dawn Linux x64 Release (NVIDIA)',
            category = 'ToT|Linux|Nvidia',
            short_name = 'x64',
        ),
        luci.console_view_entry(
            builder = 'ci/Dawn Mac x64 Builder',
            category = 'ToT|Mac|Builder',
            short_name = 'x64',
        ),
        luci.console_view_entry(
            builder = 'ci/Dawn Mac x64 Release (AMD)',
            category = 'ToT|Mac|AMD',
            short_name = 'x64',
        ),
        luci.console_view_entry(
            builder = 'ci/Dawn Mac x64 Release (Intel)',
            category = 'ToT|Mac|Intel',
            short_name = 'x64',
        ),
        luci.console_view_entry(
            builder = 'ci/Dawn Win10 x86 Builder',
            category = 'ToT|Windows|Builder',
            short_name = 'x86',
        ),
        luci.console_view_entry(
            builder = 'ci/Dawn Win10 x64 Builder',
            category = 'ToT|Windows|Builder',
            short_name = 'x64',
        ),
        luci.console_view_entry(
            builder = 'ci/Dawn Win10 x86 Release (Intel HD 630)',
            category = 'ToT|Windows|Intel',
            short_name = 'x86',
        ),
        luci.console_view_entry(
            builder = 'ci/Dawn Win10 x64 Release (Intel HD 630)',
            category = 'ToT|Windows|Intel',
            short_name = 'x64',
        ),
        luci.console_view_entry(
            builder = 'ci/Dawn Win10 x86 Release (NVIDIA)',
            category = 'ToT|Windows|Nvidia',
            short_name = 'x86',
        ),
        luci.console_view_entry(
            builder = 'ci/Dawn Win10 x64 Release (NVIDIA)',
            category = 'ToT|Windows|Nvidia',
            short_name = 'x64',
        ),
        luci.console_view_entry(
            builder = 'ci/Dawn Linux x64 DEPS Builder',
            category = 'DEPS|Linux|Builder',
            short_name = 'x64',
        ),
        luci.console_view_entry(
            builder = 'ci/Dawn Linux x64 DEPS Release (Intel HD 630)',
            category = 'DEPS|Linux|Intel',
            short_name = 'x64',
        ),
        luci.console_view_entry(
            builder = 'ci/Dawn Linux x64 DEPS Release (NVIDIA)',
            category = 'DEPS|Linux|Nvidia',
            short_name = 'x64',
        ),
        luci.console_view_entry(
            builder = 'ci/Dawn Mac x64 DEPS Builder',
            category = 'DEPS|Mac|Builder',
            short_name = 'x64',
        ),
        luci.console_view_entry(
            builder = 'ci/Dawn Mac x64 DEPS Release (AMD)',
            category = 'DEPS|Mac|AMD',
            short_name = 'x64',
        ),
        luci.console_view_entry(
            builder = 'ci/Dawn Mac x64 DEPS Release (Intel)',
            category = 'DEPS|Mac|Intel',
            short_name = 'x64',
        ),
        luci.console_view_entry(
            builder = 'ci/Dawn Win10 x86 DEPS Builder',
            category = 'DEPS|Windows|Builder',
            short_name = 'x86',
        ),
        luci.console_view_entry(
            builder = 'ci/Dawn Win10 x64 DEPS Builder',
            category = 'DEPS|Windows|Builder',
            short_name = 'x64',
        ),
        luci.console_view_entry(
            builder = 'ci/Dawn Win10 x86 DEPS Release (Intel HD 630)',
            category = 'DEPS|Windows|Intel',
            short_name = 'x86',
        ),
        luci.console_view_entry(
            builder = 'ci/Dawn Win10 x64 DEPS Release (Intel HD 630)',
            category = 'DEPS|Windows|Intel',
            short_name = 'x64',
        ),
        luci.console_view_entry(
            builder = 'ci/Dawn Win10 x86 DEPS Release (NVIDIA)',
            category = 'DEPS|Windows|Nvidia',
            short_name = 'x86',
        ),
        luci.console_view_entry(
            builder = 'ci/Dawn Win10 x64 DEPS Release (NVIDIA)',
            category = 'DEPS|Windows|Nvidia',
            short_name = 'x64',
        ),
    ],
)
