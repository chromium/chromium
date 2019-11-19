load('//lib/builders.star', 'builder', 'cpu', 'defaults', 'goma', 'os')

luci.bucket(
    name = 'webrtc.fyi',
    acls = [
        acl.entry(
            roles = acl.BUILDBUCKET_READER,
            groups = 'all',
        ),
        acl.entry(
            roles = acl.BUILDBUCKET_TRIGGERER,
            groups = 'project-chromium-ci-schedulers',
        ),
        acl.entry(
            roles = acl.BUILDBUCKET_OWNER,
            groups = 'google/luci-task-force@google.com',
        ),
    ],
)

luci.recipe.defaults.cipd_package.set(
    'infra/recipe_bundles/chromium.googlesource.com/chromium/tools/build')

defaults.bucket.set('webrtc.fyi')
defaults.builderless.set(None)
defaults.build_numbers.set(True)
defaults.cpu.set(cpu.X86_64)
defaults.executable.set(luci.recipe(name = 'chromium'))
defaults.execution_timeout.set(2 * time.hour)
defaults.mastername.set('chromium.webrtc.fyi')
defaults.os.set(os.LINUX_DEFAULT)
defaults.pool.set('luci.chromium.webrtc.fyi')
defaults.service_account.set('chromium-ci-builder@chops-service-accounts.iam.gserviceaccount.com')
defaults.swarming_tags.set(['vpython:native-python-wrapper'])


# Builders are defined in lexicographic order by name


builder(
    name = 'WebRTC Chromium FYI Android Builder',
    goma_backend = goma.backend.RBE_PROD,
)

builder(
    name = 'WebRTC Chromium FYI Android Builder (dbg)',
    goma_backend = goma.backend.RBE_PROD,
)

builder(
    name = 'WebRTC Chromium FYI Android Builder ARM64 (dbg)',
    goma_backend = goma.backend.RBE_PROD,
)

builder(
    name = 'WebRTC Chromium FYI Android Tests (dbg) (K Nexus5)',
)

builder(
    name = 'WebRTC Chromium FYI Android Tests (dbg) (M Nexus5X)',
)

builder(
    name = 'WebRTC Chromium FYI Linux Builder',
    goma_backend = goma.backend.RBE_PROD,
)

builder(
    name = 'WebRTC Chromium FYI Linux Builder (dbg)',
    goma_backend = goma.backend.RBE_PROD,
)

builder(
    name = 'WebRTC Chromium FYI Linux Tester',
)

builder(
    name = 'WebRTC Chromium FYI Mac Builder',
    cores = 8,
    caches = [
        swarming.cache(
            name = 'xcode_ios_10e1001',
            path = 'xcode_ios_10e1001.app',
        ),
    ],
    goma_backend = goma.backend.RBE_PROD,
    os = os.MAC_ANY,
    properties = {
        'xcode_build_version': '10e1001',
    },
)

builder(
    name = 'WebRTC Chromium FYI Mac Builder (dbg)',
    cores = 8,
    caches = [
        swarming.cache(
            name = 'xcode_ios_10e1001',
            path = 'xcode_ios_10e1001.app',
        ),
    ],
    goma_backend = goma.backend.RBE_PROD,
    os = os.MAC_ANY,
    properties = {
        'xcode_build_version': '10e1001',
    },
)

builder(
    name = 'WebRTC Chromium FYI Mac Tester',
    caches = [
        swarming.cache(
            name = 'xcode_ios_10e1001',
            path = 'xcode_ios_10e1001.app',
        ),
    ],
    os = os.MAC_ANY,
    properties = {
        'xcode_build_version': '10e1001',
    },
)

builder(
    name = 'WebRTC Chromium FYI Win Builder',
    os = os.WINDOWS_DEFAULT,
)

builder(
    name = 'WebRTC Chromium FYI Win Builder (dbg)',
    os = os.WINDOWS_DEFAULT,
)

builder(
    name = 'WebRTC Chromium FYI Win10 Tester',
    os = os.WINDOWS_DEFAULT,
)

builder(
    name = 'WebRTC Chromium FYI Win7 Tester',
    os = os.WINDOWS_7,
)

builder(
    name = 'WebRTC Chromium FYI Win8 Tester',
    os = os.WINDOWS_8_1,
)

builder(
    name = 'WebRTC Chromium FYI ios-device',
    caches = [
        swarming.cache(
            name = 'xcode_ios_11a1027',
            path = 'xcode_ios_11a1027.app',
        ),
    ],
    executable = luci.recipe(name = 'webrtc/chromium_ios'),
    os = os.MAC_ANY,
)

builder(
    name = 'WebRTC Chromium FYI ios-simulator',
    caches = [
        swarming.cache(
            name = 'xcode_ios_11a1027',
            path = 'xcode_ios_11a1027.app',
        ),
    ],
    executable = luci.recipe(name = 'webrtc/chromium_ios'),
    os = os.MAC_ANY,
)
