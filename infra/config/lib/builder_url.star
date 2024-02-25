# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//project.star", "settings")

def builder_url(bucket, builder, project = None):
    """A simple utility for constructing the milo URL for a builder."""
    project = project or settings.project
    url = "https://ci.chromium.org/p/%s/builders/%s/%s" % (
        project,
        bucket,
        builder,
    )
    return url

def linkify_builder(bucket, builder, project = None):
    """Returns an HTML link to a builder compatible with Milo-descriptions."""
    return "<a href=\"{}\">{}</a>".format(
        builder_url(bucket, builder, project),
        builder,
    )
