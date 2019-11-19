load('//lib/builders.star', 'builder', 'defaults', 'os')

luci.bucket(
    name = 'try',
    acls = [
        acl.entry(
            roles = acl.BUILDBUCKET_READER,
            groups = 'all',
        ),
        acl.entry(
            roles = acl.BUILDBUCKET_TRIGGERER,
            groups = [
                'project-chromium-tryjob-access',
                'service-account-cq',
            ],
            users = 'findit-for-me@appspot.gserviceaccount.com',
        ),
        acl.entry(
            roles = acl.BUILDBUCKET_OWNER,
            groups = 'service-account-chromium-tryserver',
        ),
    ],
)


luci.recipe.defaults.cipd_package.set(
    'infra/recipe_bundles/chromium.googlesource.com/chromium/tools/build')

defaults.bucket.set('try')
defaults.build_numbers.set(True)
defaults.configure_kitchen.set(True)
defaults.execution_timeout.set(4 * time.hour)
# Max. pending time for builds. CQ considers builds pending >2h as timed
# out: http://shortn/_8PaHsdYmlq. Keep this in sync.
defaults.expiration_timeout.set(2 * time.hour)
defaults.service_account.set(
    'chromium-try-builder-dev@chops-service-accounts.iam.gserviceaccount.com')
defaults.swarming_tags.set(['vpython:native-python-wrapper'])


builder(
    name = 'mac_upload_clang',
    caches = [
        swarming.cache(
            name = 'xcode_mac_9a235',
            path = 'xcode_mac_9a235.app',
        ),
    ],
    executable = luci.recipe(name = 'chromium_upload_clang'),
    mastername = 'tryserver.chromium.mac',
    os = os.MAC_ANY,
    properties = {
        '$depot_tools/osx_sdk': {
            'sdk_version': '9a235',
        },
    },
)
