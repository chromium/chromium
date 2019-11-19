luci.console_view(
    name = 'chromium.swarm',
    header = '//dev/consoles/chromium-header.textpb',
    repo = 'https://chromium.googlesource.com/chromium/src',
    entries = [
        luci.console_view_entry(builder = 'ci/Android N5 Swarm'),
        luci.console_view_entry(builder = 'ci/Android N5X Swarm'),
        luci.console_view_entry(builder = 'ci/ChromeOS Swarm'),
        luci.console_view_entry(builder = 'ci/Linux Swarm'),
        luci.console_view_entry(builder = 'ci/Mac Swarm'),
        luci.console_view_entry(builder = 'ci/Windows Swarm'),
   ],
)
