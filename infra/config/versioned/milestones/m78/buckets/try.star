load('//lib/builders.star', 'builder', 'cpu', 'defaults', 'goma', 'os')
load('//versioned/vars/try.star', 'vars')
# Load this using relative path so that the load statement doesn't
# need to be changed when making a new milestone
load('../vars.star', milestone_vars='vars')

luci.bucket(
    name = vars.bucket.get(),
    acls = [
        acl.entry(
            roles = acl.BUILDBUCKET_READER,
            groups = 'all',
        ),
        acl.entry(
            roles = acl.BUILDBUCKET_TRIGGERER,
            users = [
                'findit-for-me@appspot.gserviceaccount.com',
                'tricium-prod@appspot.gserviceaccount.com',
            ],
            groups = [
                'project-chromium-tryjob-access',
                # Allow Pinpoint to trigger builds for bisection
                'service-account-chromeperf',
                'service-account-cq',
            ],
        ),
        acl.entry(
            roles = acl.BUILDBUCKET_OWNER,
            groups = 'service-account-chromium-tryserver',
        ),
    ],
)

luci.cq_group(
    name = vars.cq_group.get(),
    # TODO(crbug/959436): enable it.
    cancel_stale_tryjobs = False,
    retry_config = cq.RETRY_ALL_FAILURES,
    tree_status_host = 'chromium-status.appspot.com/',
    watch = cq.refset(
        repo = 'https://chromium.googlesource.com/chromium/src',
        refs = [milestone_vars.cq_ref_regexp],
    ),
    acls = [
        acl.entry(
            acl.CQ_COMMITTER,
            groups = 'project-chromium-committers',
        ),
        acl.entry(
            acl.CQ_DRY_RUNNER,
            groups = 'project-chromium-tryjob-access',
        ),
    ],
)

defaults.bucket.set(vars.bucket.get())
defaults.bucketed_triggers.set(True)


def tryjob(
    *,
    disable_reuse=None,
    experiment_percentage=vars.experiment_percentage.get(),
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
        builder = vars.bucket.builder(name),
        cq_group = vars.cq_group.get(),
        disable_reuse = tryjob.disable_reuse,
        experiment_percentage = tryjob.experiment_percentage,
        location_regexp = tryjob.location_regexp,
        location_regexp_exclude = tryjob.location_regexp_exclude,
    )

  return builder(
      name = name,
      **kwargs
  )


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


def linux_builder(*, name, **kwargs):
  return try_builder(
      name = name,
      mastername = 'tryserver.chromium.linux',
      **kwargs
  )

linux_builder(
    name = 'chromium_presubmit',
    executable = luci.recipe(name = 'presubmit'),
    properties = {
        '$depot_tools/presubmit': {
            'runhooks': True,
            'timeout_s': 480,
        },
        'repo_name': 'chromium',
    },
    tryjob = tryjob(
        disable_reuse = True,
    ),
)

linux_builder(
    name = 'linux-rel',
    goma_backend = goma.backend.RBE_PROD,
    goma_jobs = goma.jobs.J150,
    tryjob = tryjob(),
    use_clang_coverage = True,
)
