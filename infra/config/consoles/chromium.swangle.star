luci.console_view(
    name = 'chromium.swangle',
    header = '//consoles/chromium-header.textpb',
    repo = 'https://chromium.googlesource.com/chromium/src',
    entries = [
        luci.console_view_entry(
            builder = 'ci/win-swangle-x86',
            category = 'DEPS|Windows',
            short_name = 'x86',
        ),
        luci.console_view_entry(
            builder = 'ci/win-swangle-x64',
            category = 'DEPS|Windows',
            short_name = 'x64',
        ),
        luci.console_view_entry(
            builder = 'ci/linux-swangle-x86',
            category = 'DEPS|Linux',
            short_name = 'x86',
        ),
        luci.console_view_entry(
            builder = 'ci/linux-swangle-x64',
            category = 'DEPS|Linux',
            short_name = 'x64',
        ),
        luci.console_view_entry(
            builder = 'ci/win-swangle-tot-angle-x86',
            category = 'ToT ANGLE|Windows',
            short_name = 'x86',
        ),
        luci.console_view_entry(
            builder = 'ci/win-swangle-tot-angle-x64',
            category = 'ToT ANGLE|Windows',
            short_name = 'x64',
        ),
        luci.console_view_entry(
            builder = 'ci/linux-swangle-tot-angle-x86',
            category = 'ToT ANGLE|Linux',
            short_name = 'x86',
        ),
        luci.console_view_entry(
            builder = 'ci/linux-swangle-tot-angle-x64',
            category = 'ToT ANGLE|Linux',
            short_name = 'x64',
        ),
        luci.console_view_entry(
            builder = 'ci/win-swangle-tot-swiftshader-x86',
            category = 'ToT SwiftShader|Windows',
            short_name = 'x86',
        ),
        luci.console_view_entry(
            builder = 'ci/win-swangle-tot-swiftshader-x64',
            category = 'ToT SwiftShader|Windows',
            short_name = 'x64',
        ),
        luci.console_view_entry(
            builder = 'ci/linux-swangle-tot-swiftshader-x86',
            category = 'ToT SwiftShader|Linux',
            short_name = 'x86',
        ),
        luci.console_view_entry(
            builder = 'ci/linux-swangle-tot-swiftshader-x64',
            category = 'ToT SwiftShader|Linux',
            short_name = 'x64',
        ),
    ],
)
