load('//lib/builders.star', 'builder', 'cpu', 'defaults', 'goma', 'os')

# Defaults that apply to all branch versions of the bucket

luci.recipe.defaults.cipd_package.set(
    'infra/recipe_bundles/chromium.googlesource.com/chromium/tools/build')

defaults.build_numbers.set(True)
defaults.configure_kitchen.set(True)
defaults.cores.set(8)
defaults.cpu.set(cpu.X86_64)
defaults.executable.set(luci.recipe(name = 'chromium'))
defaults.execution_timeout.set(3 * time.hour)
defaults.os.set(os.LINUX_DEFAULT)
defaults.service_account.set(
    'chromium-ci-builder@chops-service-accounts.iam.gserviceaccount.com')
defaults.swarming_tags.set(['vpython:native-python-wrapper'])


# Execute the versioned files to define all of the per-branch entities
# (bucket, builders, console, poller, etc.)
exec('//versioned/branches/beta/buckets/ci.star')
exec('//versioned/branches/stable/buckets/ci.star')
exec('//versioned/trunk/buckets/ci.star')


# *** After this point everything is trunk only ***
defaults.bucket.set('ci')


XCODE_IOS_11_CACHE = swarming.cache(
    name = 'xcode_ios_11a1027',
    path = 'xcode_ios_11a1027.app',
)


# Builders appear after the function used to define them, with all builders
# defined using the same function ordered lexicographically by name
# Builder functions are defined in lexicographic order by name ignoring the
# '_builder' suffix

# Builder functions are defined for each master, with additional functions
# for specializing on OS or particular configuration (e.g. builders running
# libfuzzer recipe): XXX_builder and XXX_YYY_builder where XXX is the part after
# the last dot in the mastername and YYY is the OS or configuration


builder(
    name = 'android-avd-packager',
    executable = luci.recipe(name = 'android/avd_packager'),
    properties = {
        'avd_configs': [
            'tools/android/avd/proto/generic_android23.textpb',
            'tools/android/avd/proto/generic_android28.textpb',
        ],
    },
    service_account = 'chromium-cipd-builder@chops-service-accounts.iam.gserviceaccount.com',
)

builder(
    name = 'android-sdk-packager',
    executable = luci.recipe(name = 'android/sdk_packager'),
    service_account = 'chromium-cipd-builder@chops-service-accounts.iam.gserviceaccount.com',
    properties = {
        # We still package part of build-tools;25.0.2 to support
        # http://bit.ly/2KNUygZ
        'packages': [
            {
                'sdk_package_name': 'build-tools;25.0.2',
                'cipd_yaml': 'third_party/android_sdk/cipd/build-tools/25.0.2.yaml'
            },
            {
                'sdk_package_name': 'build-tools;27.0.3',
                'cipd_yaml': 'third_party/android_sdk/cipd/build-tools/27.0.3.yaml'
            },
            {
                'sdk_package_name': 'build-tools;29.0.2',
                'cipd_yaml': 'third_party/android_sdk/cipd/build-tools/29.0.2.yaml'
            },
            {
                'sdk_package_name': 'emulator',
                'cipd_yaml': 'third_party/android_sdk/cipd/emulator.yaml'
            },
            {
                'sdk_package_name': 'extras;google;gcm',
                'cipd_yaml': 'third_party/android_sdk/cipd/extras/google/gcm.yaml'
            },
            {
                'sdk_package_name': 'patcher;v4',
                'cipd_yaml': 'third_party/android_sdk/cipd/patcher/v4.yaml'
            },
            {
                'sdk_package_name': 'platforms;android-23',
                'cipd_yaml': 'third_party/android_sdk/cipd/platforms/android-23.yaml'
            },
            {
                'sdk_package_name': 'platforms;android-28',
                'cipd_yaml': 'third_party/android_sdk/cipd/platforms/android-28.yaml'
            },
            {
                'sdk_package_name': 'platforms;android-29',
                'cipd_yaml': 'third_party/android_sdk/cipd/platforms/android-29.yaml'
            },
            {
                'sdk_package_name': 'platform-tools',
                'cipd_yaml': 'third_party/android_sdk/cipd/platform-tools.yaml'
            },
            {
                'sdk_package_name': 'sources;android-28',
                'cipd_yaml': 'third_party/android_sdk/cipd/sources/android-28.yaml'
            },
            {
                'sdk_package_name': 'sources;android-29',
                'cipd_yaml': 'third_party/android_sdk/cipd/sources/android-29.yaml'
            },
            {
                'sdk_package_name': 'system-images;android-23;google_apis;x86',
                'cipd_yaml': 'third_party/android_sdk/cipd/system_images/android-23/google_apis/x86.yaml'
            },
            {
                'sdk_package_name': 'system-images;android-28;google_apis;x86',
                'cipd_yaml': 'third_party/android_sdk/cipd/system_images/android-28/google_apis/x86.yaml'
            },
            {
                'sdk_package_name': 'system-images;android-29;google_apis;x86',
                'cipd_yaml': 'third_party/android_sdk/cipd/system_images/android-29/google_apis/x86.yaml'
            },
            {
                'sdk_package_name': 'tools',
                'cipd_yaml': 'third_party/android_sdk/cipd/tools.yaml'
            },
        ],
    },
)


def android_builder(
    *,
    name,
    # TODO(tandrii): migrate to this gradually (current value of
    # goma.jobs.MANY_JOBS_FOR_CI is 500).
    # goma_jobs=goma.jobs.MANY_JOBS_FOR_CI
    goma_jobs=goma.jobs.J150,
    **kwargs):
  return builder(
      name = name,
      goma_jobs = goma_jobs,
      mastername = 'chromium.android',
      **kwargs
  )

android_builder(
    name = 'Android ASAN (dbg)',
    goma_backend = goma.backend.RBE_PROD,
)

android_builder(
    name = 'Android WebView L (dbg)',
    goma_backend = goma.backend.RBE_PROD,
)

android_builder(
    name = 'Android WebView M (dbg)',
    goma_backend = goma.backend.RBE_PROD,
)

android_builder(
    name = 'Android WebView N (dbg)',
    goma_backend = goma.backend.RBE_PROD,
)

android_builder(
    name = 'Android WebView O (dbg)',
    goma_backend = goma.backend.RBE_PROD,
)

android_builder(
    name = 'Android WebView P (dbg)',
    goma_backend = goma.backend.RBE_PROD,
)

android_builder(
    name = 'Android arm Builder (dbg)',
    goma_backend = goma.backend.RBE_PROD,
    execution_timeout = 4 * time.hour,
)

android_builder(
    name = 'Android arm64 Builder (dbg)',
    goma_backend = goma.backend.RBE_PROD,
    goma_jobs = goma.jobs.MANY_JOBS_FOR_CI,
    execution_timeout = 4 * time.hour,
)

android_builder(
    name = 'Android x64 Builder (dbg)',
    execution_timeout = 4 * time.hour,
    goma_backend = goma.backend.RBE_PROD,
)

android_builder(
    name = 'Android x86 Builder (dbg)',
    goma_backend = goma.backend.RBE_PROD,
)

android_builder(
    name = 'Cast Android (dbg)',
    goma_backend = goma.backend.RBE_PROD,
)

android_builder(
    name = 'Deterministic Android',
    executable = luci.recipe(name = 'swarming/deterministic_build'),
    execution_timeout = 6 * time.hour,
    goma_backend = goma.backend.RBE_PROD,
)

android_builder(
    name = 'Deterministic Android (dbg)',
    executable = luci.recipe(name = 'swarming/deterministic_build'),
    execution_timeout = 6 * time.hour,
    goma_backend = goma.backend.RBE_PROD,
)

android_builder(
    name = 'KitKat Phone Tester (dbg)',
)

android_builder(
    name = 'KitKat Tablet Tester',
    # We have limited tablet capacity and thus limited ability to run
    # tests in parallel, hence the high timeout.
    execution_timeout = 8 * time.hour,
)

