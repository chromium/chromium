# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//project.star", "settings")

def _get_builder_id(bucket, builder):
    return "{}/{}".format(bucket, builder)

def _get_goma_usage(builder):
    properties = json.decode(builder.properties)
    jobs = properties.get("$build/goma", {}).get("jobs")
    if jobs != None:
        return struct(jobs = jobs, estimate = False)

    # If the dimensions don't include the cores, guess 8
    cores = 8
    estimate = True
    for d in builder.dimensions:
        if d.startswith("cores:"):
            cores = int(d[len("cores:"):])
            estimate = False
            break

    # default logic taken from
    # https://source.chromium.org/chromium/chromium/tools/build/+/master:recipes/recipe_modules/goma/api.py;l=158;drc=d29660fdde91466d1a061e3fe85ea77b51951f00
    jobs = cores * 10
    if jobs > 200:
        jobs = 200
    return struct(jobs = jobs, estimate = estimate)

def _get_builder_goma_usage(ctx):
    goma_usage_by_builder = {}
    for bucket in ctx.output["cr-buildbucket.cfg"].buckets:
        for builder in bucket.swarming.builders:
            builder_id = _get_builder_id(bucket.name, builder.name)
            goma_usage_by_builder[builder_id] = _get_goma_usage(builder)
    return goma_usage_by_builder

def _get_scheduler_goma_usage(ctx, goma_usage_by_builder):
    scheduler_cfg = ctx.output["luci-scheduler.cfg"]

    gitiles_by_job = {}
    for trigger in scheduler_cfg.trigger:
        gitiles = {
            (trigger.gitiles.repo, ref): True
            for ref in trigger.gitiles.refs
        }
        for job in trigger.triggers:
            gitiles_by_job.setdefault(job, {}).update(gitiles)

    total_gitiles_goma_usage = 0
    total_goma_usage_by_gitiles = {}
    goma_usage_by_builder_by_gitiles = {}
    total_schedule_goma_usage = 0
    total_goma_usage_by_schedule = {}
    goma_usage_by_builder_by_schedule = {}
    for job in scheduler_cfg.job:
        if job.id not in gitiles_by_job and not job.schedule:
            continue
        if not job.buildbucket.bucket:
            continue
        _, project, bucket = job.buildbucket.bucket.split(".", 2)
        if project != settings.project:
            continue
        builder_id = _get_builder_id(bucket, job.buildbucket.builder)
        goma_usage = goma_usage_by_builder[builder_id]
        concurrent = 1
        if (proto.has(job, "triggering_policy") and
            proto.has(job.triggering_policy, "max_concurrent_invocations")):
            concurrent = job.triggering_policy.max_concurrent_invocations
        goma_jobs = goma_usage.jobs * concurrent
        goma_usage = struct(
            jobs = goma_jobs,
            jobs_per_build = goma_usage.jobs,
            estimate = goma_usage.estimate,
            concurrent = concurrent,
        )

        for g in gitiles_by_job.get(job.id, []):
            gitiles_dict = goma_usage_by_builder_by_gitiles.setdefault(g, {})
            gitiles_dict[builder_id] = goma_usage
            total_goma_usage_by_gitiles.setdefault(g, 0)
            total_goma_usage_by_gitiles[g] += goma_jobs
            total_gitiles_goma_usage += goma_jobs

        if job.schedule:
            schedule_dict = goma_usage_by_builder_by_schedule.setdefault(job.schedule, {})
            schedule_dict[builder_id] = goma_usage
            total_goma_usage_by_schedule.setdefault(job.schedule, 0)
            total_goma_usage_by_schedule[job.schedule] += goma_jobs
            total_schedule_goma_usage += goma_jobs

    gitiles_goma_usage = struct(
        total = total_gitiles_goma_usage,
        by_gitiles = {
            k: struct(
                total = total_goma_usage_by_gitiles[k],
                by_builder = goma_usage_by_builder_by_gitiles[k],
            )
            for k in total_goma_usage_by_gitiles
        },
    )
    schedule_goma_usage = struct(
        total = total_schedule_goma_usage,
        by_schedule = {
            k: struct(
                total = total_goma_usage_by_schedule[k],
                by_builder = goma_usage_by_builder_by_schedule[k],
            )
            for k in total_goma_usage_by_schedule
        },
    )
    return gitiles_goma_usage, schedule_goma_usage

