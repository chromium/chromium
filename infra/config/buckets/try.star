load('//lib/builders.star', 'builder', 'cpu', 'defaults', 'goma', 'os')

# Defaults that apply to all branch versions of the bucket
luci.recipe.defaults.cipd_package.set('infra/recipe_bundles/chromium.googlesource.com/chromium/tools/build')

defaults.build_numbers.set(True)
defaults.configure_kitchen.set(True)
defaults.cores.set(8)
defaults.cpu.set(cpu.X86_64)
defaults.executable.set(luci.recipe(name = 'chromium_trybot'))
defaults.execution_timeout.set(4 * time.hour)
# Max. pending time for builds. CQ considers builds pending >2h as timed
# out: http://shortn/_8PaHsdYmlq. Keep this in sync.
defaults.expiration_timeout.set(2 * time.hour)
defaults.os.set(os.LINUX_DEFAULT)
defaults.service_account.set('chromium-try-builder@chops-service-accounts.iam.gserviceaccount.com')
defaults.swarming_tags.set(['vpython:native-python-wrapper'])
defaults.task_template_canary_percentage.set(5)

defaults.caches.set([
    swarming.cache(
        name = 'win_toolchain',
        path = 'win_toolchain',
    ),
])


def tryjob(
    *,
    disable_reuse=None,
    experiment_percentage=None,
    location_regexp=None,
    location_regexp_exclude=None):
  return struct(
      disable_reuse = disable_reuse,
      experiment_percentage = experiment_percentage,
      location_regexp = location_regexp,
      location_regexp_exclude = location_regexp_exclude,
  )

def try_builder(
    *,
    name,
    tryjob=None,
    **kwargs):
  if tryjob != None:
    luci.cq_tryjob_verifier(
        builder = 'try/' + name,
        cq_group = 'cq',
        disable_reuse = tryjob.disable_reuse,
        experiment_percentage = tryjob.experiment_percentage,
        location_regexp = tryjob.location_regexp,
        location_regexp_exclude = tryjob.location_regexp_exclude,
    )

  return builder(
      name = name,
      **kwargs
  )


# Execute the versioned files to define all of the per-branch entities
# (bucket, builders, console, cq_group, etc.)
exec('//versioned/branches/beta/buckets/try.star')
exec('//versioned/branches/stable/buckets/try.star')
exec('//versioned/trunk/buckets/try.star')


# *** After this point everything is trunk only ***
defaults.bucket.set('try')


# Builders appear after the function used to define them, with all builders
# defined using the same function ordered lexicographically by name
# Builder functions are defined in lexicographic order by name ignoring the
# '_builder' suffix

# Builder functions are defined for GPU builders on each master where they
# appear: gpu_XXX_builder where XXX is the part after the last dot in the
# mastername
# Builder functions are defined for each master, with additional functions
# for specializing on OS: XXX_builder and XXX_YYY_builder where XXX is the part
# after the last dot in the mastername and YYY is the OS


def android_builder(*, name, **kwargs):
  return try_builder(
      name = name,
      mastername = 'tryserver.chromium.android',
      **kwargs
  )

android_builder(
    name = 'android-binary-size',
    executable = luci.recipe(name = 'binary_size_trybot'),
    goma_backend = goma.backend.RBE_PROD,
    goma_jobs = goma.jobs.J150,
    tryjob = tryjob(),
)

android_builder(
    name = 'android-cronet-arm-dbg',
    goma_backend = goma.backend.RBE_PROD,
    tryjob = tryjob(
        location_regexp = [
            '.+/[+]/components/cronet/.+',
            '.+/[+]/components/grpc_support/.+',
            '.+/[+]/build/android/.+',
            '.+/[+]/build/config/android/.+',
        ],
        location_regexp_exclude = [
            '.+/[+]/components/cronet/ios/.+',
        ],
    ),
)

android_builder(
    name = 'android-deterministic-dbg',
    executable = luci.recipe(name = 'swarming/deterministic_build'),
    execution_timeout = 6 * time.hour,
    goma_backend = goma.backend.RBE_PROD,
)

android_builder(
    name = 'android-deterministic-rel',
    executable = luci.recipe(name = 'swarming/deterministic_build'),
    execution_timeout = 6 * time.hour,
    goma_backend = goma.backend.RBE_PROD,
)

android_builder(
    name = 'android-kitkat-arm-rel',
    goma_backend = goma.backend.RBE_PROD,
    goma_jobs = goma.jobs.J150,
    tryjob = tryjob(),
)

android_builder(
    name = 'android-marshmallow-arm64-coverage-rel',
    cores = 16,
    goma_backend = goma.backend.RBE_PROD,
    goma_jobs = goma.jobs.J300,
    ssd = True,
    use_java_coverage = True,
    tryjob = tryjob(
        experiment_percentage = 10,
    ),
)

android_builder(
    name = 'android-marshmallow-arm64-rel',
    cores = 16,
    goma_backend = goma.backend.RBE_PROD,
    goma_jobs = goma.jobs.J300,
    ssd = True,
    tryjob = tryjob(),
)

android_builder(
    name = 'android-marshmallow-x86-fyi-rel',
)

android_builder(
    name = 'android-opus-kitkat-arm-rel',
)

android_builder(
    name = 'android-oreo-arm64-cts-networkservice-dbg',
)

android_builder(
    name = 'android-oreo-arm64-dbg',
    goma_backend = goma.backend.RBE_PROD,
)

android_builder(
    name = 'android-pie-arm64-dbg',
    goma_backend = goma.backend.RBE_PROD,
    tryjob = tryjob(
        location_regexp = [
            '.+/[+]/chrome/android/java/src/org/chromium/chrome/browser/vr/.+',
            '.+/[+]/chrome/browser/vr/.+',
            '.+/[+]/third_party/gvr-android-sdk/.+',
        ],
    ),
)

android_builder(
    name = 'android-pie-x86-fyi-rel',
    goma_backend = goma.backend.RBE_PROD,
)

android_builder(
    name = 'android-pie-arm64-rel',
    goma_backend = goma.backend.RBE_PROD,
    tryjob = tryjob(
        experiment_percentage = 50,
    ),
)

android_builder(
    name = 'android-webview-marshmallow-arm64-dbg',
)

android_builder(
    name = 'android-webview-nougat-arm64-dbg',
)