android_builder(
    name = 'Lollipop Phone Tester',
)

android_builder(
    name = 'Lollipop Tablet Tester',
    # We have limited tablet capacity and thus limited ability to run
    # tests in parallel, hence the high timeout.
    execution_timeout = 8 * time.hour,
)

android_builder(
    name = 'Marshmallow 64 bit Tester',
)

android_builder(
    name = 'Marshmallow Tablet Tester',
    # We have limited tablet capacity and thus limited ability to run
    # tests in parallel, hence the high timeout.
    execution_timeout = 8 * time.hour,
)

android_builder(
    name = 'Nougat Phone Tester',
)

android_builder(
    name = 'Oreo Phone Tester',
)

android_builder(
    name = 'android-cronet-arm-dbg',
    goma_backend = goma.backend.RBE_PROD,
    notifies = ['cronet'],
)

android_builder(
    name = 'android-cronet-arm-rel',
    goma_backend = goma.backend.RBE_PROD,
    notifies = ['cronet'],
)

android_builder(
    name = 'android-cronet-arm64-dbg',
    goma_backend = goma.backend.RBE_PROD,
    notifies = ['cronet'],
)

android_builder(
    name = 'android-cronet-arm64-rel',
    goma_backend = goma.backend.RBE_PROD,
    notifies = ['cronet'],
)

android_builder(
    name = 'android-cronet-asan-arm-rel',
    goma_backend = goma.backend.RBE_PROD,
    notifies = ['cronet'],
)

android_builder(
    name = 'android-cronet-kitkat-arm-rel',
    goma_backend = goma.backend.RBE_PROD,
    notifies = ['cronet'],
)

android_builder(
    name = 'android-cronet-lollipop-arm-rel',
    goma_backend = goma.backend.RBE_PROD,
    notifies = ['cronet'],
)

# Runs on a specific machine with an attached phone
android_builder(
    name = 'android-cronet-marshmallow-arm64-perf-rel',
    cores = None,
    cpu = None,
    executable = luci.recipe(name = 'cronet'),
    goma_backend = goma.backend.RBE_PROD,
    notifies = ['cronet'],
    os = os.ANDROID,
)

android_builder(
    name = 'android-cronet-marshmallow-arm64-rel',
    goma_backend = goma.backend.RBE_PROD,
    notifies = ['cronet'],
)

android_builder(
    name = 'android-cronet-x86-dbg',
    goma_backend = goma.backend.RBE_PROD,
    notifies = ['cronet'],
)

android_builder(
    name = 'android-cronet-x86-rel',
    goma_backend = goma.backend.RBE_PROD,
    notifies = ['cronet'],
)

android_builder(
    name = 'android-incremental-dbg',
    goma_backend = goma.backend.RBE_PROD,
)

android_builder(
    name = 'android-kitkat-arm-rel',
    goma_backend = goma.backend.RBE_PROD,
)

android_builder(
    name = 'android-marshmallow-arm64-rel',
    goma_backend = goma.backend.RBE_PROD,
)

android_builder(
    name = 'android-pie-arm64-dbg',
    goma_backend = goma.backend.RBE_PROD,
)

android_builder(
    name = 'android-pie-arm64-rel',
    goma_backend = goma.backend.RBE_PROD,
)


def android_fyi_builder(*, name, **kwargs):
  return builder(
      name = name,
      mastername = 'chromium.android.fyi',
      **kwargs
  )

android_fyi_builder(
    name = 'android-bfcache-debug',
    goma_backend = goma.backend.RBE_PROD,
)

android_fyi_builder(
    name = 'Android WebView P FYI (rel)',
    goma_backend = goma.backend.RBE_PROD,
)

android_fyi_builder(
    name = 'Android WebView P OOR-CORS FYI (rel)',
    goma_backend = goma.backend.RBE_PROD,
)

android_fyi_builder(
    name = 'android-marshmallow-x86-fyi-rel',
)

android_fyi_builder(
    name = 'android-pie-x86-fyi-rel',
    goma_backend = goma.backend.RBE_PROD,
)

android_fyi_builder(
    name = 'Memory Infra Tester',
    notifies = ['chrome-memory-sheriffs'],
)


def chromium_builder(*, name, **kwargs):
  return builder(
      name = name,
      mastername = 'chromium',
      **kwargs
  )

chromium_builder(
    name = 'android-archive-dbg',
    # Bump to 32 if needed.
    cores = 8,
    goma_backend = goma.backend.RBE_PROD,
)

chromium_builder(
    name = 'android-archive-rel',
    cores = 32,
    goma_backend = goma.backend.RBE_PROD,
)

chromium_builder(
    name = 'linux-archive-dbg',
    # Bump to 32 if needed.
    cores = 8,
    goma_backend = goma.backend.RBE_PROD,
)

chromium_builder(
    name = 'linux-archive-rel',
    cores = 32,
    goma_backend = goma.backend.RBE_PROD,
)

chromium_builder(
    name = 'mac-archive-dbg',
    # Bump to 8 cores if needed.
    cores = 4,
    goma_backend = goma.backend.RBE_PROD,
    os = os.MAC_DEFAULT,
)

chromium_builder(
    name = 'mac-archive-rel',
    goma_backend = goma.backend.RBE_PROD,
    os = os.MAC_DEFAULT,
)

chromium_builder(
    name = 'win-archive-dbg',
    cores = 32,
    os = os.WINDOWS_DEFAULT,
)

chromium_builder(
    name = 'win-archive-rel',
    cores = 32,
    os = os.WINDOWS_DEFAULT,
)

chromium_builder(
    name = 'win32-archive-dbg',
    cores = 32,
    os = os.WINDOWS_DEFAULT,
)

chromium_builder(
    name = 'win32-archive-rel',
    cores = 32,
    os = os.WINDOWS_DEFAULT,
)


def chromiumos_builder(*, name, **kwargs):
  return builder(
      name = name,
      mastername = 'chromium.chromiumos',
  )

chromiumos_builder(
    name = 'Linux ChromiumOS Full',
)

chromiumos_builder(
    name = 'chromeos-amd64-generic-asan-rel',
)

chromiumos_builder(
    name = 'chromeos-amd64-generic-cfi-thin-lto-rel',
)

chromiumos_builder(
    name = 'chromeos-amd64-generic-dbg',
)

chromiumos_builder(
    name = 'chromeos-amd64-generic-rel',
)

chromiumos_builder(
    name = 'chromeos-arm-generic-dbg',
)

chromiumos_builder(
    name = 'chromeos-arm-generic-rel',
)

chromiumos_builder(
    name = 'chromeos-kevin-rel',
)

chromiumos_builder(
    name = 'linux-chromeos-dbg',
)

chromiumos_builder(
    name = 'linux-chromeos-rel',
)


def clang_builder(*, name, cores=32, properties=None, **kwargs):
  properties = properties or {}
  properties.update({
    'perf_dashboard_machine_group': 'ChromiumClang',
  })
  return builder(
      name = name,
      builderless = True,
      cores = cores,
      # Because these run ToT Clang, goma is not used.
      # Naturally the runtime will be ~4-8h on average, depending on config.
      # CFI builds will take even longer - around 11h.
      execution_timeout = 12 * time.hour,
      mastername = 'chromium.clang',
      properties = properties,
      **kwargs
  )

clang_builder(
    name = 'CFI Linux CF',
    goma_backend = goma.backend.RBE_PROD,
)

clang_builder(
    name = 'CFI Linux ToT',
)

clang_builder(
    name = 'CrWinAsan',
    os = os.WINDOWS_ANY,
)

clang_builder(
    name = 'CrWinAsan(dll)',
    os = os.WINDOWS_ANY,
)

clang_builder(
    name = 'ToTAndroid',
)

clang_builder(
    name = 'ToTAndroid (dbg)',
)

