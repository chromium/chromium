#!/usr/bin/env lucicfg
# See https://chromium.googlesource.com/infra/luci/luci-go/+/HEAD/lucicfg/doc/README.md
# for information on starlark/lucicfg

# Tell lucicfg what files it is allowed to touch
lucicfg.config(
    config_dir = 'generated',
    tracked_files = [
        'cr-buildbucket-dev.cfg',
        'luci-logdog-dev.cfg',
        'luci-milo-dev.cfg',
        'luci-scheduler-dev.cfg',
    ],
    fail_on_warnings = True,
)

luci.project(
    name = 'chromium',
    buildbucket = 'cr-buildbucket-dev.appspot.com',
    logdog = 'luci-logdog-dev.appspot.com',
    milo = 'luci-milo-dev.appspot.com',
    scheduler = 'luci-scheduler-dev.appspot.com',
    swarming = 'chromium-swarm-dev.appspot.com',
    acls = [
        acl.entry(
            roles = [
                acl.LOGDOG_READER,
                acl.PROJECT_CONFIGS_READER,
                acl.SCHEDULER_READER,
            ],
            groups = 'all',
        ),
        acl.entry(
            roles = acl.LOGDOG_WRITER,
            groups = 'luci-logdog-chromium-dev-writers',
        ),
        acl.entry(
            roles = acl.SCHEDULER_OWNER,
            groups = 'project-chromium-admins',
        ),
    ],
)

luci.logdog(
    gs_bucket = 'chromium-luci-logdog',
)

luci.milo(
    logo = 'https://storage.googleapis.com/chrome-infra-public/logo/chromium.svg',
)

exec('//dev/buckets/ci.star')
exec('//dev/buckets/cron.star')
exec('//dev/buckets/try.star')

exec('//dev/consoles/chromium.swarm.star')
exec('//dev/consoles/snapshots.star')
