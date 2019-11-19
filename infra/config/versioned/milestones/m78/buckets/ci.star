load('//lib/builders.star', 'builder', 'cpu', 'defaults', 'goma', 'os')
load('//versioned/vars/ci.star', 'vars')

luci.bucket(
    name = vars.bucket.get(),
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


defaults.bucket.set(vars.bucket.get())
defaults.bucketed_triggers.set(True)


# Builders appear after the function used to define them, with all builders
# defined using the same function ordered lexicographically by name
# Builder functions are defined in lexicographic order by name ignoring the
# '_builder' suffix

# Builder functions are defined for each master, with additional functions
# for specializing on OS or particular configuration (e.g. builders running
# libfuzzer recipe): XXX_builder and XXX_YYY_builder where XXX is the part after
# the last dot in the mastername and YYY is the OS or configuration

def gpu_builder(*, name, **kwargs):
  return builder(
      name = name,
      mastername = 'chromium.gpu',
      **kwargs
  )

gpu_builder(
    name = 'GPU Linux Builder',
    goma_backend = goma.backend.RBE_PROD,
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
    name = 'Linux Release (NVIDIA)',
)


def linux_builder(*, name, goma_jobs = goma.jobs.MANY_JOBS_FOR_CI, **kwargs):
  return builder(
      name = name,
      goma_jobs = goma_jobs,
      mastername = 'chromium.linux',
      **kwargs
  )

linux_builder(
    name = 'Linux Builder',
    goma_backend = goma.backend.RBE_PROD,
)

linux_builder(
    name = 'Linux Tests',
)