clang_builder(
    name = 'ToTAndroid x64',
)

clang_builder(
    name = 'ToTAndroid64',
)

clang_builder(
    name = 'ToTAndroidASan',
)

clang_builder(
    name = 'ToTAndroidCFI',
)

clang_builder(
    name = 'ToTAndroidOfficial',
)

clang_builder(
    name = 'ToTLinux',
)

clang_builder(
    name = 'ToTLinux (dbg)',
)

clang_builder(
    name = 'ToTLinuxASan',
)

clang_builder(
    name = 'ToTLinuxASanLibfuzzer',
)

clang_builder(
    name = 'ToTLinuxCoverage',
    executable = luci.recipe(name = 'chromium_clang_coverage_tot'),
)

clang_builder(
    name = 'ToTLinuxMSan',
)

clang_builder(
    name = 'ToTLinuxTSan',
)

clang_builder(
    name = 'ToTLinuxThinLTO',
)

clang_builder(
    name = 'ToTLinuxUBSanVptr',
)

clang_builder(
    name = 'ToTWin(dbg)',
    os = os.WINDOWS_ANY,
)

clang_builder(
    name = 'ToTWin(dll)',
    os = os.WINDOWS_ANY,
)

clang_builder(
    name = 'ToTWin64(dbg)',
    os = os.WINDOWS_ANY,
)

clang_builder(
    name = 'ToTWin64(dll)',
    os = os.WINDOWS_ANY,
)

clang_builder(
    name = 'ToTWinASanLibfuzzer',
    os = os.WINDOWS_ANY,
)

clang_builder(
    name = 'ToTWinCFI',
    os = os.WINDOWS_ANY,
)

clang_builder(
    name = 'ToTWinCFI64',
    os = os.WINDOWS_ANY,
)

clang_builder(
    name = 'ToTWinLibcxx64',
    os = os.WINDOWS_ANY,
)

clang_builder(
    name = 'UBSanVptr Linux',
    goma_backend = goma.backend.RBE_PROD,
)

clang_builder(
    name = 'linux-win_cross-rel',
)


def clang_ios_builder(*, name, **kwargs):
  return clang_builder(
      name = name,
      caches = [XCODE_IOS_11_CACHE],
      cores = None,
      executable = luci.recipe(name = 'ios/unified_builder_tester'),
      os = os.MAC_10_14,
      ssd = True,
  )

clang_ios_builder(
    name = 'ToTiOS',
)

clang_ios_builder(
    name = 'ToTiOSDevice',
)

def clang_mac_builder(*, name, cores=24, **kwargs):
  return clang_builder(
      name = name,
      cores = cores,
      os = os.MAC_10_14,
      ssd = True,
      properties = {
          'xcode_build_version': '11a1027',
      },
      **kwargs
  )

clang_mac_builder(
    name = 'ToTMac',
)

clang_mac_builder(
    name = 'ToTMac (dbg)',
)

clang_mac_builder(
    name = 'ToTMacASan',
)

clang_mac_builder(
    name = 'ToTMacCoverage',
    executable = luci.recipe(name = 'chromium_clang_coverage_tot'),
)

def dawn_builder(*, name, builderless=True, **kwargs):
  return builder(
      name = name,
      builderless = builderless,
      mastername = 'chromium.dawn',
      service_account = 'chromium-ci-gpu-builder@chops-service-accounts.iam.gserviceaccount.com',
      **kwargs
  )

dawn_builder(
    name = 'Dawn Linux x64 Builder',
    goma_backend = goma.backend.RBE_PROD,
)

dawn_builder(
    name = 'Dawn Linux x64 DEPS Builder',
    goma_backend = goma.backend.RBE_PROD,
)

dawn_builder(
    name = 'Dawn Linux x64 DEPS Release (Intel HD 630)',
    cores = 2,
    os = os.LINUX_DEFAULT,
)

dawn_builder(
    name = 'Dawn Linux x64 DEPS Release (NVIDIA)',
    cores = 2,
    os = os.LINUX_DEFAULT,
)

dawn_builder(
    name = 'Dawn Linux x64 Release (Intel HD 630)',
    cores = 2,
    os = os.LINUX_DEFAULT,
)

dawn_builder(
    name = 'Dawn Linux x64 Release (NVIDIA)',
    cores = 2,
    os = os.LINUX_DEFAULT,
)

dawn_builder(
    name = 'Dawn Mac x64 Builder',
    builderless = False,
    cores = None,
    goma_backend = goma.backend.RBE_PROD,
    os = os.MAC_ANY,
)

dawn_builder(
    name = 'Dawn Mac x64 DEPS Builder',
    builderless = False,
    cores = None,
    goma_backend = goma.backend.RBE_PROD,
    os = os.MAC_ANY,
)

# Note that the Mac testers are all thin Linux VMs, triggering jobs on the
# physical Mac hardware in the Swarming pool which is why they run on linux
dawn_builder(
    name = 'Dawn Mac x64 DEPS Release (AMD)',
    cores = 2,
    os = os.LINUX_DEFAULT,
)

dawn_builder(
    name = 'Dawn Mac x64 DEPS Release (Intel)',
    cores = 2,
    os = os.LINUX_DEFAULT,
)

dawn_builder(
    name = 'Dawn Mac x64 Release (AMD)',
    cores = 2,
    os = os.LINUX_DEFAULT,
)

dawn_builder(
    name = 'Dawn Mac x64 Release (Intel)',
    cores = 2,
    os = os.LINUX_DEFAULT,
)

dawn_builder(
    name = 'Dawn Win10 x86 Builder',
    os = os.WINDOWS_ANY,
)

dawn_builder(
    name = 'Dawn Win10 x64 Builder',
    os = os.WINDOWS_ANY,
)

# Note that the Win testers are all thin Linux VMs, triggering jobs on the
# physical Win hardware in the Swarming pool, which is why they run on linux
dawn_builder(
    name = 'Dawn Win10 x86 Release (Intel HD 630)',
    cores = 2,
    os = os.LINUX_DEFAULT,
)

dawn_builder(
    name = 'Dawn Win10 x64 Release (Intel HD 630)',
    cores = 2,
    os = os.LINUX_DEFAULT,
)

dawn_builder(
    name = 'Dawn Win10 x86 Release (NVIDIA)',
    cores = 2,
    os = os.LINUX_DEFAULT,
)

dawn_builder(
    name = 'Dawn Win10 x64 Release (NVIDIA)',
    cores = 2,
    os = os.LINUX_DEFAULT,
)

dawn_builder(
    name = 'Dawn Win10 x86 DEPS Builder',
    os = os.WINDOWS_ANY,
)

dawn_builder(
    name = 'Dawn Win10 x64 DEPS Builder',
    os = os.WINDOWS_ANY,
)

dawn_builder(
    name = 'Dawn Win10 x86 DEPS Release (Intel HD 630)',
    cores = 2,
    os = os.LINUX_DEFAULT,
)

dawn_builder(
    name = 'Dawn Win10 x64 DEPS Release (Intel HD 630)',
    cores = 2,
    os = os.LINUX_DEFAULT,
)

dawn_builder(
    name = 'Dawn Win10 x86 DEPS Release (NVIDIA)',
    cores = 2,
    os = os.LINUX_DEFAULT,
)

dawn_builder(
    name = 'Dawn Win10 x64 DEPS Release (NVIDIA)',
    cores = 2,
    os = os.LINUX_DEFAULT,
)


def fuzz_builder(*, name, **kwargs):
  return builder(
      name = name,
      mastername = 'chromium.fuzz',
      notifies = ['chromesec-lkgr-failures'],
      **kwargs
  )

fuzz_builder(
    name = 'ASAN Debug',
    goma_backend = goma.backend.RBE_PROD,
)

fuzz_builder(
    name = 'ASan Debug (32-bit x86 with V8-ARM)',
    goma_backend = goma.backend.RBE_PROD,
)