android_builder(
    name = 'android-webview-oreo-arm64-dbg',
)

android_builder(
    name = 'android-webview-pie-arm64-dbg',
)

android_builder(
    name = 'android-webview-pie-arm64-fyi-rel',
    goma_backend = goma.backend.RBE_PROD,
)

android_builder(
    name = 'android_archive_rel_ng',
)

android_builder(
    name = 'android_arm64_dbg_recipe',
    goma_backend = goma.backend.RBE_PROD,
    goma_jobs = goma.jobs.J300,
)

android_builder(
    name = 'android_blink_rel',
    goma_backend = goma.backend.RBE_PROD,
)

android_builder(
    name = 'android_cfi_rel_ng',
    cores = 32,
    goma_backend = goma.backend.RBE_PROD,
)

android_builder(
    name = 'android_clang_dbg_recipe',
    goma_backend = goma.backend.RBE_PROD,
    goma_jobs = goma.jobs.J300,
)

android_builder(
    name = 'android_compile_dbg',
    goma_backend = goma.backend.RBE_PROD,
    goma_jobs = goma.jobs.J150,
    tryjob = tryjob(),
)

android_builder(
    name = 'android_compile_rel',
)

android_builder(
    name = 'android_compile_x64_dbg',
    goma_backend = goma.backend.RBE_PROD,
    tryjob = tryjob(
        location_regexp = [
            '.+/[+]/chrome/android/java/src/org/chromium/chrome/browser/vr/.+',
            '.+/[+]/chrome/browser/vr/.+',
            '.+/[+]/sandbox/linux/seccomp-bpf/.+',
            '.+/[+]/sandbox/linux/seccomp-bpf-helpers/.+',
            '.+/[+]/sandbox/linux/system_headers/.+',
            '.+/[+]/sandbox/linux/tests/.+',
            '.+/[+]/third_party/gvr-android-sdk/.+',
        ],
    ),
)

android_builder(
    name = 'android_compile_x86_dbg',
    goma_backend = goma.backend.RBE_PROD,
    tryjob = tryjob(
        location_regexp = [
            '.+/[+]/chrome/android/java/src/org/chromium/chrome/browser/vr/.+',
            '.+/[+]/chrome/browser/vr/.+',
            '.+/[+]/sandbox/linux/seccomp-bpf/.+',
            '.+/[+]/sandbox/linux/seccomp-bpf-helpers/.+',
            '.+/[+]/sandbox/linux/system_headers/.+',
            '.+/[+]/sandbox/linux/tests/.+',
            '.+/[+]/third_party/gvr-android-sdk/.+',
        ],
    ),
)

android_builder(
    name = 'android_cronet',
    goma_backend = goma.backend.RBE_PROD,
    tryjob = tryjob(),
)

android_builder(
    name = 'android_cronet_tester',
    properties = {
        'buildername': 'android-cronet-arm-dbg',
    },
)

android_builder(
    name = 'android_mojo',
    goma_backend = goma.backend.RBE_PROD,
)

android_builder(
    name = 'android_n5x_swarming_dbg',
    goma_backend = goma.backend.RBE_PROD,
)

android_builder(
    name = 'android_unswarmed_pixel_aosp',
    goma_backend = goma.backend.RBE_PROD,
)

android_builder(
    name = 'cast_shell_android',
    goma_backend = goma.backend.RBE_PROD,
    tryjob = tryjob(),
)

android_builder(
    name = 'linux_android_dbg_ng',
    goma_backend = goma.backend.RBE_PROD,
)

android_builder(
    name = 'try-nougat-phone-tester',
)


def angle_builder(*, name, **kwargs):
  return try_builder(
      name = name,
      builderless = False,
      mastername = 'tryserver.chromium.angle',
      service_account = 'chromium-try-gpu-builder@chops-service-accounts.iam.gserviceaccount.com',
      **kwargs
  )

angle_builder(
    name = 'android_angle_deqp_rel_ng',
    goma_backend = goma.backend.RBE_PROD,
)

angle_builder(
    name = 'android_angle_rel_ng',
    goma_backend = goma.backend.RBE_PROD,
)

angle_builder(
    name = 'android_angle_vk32_deqp_rel_ng',
    goma_backend = goma.backend.RBE_PROD,
)

angle_builder(
    name = 'android_angle_vk32_rel_ng',
    goma_backend = goma.backend.RBE_PROD,
)

angle_builder(
    name = 'android_angle_vk64_deqp_rel_ng',
    goma_backend = goma.backend.RBE_PROD,
)

angle_builder(
    name = 'android_angle_vk64_rel_ng',
    goma_backend = goma.backend.RBE_PROD,
)

angle_builder(
    name = 'fuchsia-angle-rel',
    goma_backend = goma.backend.RBE_PROD,
)

angle_builder(
    name = 'linux-angle-rel',
    goma_backend = goma.backend.RBE_PROD,
)

angle_builder(
    name = 'linux_angle_deqp_rel_ng',
    goma_backend = goma.backend.RBE_PROD,
)

angle_builder(
    name = 'linux_angle_ozone_rel_ng',
    goma_backend = goma.backend.RBE_PROD,
)

angle_builder(
    name = 'mac-angle-rel',
    cores = None,
    goma_backend = goma.backend.RBE_PROD,
    os = os.MAC_ANY,
)

angle_builder(
    name = 'win-angle-deqp-rel-32',
    os = os.WINDOWS_ANY,
)

angle_builder(
    name = 'win-angle-deqp-rel-64',
    os = os.WINDOWS_ANY,
)

angle_builder(
    name = 'win-angle-rel-32',
    os = os.WINDOWS_ANY,
)

angle_builder(
    name = 'win-angle-rel-64',
    os = os.WINDOWS_ANY,
)


def blink_builder(*, name, **kwargs):
  return try_builder(
      name = name,
      mastername = 'tryserver.blink',
      **kwargs
  )

blink_builder(
    name = 'linux-blink-rel',
    goma_backend = goma.backend.RBE_PROD,
    tryjob = tryjob(
        location_regexp = [
            '.+/[+]/cc/.+',
            '.+/[+]/third_party/blink/renderer/core/paint/.+',
            '.+/[+]/third_party/blink/renderer/core/svg/.+',
            '.+/[+]/third_party/blink/renderer/platform/graphics/.+',
            '.+/[+]/third_party/blink/web_tests/FlagExpectations/enable-blink-features=CompositeAfterPaint',
            '.+/[+]/third_party/blink/web_tests/flag-specific/enable-blink-features=CompositeAfterPaint/.+',
        ],
    ),
)

