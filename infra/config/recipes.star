# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

_RECIPE_NAME_PREFIX = "recipe:"

def _recipe_for_package(cipd_package):
    def recipe(*, name, cipd_version = None, recipe = None, use_bbagent = False):
        # Force the caller to put the recipe prefix rather than adding it
        # programatically to make the string greppable
        if not name.startswith(_RECIPE_NAME_PREFIX):
            fail("Recipe name {!r} does not start with {!r}"
                .format(name, _RECIPE_NAME_PREFIX))
        if recipe == None:
            recipe = name[len(_RECIPE_NAME_PREFIX):]
        return luci.recipe(
            name = name,
            cipd_package = cipd_package,
            cipd_version = cipd_version,
            recipe = recipe,
            use_bbagent = use_bbagent,
        )

    return recipe

build_recipe = _recipe_for_package(
    "infra/recipe_bundles/chromium.googlesource.com/chromium/tools/build",
)

build_recipe(
    name = "recipe:android/avd_packager",
)

build_recipe(
    name = "recipe:android/sdk_packager",
)

build_recipe(
    name = "recipe:angle_chromium",
)

build_recipe(
    name = "recipe:angle_chromium_trybot",
)

build_recipe(
    name = "recipe:binary_size_generator_tot",
)

build_recipe(
    name = "recipe:binary_size_trybot",
)

build_recipe(
    name = "recipe:celab",
)

build_recipe(
    name = "recipe:chromium",
)

build_recipe(
    name = "recipe:chromium_afl",
)

build_recipe(
    name = "recipe:chromium_clang_coverage_tot",
)

build_recipe(
    name = "recipe:chromium_codesearch",
    use_bbagent = True,
)

build_recipe(
    name = "recipe:chromium_export_metadata",
    use_bbagent = True,
)

build_recipe(
    name = "recipe:chromium_libfuzzer",
)

build_recipe(
    name = "recipe:chromium_libfuzzer_trybot",
)

build_recipe(
    name = "recipe:chromium_trybot",
)

build_recipe(
    name = "recipe:chromium_upload_clang",
)

build_recipe(
    name = "recipe:closure_compilation",
)

build_recipe(
    name = "recipe:cronet",
)

build_recipe(
    name = "recipe:findit/chromium/compile",
)

build_recipe(
    name = "recipe:findit/chromium/export_bot_db",
)

build_recipe(
    name = "recipe:findit/chromium/single_revision",
)

build_recipe(
    name = "recipe:findit/chromium/update_components",
)

build_recipe(
    name = "recipe:presubmit",
)

build_recipe(
    name = "recipe:swarming/deterministic_build",
)

build_recipe(
    name = "recipe:tricium_clang_tidy_wrapper",
)

build_recipe(
    name = "recipe:tricium_metrics",
)

build_recipe(
    name = "recipe:webrtc/chromium_ios",
)