fuzz_builder(
    name = 'ASAN Release',
    goma_backend = goma.backend.RBE_PROD,
)

fuzz_builder(
    name = 'ASan Release (32-bit x86 with V8-ARM)',
    goma_backend = goma.backend.RBE_PROD,
)

fuzz_builder(
    name = 'ASAN Release Media',
    goma_backend = goma.backend.RBE_PROD,
)

fuzz_builder(
    name = 'Afl Upload Linux ASan',
    executable = luci.recipe(name = 'chromium_afl'),
    goma_backend = goma.backend.RBE_PROD,
)

fuzz_builder(
    name = 'ASan Release Media (32-bit x86 with V8-ARM)',
    goma_backend = goma.backend.RBE_PROD,
)

fuzz_builder(
    name = 'ChromiumOS ASAN Release',
)

fuzz_builder(
    name = 'MSAN Release (chained origins)',
    goma_backend = goma.backend.RBE_PROD,
)

fuzz_builder(
    name = 'MSAN Release (no origins)',
    goma_backend = goma.backend.RBE_PROD,
)

fuzz_builder(
    name = 'Mac ASAN Release',
    builderless = False,
    cores = 4,
    goma_backend = goma.backend.RBE_PROD,
    os = os.MAC_DEFAULT,
)

fuzz_builder(
    name = 'Mac ASAN Release Media',
    builderless = False,
    cores = 4,
    goma_backend = goma.backend.RBE_PROD,
    os = os.MAC_DEFAULT,
)

fuzz_builder(
    name = 'TSAN Debug',
    goma_backend = goma.backend.RBE_PROD,
)

fuzz_builder(
    name = 'TSAN Release',
    goma_backend = goma.backend.RBE_PROD,
)

fuzz_builder(
    name = 'UBSan Release',
    goma_backend = goma.backend.RBE_PROD,
)

fuzz_builder(
    name = 'UBSan vptr Release',
    goma_backend = goma.backend.RBE_PROD,
)

fuzz_builder(
    name = 'Win ASan Release',
    builderless = False,
    goma_enable_ats = True,
    goma_backend = goma.backend.RBE_PROD,
    os = os.WINDOWS_DEFAULT,
)

fuzz_builder(
    name = 'Win ASan Release Media',
    builderless = False,
    goma_enable_ats = True,
    goma_backend = goma.backend.RBE_PROD,
    os = os.WINDOWS_DEFAULT
)


def fuzz_libfuzzer_builder(*, name, **kwargs):
  return fuzz_builder(
      name = name,
      executable = luci.recipe(name = 'chromium_libfuzzer'),
      **kwargs
  )

fuzz_libfuzzer_builder(
    name = 'Libfuzzer Upload Chrome OS ASan',
)

fuzz_libfuzzer_builder(
    name = 'Libfuzzer Upload Linux ASan',
    goma_backend = goma.backend.RBE_PROD,
)

fuzz_libfuzzer_builder(
    name = 'Libfuzzer Upload Linux ASan Debug',
    goma_backend = goma.backend.RBE_PROD,
)

fuzz_libfuzzer_builder(
    name = 'Libfuzzer Upload Linux MSan',
    goma_backend = goma.backend.RBE_PROD,
)

fuzz_libfuzzer_builder(
    name = 'Libfuzzer Upload Linux UBSan',
    # Do not use builderless for this (crbug.com/980080).
    builderless = False,
    goma_backend = goma.backend.RBE_PROD,
)

fuzz_libfuzzer_builder(
    name = 'Libfuzzer Upload Linux V8-ARM64 ASan',
    goma_backend = goma.backend.RBE_PROD,
)

fuzz_libfuzzer_builder(
    name = 'Libfuzzer Upload Linux V8-ARM64 ASan Debug',
    goma_backend = goma.backend.RBE_PROD,
)

fuzz_libfuzzer_builder(
    name = 'Libfuzzer Upload Linux32 ASan',
    goma_backend = goma.backend.RBE_PROD,
)

fuzz_libfuzzer_builder(
    name = 'Libfuzzer Upload Linux32 ASan Debug',
    goma_backend = goma.backend.RBE_PROD,
)

fuzz_libfuzzer_builder(
    name = 'Libfuzzer Upload Linux32 V8-ARM ASan',
    goma_backend = goma.backend.RBE_PROD,
)

fuzz_libfuzzer_builder(
    name = 'Libfuzzer Upload Linux32 V8-ARM ASan Debug',
    goma_backend = goma.backend.RBE_PROD,
)

fuzz_libfuzzer_builder(
    name = 'Libfuzzer Upload Mac ASan',
    cores = 24,
    execution_timeout = 4 * time.hour,
    goma_backend = goma.backend.RBE_PROD,
    os = os.MAC_DEFAULT,
)

fuzz_libfuzzer_builder(
    name = 'Libfuzzer Upload Windows ASan',
    os = os.WINDOWS_DEFAULT,
)


def fyi_builder(
    *,
    name,
    execution_timeout=10 * time.hour,
    **kwargs):
  return builder(
      name = name,
      execution_timeout = execution_timeout,
      mastername = 'chromium.fyi',
      **kwargs
  )

fyi_builder(
    name = 'Closure Compilation Linux',
    executable = luci.recipe(name = 'closure_compilation'),
    goma_backend = goma.backend.RBE_PROD,
)

fyi_builder(
    name = 'Linux Viz',
    goma_backend = goma.backend.RBE_PROD,
)

fyi_builder(
    name = 'Linux remote_run Builder',
    goma_backend = goma.backend.RBE_PROD,
)

fyi_builder(
    name = 'Linux remote_run Tester',
)

fyi_builder(
    name = 'Mojo Android',
    goma_backend = goma.backend.RBE_PROD,
)

fyi_builder(
    name = 'Mojo ChromiumOS',
)

fyi_builder(
    name = 'Mojo Linux',
    goma_backend = goma.backend.RBE_PROD,
)

fyi_builder(
    name = 'Site Isolation Android',
    goma_backend = goma.backend.RBE_PROD,
)

fyi_builder(
    name = 'VR Linux',
    goma_backend = goma.backend.RBE_PROD,
)

fyi_builder(
    name = 'android-mojo-webview-rel',
    goma_backend = goma.backend.RBE_PROD,
)

fyi_builder(
    name = 'chromeos-amd64-generic-rel-vm-tests',
)

fyi_builder(
    name = 'chromeos-kevin-rel-hw-tests',
)

fyi_builder(
    name = 'fuchsia-fyi-arm64-rel',
    goma_backend = goma.backend.RBE_PROD,
    notifies = ['cr-fuchsia'],
)

fyi_builder(
    name = 'fuchsia-fyi-x64-dbg',
    goma_backend = goma.backend.RBE_PROD,
    notifies = ['cr-fuchsia'],
)

fyi_builder(
    name = 'fuchsia-fyi-x64-rel',
    goma_backend = goma.backend.RBE_PROD,
    notifies = ['cr-fuchsia'],
)

fyi_builder(
    name = 'linux-annotator-rel',
    goma_backend = goma.backend.RBE_PROD,
)

fyi_builder(
    name = 'linux-bfcache-debug',
    goma_backend = goma.backend.RBE_PROD,
)

fyi_builder(
    name = 'linux-blink-animation-use-time-delta',
    goma_backend = goma.backend.RBE_PROD,
)

fyi_builder(
    name = 'linux-blink-heap-concurrent-marking-tsan-rel',
    goma_backend = goma.backend.RBE_PROD,
)

fyi_builder(
    name = 'linux-blink-heap-verification',
    goma_backend = goma.backend.RBE_PROD,
)

fyi_builder(
    name = 'linux-chromium-tests-staging-builder',
    goma_backend = goma.backend.RBE_PROD,
)

fyi_builder(
    name = 'linux-chromium-tests-staging-tests',
)

