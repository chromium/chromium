# Don't make a habit of this - it isn't public API
load('@stdlib//internal/luci/proto.star', 'scheduler_pb')


def _validate_scheduler(ctx):
  builders = {}

  for bucket in ctx.output['cr-buildbucket.cfg'].buckets:
    builders[bucket.name] = {}
    for builder in bucket.swarming.builders:
      builders[bucket.name][builder.name] = True

  scheduler_cfg = proto.from_textpb(
      scheduler_pb.ProjectConfig, ctx.output['luci-scheduler.cfg'])

  jobs = {}

  jobs_with_invalid_builders = []

  for job in scheduler_cfg.job:
    name = job.id
    jobs[name] = True
    # luci.<project>.<bucket>
    bucket = job.buildbucket.bucket.split('.', 2)[-1]
    builder = job.buildbucket.builder
    if bucket and not builders.get(bucket, {}).get(builder, False):
      jobs_with_invalid_builders.append(
          (name, '{}/{}'.format(bucket, builder)))

  if jobs_with_invalid_builders:
    fail('The following jobs refer to undefined builders:\n  '
         + '\n  '.join(['{!r} -> {!r}'.format(job, builder)
                        for job, builder in jobs_with_invalid_builders]))

  triggers_with_invalid_jobs = {}

  for trigger in scheduler_cfg.trigger:
    for job in trigger.triggers:
      if job not in jobs:
        triggers_with_invalid_jobs.setdefault(trigger.id, []).append(job)

  if triggers_with_invalid_jobs:
    msg = ['The following triggers refer to undefined jobs']
    for trigger, jobs in triggers_with_invalid_jobs.items():
      msg.append('\n  ')
      msg.append(repr(trigger))
      msg.append(' -> [')
      for job in jobs:
        msg.append('\n    ')
        msg.append(repr(job))
        msg.append(',')
      msg.append('\n  ]')
    fail(''.join(msg))

lucicfg.generator(_validate_scheduler)