blink_builder(
    name = 'win10-blink-rel',
    os = os.WINDOWS_ANY,
)

blink_builder(
    name = 'win7-blink-rel',
    os = os.WINDOWS_ANY,
)


def blink_mac_builder(*, name, **kwargs):
  return blink_builder(
      name = name,
      cores = None,
      os = os.MAC_ANY,
      **kwargs
  )

blink_mac_builder(
    name = 'mac10.10-blink-rel',
    goma_backend = goma.backend.RBE_PROD,
)

blink_mac_builder(
    name = 'mac10.11-blink-rel',
    goma_backend = goma.backend.RBE_PROD,
)

blink_mac_builder(
    name = 'mac10.12-blink-rel',
    goma_backend = goma.backend.RBE_PROD,
)

blink_mac_builder(
    name = 'mac10.13-blink-rel',
    goma_backend = goma.backend.RBE_PROD,
)

blink_mac_builder(
    name = 'mac10.13_retina-blink-rel',
    goma_backend = goma.backend.RBE_PROD,
)


def chromiumos_builder(*, name, **kwargs):
  return try_builder(
      name = name,
      mastername = 'tryserver.chromium.chromiumos',
      **kwargs
  )

chromiumos_builder(
    name = 'chromeos-amd64-generic-dbg',
    tryjob = tryjob(
        location_regexp = [
            '.+/[+]/content/gpu/.+',
            '.+/[+]/media/.+',
        ],
    ),
)

chromiumos_builder(
    name = 'chromeos-amd64-generic-cfi-thin-lto-rel',
)

chromiumos_builder(
    name = 'chromeos-amd64-generic-rel',
    tryjob = tryjob(),
)

chromiumos_builder(
    name = 'chromeos-arm-generic-dbg',
)

chromiumos_builder(
    name = 'chromeos-arm-generic-rel',
    tryjob = tryjob(),
)

chromiumos_builder(
    name = 'chromeos-kevin-compile-rel',
    tryjob = tryjob(
        location_regexp = [
            '.+/[+]/chromeos/CHROMEOS_LKGM',
        ],
    ),
)

chromiumos_builder(
    name = 'chromeos-kevin-rel',
    tryjob = tryjob(
        location_regexp = [
            '.+/[+]/build/chromeos/.+',
            '.+/[+]/build/config/chromeos/.*',
        ],
    ),
)

chromiumos_builder(
    name = 'linux-chromeos-compile-dbg',
    tryjob = tryjob(),
)

chromiumos_builder(
    name = 'linux-chromeos-dbg',
)

chromiumos_builder(
    name = 'linux-chromeos-rel',
    goma_jobs = goma.jobs.J150,
    tryjob = tryjob(),
    use_clang_coverage = True,
)


def dawn_builder(*, name, **kwargs):
  return try_builder(
      name = name,
      builderless = False,
      cores = None,
      mastername = 'tryserver.chromium.dawn',
      service_account = 'chromium-try-gpu-builder@chops-service-accounts.iam.gserviceaccount.com',
      **kwargs
  )

dawn_builder(
    name = 'dawn-linux-x64-deps-rel',
    goma_backend = goma.backend.RBE_PROD,
    tryjob = tryjob(
        location_regexp = [
            '.+/[+]/gpu/.+',
            '.+/[+]/testing/buildbot/chromium.dawn.json',
            '.+/[+]/third_party/blink/renderer/modules/webgpu/.+',
            '.+/[+]/third_party/blink/web_tests/external/wpt/webgpu/.+',
            '.+/[+]/third_party/blink/web_tests/wpt_internal/webgpu/.+',
            '.+/[+]/third_party/blink/web_tests/WebGPUExpectations',
            '.+/[+]/third_party/dawn/.+',
        ],
    ),
)

dawn_builder(
    name = 'dawn-mac-x64-deps-rel',
    goma_backend = goma.backend.RBE_PROD,
    os = os.MAC_ANY,
    tryjob = tryjob(
        location_regexp = [
            '.+/[+]/gpu/.+',
            '.+/[+]/testing/buildbot/chromium.dawn.json',
            '.+/[+]/third_party/blink/renderer/modules/webgpu/.+',
            '.+/[+]/third_party/blink/web_tests/external/wpt/webgpu/.+',
            '.+/[+]/third_party/blink/web_tests/wpt_internal/webgpu/.+',
            '.+/[+]/third_party/blink/web_tests/WebGPUExpectations',
            '.+/[+]/third_party/dawn/.+',
        ],
    ),
)

dawn_builder(
    name = 'dawn-win10-x64-deps-rel',
    os = os.WINDOWS_ANY,
    tryjob = tryjob(
        location_regexp = [
            '.+/[+]/gpu/.+',
            '.+/[+]/testing/buildbot/chromium.dawn.json',
            '.+/[+]/third_party/blink/renderer/modules/webgpu/.+',
            '.+/[+]/third_party/blink/web_tests/external/wpt/webgpu/.+',
            '.+/[+]/third_party/blink/web_tests/wpt_internal/webgpu/.+',
            '.+/[+]/third_party/blink/web_tests/WebGPUExpectations',
            '.+/[+]/third_party/dawn/.+',
        ],
    ),
)

dawn_builder(
    name = 'dawn-win10-x86-deps-rel',
    os = os.WINDOWS_ANY,
    tryjob = tryjob(
        location_regexp = [
            '.+/[+]/gpu/.+',
            '.+/[+]/testing/buildbot/chromium.dawn.json',
            '.+/[+]/third_party/blink/renderer/modules/webgpu/.+',
            '.+/[+]/third_party/blink/web_tests/external/wpt/webgpu/.+',
            '.+/[+]/third_party/blink/web_tests/wpt_internal/webgpu/.+',
            '.+/[+]/third_party/blink/web_tests/WebGPUExpectations',
            '.+/[+]/third_party/dawn/.+',
        ],
    ),
)

dawn_builder(
    name = 'linux-dawn-rel',
    goma_backend = goma.backend.RBE_PROD,
)

