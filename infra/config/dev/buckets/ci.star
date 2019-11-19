load('//lib/builders.star', 'builder', 'cpu', 'defaults', 'os')

luci.bucket(
    name = 'ci',
    acls = [
        acl.entry(
            roles = acl.BUILDBUCKET_READER,
            groups = 'all',
        ),
        acl.entry(
            roles = acl.BUILDBUCKET_TRIGGERER,
            users = [
                'luci-scheduler-dev@appspot.gserviceaccount.com',
                'chromium-ci-builder-dev@chops-service-accounts.iam.gserviceaccount.com',
            ],
        ),
        acl.entry(
            roles = acl.BUILDBUCKET_OWNER,
            groups = 'google/luci-task-force@google.com',
        ),
    ],
)

luci.gitiles_poller(
    name = 'master-gitiles-trigger',
    bucket = 'ci',
    repo = 'https://chromium.googlesource.com/chromium/src',
)


luci.recipe.defaults.cipd_package.set(
    'infra/recipe_bundles/chromium.googlesource.com/chromium/tools/build')


defaults.bucket.set('ci')
defaults.build_numbers.set(True)
defaults.builderless.set(None)
defaults.cpu.set(cpu.X86_64)
defaults.executable.set(luci.recipe(name = 'swarming/staging'))
defaults.execution_timeout.set(3 * time.hour)
defaults.mastername.set('chromium.swarm')
defaults.os.set(os.LINUX_DEFAULT)
defaults.service_account.set(
    'chromium-ci-builder-dev@chops-service-accounts.iam.gserviceaccount.com')
defaults.swarming_tags.set(['vpython:native-python-wrapper'])


def ci_builder(*, name, **kwargs):
  return builder(
      name = name,
      triggered_by = ['master-gitiles-trigger'],
      **kwargs
  )

ci_builder(
    name = 'Android N5 Swarm',
)

ci_builder(
    name = 'Android N5X Swarm',
)

ci_builder(
    name = 'ChromeOS Swarm',
)

ci_builder(
    name = 'Linux Swarm',
)

ci_builder(
    name = 'Mac Swarm',
    os = os.MAC_DEFAULT,
)

ci_builder(
    name = 'Windows Swarm',
    os = os.WINDOWS_DEFAULT,
)
