load('//lib/builders.star', 'builder', 'cpu', 'defaults', 'goma', 'os')

luci.bucket(
    name = 'webrtc',
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

defaults.bucket.set('webrtc')
defaults.builderless.set(False)
defaults.build_numbers.set(True)
defaults.cpu.set(cpu.X86_64)
defaults.executable.set(luci.recipe(name = 'chromium'))
defaults.execution_timeout.set(2 * time.hour)
defaults.mastername.set('chromium.webrtc')
defaults.os.set(os.LINUX_DEFAULT)
defaults.service_account.set('chromium-ci-builder@chops-service-accounts.iam.gserviceaccount.com')
defaults.swarming_tags.set(['vpython:native-python-wrapper'])

defaults.properties.set({
    'perf_dashboard_machine_group': 'ChromiumWebRTC',
})


# Builders are defined in lexicographic order by name


builder(
    name = 'WebRTC Chromium Android Builder',
    goma_backend = goma.backend.RBE_PROD,
)

builder(
    name = 'WebRTC Chromium Android Tester',
)

builder(
    name = 'WebRTC Chromium Linux Builder',
    goma_backend = goma.backend.RBE_PROD,
)

builder(
    name = 'WebRTC Chromium Linux Tester',
)

builder(
    name = 'WebRTC Chromium Mac Builder',
    cores = 8,
    goma_backend = goma.backend.RBE_PROD,
    os = os.MAC_ANY,
)

builder(
    name = 'WebRTC Chromium Mac Tester',
    os = os.MAC_ANY,
)

builder(
    name = 'WebRTC Chromium Win Builder',
    os = os.WINDOWS_ANY,
)

builder(
    name = 'WebRTC Chromium Win10 Tester',
    os = os.WINDOWS_ANY,
)

builder(
    name = 'WebRTC Chromium Win7 Tester',
    os = os.WINDOWS_ANY,
)

builder(
    name = 'WebRTC Chromium Win8 Tester',
    os = os.WINDOWS_ANY,
)