dawn_builder(
    name = 'mac-dawn-rel',
    goma_backend = goma.backend.RBE_PROD,
    os = os.MAC_ANY,
)

dawn_builder(
    name = 'win-dawn-rel',
    os = os.WINDOWS_ANY,
)


def gpu_builder(*, name, builderless=False, execution_timeout=6 * time.hour, **kwargs):
  return try_builder(
      name = name,
      builderless = builderless,
      execution_timeout = execution_timeout,
      service_account = 'chromium-try-gpu-builder@chops-service-accounts.iam.gserviceaccount.com',
      **kwargs
  )


def gpu_android_builder(*, name, **kwargs):
  return gpu_builder(
      name = name,
      mastername = 'tryserver.chromium.android',
      **kwargs
  )

gpu_android_builder(
    name = 'android_optional_gpu_tests_rel',
    tryjob = tryjob(
        location_regexp = [
            '.+/[+]/cc/.+',
            '.+/[+]/chrome/browser/vr/.+',
            '.+/[+]/components/viz/.+',
            '.+/[+]/content/test/gpu/.+',
            '.+/[+]/gpu/.+',
            '.+/[+]/media/audio/.+',
            '.+/[+]/media/filters/.+',
            '.+/[+]/media/gpu/.+',
            '.+/[+]/services/viz/.+',
            '.+/[+]/testing/trigger_scripts/.+',
            '.+/[+]/third_party/blink/renderer/modules/webgl/.+',
            '.+/[+]/third_party/blink/renderer/platform/graphics/gpu/.+',
            '.+/[+]/ui/gl/.+',
        ],
    ),
)

gpu_android_builder(
    name = 'gpu-fyi-try-android-l-nexus-5-32',
    goma_backend = goma.backend.RBE_PROD,
)

gpu_android_builder(
    name = 'gpu-fyi-try-android-l-nexus-6-32',
    goma_backend = goma.backend.RBE_PROD,
)

gpu_android_builder(
    name = 'gpu-fyi-try-android-m-nexus-5x-64',
    goma_backend = goma.backend.RBE_PROD,
)

gpu_android_builder(
    name = 'gpu-fyi-try-android-m-nexus-5x-deqp-64',
    goma_backend = goma.backend.RBE_PROD,
)

gpu_android_builder(
    name = 'gpu-fyi-try-android-m-nexus-5x-skgl-64',
    goma_backend = goma.backend.RBE_PROD,
)

gpu_android_builder(
    name = 'gpu-fyi-try-android-m-nexus-6p-64',
    goma_backend = goma.backend.RBE_PROD,
)

gpu_android_builder(
    name = 'gpu-fyi-try-android-m-nexus-9-64',
    goma_backend = goma.backend.RBE_PROD,
)

gpu_android_builder(
    name = 'gpu-fyi-try-android-n-nvidia-shield-tv-64',
    goma_backend = goma.backend.RBE_PROD,
)

gpu_android_builder(
    name = 'gpu-fyi-try-android-p-pixel-2-32',
    goma_backend = goma.backend.RBE_PROD,
)

gpu_android_builder(
    name = 'gpu-fyi-try-android-p-pixel-2-skv-32',
    goma_backend = goma.backend.RBE_PROD,
    tryjob = tryjob(
        # Some locations disabled due to limited capacity.
        location_regexp = [
            #'.+/[+]/cc/.+',
            '.+/[+]/components/viz/.+',
            '.+/[+]/content/test/gpu/gpu_tests/.+py',
            '.+/[+]/content/test/gpu/gpu_tests/test_expectations/pixel_expectations.txt',
            '.+/[+]/gpu/vulkan/.+',
            #'.+/[+]/media/gpu/.+',
            '.+/[+]/services/viz/.+',
            #'.+/[+]/third_party/blink/renderer/platform/graphics/gpu/.+',
            #'.+/[+]/third_party/skia/src/gpu/.+',
            #'.+/[+]/third_party/skia/include/gpu/.+',
        ],
    ),
)

gpu_android_builder(
    name = 'gpu-fyi-try-android-q-pixel-2-deqp-vk-32',
    goma_backend = goma.backend.RBE_PROD,
)

gpu_android_builder(
    name = 'gpu-fyi-try-android-q-pixel-2-deqp-vk-64',
    goma_backend = goma.backend.RBE_PROD,
)

gpu_android_builder(
    name = 'gpu-fyi-try-android-q-pixel-2-vk-32',
    goma_backend = goma.backend.RBE_PROD,
)

gpu_android_builder(
    name = 'gpu-fyi-try-android-q-pixel-2-vk-64',
    goma_backend = goma.backend.RBE_PROD,
)

gpu_android_builder(
    name = 'gpu-try-android-m-nexus-5x-64',
    goma_backend = goma.backend.RBE_PROD,
)


def gpu_linux_builder(*, name, **kwargs):
  return gpu_builder(
      name = name,
      mastername = 'tryserver.chromium.linux',
      **kwargs
  )

gpu_linux_builder(
    name = 'gpu-fyi-try-linux-intel-dqp',
)

gpu_linux_builder(
    name = 'gpu-fyi-try-linux-intel-exp',
)

gpu_linux_builder(
    name = 'gpu-fyi-try-linux-intel-rel',
    goma_backend = goma.backend.RBE_PROD,
)

gpu_linux_builder(
    name = 'gpu-fyi-try-linux-intel-skv',
)

gpu_linux_builder(
    name = 'gpu-fyi-try-linux-nvidia-dbg',
)

gpu_linux_builder(
    name = 'gpu-fyi-try-linux-nvidia-dqp',
)

gpu_linux_builder(
    name = 'gpu-fyi-try-linux-nvidia-exp',
)

gpu_linux_builder(
    name = 'gpu-fyi-try-linux-nvidia-rel',
)

gpu_linux_builder(
    name = 'gpu-fyi-try-linux-nvidia-skv',
)

gpu_linux_builder(
    name = 'gpu-fyi-try-linux-nvidia-tsn',
    goma_backend = goma.backend.RBE_PROD,
)

gpu_linux_builder(
    name = 'gpu-try-linux-nvidia-dbg',
)

gpu_linux_builder(
    name = 'gpu-try-linux-nvidia-rel',
    goma_backend = goma.backend.RBE_PROD,
)