def _get_cq_goma_usage(ctx, goma_usage_by_builder):
    cq_cfg = ctx.output["commit-queue.cfg"]

    total_cq_goma_usage = 0
    cq_goma_usage_by_builder = {}

    for group in cq_cfg.config_groups:
        if not proto.has(group.verifiers, "tryjob"):
            continue
        for builder in group.verifiers.tryjob.builders:
            if builder.includable_only:
                continue
            project, builder_id = builder.name.split("/", 1)
            if project != settings.project:
                continue
            goma_usage = goma_usage_by_builder[builder_id]
            jobs = goma_usage.jobs
            experiment_percentage = None
            if builder.experiment_percentage:
                experiment_percentage = builder.experiment_percentage
                jobs = jobs * experiment_percentage * 0.01
            goma_usage = struct(
                jobs = jobs,
                jobs_per_build = goma_usage.jobs,
                experiment_percentage = experiment_percentage,
                estimate = goma_usage.estimate,
            )
            cq_goma_usage_by_builder[builder_id] = goma_usage
            total_cq_goma_usage += jobs

    return struct(
        total = total_cq_goma_usage,
        by_builder = cq_goma_usage_by_builder,
    )

def _pyl_formatter(output):
    indent = [""]

    def add_line(*lines):
        for line in lines:
            line = line.strip()
            if line:
                if line[0] in ")}]":
                    indent[0] = indent[0][:-2]
                output.append(indent[0])
                output.append(line)
                if line[-1] in "[{(":
                    indent[0] += "  "
            output.append("\n")

    return add_line

# About 150 concurrent CQ attempts, assume 1/3 is spent compiling
_CQ_WEIGHT = 50

def _generate_goma_usage(ctx):
    goma_usage_by_builder = _get_builder_goma_usage(ctx)

    gitiles_goma_usage, schedule_goma_usage = (
        _get_scheduler_goma_usage(ctx, goma_usage_by_builder)
    )

    cq_goma_usage = _get_cq_goma_usage(ctx, goma_usage_by_builder)

    output = []

    _ = _pyl_formatter(output)

    _(
        "# This is a non-LUCI generated file",
        "# This file provides an abstract notion of how heavily goma is being used",
        "# This is consumed by presubmit checks that need to validate the config",
        "",
    )

    scheduler_total = gitiles_goma_usage.total + schedule_goma_usage.total
    weighted_cq_total = cq_goma_usage.total * _CQ_WEIGHT

    _(
        "{",
        "# {scheduler_total} (scheduler total) + {weighted_cq_total} (weighted CQ total)".format(
            scheduler_total = scheduler_total,
            weighted_cq_total = weighted_cq_total,
        ),
        "# (weighted CQ total) = {cq_total} (CQ total) * {cq_weight} (CQ weight)".format(
            cq_total = cq_goma_usage.total,
            cq_weight = _CQ_WEIGHT,
        ),
        "'*weighted total*': {},".format(scheduler_total + cq_goma_usage.total * _CQ_WEIGHT),
    )

    _(
        "'scheduler': {",
        "'*total*': {},".format(scheduler_total),
    )

    def _output_scheduler_goma_usage(name, goma_usage):
        _(
            "{}: {{".format(name),
            "'*total*': {},".format(goma_usage.total),
        )
        for builder, goma_usage_for_builder in sorted(goma_usage.by_builder.items()):
            if goma_usage_for_builder.concurrent > 1:
                _("# {concurrent} concurrent builds x {jobs} jobs".format(
                    concurrent = goma_usage_for_builder.concurrent,
                    jobs = goma_usage_for_builder.jobs_per_build,
                ))
            if goma_usage_for_builder.estimate:
                _("# jobs count assumes an 8-core machine")
            _("'{}': {},".format(builder, goma_usage_for_builder.jobs))

        # Close the dict for name
        _("},")

    _(
        "'triggered': {",
        "'*total*': {},".format(gitiles_goma_usage.total),
    )
    for gitiles, goma_usage_for_gitiles in sorted(gitiles_goma_usage.by_gitiles.items()):
        name = "('{}', '{}')".format(*gitiles)
        _output_scheduler_goma_usage(name, goma_usage_for_gitiles)

    # Close the 'triggered' dict
    _("},")

    _(
        "'scheduled': {",
        "'*total*': {},".format(schedule_goma_usage.total),
    )
    for schedule, goma_usage_for_schedule in sorted(schedule_goma_usage.by_schedule.items()):
        name = "'{}'".format(schedule)
        _output_scheduler_goma_usage(name, goma_usage_for_schedule)

    # Close the 'scheduled' dict
    _("},")

    # Close the 'scheduler' dict
    _("},")

    _(
        "'cq': {",
        "'*total*': {},".format(cq_goma_usage.total),
    )
    for builder, goma_usage in sorted(cq_goma_usage.by_builder.items()):
        if goma_usage.experiment_percentage:
            _("# {jobs} jobs x {percent}% experiment".format(
                jobs = goma_usage.jobs_per_build,
                percent = goma_usage.experiment_percentage,
            ))
        if goma_usage.estimate:
            _("# jobs count assumes an 8-core machine")
        _("'{}': {},".format(builder, goma_usage.jobs))

    # Close the 'cq' dict
    _("},")

    _(
        # Close the top-level dict
        "}",
        # End with a newline
        "",
    )

    ctx.output["goma-usage.pyl"] = "".join(output)

lucicfg.generator(_generate_goma_usage)