fyi_builder(
    name = 'linux-fieldtrial-rel',
    goma_backend = goma.backend.RBE_PROD,
)

fyi_builder(
    name = 'linux-oor-cors-rel',
    goma_backend = goma.backend.RBE_PROD,
)

fyi_builder(
    name = 'linux-wpt-fyi-rel',
    experimental = True,
    goma_backend = None
)

# This is launching & collecting entirely isolated tests.
# OS shouldn't matter.
fyi_builder(
    name = 'mac-osxbeta-rel',
    goma_backend = goma.backend.RBE_PROD,
)

fyi_builder(
    name = 'win-pixel-builder-rel',
    os = None,
)

fyi_builder(
    name = 'win-pixel-tester-rel',
    os = None,
)


def fyi_celab_builder(*, name, **kwargs):
  return builder(
      name = name,
      mastername = 'chromium.fyi',
      os = os.WINDOWS_ANY,
      executable = luci.recipe(name = 'celab'),
      properties = {
          'exclude': 'chrome_only',
          'pool_name': 'celab-chromium-ci',
          'pool_size': 20,
          'tests': '*',
      },
  )

fyi_celab_builder(
    name = 'win-celab-builder-rel',
)

fyi_celab_builder(
    name = 'win-celab-tester-rel',
)


def fyi_coverage_builder(
    *,
    name,
    cores=32,
    execution_timeout=20 * time.hour,
    **kwargs):
  return fyi_builder(
      name = name,
      cores = cores,
      execution_timeout = execution_timeout,
      **kwargs
  )

fyi_coverage_builder(
    name = 'android-code-coverage',
    goma_backend = goma.backend.RBE_PROD,
    use_java_coverage = True,
    ssd = True,
)

fyi_coverage_builder(
    name = 'android-code-coverage-native',
    use_clang_coverage = True,
    ssd = True,
)

fyi_coverage_builder(
    name = 'chromeos-vm-code-coverage',
    ssd = True,
    use_clang_coverage = True,
)

fyi_coverage_builder(
    name = 'ios-simulator-code-coverage',
    caches = [XCODE_IOS_11_CACHE],
    cores = None,
    os = os.MAC_ANY,
    use_clang_coverage = True,
    properties = {
        'xcode_build_version': '11m382q',
    },
)

fyi_coverage_builder(
    name = 'linux-chromeos-code-coverage',
    ssd = True,
    use_clang_coverage = True,
)

fyi_coverage_builder(
    name = 'linux-code-coverage',
    goma_backend = goma.backend.RBE_PROD,
    os = None,
    use_clang_coverage = True,
)

fyi_coverage_builder(
    name = 'mac-code-coverage',
    builderless = True,
    cores = 24,
    goma_backend = goma.backend.RBE_PROD,
    os = os.MAC_ANY,
    ssd = True,
    use_clang_coverage = True,
)

fyi_coverage_builder(
    name = 'win10-code-coverage',
    builderless = True,
    goma_backend = goma.backend.RBE_PROD,
    goma_enable_ats = True,
    os = os.WINDOWS_DEFAULT,
    ssd = True,
    use_clang_coverage = True,
)


def fyi_ios_builder(
    *,
    name,
    executable=luci.recipe(name = 'ios/unified_builder_tester'),
    **kwargs):
  return fyi_builder(
      name = name,
      caches = [XCODE_IOS_11_CACHE],
      cores = None,
      executable = executable,
      os = os.MAC_ANY,
      **kwargs
  )

fyi_ios_builder(
    name = 'ios-simulator-cr-recipe',
    executable = luci.recipe(name = 'chromium'),
    properties = {
        'xcode_build_version': '11a1027',
    },
)

fyi_ios_builder(
    name = 'ios-simulator-cronet',
    notifies = ['cronet'],
)

fyi_ios_builder(
    name = 'ios-webkit-tot',
)

fyi_ios_builder(
    name = 'ios13-beta-simulator',
)

fyi_ios_builder(
    name = 'ios13-sdk-device',
)

fyi_ios_builder(
    name = 'ios13-sdk-simulator',
)


def fyi_mac_builder(
    *,
    name,
    cores=4,
    os=os.MAC_DEFAULT,
    **kwargs):
  return fyi_builder(
      name = name,
      cores = cores,
      os = os,
      **kwargs
  )

fyi_mac_builder(
    name = 'Mac Builder Next',
    cores = None,
    os = os.MAC_10_14,
    goma_backend = goma.backend.RBE_PROD,
)

fyi_mac_builder(
    name = 'Mac10.14 Tests',
    cores = None,
    os = os.MAC_10_14,
)

fyi_mac_builder(
    name = 'Mac deterministic',
    cores = None,
    executable = luci.recipe(name = 'swarming/deterministic_build'),
    execution_timeout = 6 * time.hour,
    goma_backend = goma.backend.RBE_PROD,
)

fyi_mac_builder(
    name = 'Mac deterministic (dbg)',
    cores = None,
    executable = luci.recipe(name = 'swarming/deterministic_build'),
    execution_timeout = 6 * time.hour,
    goma_backend = goma.backend.RBE_PROD,
)

fyi_mac_builder(
    name = 'mac-hermetic-upgrade-rel',
    cores = 8,
    goma_backend = goma.backend.RBE_PROD,
)

fyi_mac_builder(
    name = 'mac-mojo-rel',
    goma_backend = goma.backend.RBE_PROD,
    os = os.MAC_ANY,
)


def fyi_windows_builder(*, name, os=os.WINDOWS_DEFAULT, **kwargs):
  return fyi_builder(
      name = name,
      os = os,
      **kwargs
  )

fyi_windows_builder(
    name = 'Win 10 Fast Ring',
    os = os.WINDOWS_10,
)

fyi_windows_builder(
    name = 'win32-arm64-rel',
    cpu = cpu.X86,
    goma_jobs = goma.jobs.J150,
)

fyi_windows_builder(
    name = 'Win10 Tests x64 1803',
    os = os.WINDOWS_10,
)

fyi_windows_builder(
    name = 'win-annotator-rel',
    execution_timeout = 16 * time.hour,
)

fyi_windows_builder(
    name = 'Mojo Windows',
)


def gpu_fyi_builder(*, name, **kwargs):
  return builder(
      name = name,
      mastername = 'chromium.gpu.fyi',
      service_account = 'chromium-ci-gpu-builder@chops-service-accounts.iam.gserviceaccount.com',
      **kwargs
  )


def gpu_fyi_linux_builder(
    *,
    name,
    execution_timeout=6 * time.hour,
    **kwargs):
  return gpu_fyi_builder(
      name = name,
      execution_timeout = execution_timeout,
      **kwargs
  )

gpu_fyi_linux_builder(
    name = 'Android FYI 32 Vk Release (Pixel 2)',
    goma_backend = goma.backend.RBE_PROD,
)

gpu_fyi_linux_builder(
    name = 'Android FYI 32 dEQP Vk Release (Pixel 2)',
    goma_backend = goma.backend.RBE_PROD,
)

gpu_fyi_linux_builder(
    name = 'Android FYI 64 Perf (Pixel 2)',
    cores = 2,
)

gpu_fyi_linux_builder(
    name = 'Android FYI 64 Vk Release (Pixel 2)',
    goma_backend = goma.backend.RBE_PROD,
)

gpu_fyi_linux_builder(
    name = 'Android FYI 64 dEQP Vk Release (Pixel 2)',
    goma_backend = goma.backend.RBE_PROD,
)

gpu_fyi_linux_builder(
    name = 'Android FYI Release (NVIDIA Shield TV)',
    goma_backend = goma.backend.RBE_PROD,
)

gpu_fyi_linux_builder(
    name = 'Android FYI Release (Nexus 5)',
    goma_backend = goma.backend.RBE_PROD,
)

