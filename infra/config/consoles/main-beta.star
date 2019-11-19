# TOD(https://crbug.com/922150) Add to chromium header
luci.console_view(
    name = 'main-beta',
    header = '//consoles/chromium-header.textpb',
    repo = 'https://chromium.googlesource.com/chromium/src',
    # TODO(gbeaty) Define the main consoles inside the respective versioned
    # directories once their contents are stablilized
    refs = ['refs/branch-heads/3945'],
    title = 'Chromium Beta Console',
    entries = [
        luci.console_view_entry(
            builder = 'ci-beta/Linux Builder',
            category = 'chromium.linux|release',
            short_name = 'bld',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/Linux Tests',
            category = 'chromium.linux|release',
            short_name = 'tst',
        ),
        # TODO(https://crbug.com/922150) Move these to an appropriate console
        # and/or don't have linux-rel mirror these since they do not appear on
        # the main console
        luci.console_view_entry(
            builder = 'ci-beta/GPU Linux Builder',
            category = 'chromium.gpu',
        ),
        luci.console_view_entry(
            builder = 'ci-beta/Linux Release (NVIDIA)',
            category = 'chromium.gpu',
        ),
    ],
)