gpu_linux_builder(
    name = 'linux_optional_gpu_tests_rel',
    goma_backend = goma.backend.RBE_PROD,
    tryjob = tryjob(
        location_regexp = [
            '.+/[+]/chrome/browser/vr/.+',
            '.+/[+]/content/test/gpu/.+',
            '.+/[+]/gpu/.+',
            '.+/[+]/media/audio/.+',
            '.+/[+]/media/filters/.+',
            '.+/[+]/media/gpu/.+',
            '.+/[+]/testing/buildbot/chromium.gpu.fyi.json',
            '.+/[+]/testing/trigger_scripts/.+',
            '.+/[+]/third_party/blink/renderer/modules/webgl/.+',
            '.+/[+]/third_party/blink/renderer/platform/graphics/gpu/.+',
            '.+/[+]/ui/gl/.+',
        ],
    ),
)


def gpu_mac_builder(*, name, **kwargs):
  return gpu_builder(
      name = name,
      cores = None,
      mastername = 'tryserver.chromium.mac',
      os = os.MAC_ANY,
      **kwargs
  )

gpu_mac_builder(
    name = 'gpu-fyi-try-mac-amd-dqp',
    goma_backend = goma.backend.RBE_PROD,
)

gpu_mac_builder(
    name = 'gpu-fyi-try-mac-amd-pro-rel',
)

gpu_mac_builder(
    name = 'gpu-fyi-try-mac-amd-retina-dbg',
    goma_backend = goma.backend.RBE_PROD,
)

gpu_mac_builder(
    name = 'gpu-fyi-try-mac-amd-retina-exp',
)

gpu_mac_builder(
    name = 'gpu-fyi-try-mac-amd-retina-rel',
)

gpu_mac_builder(
    name = 'gpu-fyi-try-mac-asan',
    goma_backend = goma.backend.RBE_PROD,
)

gpu_mac_builder(
    name = 'gpu-fyi-try-mac-intel-dbg',
    goma_backend = goma.backend.RBE_PROD,
)

gpu_mac_builder(
    name = 'gpu-fyi-try-mac-intel-dqp',
    goma_backend = goma.backend.RBE_PROD,
)

gpu_mac_builder(
    name = 'gpu-fyi-try-mac-intel-exp',
)

gpu_mac_builder(
    name = 'gpu-fyi-try-mac-intel-rel',
    goma_backend = goma.backend.RBE_PROD,
)

gpu_mac_builder(
    name = 'gpu-fyi-try-mac-nvidia-retina-dbg',
    goma_backend = goma.backend.RBE_PROD,
)

gpu_mac_builder(
    name = 'gpu-fyi-try-mac-nvidia-retina-exp',
    # This bot has one machine backing its tests at the moment.
    # If it gets more, the modified execution_timeout should be removed.
    # See crbug.com/853307 for more context.
    execution_timeout = 12 * time.hour,
)

gpu_mac_builder(
    name = 'gpu-fyi-try-mac-nvidia-retina-rel',
)

gpu_mac_builder(
    name = 'gpu-try-mac-amd-retina-dbg',
    goma_backend = goma.backend.RBE_PROD,
)

gpu_mac_builder(
    name = 'gpu-try-mac-intel-dbg',
    goma_backend = goma.backend.RBE_PROD,
)

gpu_mac_builder(
    name = 'mac_optional_gpu_tests_rel',
    goma_backend = goma.backend.RBE_PROD,
    tryjob = tryjob(
        location_regexp = [
            '.+/[+]/chrome/browser/vr/.+',
            '.+/[+]/content/test/gpu/.+',
            '.+/[+]/gpu/.+',
            '.+/[+]/media/audio/.+',
            '.+/[+]/media/filters/.+',
            '.+/[+]/media/gpu/.+',
            '.+/[+]/services/shape_detection/.+',
            '.+/[+]/testing/buildbot/chromium.gpu.fyi.json',
            '.+/[+]/testing/trigger_scripts/.+',
            '.+/[+]/third_party/blink/renderer/modules/webgl/.+',
            '.+/[+]/third_party/blink/renderer/platform/graphics/gpu/.+',
            '.+/[+]/ui/gl/.+',
        ],
    ),
)


def gpu_win_builder(*, name, os=os.WINDOWS_ANY, **kwargs):
  return gpu_builder(
      name = name,
      mastername = 'tryserver.chromium.win',
      os = os,
      **kwargs
  )

gpu_win_builder(
    name = 'gpu-fyi-try-win-xr-builder-64',
)

gpu_win_builder(
    name = 'gpu-fyi-try-win10-intel-dqp-64',
)

gpu_win_builder(
    name = 'gpu-fyi-try-win10-intel-exp-64',
)

gpu_win_builder(
    name = 'gpu-fyi-try-win10-intel-rel-64',
)

gpu_win_builder(
    name = 'gpu-fyi-try-win10-nvidia-dbg-64',
)

gpu_win_builder(
    name = 'gpu-fyi-try-win10-nvidia-dqp-64',
)

gpu_win_builder(
    name = 'gpu-fyi-try-win10-nvidia-dx12vk-dbg-64',
)

gpu_win_builder(
    name = 'gpu-fyi-try-win10-nvidia-dx12vk-rel-64',
)

gpu_win_builder(
    name = 'gpu-fyi-try-win10-nvidia-exp-64',
)

gpu_win_builder(
    name = 'gpu-fyi-try-win10-nvidia-skgl-64',
)

gpu_win_builder(
    name = 'gpu-fyi-try-win10-nvidia-rel-32',
)

gpu_win_builder(
    name = 'gpu-fyi-try-win10-nvidia-rel-64',
)

gpu_win_builder(
    name = 'gpu-fyi-try-win7-amd-dbg-32',
)

gpu_win_builder(
    name = 'gpu-fyi-try-win7-amd-dqp-32',
)

gpu_win_builder(
    name = 'gpu-fyi-try-win7-amd-rel-32',
)

gpu_win_builder(
    name = 'gpu-fyi-try-win7-nvidia-dqp-64',
)

gpu_win_builder(
    name = 'gpu-fyi-try-win7-nvidia-rel-32',
)

gpu_win_builder(
    name = 'gpu-fyi-try-win7-nvidia-rel-64',
)

gpu_win_builder(
    name = 'gpu-try-win10-nvidia-rel',
)