gpu_fyi_linux_builder(
    name = 'Android FYI Release (Nexus 5X)',
    goma_backend = goma.backend.RBE_PROD,
)

gpu_fyi_linux_builder(
    name = 'Android FYI Release (Nexus 6)',
    goma_backend = goma.backend.RBE_PROD,
)

gpu_fyi_linux_builder(
    name = 'Android FYI Release (Nexus 6P)',
    goma_backend = goma.backend.RBE_PROD,
)

gpu_fyi_linux_builder(
    name = 'Android FYI Release (Nexus 9)',
    goma_backend = goma.backend.RBE_PROD,
)

gpu_fyi_linux_builder(
    name = 'Android FYI Release (Pixel 2)',
    goma_backend = goma.backend.RBE_PROD,
)

gpu_fyi_linux_builder(
    name = 'Android FYI SkiaRenderer GL (Nexus 5X)',
    goma_backend = goma.backend.RBE_PROD,
)

gpu_fyi_linux_builder(
    name = 'Android FYI SkiaRenderer Vulkan (Pixel 2)',
    goma_backend = goma.backend.RBE_PROD,
)

gpu_fyi_linux_builder(
    name = 'Android FYI dEQP Release (Nexus 5X)',
    goma_backend = goma.backend.RBE_PROD,
)

gpu_fyi_linux_builder(
    name = 'GPU FYI Linux Builder',
    goma_backend = goma.backend.RBE_PROD,
)

gpu_fyi_linux_builder(
    name = 'GPU FYI Linux Builder (dbg)',
    goma_backend = goma.backend.RBE_PROD,
)

gpu_fyi_linux_builder(
    name = 'GPU FYI Linux Ozone Builder',
    goma_backend = goma.backend.RBE_PROD,
)

gpu_fyi_linux_builder(
    name = 'GPU FYI Linux dEQP Builder',
    goma_backend = goma.backend.RBE_PROD,
)

gpu_fyi_linux_builder(
    name = 'GPU FYI Perf Android 64 Builder',
    goma_backend = goma.backend.RBE_PROD,
)

gpu_fyi_linux_builder(
    name = 'Linux FYI GPU TSAN Release',
    goma_backend = goma.backend.RBE_PROD,
)


# Many of the GPU testers are thin testers, they use linux VMS regardless of the
# actual OS that the tests are built for
def gpu_fyi_linux_ci_tester(*, name, execution_timeout=6 * time.hour, **kwargs):
  return gpu_fyi_linux_builder(
      name = name,
      cores = 2,
      execution_timeout = execution_timeout,
      **kwargs
  )

gpu_fyi_linux_ci_tester(
    name = 'Linux FYI Debug (NVIDIA)',
)

gpu_fyi_linux_ci_tester(
    name = 'Linux FYI Experimental Release (Intel HD 630)',
)

gpu_fyi_linux_ci_tester(
    name = 'Linux FYI Experimental Release (NVIDIA)',
)

gpu_fyi_linux_ci_tester(
    name = 'Linux FYI Ozone (Intel)',
)

gpu_fyi_linux_ci_tester(
    name = 'Linux FYI Release (NVIDIA)',
)

gpu_fyi_linux_ci_tester(
    name = 'Linux FYI Release (AMD R7 240)',
)

gpu_fyi_linux_ci_tester(
    name = 'Linux FYI Release (Intel HD 630)',
)

gpu_fyi_linux_ci_tester(
    name = 'Linux FYI Release (Intel UHD 630)',
    # TODO(https://crbug.com/986939): Remove this increased timeout once more
    # devices are added.
    execution_timeout = 18 * time.hour,
)

gpu_fyi_linux_ci_tester(
    name = 'Linux FYI SkiaRenderer Vulkan (Intel HD 630)',
)

gpu_fyi_linux_ci_tester(
    name = 'Linux FYI SkiaRenderer Vulkan (NVIDIA)',
)

gpu_fyi_linux_ci_tester(
    name = 'Linux FYI dEQP Release (Intel HD 630)',
)

gpu_fyi_linux_ci_tester(
    name = 'Linux FYI dEQP Release (NVIDIA)',
)

gpu_fyi_linux_ci_tester(
    name = 'Mac FYI Debug (Intel)',
)

gpu_fyi_linux_ci_tester(
    name = 'Mac FYI Experimental Release (Intel)',
)

gpu_fyi_linux_ci_tester(
    name = 'Mac FYI Experimental Retina Release (AMD)',
)

gpu_fyi_linux_ci_tester(
    name = 'Mac FYI Experimental Retina Release (NVIDIA)',
    # This bot has one machine backing its tests at the moment.
    # If it gets more, this can be removed.
    # See crbug.com/853307 for more context.
    execution_timeout = 12 * time.hour,
)

gpu_fyi_linux_ci_tester(
    name = 'Mac FYI Release (Intel)',
)

gpu_fyi_linux_ci_tester(
    name = 'Mac FYI Retina Debug (AMD)',
)

gpu_fyi_linux_ci_tester(
    name = 'Mac FYI Retina Debug (NVIDIA)',
)

gpu_fyi_linux_ci_tester(
    name = 'Mac FYI Retina Release (AMD)',
)

gpu_fyi_linux_ci_tester(
    name = 'Mac FYI Retina Release (NVIDIA)',
)

gpu_fyi_linux_ci_tester(
    name = 'Mac FYI dEQP Release AMD',
)

gpu_fyi_linux_ci_tester(
    name = 'Mac FYI dEQP Release Intel',
)

gpu_fyi_linux_ci_tester(
    name = 'Mac Pro FYI Release (AMD)',
)

gpu_fyi_linux_ci_tester(
    name = 'Win10 FYI x64 Debug (NVIDIA)',
)

gpu_fyi_linux_ci_tester(
    name = 'Win10 FYI x64 DX12 Vulkan Debug (NVIDIA)',
)

gpu_fyi_linux_ci_tester(
    name = 'Win10 FYI x64 DX12 Vulkan Release (NVIDIA)',
)

gpu_fyi_linux_ci_tester(
    name = 'Win10 FYI x64 Exp Release (Intel HD 630)',
)

gpu_fyi_linux_ci_tester(
    name = 'Win10 FYI x64 Exp Release (NVIDIA)',
)

gpu_fyi_linux_ci_tester(
    name = 'Win10 FYI x64 Release (AMD RX 550)',
)

gpu_fyi_linux_ci_tester(
    name = 'Win10 FYI x64 Release (Intel HD 630)',
)

gpu_fyi_linux_ci_tester(
    name = 'Win10 FYI x64 Release (Intel UHD 630)',
    # TODO(https://crbug.com/986939): Remove this increased timeout once
    # more devices are added.
    execution_timeout = 18 * time.hour,
)

gpu_fyi_linux_ci_tester(
    name = 'Win10 FYI x64 Release (NVIDIA GeForce GTX 1660)',
    execution_timeout = 18 * time.hour,
)

gpu_fyi_linux_ci_tester(
    name = 'Win10 FYI x64 Release (NVIDIA)',
)

gpu_fyi_linux_ci_tester(
    name = 'Win10 FYI x64 Release XR Perf (NVIDIA)',
)

gpu_fyi_linux_ci_tester(
    name = 'Win10 FYI x64 SkiaRenderer GL (NVIDIA)',
)

gpu_fyi_linux_ci_tester(
    name = 'Win10 FYI x64 dEQP Release (Intel HD 630)',
)

gpu_fyi_linux_ci_tester(
    name = 'Win10 FYI x64 dEQP Release (NVIDIA)',
)

gpu_fyi_linux_ci_tester(
    name = 'Win10 FYI x86 Release (NVIDIA)',
)

gpu_fyi_linux_ci_tester(
    name = 'Win7 FYI Debug (AMD)',
)

gpu_fyi_linux_ci_tester(
    name = 'Win7 FYI Release (AMD)',
)

