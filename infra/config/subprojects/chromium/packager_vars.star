# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

def _chromium_3pp_properties(*, package_paths, platform):
    """Declares the properties of a builder that use the recipe chromium_3pp.

    See https://chromium.googlesource.com/chromium/tools/build/+/master/recipes/recipes/chromium_3pp.proto
    for the proto definitions.
    """
    return {
        "package_path": package_paths,
        "platform": platform,
    }

# Define the properties of chromium 3pp packagers and should be shared between
# CI and matching try builders.
CHROMIUM_3PP_PROPERTIES = {
    "3pp-linux-amd64-packager": _chromium_3pp_properties(
        platform = "linux-amd64",
        package_paths = [],
    ),
}
