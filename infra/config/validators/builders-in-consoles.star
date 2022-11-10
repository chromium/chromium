# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

def _validate_builders_in_console(ctx):
    builders = {}

    for console in ctx.output["luci/luci-milo.cfg"].consoles:
        for builder in console.builders:
            _, long_bucket, builder_name = builder.name.split("/")
            _, _, bucket = long_bucket.split(".", 2)
            builders.setdefault(bucket, {})[builder_name] = True

    builders_without_console = []

    for bucket in ctx.output["luci/cr-buildbucket.cfg"].buckets:
        if not proto.has(bucket, "swarming"):
            continue
        bucket_builders = builders.get(bucket.name, {})
        for builder in bucket.swarming.builders:
            if builder.name not in bucket_builders:
                builders_without_console.append(
                    "{}/{}".format(bucket.name, builder.name),
                )

    if builders_without_console:
        fail("The following builders do not appear in any console:\n  " +
             "\n  ".join([repr(b) for b in builders_without_console]))

lucicfg.generator(_validate_builders_in_console)