gpu_fyi_linux_ci_tester(
    name = 'Win7 FYI Release (NVIDIA)',
)

gpu_fyi_linux_ci_tester(
    name = 'Win7 FYI dEQP Release (AMD)',
)

gpu_fyi_linux_ci_tester(
    name = 'Win7 FYI x64 Release (NVIDIA)',
)

gpu_fyi_linux_ci_tester(
    name = 'Win7 FYI x64 dEQP Release (NVIDIA)',
)


def gpu_fyi_mac_builder(*, name, **kwargs):
  return gpu_fyi_builder(
      name = name,
      cores = 4,
      execution_timeout = 6 * time.hour,
      os = os.MAC_ANY,
      **kwargs
  )

gpu_fyi_mac_builder(
    name = 'Mac FYI GPU ASAN Release',
    goma_backend = goma.backend.RBE_PROD,
)

gpu_fyi_mac_builder(
    name = 'GPU FYI Mac Builder',
    goma_backend = goma.backend.RBE_PROD,
)

gpu_fyi_mac_builder(
    name = 'GPU FYI Mac Builder (dbg)',
    goma_backend = goma.backend.RBE_PROD,
)

gpu_fyi_mac_builder(
    name = 'GPU FYI Mac dEQP Builder',
    goma_backend = goma.backend.RBE_PROD,
)


def gpu_fyi_windows_builder(*, name, **kwargs):
  return gpu_fyi_builder(
      name = name,
      builderless = True,
      os = os.WINDOWS_ANY,
      **kwargs
  )

gpu_fyi_windows_builder(
    name = 'GPU FYI Win Builder',
)

gpu_fyi_windows_builder(
    name = 'GPU FYI Win Builder (dbg)',
)

gpu_fyi_windows_builder(
    name = 'GPU FYI Win dEQP Builder',
)

gpu_fyi_windows_builder(
    name = 'GPU FYI Win x64 Builder',
)

gpu_fyi_windows_builder(
    name = 'GPU FYI Win x64 Builder (dbg)',
)

gpu_fyi_windows_builder(
    name = 'GPU FYI Win x64 dEQP Builder',
)

gpu_fyi_windows_builder(
    name = 'GPU FYI Win x64 DX12 Vulkan Builder',
)

gpu_fyi_windows_builder(
    name = 'GPU FYI Win x64 DX12 Vulkan Builder (dbg)',
)

gpu_fyi_windows_builder(
    name = 'GPU FYI XR Win x64 Builder',
)


def gpu_builder(*, name, **kwargs):
  return builder(
      name = name,
      mastername = 'chromium.gpu',
      **kwargs
  )

gpu_builder(
    name = 'Android Release (Nexus 5X)',
    goma_backend = goma.backend.RBE_PROD,
)

gpu_builder(
    name = 'GPU Linux Builder (dbg)',
    goma_backend = goma.backend.RBE_PROD,
)

gpu_builder(
    name = 'GPU Mac Builder',
    cores = None,
    goma_backend = goma.backend.RBE_PROD,
    os = os.MAC_ANY,
)

gpu_builder(
    name = 'GPU Mac Builder (dbg)',
    cores = None,
    goma_backend = goma.backend.RBE_PROD,
    os = os.MAC_ANY,
)

gpu_builder(
    name = 'GPU Win x64 Builder',
    builderless = True,
    os = os.WINDOWS_ANY,
)

gpu_builder(
    name = 'GPU Win x64 Builder (dbg)',
    builderless = True,
    os = os.WINDOWS_ANY,
)


# Many of the GPU testers are thin testers, they use linux VMS regardless of the
# actual OS that the tests are built for
def gpu_linux_ci_tester(*, name, **kwargs):
  return gpu_builder(
      name = name,
      cores = 2,
      os = os.LINUX_DEFAULT,
      **kwargs
  )

gpu_linux_ci_tester(
    name = 'Linux Debug (NVIDIA)',
)

gpu_linux_ci_tester(
    name = 'Mac Debug (Intel)',
)

gpu_linux_ci_tester(
    name = 'Mac Release (Intel)',
)

gpu_linux_ci_tester(
    name = 'Mac Retina Debug (AMD)',
)

gpu_linux_ci_tester(
    name = 'Mac Retina Release (AMD)',
)

gpu_linux_ci_tester(
    name = 'Win10 x64 Debug (NVIDIA)',
)

gpu_linux_ci_tester(
    name = 'Win10 x64 Release (NVIDIA)',
)


def linux_builder(*, name, goma_jobs=goma.jobs.MANY_JOBS_FOR_CI, **kwargs):
  return builder(
      name = name,
      goma_jobs = goma_jobs,
      mastername = 'chromium.linux',
      **kwargs
  )

linux_builder(
    name = 'Fuchsia x64',
    goma_backend = goma.backend.RBE_PROD,
    notifies = ['cr-fuchsia'],
)

linux_builder(
    name = 'Cast Audio Linux',
    goma_backend = goma.backend.RBE_PROD,
    ssd = True,
)

linux_builder(
    name = 'Cast Linux',
    goma_backend = goma.backend.RBE_PROD,
    goma_jobs = goma.jobs.J50,
)

linux_builder(
    name = 'Deterministic Fuchsia (dbg)',
    executable = luci.recipe(name = 'swarming/deterministic_build'),
    execution_timeout = 6 * time.hour,
    goma_backend = goma.backend.RBE_PROD,
    goma_jobs = None,
)

linux_builder(
    name = 'Deterministic Linux',
    executable = luci.recipe(name = 'swarming/deterministic_build'),
    execution_timeout = 6 * time.hour,
    goma_backend = goma.backend.RBE_PROD,
)

linux_builder(
    name = 'Deterministic Linux (dbg)',
    cores = 32,
    executable = luci.recipe(name = 'swarming/deterministic_build'),
    execution_timeout = 6 * time.hour,
    goma_backend = goma.backend.RBE_PROD,
)

linux_builder(
    name = 'Fuchsia ARM64',
    goma_backend = goma.backend.RBE_PROD,
    notifies = ['cr-fuchsia'],
)

linux_builder(
    name = 'Leak Detection Linux',
    goma_backend = goma.backend.RBE_PROD,
)

linux_builder(
    name = 'Linux Builder (dbg)',
    goma_backend = goma.backend.RBE_PROD,
)

linux_builder(
    name = 'Linux Builder (dbg)(32)',
    goma_backend = goma.backend.RBE_PROD,
)

linux_builder(
    name = 'Linux Tests (dbg)(1)',
)

linux_builder(
    name = 'fuchsia-arm64-cast',
    goma_backend = goma.backend.RBE_PROD,
    notifies = ['cr-fuchsia'],
)

linux_builder(
    name = 'fuchsia-x64-cast',
    goma_backend = goma.backend.RBE_PROD,
    notifies = ['cr-fuchsia'],
)

linux_builder(
    name = 'fuchsia-x64-dbg',
    goma_backend = goma.backend.RBE_PROD,
    notifies = ['cr-fuchsia'],
)

linux_builder(
    name = 'linux-gcc-rel',
)

linux_builder(
    name = 'linux-ozone-rel',
    goma_backend = goma.backend.RBE_PROD,
)

linux_builder(
    name = 'linux-trusty-rel',
    goma_backend = goma.backend.RBE_PROD,
    os = os.LINUX_TRUSTY,
)

linux_builder(
    name = 'linux_chromium_component_updater',
    executable = luci.recipe(name = 'findit/chromium/update_components'),
    goma_backend = goma.backend.RBE_PROD,
    service_account = 'component-mapping-updater@chops-service-accounts.iam.gserviceaccount.com'
)


def mac_builder(*, name, cores=None, os=os.MAC_DEFAULT, **kwargs):
  return builder(
      name = name,
      cores = cores,
      mastername = 'chromium.mac',
      os = os,
      **kwargs
  )

mac_builder(
    name = 'Mac Builder',
    goma_backend = goma.backend.RBE_PROD,
)