gpu_win_builder(
    name = 'win_optional_gpu_tests_rel',
    builderless = True,
    os = os.WINDOWS_DEFAULT,
    tryjob = tryjob(
        location_regexp = [
            '.+/[+]/chrome/browser/vr/.+',
            '.+/[+]/content/test/gpu/.+',
            '.+/[+]/device/vr/.+',
            '.+/[+]/gpu/.+',
            '.+/[+]/media/audio/.+',
            '.+/[+]/media/filters/.+',
            '.+/[+]/media/gpu/.+',
            '.+/[+]/testing/buildbot/chromium.gpu.fyi.json',
            '.+/[+]/testing/trigger_scripts/.+',
            '.+/[+]/third_party/blink/renderer/modules/vr/.+',
            '.+/[+]/third_party/blink/renderer/modules/webgl/.+',
            '.+/[+]/third_party/blink/renderer/modules/xr/.+',
            '.+/[+]/third_party/blink/renderer/platform/graphics/gpu/.+',
            '.+/[+]/ui/gl/.+',
        ],
    ),
)


def linux_builder(*, name, **kwargs):
  return try_builder(
      name = name,
      mastername = 'tryserver.chromium.linux',
      **kwargs
  )

linux_builder(
    name = 'cast_shell_audio_linux',
    goma_backend = goma.backend.RBE_PROD,
)

linux_builder(
    name = 'cast_shell_linux',
    goma_backend = goma.backend.RBE_PROD,
    tryjob = tryjob(),
)

linux_builder(
    name = 'closure_compilation',
    executable = luci.recipe(name = 'closure_compilation'),
    tryjob = tryjob(
        location_regexp = [
            '.+/[+]/third_party/closure_compiler/.+',
        ],
    ),
)

linux_builder(
    name = 'fuchsia-arm64-cast',
    goma_backend = goma.backend.RBE_PROD,
    tryjob = tryjob(
        location_regexp = [
            '.+/[+]/chromecast/.+',
        ],
    ),
)

linux_builder(
    name = 'fuchsia-compile-x64-dbg',
    goma_backend = goma.backend.RBE_PROD,
    tryjob = tryjob(
        experiment_percentage = 50,
    ),
)

linux_builder(
    name = 'fuchsia-fyi-arm64-rel',
    goma_backend = goma.backend.RBE_PROD,
)

linux_builder(
    name = 'fuchsia-fyi-x64-dbg',
    goma_backend = goma.backend.RBE_PROD,
)

linux_builder(
    name = 'fuchsia-fyi-x64-rel',
    goma_backend = goma.backend.RBE_PROD,
)

linux_builder(
    name = 'fuchsia-x64-cast',
    goma_backend = goma.backend.RBE_PROD,
    tryjob = tryjob(
        location_regexp = [
            '.+/[+]/chromecast/.+',
        ],
    ),
)

linux_builder(
    name = 'fuchsia_arm64',
    goma_backend = goma.backend.RBE_PROD,
    tryjob = tryjob(),
)

linux_builder(
    name = 'fuchsia_x64',
    goma_backend = goma.backend.RBE_PROD,
    tryjob = tryjob(),
)

linux_builder(
    name = 'layout_test_leak_detection',
    goma_backend = goma.backend.RBE_PROD,
)

linux_builder(
    name = 'leak_detection_linux',
    goma_backend = goma.backend.RBE_PROD,
)

linux_builder(
    name = 'linux-annotator-rel',
    goma_backend = goma.backend.RBE_PROD,
)

linux_builder(
    name = 'linux-blink-heap-concurrent-marking-tsan-rel',
    goma_backend = goma.backend.RBE_PROD,
)

linux_builder(
    name = 'linux-blink-heap-verification-try',
    goma_backend = goma.backend.RBE_PROD,
)

linux_builder(
    name = 'linux-clang-tidy-dbg',
    executable = luci.recipe(name = 'tricium_clang_tidy_wrapper'),
    goma_jobs = goma.jobs.J150,
)

linux_builder(
    name = 'linux-clang-tidy-rel',
    executable = luci.recipe(name = 'tricium_clang_tidy_wrapper'),
    goma_jobs = goma.jobs.J150,
)

linux_builder(
    name = 'linux-dcheck-off-rel',
    goma_backend = goma.backend.RBE_PROD,
)

linux_builder(
    name = 'linux-gcc-rel',
)

linux_builder(
    name = 'linux-libfuzzer-asan-rel',
    executable = luci.recipe(name = 'chromium_libfuzzer_trybot'),
    goma_backend = goma.backend.RBE_PROD,
    tryjob = tryjob(),
)

linux_builder(
    name = 'linux-ozone-rel',
    goma_backend = goma.backend.RBE_PROD,
    tryjob = tryjob(),
)

linux_builder(
    name = 'linux-trusty-rel',
    goma_backend = goma.backend.RBE_PROD,
    goma_jobs = goma.jobs.J150,
    os = os.LINUX_TRUSTY,
)

linux_builder(
    name = 'linux-viz-rel',
)

linux_builder(
    name = 'linux-webkit-msan-rel',
    goma_backend = goma.backend.RBE_PROD,
)

linux_builder(
    name = 'linux_arm',
    # TODO(crbug.com/986191): re-enable RBE+ATS when the issue is fixed.
    # goma_backend = goma.backend.RBE_PROD,
    # goma_enable_ats = True,
)

linux_builder(
    name = 'linux_chromium_analysis',
)

linux_builder(
    name = 'linux_chromium_archive_rel_ng',
)

linux_builder(
    name = 'linux_chromium_asan_rel_ng',
    goma_backend = goma.backend.RBE_PROD,
    goma_jobs = goma.jobs.J150,
    ssd = True,
    tryjob = tryjob(),
)

linux_builder(
    name = 'linux_chromium_cfi_rel_ng',
    cores = 32,
    goma_backend = goma.backend.RBE_PROD,
)

linux_builder(
    name = 'linux_chromium_chromeos_asan_rel_ng',
    goma_jobs = goma.jobs.J150,
)

linux_builder(
    name = 'linux_chromium_chromeos_msan_rel_ng',
    goma_jobs = goma.jobs.J150,
)

linux_builder(
    name = 'linux_chromium_clobber_deterministic',
    executable = luci.recipe(name = 'swarming/deterministic_build'),
    execution_timeout = 6 * time.hour,
)

