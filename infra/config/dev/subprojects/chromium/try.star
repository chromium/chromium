# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# A realm for tryjob data.
luci.realm(
    name = "try",
    bindings = [
        luci.binding(
            roles = "role/buildbucket.reader",
            groups = "all",
        ),
        # Other roles are inherited from @root which grants them to group:all.
    ],
)