mac_builder(
    name = 'Mac Builder (dbg)',
    goma_backend = goma.backend.RBE_PROD,
    os = os.MAC_ANY,
)

# The build runs on 10.13, but triggers tests on 10.10 bots.
mac_builder(
    name = 'Mac10.10 Tests',
)

# The build runs on 10.13, but triggers tests on 10.11 bots.
mac_builder(
    name = 'Mac10.11 Tests',
)

mac_builder(
    name = 'Mac10.12 Tests',
    os = os.MAC_10_12,
)

mac_builder(
    name = 'Mac10.13 Tests',
    os = os.MAC_10_13,
)

mac_builder(
    name = 'Mac10.13 Tests (dbg)',
    os = os.MAC_ANY,
)

mac_builder(
    name = 'WebKit Mac10.13 (retina)',
    goma_backend = goma.backend.RBE_PROD,
    os = os.MAC_10_13,
)

def mac_ios_builder(*, name, **kwargs):
  return mac_builder(
      name = name,
      caches = [XCODE_IOS_11_CACHE],
      executable = luci.recipe(name = 'ios/unified_builder_tester'),
      os = os.MAC_ANY,
      **kwargs
  )

mac_ios_builder(
    name = 'ios-device',
)

mac_ios_builder(
    name = 'ios-device-xcode-clang',
)

mac_ios_builder(
    name = 'ios-simulator',
)

mac_ios_builder(
    name = 'ios-simulator-full-configs',
)

mac_ios_builder(
    name = 'ios-simulator-noncq',
)

mac_ios_builder(
    name = 'ios-simulator-xcode-clang',
)

mac_ios_builder(
    name = 'ios-slimnav',
)


def memory_builder(
    *,
    name,
    goma_jobs=goma.jobs.MANY_JOBS_FOR_CI,
    **kwargs):
  return builder(
      name = name,
      goma_jobs = goma_jobs,
      mastername = 'chromium.memory',
      **kwargs
  )

memory_builder(
    name = 'Android CFI',
    cores = 32,
    # TODO(https://crbug.com/919430) Remove the larger timeout once compile
    # times have been brought down to reasonable level
    execution_timeout = time.hour * 9 / 2,  # 4.5 (can't multiply float * duration)
    goma_backend = goma.backend.RBE_PROD,
)

memory_builder(
    name = 'Linux ASan LSan Builder',
    goma_backend = goma.backend.RBE_PROD,
    ssd = True,
)

memory_builder(
    name = 'Linux ASan LSan Tests (1)',
)

memory_builder(
    name = 'Linux ASan Tests (sandboxed)',
)

memory_builder(
    name = 'Linux CFI',
    cores = 32,
    # TODO(thakis): Remove once https://crbug.com/927738 is resolved.
    execution_timeout = 4 * time.hour,
    goma_backend = goma.backend.RBE_PROD,
    goma_jobs = goma.jobs.MANY_JOBS_FOR_CI,
)

memory_builder(
    name = 'Linux Chromium OS ASan LSan Builder',
)

memory_builder(
    name = 'Linux Chromium OS ASan LSan Tests (1)',
)

memory_builder(
    name = 'Linux ChromiumOS MSan Builder',
)

memory_builder(
    name = 'Linux ChromiumOS MSan Tests',
)

memory_builder(
    name = 'Linux MSan Builder',
    goma_backend = goma.backend.RBE_PROD,
    goma_jobs = goma.jobs.MANY_JOBS_FOR_CI,
)

memory_builder(
    name = 'Linux MSan Tests',
)

memory_builder(
    name = 'Linux TSan Builder',
    goma_backend = goma.backend.RBE_PROD,
)

memory_builder(
    name = 'Linux TSan Tests',
)

memory_builder(
    name = 'Mac ASan 64 Builder',
    builderless = False,
    goma_backend = goma.backend.RBE_PROD,
    goma_debug = True,  # TODO(hinoka): Remove this after debugging.
    goma_jobs = None,
    cores = None,  # Swapping between 8 and 24
    os = os.MAC_DEFAULT,
)

memory_builder(
    name = 'Mac ASan 64 Tests (1)',
    builderless = False,
    os = os.MAC_DEFAULT,
)

memory_builder(
    name = 'WebKit Linux ASAN',
    goma_backend = goma.backend.RBE_PROD,
)

memory_builder(
    name = 'WebKit Linux Leak',
    goma_backend = goma.backend.RBE_PROD,
)

memory_builder(
    name = 'WebKit Linux MSAN',
    goma_backend = goma.backend.RBE_PROD,
)

memory_builder(
    name = 'android-asan',
    goma_backend = goma.backend.RBE_PROD,
)

memory_builder(
    name = 'win-asan',
    cores = 32,
    os = os.WINDOWS_DEFAULT,
)


def swangle_builder(*, name, **kwargs):
  return builder(
      name = name,
      builderless = True,
      mastername = 'chromium.swangle',
      service_account = 'chromium-ci-gpu-builder@chops-service-accounts.iam.gserviceaccount.com',
      **kwargs
  )


def swangle_linux_builder(
    *,
    name,
    **kwargs):
  return swangle_builder(
      name = name,
      os = os.LINUX_DEFAULT,
      **kwargs
  )

swangle_linux_builder(
    name = 'linux-swangle-tot-angle-x64'
)

swangle_linux_builder(
    name = 'linux-swangle-tot-angle-x86'
)

swangle_linux_builder(
    name = 'linux-swangle-tot-swiftshader-x64'
)

swangle_linux_builder(
    name = 'linux-swangle-tot-swiftshader-x86'
)

swangle_linux_builder(
    name = 'linux-swangle-x64'
)

swangle_linux_builder(
    name = 'linux-swangle-x86'
)


def swangle_windows_builder(*, name, **kwargs):
  return swangle_builder(
      name = name,
      os = os.WINDOWS_DEFAULT,
      **kwargs
  )

swangle_windows_builder(
    name = 'win-swangle-tot-angle-x64'
)

swangle_windows_builder(
    name = 'win-swangle-tot-angle-x86'
)

swangle_windows_builder(
    name = 'win-swangle-tot-swiftshader-x64'
)

swangle_windows_builder(
    name = 'win-swangle-tot-swiftshader-x86'
)

swangle_windows_builder(
    name = 'win-swangle-x64'
)

swangle_windows_builder(
    name = 'win-swangle-x86'
)


def win_builder(*, name, os=os.WINDOWS_DEFAULT, **kwargs):
  return builder(
      name = name,
      mastername = 'chromium.win',
      os = os,
      **kwargs
  )

win_builder(
    name = 'WebKit Win10',
)

win_builder(
    name = 'Win 7 Tests x64 (1)',
    os = os.WINDOWS_7,
)

win_builder(
    name = 'Win Builder',
    cores = 32,
    os = os.WINDOWS_ANY,
)

win_builder(
    name = 'Win Builder (dbg)',
    cores = 32,
    os = os.WINDOWS_ANY,
)

win_builder(
    name = 'Win x64 Builder',
    cores = 32,
    os = os.WINDOWS_ANY,
)

win_builder(
    name = 'Win x64 Builder (dbg)',
    cores = 32,
    os = os.WINDOWS_ANY,
)

win_builder(
    name = 'Win10 Tests x64',
)

win_builder(
    name = 'Win10 Tests x64 (dbg)',
)

win_builder(
    name = 'Win7 (32) Tests',
    os = os.WINDOWS_7,
)

win_builder(
    name = 'Win7 Tests (1)',
    os = os.WINDOWS_7,
)

win_builder(
    name = 'Win7 Tests (dbg)(1)',
    os = os.WINDOWS_7,
)

win_builder(
    name = 'Windows deterministic',
    executable = luci.recipe(name = 'swarming/deterministic_build'),
    execution_timeout = 6 * time.hour,
)