linux_builder(
    name = 'linux_chromium_clobber_rel_ng',
)

linux_builder(
    name = 'linux_chromium_compile_dbg_32_ng',
)

linux_builder(
    name = 'linux_chromium_compile_dbg_ng',
    caches = [
        swarming.cache(
            name = 'builder',
            path = 'linux_debug',
        ),
    ],
    goma_backend = goma.backend.RBE_PROD,
    goma_jobs = goma.jobs.J150,
    tryjob = tryjob(),
)

linux_builder(
    name = 'linux_chromium_compile_rel_ng',
    goma_backend = goma.backend.RBE_PROD,
)

linux_builder(
    name = 'linux_chromium_dbg_ng',
    caches = [
        swarming.cache(
            name = 'builder',
            path = 'linux_debug',
        ),
    ],
    goma_backend = goma.backend.RBE_PROD,
    tryjob = tryjob(
        location_regexp = [
            '.+/[+]/build/.*check_gn_headers.*',
        ],
    ),
)

linux_builder(
    name = 'linux_chromium_msan_rel_ng',
    goma_backend = goma.backend.RBE_PROD,
    goma_jobs = goma.jobs.J150,
)

linux_builder(
    name = 'linux_chromium_tsan_rel_ng',
    goma_backend = goma.backend.RBE_PROD,
    goma_jobs = goma.jobs.J150,
    tryjob = tryjob(),
)

linux_builder(
    name = 'linux_chromium_ubsan_rel_ng',
    goma_backend = goma.backend.RBE_PROD,
)

linux_builder(
    name = 'linux_layout_tests_composite_after_paint',
    goma_backend = goma.backend.RBE_PROD,
    tryjob = tryjob(
        location_regexp = [
            '.+/[+]/third_party/blink/renderer/core/paint/.+',
            '.+/[+]/third_party/blink/renderer/core/svg/.+',
            '.+/[+]/third_party/blink/renderer/platform/graphics/.+',
            '.+/[+]/third_party/blink/web_tests/FlagExpectations/enable-blink-features=CompositeAfterPaint',
            '.+/[+]/third_party/blink/web_tests/flag-specific/enable-blink-features=CompositeAfterPaint/.+',
        ],
    ),
)

linux_builder(
    name = 'linux_layout_tests_layout_ng_disabled',
    goma_backend = goma.backend.RBE_PROD,
    tryjob = tryjob(
        location_regexp = [
            '.+/[+]/third_party/blink/renderer/core/editing/.+',
            '.+/[+]/third_party/blink/renderer/core/layout/.+',
            '.+/[+]/third_party/blink/renderer/core/paint/.+',
            '.+/[+]/third_party/blink/renderer/core/svg/.+',
            '.+/[+]/third_party/blink/renderer/platform/fonts/shaping/.+',
            '.+/[+]/third_party/blink/renderer/platform/graphics/.+',
            '.+/[+]/third_party/blink/web_tests/flag-specific/enable-blink-features=LayoutNG/.+',
        ],
    ),
)

linux_builder(
    name = 'linux-layout-tests-fragment-item',
)

linux_builder(
    name = 'linux-layout-tests-fragment-paint',
)

linux_builder(
    name = 'linux_mojo',
    goma_backend = goma.backend.RBE_PROD,
)

linux_builder(
    name = 'linux_mojo_chromeos',
)

linux_builder(
    name = 'linux_upload_clang',
    builderless = False,
    cores = 32,
    executable = luci.recipe(name = 'chromium_upload_clang'),
    os = os.LINUX_TRUSTY,
)

linux_builder(
    name = 'linux_vr',
    goma_backend = goma.backend.RBE_PROD,
    tryjob = tryjob(
        location_regexp = [
            '.+/[+]/chrome/browser/vr/.+',
        ],
    ),
)

linux_builder(
    name = 'tricium-metrics-analysis',
    executable = luci.recipe(name = 'tricium_metrics'),
)


def mac_builder(*, name, cores=None, os=os.MAC_ANY, **kwargs):
  return try_builder(
      name = name,
      cores = cores,
      mastername = 'tryserver.chromium.mac',
      os = os,
      **kwargs
  )

mac_builder(
    name = 'mac-osxbeta-rel',
    builderless = True,
    os = os.MAC_DEFAULT,
    ssd = True,
)

mac_builder(
    name = 'mac-rel',
    goma_backend = goma.backend.RBE_PROD,
    goma_jobs = goma.jobs.J150,
    tryjob = tryjob(),
)

# NOTE: the following 3 trybots aren't sensitive to Mac version on which
# they are built, hence no additional dimension is specified.
# The 10.xx version translates to which bots will run isolated tests.
mac_builder(
    name = 'mac_chromium_10.10',
    goma_backend = goma.backend.RBE_PROD,
)

mac_builder(
    name = 'mac_chromium_10.12_rel_ng',
    goma_backend = goma.backend.RBE_PROD,
)

mac_builder(
    name = 'mac_chromium_10.13_rel_ng',
    goma_backend = goma.backend.RBE_PROD,
)

mac_builder(
    name = 'mac_chromium_10.14_rel_ng',
    goma_backend = goma.backend.RBE_PROD,
)

mac_builder(
    name = 'mac_chromium_archive_rel_ng',
)

mac_builder(
    name = 'mac_chromium_asan_rel_ng',
    goma_jobs = goma.jobs.J150,
    goma_backend = goma.backend.RBE_PROD,
)

mac_builder(
    name = 'mac_chromium_compile_dbg_ng',
    builderless = True,
    goma_backend = goma.backend.RBE_PROD,
    goma_jobs = goma.jobs.J150,
    os = os.MAC_10_13,
    ssd = True,
    tryjob = tryjob(),
)

mac_builder(
    name = 'mac_chromium_compile_rel_ng',
    goma_backend = goma.backend.RBE_PROD,
)

mac_builder(
    name = 'mac_chromium_dbg_ng',
    goma_backend = goma.backend.RBE_PROD,
)

mac_builder(
    name = 'mac_upload_clang',
    caches = [
        swarming.cache(
            name = 'xcode_mac_9a235',
            path = 'xcode_mac_9a235.app',
        ),
    ],
    executable = luci.recipe(name = 'chromium_upload_clang'),
    execution_timeout = 6 * time.hour,
    properties = {
        '$depot_tools/osx_sdk': {
            'sdk_version': '9a235',
        },
    },
)


