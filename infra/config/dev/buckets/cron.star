load('//lib/builders.star', 'builder', 'defaults', 'os')

luci.bucket(
    name = 'cron',
    acls = [
        acl.entry(
            roles = acl.BUILDBUCKET_READER,
            groups = 'all',
        ),
        acl.entry(
            roles = acl.BUILDBUCKET_TRIGGERER,
            users = [
                'luci-scheduler-dev@appspot.gserviceaccount.com',
                'snapshot-builder@chops-service-accounts.iam.gserviceaccount.com',
            ],
        ),
    ],
)


luci.recipe.defaults.cipd_package.set(
    'infra/recipe_bundles/chromium.googlesource.com/infra/infra')

defaults.bucket.set('cron')
defaults.builderless.set(None)
defaults.execution_timeout.set(time.hour)


builder(
    name = 'Snapshot Builder',
    executable = luci.recipe(name = 'snapshots/builder'),
    os = os.LINUX_DEFAULT,
    # TODO(smut): Adjust frequency
    schedule = 'with 300s interval',
    service_account = 'snapshot-builder@chops-service-accounts.iam.gserviceaccount.com',
)

builder(
    name = 'Snapshots',
    executable = luci.recipe(name = 'snapshots/snapshot'),
)
