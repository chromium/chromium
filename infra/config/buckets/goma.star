load('//lib/builders.star', 'builder', 'cpu', 'defaults', 'goma', 'os')

luci.bucket(
    name = 'goma',
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

defaults.bucket.set('goma')
defaults.build_numbers.set(True)
defaults.configure_kitchen.set(True)
defaults.cores.set(8)
defaults.cpu.set(cpu.X86_64)
defaults.executable.set(luci.recipe(name = 'chromium'))
defaults.execution_timeout.set(3 * time.hour)
defaults.os.set(os.LINUX_DEFAULT)
defaults.pool.set('luci.chromium.ci')
defaults.service_account.set(
        'chromium-ci-builder@chops-service-accounts.iam.gserviceaccount.com')
defaults.swarming_tags.set(['vpython:native-python-wrapper'])


# Builders appear after the function used to define them, with all builders
# defined using the same function ordered lexicographically by name
# Builder functions are defined in lexicographic order by name ignoring the
# '_builder' suffix

# Builder functions are defined for each master that goma builders appear on,
# with additional functions for specializing on OS or goma grouping (canary,
# latest client, etc.): XXX_YYY_builder where XXX is the part after the last dot
# in the mastername and YYY is the OS or goma grouping


def fyi_goma_canary_builder(*, name, **kwargs):
  return builder(
      name = name,
      mastername = 'chromium.fyi',
      execution_timeout = 10 * time.hour,
      **kwargs
  )

fyi_goma_canary_builder(
    name = 'Linux Builder Goma Canary',
    # keep to use trusty for this until chrome drops support of development
    # on trusty.
    os = os.LINUX_TRUSTY,
)

fyi_goma_canary_builder(
    name = 'Mac Builder (dbg) Goma Canary',
    cores = 4,
    os = os.MAC_DEFAULT,
)

fyi_goma_canary_builder(
    name = 'Mac Builder (dbg) Goma Canary (clobber)',
    cores = 4,
    os = os.MAC_DEFAULT,
)

fyi_goma_canary_builder(
    name = 'Mac Builder Goma Canary',
    cores = 4,
    os = os.MAC_DEFAULT,
)

fyi_goma_canary_builder(
    name = 'Win Builder (dbg) Goma Canary',
    os = os.WINDOWS_DEFAULT,
)

fyi_goma_canary_builder(
    name = 'Win Builder Goma Canary',
    os = os.WINDOWS_DEFAULT,
)

fyi_goma_canary_builder(
    name = 'Win cl.exe Goma Canary LocalOutputCache',
    os = os.WINDOWS_DEFAULT,
)

fyi_goma_canary_builder(
    name = 'Win7 Builder (dbg) Goma Canary',
    os = os.WINDOWS_7,
)

fyi_goma_canary_builder(
    name = 'Win7 Builder Goma Canary',
    os = os.WINDOWS_7,
)

fyi_goma_canary_builder(
    name = 'WinMSVC64 Goma Canary',
    os = os.WINDOWS_DEFAULT,
)

fyi_goma_canary_builder(
    name = 'android-archive-dbg-goma-canary',
)

fyi_goma_canary_builder(
    name = 'chromeos-amd64-generic-rel-goma-canary',
)

fyi_goma_canary_builder(
    name = 'ios-device-goma-canary-clobber',
    caches = [
        swarming.cache(
            name = 'xcode_ios_11a1027',
            path = 'xcode_ios_11a1027.app',
        ),
    ],
    cores = None,
    executable = luci.recipe(name = 'ios/unified_builder_tester'),
    os = os.MAC_ANY,
)

fyi_goma_canary_builder(
    name = 'linux-archive-rel-goma-canary',
)

fyi_goma_canary_builder(
    name = 'linux-archive-rel-goma-canary-localoutputcache',
)

fyi_goma_canary_builder(
    name = 'mac-archive-rel-goma-canary',
    cores = 4,
    os = os.MAC_DEFAULT,
)

fyi_goma_canary_builder(
    name = 'mac-archive-rel-goma-canary-localoutputcache',
    cores = 4,
    os = os.MAC_DEFAULT,
)

fyi_goma_canary_builder(
    name = 'win32-archive-rel-goma-canary-localoutputcache',
    os = os.WINDOWS_DEFAULT,
)


def fyi_goma_rbe_canary_builder(
    *,
    name,
    goma_backend=goma.backend.RBE_PROD,
    os=os.LINUX_DEFAULT,
    **kwargs):
  return builder(
      name = name,
      execution_timeout = 10 * time.hour,
      goma_backend = goma_backend,
      mastername = 'chromium.fyi',
      os = os,
      **kwargs
  )

fyi_goma_rbe_canary_builder(
    name = 'Linux Builder Goma RBE Canary',
)

fyi_goma_rbe_canary_builder(
    name = 'Mac Builder (dbg) Goma RBE Canary (clobber)',
    cores = 4,
    goma_backend = None,
    goma_jobs = goma.jobs.J80,
    os = os.MAC_DEFAULT,
)

fyi_goma_rbe_canary_builder(
    name = 'android-archive-dbg-goma-rbe-ats-canary',
    goma_enable_ats = True,
)

fyi_goma_rbe_canary_builder(
    name = 'android-archive-dbg-goma-rbe-canary',
)

fyi_goma_rbe_canary_builder(
    name = 'chromeos-amd64-generic-rel-goma-rbe-canary',
    goma_enable_ats = True,
)

fyi_goma_rbe_canary_builder(
    name = 'ios-device-goma-rbe-canary-clobber',
    caches = [
        swarming.cache(
            name = 'xcode_ios_11a1027',
            path = 'xcode_ios_11a1027.app',
        ),
    ],
    cores = None,
    executable = luci.recipe(name = 'ios/unified_builder_tester'),
    os = os.MAC_ANY,
)

fyi_goma_rbe_canary_builder(
    name = 'linux-archive-rel-goma-rbe-ats-canary',
    goma_enable_ats = True,
)

fyi_goma_rbe_canary_builder(
    name = 'linux-archive-rel-goma-rbe-canary',
)

fyi_goma_rbe_canary_builder(
    name = 'mac-archive-rel-goma-rbe-canary',
    cores = 4,
    goma_backend = None,
    goma_jobs = goma.jobs.J80,
    os = os.MAC_DEFAULT,
)


def fyi_goma_latest_client_builder(*, name, os=os.LINUX_DEFAULT, **kwargs):
  return builder(
      name = name,
      mastername = 'chromium.fyi',
      execution_timeout = 10 * time.hour,
      os = os,
      **kwargs
  )

fyi_goma_latest_client_builder(
    name = 'Linux Builder Goma Latest Client',
)

fyi_goma_latest_client_builder(
    name = 'Mac Builder (dbg) Goma Latest Client',
    cores = 4,
    os = os.MAC_DEFAULT,
)

fyi_goma_latest_client_builder(
    name = 'Mac Builder (dbg) Goma Latest Client (clobber)',
    cores = 4,
    os = os.MAC_DEFAULT,
)

fyi_goma_latest_client_builder(
    name = 'Mac Builder Goma Latest Client',
    cores = 4,
    os = os.MAC_DEFAULT,
)

fyi_goma_latest_client_builder(
    name = 'Win Builder (dbg) Goma Latest Client',
    os = os.WINDOWS_DEFAULT,
)

fyi_goma_latest_client_builder(
    name = 'Win Builder Goma Latest Client',
    os = os.WINDOWS_DEFAULT,
)

fyi_goma_latest_client_builder(
    name = 'Win cl.exe Goma Latest Client LocalOutputCache',
    os = os.WINDOWS_DEFAULT,
)

fyi_goma_latest_client_builder(
    name = 'Win7 Builder (dbg) Goma Latest Client',
    os = os.WINDOWS_7,
)

fyi_goma_latest_client_builder(
    name = 'Win7 Builder Goma Latest Client',
    os = os.WINDOWS_7,
)

fyi_goma_latest_client_builder(
    name = 'WinMSVC64 Goma Latest Client',
    os = os.WINDOWS_DEFAULT,
)

fyi_goma_latest_client_builder(
    name = 'android-archive-dbg-goma-latest',
)

fyi_goma_latest_client_builder(
    name = 'chromeos-amd64-generic-rel-goma-latest',
)

fyi_goma_latest_client_builder(
    name = 'ios-device-goma-latest-clobber',
    caches = [
        swarming.cache(
            name = 'xcode_ios_11a1027',
            path = 'xcode_ios_11a1027.app',
        ),
    ],
    cores = None,
    executable = luci.recipe(name = 'ios/unified_builder_tester'),
    os = os.MAC_ANY,
)

fyi_goma_latest_client_builder(
    name = 'linux-archive-rel-goma-latest',
)

fyi_goma_latest_client_builder(
    name = 'linux-archive-rel-goma-latest-localoutputcache',
)

fyi_goma_latest_client_builder(
    name = 'mac-archive-rel-goma-latest',
    cores = 4,
    os = os.MAC_DEFAULT,
)

fyi_goma_latest_client_builder(
    name = 'mac-archive-rel-goma-latest-localoutputcache',
    cores = 4,
    os = os.MAC_DEFAULT,
)

fyi_goma_latest_client_builder(
    name = 'win32-archive-rel-goma-latest-localoutputcache',
    os = os.WINDOWS_DEFAULT,
)


def fyi_goma_rbe_latest_client_builder(
    *,
    name,
    goma_backend=goma.backend.RBE_PROD,
    os=os.LINUX_DEFAULT,
    **kwargs):
  return builder(
      name = name,
      execution_timeout = 10 * time.hour,
      goma_backend = goma_backend,
      mastername = 'chromium.fyi',
      os = os,
      **kwargs
  )

fyi_goma_rbe_latest_client_builder(
    name = 'Linux Builder Goma RBE Latest Client',
)

fyi_goma_rbe_latest_client_builder(
    name = 'Mac Builder (dbg) Goma RBE Latest Client (clobber)',
    cores = 4,
    goma_backend = None,
    goma_jobs = goma.jobs.J80,
    os = os.MAC_DEFAULT,
)

fyi_goma_rbe_latest_client_builder(
    name = 'Win Builder (dbg) Goma RBE Latest Client',
    goma_backend = goma.backend.RBE_STAGING,
    goma_enable_ats = True,
    os = os.WINDOWS_DEFAULT,
)

fyi_goma_rbe_latest_client_builder(
    name = 'Win Builder Goma RBE Latest Client',
    goma_backend = goma.backend.RBE_STAGING,
    goma_enable_ats = True,
    os = os.WINDOWS_DEFAULT,
)

fyi_goma_rbe_latest_client_builder(
    name = 'android-archive-dbg-goma-rbe-ats-latest',
    goma_enable_ats = True,
)

fyi_goma_rbe_latest_client_builder(
    name = 'android-archive-dbg-goma-rbe-latest',
)

fyi_goma_rbe_latest_client_builder(
    name = 'chromeos-amd64-generic-rel-goma-rbe-latest',
    goma_enable_ats = True,
)

fyi_goma_rbe_latest_client_builder(
    name = 'ios-device-goma-rbe-latest-clobber',
    caches = [
        swarming.cache(
            name = 'xcode_ios_11a1027',
            path = 'xcode_ios_11a1027.app',
        ),
    ],
    cores = None,
    executable = luci.recipe(name = 'ios/unified_builder_tester'),
    os = os.MAC_ANY,
)

fyi_goma_rbe_latest_client_builder(
    name = 'linux-archive-rel-goma-rbe-ats-latest',
    goma_enable_ats = True,
)

fyi_goma_rbe_latest_client_builder(
    name = 'linux-archive-rel-goma-rbe-latest',
)

fyi_goma_rbe_latest_client_builder(
    name = 'mac-archive-rel-goma-rbe-latest',
    cores = 4,
    goma_backend = None,
    goma_jobs = goma.jobs.J80,
    os = os.MAC_DEFAULT,
)


def goma_builder(*, name, builderless=False, os=os.LINUX_DEFAULT, **kwargs):
  return builder(
      name = name,
      builderless = builderless,
      mastername = 'chromium.goma',
      os = os,
      **kwargs
  )

goma_builder(
    name = 'Chromium Android ARM 32-bit Goma RBE Prod',
    goma_backend = goma.backend.RBE_PROD,
)

goma_builder(
    name = 'Chromium Android ARM 32-bit Goma RBE Prod (clobber)',
    goma_backend = goma.backend.RBE_PROD,
)

goma_builder(
    name = 'Chromium Android ARM 32-bit Goma RBE Prod (dbg)',
    goma_backend = goma.backend.RBE_PROD,
)

goma_builder(
    name = 'Chromium Android ARM 32-bit Goma RBE Prod (dbg) (clobber)',
    goma_backend = goma.backend.RBE_PROD,
)

goma_builder(
    name = 'Chromium Android ARM 32-bit Goma RBE Staging',
    goma_backend = goma.backend.RBE_STAGING,
)

goma_builder(
    name = 'Chromium Android ARM 32-bit Goma RBE ToT',
    goma_backend = goma.backend.RBE_TOT,
)

goma_builder(
    name = 'Chromium Android ARM 32-bit Goma RBE ToT (ATS)',
    goma_backend = goma.backend.RBE_TOT,
    goma_enable_ats = True,
)

goma_builder(
    name = 'Chromium Linux Goma RBE Prod',
    goma_backend = goma.backend.RBE_PROD,
)

goma_builder(
    name = 'Chromium Linux Goma RBE Prod (clobber)',
    goma_backend = goma.backend.RBE_PROD,
)

goma_builder(
    name = 'Chromium Linux Goma RBE Prod (dbg)',
    goma_backend = goma.backend.RBE_PROD,
)

goma_builder(
    name = 'Chromium Linux Goma RBE Prod (dbg) (clobber)',
    goma_backend = goma.backend.RBE_PROD,
)

goma_builder(
    name = 'Chromium Linux Goma RBE Staging',
    goma_backend = goma.backend.RBE_STAGING,
)

goma_builder(
    name = 'Chromium Linux Goma RBE Staging (clobber)',
    goma_backend = goma.backend.RBE_STAGING,
)

goma_builder(
    name = 'Chromium Linux Goma RBE Staging (dbg)',
    goma_backend = goma.backend.RBE_STAGING,
)

goma_builder(
    name = 'Chromium Linux Goma RBE Staging (dbg) (clobber)',
    goma_backend = goma.backend.RBE_STAGING,
)

goma_builder(
    name = 'Chromium Linux Goma Staging',
)

goma_builder(
    name = 'Chromium Linux Goma RBE ToT',
    goma_backend = goma.backend.RBE_TOT,
)

goma_builder(
    name = 'Chromium Linux Goma RBE ToT (ATS)',
    goma_backend = goma.backend.RBE_TOT,
    goma_enable_ats = True,
)

goma_builder(
    name = 'chromeos-amd64-generic-rel (Goma RBE FYI)',
    builderless = True,
    goma_backend = goma.backend.RBE_PROD,
    goma_enable_ats = True,
)

goma_builder(
    name = 'fuchsia-fyi-arm64-rel (Goma RBE FYI)',
    builderless = True,
    goma_backend = goma.backend.RBE_PROD,
    goma_enable_ats = True,
)

goma_builder(
    name = 'fuchsia-fyi-x64-rel (Goma RBE FYI)',
    builderless = True,
    goma_backend = goma.backend.RBE_PROD,
    goma_enable_ats = True,
)


def goma_mac_builder(*, name, **kwargs):
  return goma_builder(
      name = name,
      cores = 4,
      goma_jobs = goma.jobs.J80,
      os = os.MAC_DEFAULT,
      **kwargs
  )

goma_mac_builder(
    name = 'Chromium Mac Goma RBE Prod',
    goma_backend = goma.backend.RBE_PROD,
)

goma_mac_builder(
    name = 'Chromium Mac Goma RBE Staging',
    goma_backend = goma.backend.RBE_STAGING,
)

goma_mac_builder(
    name = 'Chromium Mac Goma RBE Staging (clobber)',
    goma_backend = goma.backend.RBE_STAGING,
)

goma_mac_builder(
    name = 'Chromium Mac Goma RBE Staging (dbg)',
    goma_backend = goma.backend.RBE_STAGING,
)

goma_mac_builder(
    name = 'Chromium Mac Goma RBE ToT',
    goma_backend = goma.backend.RBE_TOT,
)

goma_mac_builder(
    name = 'Chromium Mac Goma Staging',
)


def goma_windows_builder(*, name, goma_enable_ats=True, cores=32, **kwargs):
  return goma_builder(
      name = name,
      cores = cores,
      goma_enable_ats = goma_enable_ats,
      os = os.WINDOWS_DEFAULT,
      **kwargs
  )

goma_windows_builder(
    name = 'Chromium Win Goma RBE Prod',
    goma_backend = goma.backend.RBE_PROD,
)

goma_windows_builder(
    name = 'Chromium Win Goma RBE Prod (clobber)',
    goma_backend = goma.backend.RBE_PROD,
)

goma_windows_builder(
    name = 'Chromium Win Goma RBE Prod (dbg)',
    goma_backend = goma.backend.RBE_PROD,
)

goma_windows_builder(
    name = 'Chromium Win Goma RBE Prod (dbg) (clobber)',
    goma_backend = goma.backend.RBE_PROD,
)

goma_windows_builder(
    name = 'Chromium Win Goma RBE Staging',
    goma_backend = goma.backend.RBE_STAGING,
)

goma_windows_builder(
    name = 'Chromium Win Goma RBE Staging (clobber)',
    goma_backend = goma.backend.RBE_STAGING,
)

goma_windows_builder(
    name = 'Chromium Win Goma RBE ToT',
    goma_backend = goma.backend.RBE_TOT,
)

goma_windows_builder(
    name = 'CrWinGomaStaging',
    cores = 8,
    goma_enable_ats = False,
)