def mac_ios_builder(*, name, executable=luci.recipe(name = 'ios/try'), **kwargs):
  return try_builder(
      name = name,
      caches = [
          swarming.cache(
              name = 'xcode_ios_11a1027',
              path = 'xcode_ios_11a1027.app',
          ),
      ],
      cores = None,
      executable = executable,
      mastername = 'tryserver.chromium.mac',
      os = os.MAC_ANY,
      **kwargs
  )

mac_ios_builder(
    name = 'ios-device',
    tryjob = tryjob(
        # https://crbug.com/739556; make this non-experimental ASAP.
        experiment_percentage = 10,
    ),
)

mac_ios_builder(
    name = 'ios-device-xcode-clang',
    tryjob = tryjob(
        # https://crbug.com/739556
        experiment_percentage = 10,
    ),
)

mac_ios_builder(
    name = 'ios-simulator',
    tryjob = tryjob(),
)

mac_ios_builder(
    name = 'ios-simulator-cr-recipe',
    executable = luci.recipe(name = 'chromium_trybot'),
    properties = {
        'xcode_build_version': '11a1027',
    },
)

mac_ios_builder(
    name = 'ios-simulator-cronet',
    tryjob = tryjob(
        location_regexp = [
            '.+/[+]/components/cronet/.+',
            '.+/[+]/components/grpc_support/.+',
            '.+/[+]/ios/.+',
        ],
        location_regexp_exclude = [
            '.+/[+]/components/cronet/android/.+',
        ],
    ),
)

mac_ios_builder(
    name = 'ios-simulator-eg',
)

mac_ios_builder(
    name = 'ios-simulator-noncq',
)

mac_ios_builder(
    name = 'ios-simulator-full-configs',
    tryjob = tryjob(
        location_regexp = [
            '.+/[+]/ios/.+',
        ],
    ),
)

mac_ios_builder(
    name = 'ios-simulator-xcode-clang',
    tryjob = tryjob(
        # https://crbug.com/739556
        experiment_percentage = 10,
    ),
)

mac_ios_builder(
    name = 'ios-slimnav',
)

mac_ios_builder(
    name = 'ios13-beta-simulator',
)

mac_ios_builder(
    name = 'ios13-sdk-simulator',
)


def swangle_builder(*, name, **kwargs):
  return try_builder(
      name = name,
      builderless = True,
      mastername = 'tryserver.chromium.swangle',
      service_account = 'chromium-try-gpu-builder@chops-service-accounts.iam.gserviceaccount.com',
      **kwargs
  )


def swangle_linux_builder(*, name, **kwargs):
  return swangle_builder(
      name = name,
      os = os.LINUX_DEFAULT,
      **kwargs
  )

swangle_linux_builder(
    name = 'linux-swangle-try-tot-angle-x64'
)

swangle_linux_builder(
    name = 'linux-swangle-try-tot-angle-x86'
)

swangle_linux_builder(
    name = 'linux-swangle-try-tot-swiftshader-x64'
)

swangle_linux_builder(
    name = 'linux-swangle-try-tot-swiftshader-x86'
)

swangle_linux_builder(
    name = 'linux-swangle-try-x64'
)

swangle_linux_builder(
    name = 'linux-swangle-try-x86'
)


def swangle_windows_builder(*, name, **kwargs):
  return swangle_builder(
      name = name,
      os = os.WINDOWS_DEFAULT,
      **kwargs
  )

swangle_windows_builder(
    name = 'win-swangle-try-tot-angle-x64'
)

swangle_windows_builder(
    name = 'win-swangle-try-tot-angle-x86'
)

swangle_windows_builder(
    name = 'win-swangle-try-tot-swiftshader-x64'
)

swangle_windows_builder(
    name = 'win-swangle-try-tot-swiftshader-x86'
)

swangle_windows_builder(
    name = 'win-swangle-try-x64'
)

swangle_windows_builder(
    name = 'win-swangle-try-x86'
)


def win_builder(*, name, builderless=True, os=os.WINDOWS_DEFAULT, **kwargs):
  return try_builder(
      name = name,
      builderless = builderless,
      mastername = 'tryserver.chromium.win',
      os = os,
      **kwargs
  )

win_builder(
    name = 'win-annotator-rel',
)

win_builder(
    name = 'win-asan',
    goma_jobs = goma.jobs.J150,
)

win_builder(
    name = 'win-celab-try-rel',
    executable = luci.recipe(name = 'celab'),
    properties = {
        'exclude': 'chrome_only',
        'pool_name': 'celab-chromium-try',
        'pool_size': 20,
        'tests': '*',
    },
)

win_builder(
    name = 'win-libfuzzer-asan-rel',
    builderless = False,
    executable = luci.recipe(name = 'chromium_libfuzzer_trybot'),
    os = os.WINDOWS_ANY,
    tryjob = tryjob(),
)

win_builder(
    name = 'win10_chromium_x64_dbg_ng',
    os = os.WINDOWS_10,
)

win_builder(
    name = 'win10_chromium_x64_rel_ng',
    goma_jobs = goma.jobs.J150,
    os = os.WINDOWS_10,
    ssd = True,
    tryjob = tryjob(),
)

win_builder(
    name = 'win10_chromium_x64_rel_ng_exp',
    builderless = False,
    os = os.WINDOWS_ANY,
)

win_builder(
    name = 'win7-rel',
    execution_timeout = time.hour * 9 / 2,  # 4.5 (can't multiply float * duration)
    goma_jobs = goma.jobs.J300,
    ssd = True,
)

win_builder(
    name = 'win_archive',
)

win_builder(
    name = 'win_chromium_compile_dbg_ng',
    goma_jobs = goma.jobs.J150,
    tryjob = tryjob(),
)

win_builder(
    name = 'win_chromium_compile_rel_ng',
)

win_builder(
    name = 'win_chromium_dbg_ng',
)

win_builder(
    name = 'win_chromium_x64_rel_ng',
)

win_builder(
    name = 'win_mojo',
)

win_builder(
    name = 'win_upload_clang',
    builderless = False,
    cores = 32,
    executable = luci.recipe(name = 'chromium_upload_clang'),
    os = os.WINDOWS_ANY,
)

win_builder(
    name = 'win_x64_archive',
)
