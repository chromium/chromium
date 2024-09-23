# Copyright 2024 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Library for registering builder owner alias to builders they own."""

def _generate_builder_owner_alias_files(ctx):
    cfg = None
    for f in ctx.output:
        if f.startswith("luci/cr-buildbucket"):
            cfg = ctx.output[f]
            break

    builders_by_contact_email = {}
    for bucket in cfg.buckets:
        if not proto.has(bucket, "swarming"):
            continue
        for builder in bucket.swarming.builders:
            email = builder.contact_team_email or "~unowned"
            builders_by_contact_email.setdefault(email, []).append("{}/{}".format(bucket.name, builder.name))

    for contact_email, builders in builders_by_contact_email.items():
        builder_owner_alias_file = "builder-owners/{}.txt".format(contact_email)
        ctx.output[builder_owner_alias_file] = "\n".join(sorted(builders))

lucicfg.generator(_generate_builder_owner_alias_files)
