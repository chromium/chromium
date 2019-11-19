# These are used for monitoring builders that have recently been migrated to
# Goma RBE (See crbug.com/950413).
luci.console_view(
    name = 'chromium.goma.migration',
    header = '//consoles/chromium-header.textpb',
    include_experimental_builds = True,
    repo = 'https://chromium.googlesource.com/chromium/src',
    entries = [
        luci.console_view_entry(
            builder = 'ci/VR Linux',
            category = 'week1|linux',
            short_name = 'vr',
        ),
        luci.console_view_entry(
            builder = 'ci/Mojo Linux',
            category = 'week1|linux',
            short_name = 'mojo',
        ),
        luci.console_view_entry(
            builder = 'ci/Linux Builder (dbg)',
            category = 'week1|linux|dbg',
        ),
        luci.console_view_entry(
            builder = 'ci/Linux Builder (dbg)(32)',
            category = 'week1|linux|dbg',
            short_name = '32bit',
        ),
        luci.console_view_entry(
            builder = 'ci/Linux CFI',
            category = 'week1|linux|cfi',
        ),
        luci.console_view_entry(
            builder = 'ci/CFI Linux CF',
            category = 'week1|linux|cfi',
            short_name = 'cf',
        ),
        luci.console_view_entry(
            builder = 'ci/Linux MSan Builder',
            category = 'week1|linux',
            short_name = 'msan',
        ),
        luci.console_view_entry(
            builder = 'ci/Afl Upload Linux ASan',
            category = 'week1|linux',
            short_name = 'afl-asan',
        ),
        luci.console_view_entry(
            builder = 'ci/WebKit Linux ASAN',
            category = 'week1|linux|webkit',
            short_name = 'asan',
        ),
        luci.console_view_entry(
            builder = 'ci/WebKit Linux Leak',
            category = 'week1|linux|webkit',
            short_name = 'leak',
        ),
        luci.console_view_entry(
            builder = 'ci/WebKit Linux MSAN',
            category = 'week1|linux|webkit',
            short_name = 'msan',
        ),
        luci.console_view_entry(
            builder = 'ci/Android FYI 32 Vk Release (Pixel 2)',
            category = 'week2a|android|32',
            short_name = 'p2',
        ),
        luci.console_view_entry(
            builder = 'ci/Android FYI 32 dEQP Vk Release (Pixel 2)',
            category = 'week2a|android|32deqp',
            short_name = 'p2',
        ),
        luci.console_view_entry(
            builder = 'ci/Android FYI 64 Vk Release (Pixel 2)',
            category = 'week2a|android|64',
            short_name = 'p2',
        ),
        luci.console_view_entry(
            builder = 'ci/Android FYI 64 dEQP Vk Release (Pixel 2)',
            category = 'week2a|android|64deqp',
            short_name = 'p2',
        ),
        luci.console_view_entry(
            builder = 'ci/Android FYI Release (NVIDIA Shield TV)',
            category = 'week2a|android|rel',
            short_name = 'shdtv',
        ),
        luci.console_view_entry(
            builder = 'ci/Android FYI Release (Nexus 5)',
            category = 'week2a|android|rel',
            short_name = 'n5',
        ),
        luci.console_view_entry(
            builder = 'ci/Android FYI Release (Nexus 5X)',
            category = 'week2a|android|rel',
            short_name = 'n5x',
        ),
        luci.console_view_entry(
            builder = 'ci/Android FYI Release (Nexus 6)',
            category = 'week2a|android|rel',
            short_name = 'n6',
        ),
        luci.console_view_entry(
            builder = 'ci/Android FYI Release (Nexus 6P)',
            category = 'week2a|android|rel',
            short_name = 'n6p',
        ),
        luci.console_view_entry(
            builder = 'ci/Android FYI Release (Nexus 9)',
            category = 'week2a|android|rel',
            short_name = 'n9',
        ),
        luci.console_view_entry(
            builder = 'ci/Android FYI Release (Pixel 2)',
            category = 'week2a|android|rel',
            short_name = 'p2',
        ),
        luci.console_view_entry(
            builder = 'ci/Android FYI dEQP Release (Nexus 5X)',
            category = 'week2a|android|deqp',
            short_name = 'n5x',
        ),
        luci.console_view_entry(
            builder = 'ci/Deterministic Android',
            category = 'week2a|android|det',
        ),
        luci.console_view_entry(
            builder = 'ci/Deterministic Android (dbg)',
            category = 'week2a|android|det',
            short_name = 'dbg',
        ),
        luci.console_view_entry(
            builder = 'webrtc.fyi/WebRTC Chromium FYI Android Builder',
            category = 'week2b|android|release',
            short_name = '32',
        ),
        luci.console_view_entry(
            builder = 'webrtc.fyi/WebRTC Chromium FYI Android Builder (dbg)',
            category = 'week2b|android|debug|builder',
            short_name = '32',
        ),
        luci.console_view_entry(
            builder = 'webrtc.fyi/WebRTC Chromium FYI Android Builder ARM64 (dbg)',
            category = 'week2b|android|debug|builder',
            short_name = '64',
        ),
        luci.console_view_entry(
            builder = 'webrtc.fyi/WebRTC Chromium FYI Linux Builder',
            category = 'week2b|linux|release',
            short_name = 'bld',
        ),
        luci.console_view_entry(
            builder = 'webrtc.fyi/WebRTC Chromium FYI Linux Builder (dbg)',
            category = 'week2b|linux|debug',
            short_name = 'bld',
        ),
        luci.console_view_entry(
            builder = 'webrtc.fyi/WebRTC Chromium FYI Mac Builder',
            category = 'week2b|mac|release',
            short_name = 'bld',
        ),
        luci.console_view_entry(
            builder = 'webrtc.fyi/WebRTC Chromium FYI Mac Builder (dbg)',
            category = 'week2b|mac|debug',
            short_name = 'bld',
        ),
        luci.console_view_entry(
            builder = 'webrtc/WebRTC Chromium Android Builder',
            category = 'week2b|android',
            short_name = 'bld',
        ),
        luci.console_view_entry(
            builder = 'webrtc/WebRTC Chromium Linux Builder',
            category = 'week2b|linux',
            short_name = 'bld',
        ),
        luci.console_view_entry(
            builder = 'webrtc/WebRTC Chromium Mac Builder',
            category = 'week2b|mac',
            short_name = 'bld',
        ),
        luci.console_view_entry(
            builder = 'ci/Mac ASAN Release',
            category = 'week2c|mac|asan',
        ),
        luci.console_view_entry(
            builder = 'ci/Mac ASAN Release Media',
            category = 'week2c|mac|asan',
            short_name = 'media',
        ),
        luci.console_view_entry(
            builder = 'ci/Mac ASan 64 Builder',
            category = 'week2c|mac|asan',
            short_name = '64',
        ),
        luci.console_view_entry(
            builder = 'ci/Libfuzzer Upload Mac ASan',
            category = 'week2c|mac|asan',
            short_name = 'fuzz',
        ),
        luci.console_view_entry(
            builder = 'ci/WebKit Mac10.13 (retina)',
            category = 'week2c|mac',
            short_name = 'webkit',
        ),
        luci.console_view_entry(
            builder = 'ci/Android CFI',
            category = 'week2c|android',
            short_name = 'cfi',
        ),
        luci.console_view_entry(
            builder = 'ci/Site Isolation Android',
            category = 'week2c|android',
            short_name = 'isolate',
        ),
        luci.console_view_entry(
            builder = 'ci/Mojo Android',
            category = 'week2c|android',
            short_name = 'mojo',
        ),
        luci.console_view_entry(
            builder = 'ci/Android x64 Builder (dbg)',
            category = 'week2c|android|dbg',
            short_name = 'x64',
        ),
        luci.console_view_entry(
            builder = 'ci/Android x86 Builder (dbg)',
            category = 'week2c|android|dbg',
            short_name = 'x86',
        ),
        luci.console_view_entry(
            builder = 'ci/Android WebView L (dbg)',
            category = 'week2c|android|dbg|webview',
            short_name = 'l',
        ),
        luci.console_view_entry(
            builder = 'ci/Android WebView M (dbg)',
            category = 'week2c|android|dbg|webview',
            short_name = 'm',
        ),
        luci.console_view_entry(
            builder = 'ci/Android WebView N (dbg)',
            category = 'week2c|android|dbg|webview',
            short_name = 'n',
        ),
        luci.console_view_entry(
            builder = 'ci/Android WebView O (dbg)',
            category = 'week2c|android|dbg|webview',
            short_name = 'o',
        ),
        luci.console_view_entry(
            builder = 'ci/Android WebView P FYI (rel)',
            category = 'week2c|android|rel|webview',
            short_name = 'p',
        ),
        luci.console_view_entry(
            builder = 'ci/Dawn Linux x64 Builder',
            category = 'week2d|linux|dawn',
        ),
        luci.console_view_entry(
            builder = 'ci/Dawn Linux x64 DEPS Builder',
            category = 'week2d|linux|dawn',
            short_name = 'deps',
        ),
        luci.console_view_entry(
            builder = 'ci/GPU FYI Linux Builder',
            category = 'week2d|linux|gpu|fyi',
        ),
        luci.console_view_entry(
            builder = 'ci/GPU FYI Linux Builder (dbg)',
            category = 'week2d|linux|gpu|fyi',
            short_name = 'dbg',
        ),
        luci.console_view_entry(
            builder = 'ci/GPU FYI Linux Ozone Builder',
            category = 'week2d|linux|gpu|fyi',
            short_name = 'ozone',
        ),
        luci.console_view_entry(
            builder = 'ci/GPU FYI Linux dEQP Builder',
            category = 'week2d|linux|gpu|fyi',
            short_name = 'deqp',
        ),
        luci.console_view_entry(
            builder = 'ci/Linux FYI GPU TSAN Release',
            category = 'week2d|linux|gpu|fyi',
            short_name = 'tsan',
        ),
        luci.console_view_entry(
            builder = 'ci/GPU Linux Builder (dbg)',
            category = 'week2d|linux|gpu',
            short_name = 'dbg',
        ),
        luci.console_view_entry(
            builder = 'ci/Linux Viz',
            category = 'week2d|linux',
            short_name = 'viz',
        ),
        luci.console_view_entry(
            builder = 'ci/Linux remote_run Builder',
            category = 'week2d|linux',
            short_name = 'rem',
        ),
        luci.console_view_entry(
            builder = 'ci/Closure Compilation Linux',
            category = 'week2d|linux',
            short_name = 'clsr',
        ),
        luci.console_view_entry(
            builder = 'ci/Deterministic Linux',
            category = 'week2d|linux|det',
        ),
        luci.console_view_entry(
            builder = 'ci/Deterministic Linux (dbg)',
            category = 'week2d|linux|det',
            short_name = 'dbg',
        ),
        luci.console_view_entry(
            builder = 'ci/Dawn Mac x64 Builder',
            category = 'week2d|mac|dawn',
        ),
        luci.console_view_entry(
            builder = 'ci/Dawn Mac x64 DEPS Builder',
            category = 'week2d|mac|dawn',
            short_name = 'deps',
        ),
        luci.console_view_entry(
            builder = 'ci/GPU FYI Mac Builder',
            category = 'week2d|mac|gpu|fyi',
        ),
        luci.console_view_entry(
            builder = 'ci/GPU FYI Mac Builder (dbg)',
            category = 'week2d|mac|gpu|fyi',
            short_name = 'dbg',
        ),
        luci.console_view_entry(
            builder = 'ci/GPU FYI Mac dEQP Builder',
            category = 'week2d|mac|gpu|fyi',
            short_name = 'deqp',
        ),
        luci.console_view_entry(
            builder = 'ci/Mac FYI GPU ASAN Release',
            category = 'week2d|mac|gpu|fyi',
            short_name = 'asan',
        ),
        luci.console_view_entry(
            builder = 'ci/GPU Mac Builder (dbg)',
            category = 'week2d|mac|gpu',
            short_name = 'dbg',
        ),
        luci.console_view_entry(
            builder = 'ci/Mac deterministic',
            category = 'week2d|mac|det',
        ),
        luci.console_view_entry(
            builder = 'ci/Mac deterministic (dbg)',
            category = 'week2d|mac|det',
            short_name = 'dbg',
        ),
        luci.console_view_entry(
            builder = 'ci/Linux Builder',
            category = 'week2.5|linux',
        ),
        luci.console_view_entry(
            builder = 'ci/GPU Linux Builder',
            category = 'week2.5|linux',
            short_name = 'gpu',
        ),
        luci.console_view_entry(
            builder = 'ci/linux-ozone-rel',
            category = 'week3a|linux',
            short_name = 'ozone',
        ),
        luci.console_view_entry(
            builder = 'ci/linux-annotator-rel',
            category = 'week3a|linux',
            short_name = 'anno',
        ),
        luci.console_view_entry(
            builder = 'ci/linux_chromium_component_updater',
            category = 'week3a|linux',
            short_name = 'cc_upd',
        ),
        luci.console_view_entry(
            builder = 'ci/linux-code-coverage',
            category = 'week3a|linux',
            short_name = 'code',
        ),
        luci.console_view_entry(
            builder = 'ci/linux-blink-animation-use-time-delta',
            category = 'week3a|linux|blink',
            short_name = 'anim',
        ),
        luci.console_view_entry(
            builder = 'ci/linux-blink-heap-concurrent-marking-tsan-rel',
            category = 'week3a|linux|blink',
            short_name = 'tsan',
        ),
        luci.console_view_entry(
            builder = 'ci/linux-blink-heap-verification',
            category = 'week3a|linux|blink',
            short_name = 'ver',
        ),
        luci.console_view_entry(
            builder = 'ci/linux-chromium-tests-staging-builder',
            category = 'week3a|linux',
            short_name = 'crtests',
        ),
        luci.console_view_entry(
            builder = 'ci/android-cronet-arm-dbg',
            category = 'week3b|android|cronet|arm',
            short_name = 'dbg',
        ),
        luci.console_view_entry(
            builder = 'ci/android-cronet-arm-rel',
            category = 'week3b|android|cronet|arm',
        ),
        luci.console_view_entry(
            builder = 'ci/android-cronet-asan-arm-rel',
            category = 'week3b|android|cronet|arm',
            short_name = 'asan',
        ),
        luci.console_view_entry(
            builder = 'ci/android-cronet-kitkat-arm-rel',
            category = 'week3b|android|cronet|arm',
            short_name = 'kkat',
        ),
        luci.console_view_entry(
            builder = 'ci/android-cronet-lollipop-arm-rel',
            category = 'week3b|android|cronet|arm',
            short_name = 'lpop',
        ),
        luci.console_view_entry(
            builder = 'ci/android-cronet-arm64-rel',
            category = 'week3b|android|cronet|arm64',
        ),
        luci.console_view_entry(
            builder = 'ci/android-cronet-arm64-dbg',
            category = 'week3b|android|cronet|arm64',
            short_name = 'dbg',
        ),
        luci.console_view_entry(
            builder = 'ci/android-cronet-marshmallow-arm64-rel',
            category = 'week3b|android|cronet|arm64',
            short_name = 'marsh',
        ),
        luci.console_view_entry(
            builder = 'ci/android-cronet-marshmallow-arm64-perf-rel',
            category = 'week3b|android|cronet|arm64',
            short_name = 'perf',
        ),
        luci.console_view_entry(
            builder = 'ci/android-cronet-x86-rel',
            category = 'week3b|android|cronet|x86',
        ),
        luci.console_view_entry(
            builder = 'ci/android-cronet-x86-dbg',
            category = 'week3b|android|cronet|x86',
            short_name = 'dbg',
        ),
        luci.console_view_entry(
            builder = 'ci/android-incremental-dbg',
            category = 'week3b|android',
            short_name = 'inc',
        ),
        luci.console_view_entry(
            builder = 'ci/android-kitkat-arm-rel',
            category = 'week3b|android',
            short_name = 'kkat',
        ),
        luci.console_view_entry(
            builder = 'ci/android-mojo-webview-rel',
            category = 'week3b|android',
            short_name = 'mojo',
        ),
        luci.console_view_entry(
            builder = 'ci/android-pie-arm64-dbg',
            category = 'week3b|linux',
            short_name = 'pie',
        ),
        luci.console_view_entry(
            builder = 'ci/mac-code-coverage',
            category = 'week3c|mac',
            short_name = 'code',
        ),
        luci.console_view_entry(
            builder = 'ci/mac-hermetic-upgrade-rel',
            category = 'week3c|mac',
            short_name = 'herm',
        ),
        luci.console_view_entry(
            builder = 'ci/mac-mojo-rel',
            category = 'week3c|mac',
            short_name = 'mojo',
        ),
        luci.console_view_entry(
            builder = 'ci/mac-osxbeta-rel',
            category = 'week3c|mac',
            short_name = 'osx',
        ),
        luci.console_view_entry(
            builder = 'ci/Mac Builder',
            category = 'week3c|mac',
        ),
        luci.console_view_entry(
            builder = 'ci/Mac Builder (dbg)',
            category = 'week3c|mac',
            short_name = 'dbg',
        ),
        luci.console_view_entry(
            builder = 'ci/GPU Mac Builder',
            category = 'week3c|mac',
            short_name = 'gpu',
        ),
        luci.console_view_entry(
            builder = 'ci/Libfuzzer Upload Linux ASan',
            category = 'week4|linux',
            short_name = 'asan',
        ),
        luci.console_view_entry(
            builder = 'ci/Libfuzzer Upload Linux ASan Debug',
            category = 'week4|linux',
            short_name = 'dbg',
        ),
        luci.console_view_entry(
            builder = 'ci/Libfuzzer Upload Linux MSan',
            category = 'week4|linux',
            short_name = 'msan',
        ),
        luci.console_view_entry(
            builder = 'ci/Libfuzzer Upload Linux UBSan',
            category = 'week4|linux',
            short_name = 'ubsan',
        ),
        luci.console_view_entry(
            builder = 'ci/Libfuzzer Upload Linux V8-ARM64 ASan',
            category = 'week4|linux|v8arm',
            short_name = 'asan',
        ),
        luci.console_view_entry(
            builder = 'ci/Libfuzzer Upload Linux V8-ARM64 ASan Debug',
            category = 'week4|linux|v8arm',
            short_name = 'dbg',
        ),
        luci.console_view_entry(
            builder = 'ci/Libfuzzer Upload Linux32 ASan',
            category = 'week4|linux32',
            short_name = 'asan',
        ),
        luci.console_view_entry(
            builder = 'ci/Libfuzzer Upload Linux32 ASan Debug',
            category = 'week4|linux32',
            short_name = 'dbg',
        ),
        luci.console_view_entry(
            builder = 'ci/Libfuzzer Upload Linux32 V8-ARM ASan',
            category = 'week4|linux32|v8arm',
            short_name = 'asan',
        ),
        luci.console_view_entry(
            builder = 'ci/Libfuzzer Upload Linux32 V8-ARM ASan Debug',
            category = 'week4|linux32|v8arm',
            short_name = 'dbg',
        ),
        luci.console_view_entry(
            builder = 'ci/ASan Debug (32-bit x86 with V8-ARM)',
            category = 'week5|asan',
            short_name = 'dbg',
        ),
        luci.console_view_entry(
            builder = 'ci/ASan Release (32-bit x86 with V8-ARM)',
            category = 'week5|asan',
            short_name = 'rel',
        ),
        luci.console_view_entry(
            builder = 'ci/ASan Release Media (32-bit x86 with V8-ARM)',
            category = 'week5|asan',
            short_name = 'media',
        ),
        luci.console_view_entry(
            builder = 'ci/ASAN Debug',
            category = 'week6|asan',
            short_name = 'dbg',
        ),
        luci.console_view_entry(
            builder = 'ci/ASAN Release',
            category = 'week6|asan',
            short_name = 'rel',
        ),
        luci.console_view_entry(
            builder = 'ci/ASAN Release Media',
            category = 'week6|asan',
            short_name = 'media',
        ),
        luci.console_view_entry(
            builder = 'ci/MSAN Release (chained origins)',
            category = 'week7|msan',
            short_name = 'chain',
        ),
        luci.console_view_entry(
            builder = 'ci/MSAN Release (no origins)',
            category = 'week7|msan',
            short_name = 'none',
        ),
        luci.console_view_entry(
            builder = 'ci/TSAN Release',
            category = 'week8|tsan',
            short_name = 'rel',
        ),
        luci.console_view_entry(
            builder = 'ci/TSAN Debug',
            category = 'week8|tsan',
            short_name = 'dbg',
        ),
        luci.console_view_entry(
            builder = 'ci/UBSan Release',
            category = 'week9|ubsan',
        ),
        luci.console_view_entry(
            builder = 'ci/UBSan vptr Release',
            category = 'week9|ubsan|vptr',
            short_name = 'rel',
        ),
        luci.console_view_entry(
            builder = 'ci/UBSanVptr Linux',
            category = 'week9|ubsan|vptr',
            short_name = 'lnx',
        ),
        luci.console_view_entry(
            builder = 'ci/Cast Android (dbg)',
            category = 'week10|android',
        ),
        luci.console_view_entry(
            builder = 'ci/Cast Audio Linux',
            category = 'week10|linux',
            short_name = 'audio',
        ),
        luci.console_view_entry(
            builder = 'ci/Cast Linux',
            category = 'week10|linux',
        ),
        luci.console_view_entry(
            builder = 'ci/Fuchsia ARM64',
            category = 'week11|fuchsia|arm64',
        ),
        luci.console_view_entry(
            builder = 'ci/fuchsia-arm64-cast',
            category = 'week11|fuchsia|arm64',
            short_name = 'cast',
        ),
        luci.console_view_entry(
            builder = 'ci/Fuchsia x64',
            category = 'week11|fuchsia|x64',
        ),
        luci.console_view_entry(
            builder = 'ci/fuchsia-x64-cast',
            category = 'week11|fuchsia|x64',
            short_name = 'cast',
        ),
        luci.console_view_entry(
            builder = 'ci/fuchsia-fyi-arm64-rel',
            category = 'week11|fuchsia|fyi',
            short_name = 'arm64',
        ),
        luci.console_view_entry(
            builder = 'ci/fuchsia-fyi-x64-dbg',
            category = 'week11|fuchsia|fyi',
            short_name = 'x64 dbg',
        ),
        luci.console_view_entry(
            builder = 'ci/fuchsia-fyi-x64-rel',
            category = 'week11|fuchsia|fyi',
            short_name = 'x64 rel',
        ),
        luci.console_view_entry(
            builder = 'ci/android-marshmallow-arm64-rel',
            category = 'week13|android',
            short_name = 'marsh',
        ),
        luci.console_view_entry(
            builder = 'ci/Android Release (Nexus 5X)',
            category = 'week13|android',
            short_name = 'n5x',
        ),
        luci.console_view_entry(
            builder = 'ci/Linux ASan LSan Builder',
            category = 'week14a|linux',
            short_name = 'asanlsan',
        ),
        luci.console_view_entry(
            builder = 'ci/Linux TSan Builder',
            category = 'week14a|linux',
            short_name = 'tsan',
        ),
        luci.console_view_entry(
            builder = 'ci/Android ASAN (dbg)',
            category = 'week14b|android',
            short_name = 'asanlsan',
        ),
        luci.console_view_entry(
            builder = 'ci/Android arm Builder (dbg)',
            category = 'week14b|android',
            short_name = 'tsan',
        ),
        luci.console_view_entry(
            builder = 'ci/Android arm64 Builder (dbg)',
            category = 'week14b|android',
            short_name = 'tsan',
        ),
        luci.console_view_entry(
            builder = 'ci/fuchsia-x64-dbg',
            category = 'week15a|fuchsia',
            short_name = 'x64dbg',
        ),
        luci.console_view_entry(
            builder = 'ci/linux-archive-dbg',
            category = 'week15a|linux|archive',
            short_name = 'dbg',
        ),
        luci.console_view_entry(
            builder = 'ci/linux-archive-rel',
            category = 'week15a|linux|archive',
            short_name = 'rel',
        ),
        luci.console_view_entry(
            builder = 'ci/linux-bfcache-debug',
            category = 'week15a|linux',
            short_name = 'bfc',
        ),
        luci.console_view_entry(
            builder = 'ci/linux-fieldtrial-rel',
            category = 'week15a|linux',
            short_name = 'field',
        ),
        luci.console_view_entry(
            builder = 'ci/linux-oor-cors-rel',
            category = 'week15a|linux',
            short_name = 'oorcors',
        ),
        luci.console_view_entry(
            builder = 'ci/Android WebView P OOR-CORS FYI (rel)',
            category = 'week15b|android|webview p',
            short_name = 'oorcors',
        ),
        luci.console_view_entry(
            builder = 'ci/android-archive-rel',
            category = 'week15b|android|archive',
            short_name = 'rel',
        ),
        luci.console_view_entry(
            builder = 'ci/android-archive-dbg',
            category = 'week15b|android|archive',
            short_name = 'dbg',
        ),
        luci.console_view_entry(
            builder = 'ci/android-asan',
            category = 'week15b|android',
            short_name = 'asan',
        ),
        luci.console_view_entry(
            builder = 'ci/android-bfcache-debug',
            category = 'week15b|android',
            short_name = 'bfc',
        ),
        luci.console_view_entry(
            builder = 'ci/android-code-coverage',
            category = 'week15b|android',
            short_name = 'code',
        ),
        luci.console_view_entry(
            builder = 'ci/android-pie-arm64-rel',
            category = 'week15b|android|pie',
            short_name = 'arm64',
        ),
        luci.console_view_entry(
            builder = 'ci/android-pie-x86-fyi-rel',
            category = 'week15b|android|pie',
            short_name = 'x86',
        ),
        luci.console_view_entry(
            builder = 'ci/GPU FYI Perf Android 64 Builder',
            category = 'week15b|android|gpu',
            short_name = 'perf64',
        ),
        luci.console_view_entry(
            builder = 'ci/mac-archive-rel',
            category = 'week15b|mac|archive',
            short_name = 'rel',
        ),
        luci.console_view_entry(
            builder = 'ci/mac-archive-dbg',
            category = 'week15b|mac|archive',
            short_name = 'dbg',
        ),
        luci.console_view_entry(
            builder = 'ci/Win ASan Release',
            category = 'win|week1|asan',
        ),
        luci.console_view_entry(
            builder = 'ci/Win ASan Release Media',
            category = 'win|week1|asan',
            short_name = 'media',
        ),
        luci.console_view_entry(
            builder = 'ci/win10-code-coverage',
            category = 'win|week1.1',
            short_name = 'code',
        ),
    ],
)
