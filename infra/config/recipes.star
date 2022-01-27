# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/bootstrap.star", "PROPERTIES_OPTIONAL", "register_recipe_bootstrappability")
load("//lib/recipe_experiments.star", "register_recipe_experiments")

_RECIPE_NAME_PREFIX = "recipe:"

def _recipe_for_package(cipd_package):
    def recipe(
            *,
            name,
            cipd_version = None,
            recipe = None,
            use_python3 = False,
            bootstrappable = False,
            experiments = None):
        """Declare a recipe for the given package.

        A wrapper around luci.recipe with a fixed cipd_package and some
        chromium-specific functionality. See
        https://chromium.googlesource.com/infra/luci/luci-go/+/HEAD/lucicfg/doc/README.md#luci.recipe
        for more information.

        Args:
            name: The name to use to refer to the executable in builder
              definitions. Must start with "recipe:". See luci.recipe for more
              information.
            cipd_version: See luci.recipe.
            recipe: See luci.recipe.
            use_python3: See luci.recipe.
            bootstrappable: Whether or not the recipe supports the chromium
              bootstrapper. A recipe supports the bootstrapper if the following
              conditions are met:
              * chromium_bootstrap.update_gclient_config is called to update the
                gclient config that is used for bot_update. This will be true if
                calling chromium_checkout.ensure_checkout or
                chromium_tests.prepare_checkout.
              * If the recipe does analysis to reduce compilation/testing, it
                skips analysis and performs a full build if
                chromium_bootstrap.skip_analysis_reasons is non-empty. This will
                be true if calling chromium_tests.determine_compilation_targets.
              In addition to a True or False value, PROPERTIES_OPTIONAL can be
              specified. This value will cause the builder's executable to be
              changed to the bootstrapper in properties optional mode, which
              will by default not bootstrap any properties. On a per-run basis
              the $bootstrap/properties property can be set to bootstrap properties.
            experiments: Experiments to apply to a builder using the recipe. If
              the builder specifies an experiment, the experiment value from the
              recipe will be ignored.
        """

        # Force the caller to put the recipe prefix rather than adding it
        # programatically to make the string greppable
        if not name.startswith(_RECIPE_NAME_PREFIX):
            fail("Recipe name {!r} does not start with {!r}"
                .format(name, _RECIPE_NAME_PREFIX))
        if recipe == None:
            recipe = name[len(_RECIPE_NAME_PREFIX):]
        ret = luci.recipe(
            name = name,
            cipd_package = cipd_package,
            cipd_version = cipd_version,
            recipe = recipe,
            use_bbagent = True,
            use_python3 = use_python3,
        )

        register_recipe_bootstrappability(name, bootstrappable)

        register_recipe_experiments(name, experiments or {})

        return ret

    return recipe

build_recipe = _recipe_for_package(
    "infra/recipe_bundles/chromium.googlesource.com/chromium/tools/build",
)

build_recipe(
    name = "recipe:android/androidx_packager",
)

build_recipe(
    name = "recipe:android/avd_packager",
)

build_recipe(
    name = "recipe:android/sdk_packager",
    use_python3 = True,
)

build_recipe(
    name = "recipe:angle_chromium",
)

build_recipe(
    name = "recipe:angle_chromium_trybot",
)

build_recipe(
    name = "recipe:binary_size_generator_tot",
    use_python3 = True,
)

build_recipe(
    name = "recipe:binary_size_trybot",
    use_python3 = True,
)

build_recipe(
    name = "recipe:binary_size_cast_trybot",
)

build_recipe(
    name = "recipe:binary_size_fuchsia_trybot",
    use_python3 = True,
)

build_recipe(
    name = "recipe:branch_configuration/tester",
    use_python3 = True,
)

build_recipe(
    name = "recipe:celab",
)

build_recipe(
    name = "recipe:chromium",
    bootstrappable = True,
    experiments = {
        "luci.recipes.use_python3": 100,
    },
)

build_recipe(
    name = "recipe:chromium/orchestrator",
    bootstrappable = True,
    use_python3 = True,
)

build_recipe(
    name = "recipe:chromium/compilator",
    bootstrappable = True,
    use_python3 = True,
)

build_recipe(
    name = "recipe:chromium_3pp",
)

build_recipe(
    name = "recipe:chromium_afl",
    use_python3 = True,
)

build_recipe(
    name = "recipe:chromium_clang_coverage_tot",
    use_python3 = True,
)

build_recipe(
    name = "recipe:chromium_codesearch",
)

build_recipe(
    name = "recipe:chromium_export_metadata",
    use_python3 = True,
)

build_recipe(
    name = "recipe:chromium_libfuzzer",
    use_python3 = True,
)

build_recipe(
    name = "recipe:chromium_libfuzzer_trybot",
    use_python3 = True,
)

build_recipe(
    name = "recipe:chromium_rts/create_model",
    use_python3 = True,
)

build_recipe(
    name = "recipe:chromium_trybot",
    bootstrappable = True,
    use_python3 = True,
)

build_recipe(
    name = "recipe:chromium_upload_clang",
    use_python3 = True,
)

build_recipe(
    name = "recipe:cronet",
)

build_recipe(
    name = "recipe:flakiness/generate_builder_test_data",
)

build_recipe(
    name = "recipe:findit/chromium/compile",
)

build_recipe(
    name = "recipe:findit/chromium/export_bot_db",
    use_python3 = True,
)

build_recipe(
    name = "recipe:findit/chromium/single_revision",
    bootstrappable = PROPERTIES_OPTIONAL,
)

build_recipe(
    name = "recipe:findit/chromium/update_components",
)

build_recipe(
    name = "recipe:presubmit",
    use_python3 = True,
)

build_recipe(
    name = "recipe:reclient_config_deploy_check/tester",
)

build_recipe(
    name = "recipe:reclient_goma_comparison",
)

build_recipe(
    name = "recipe:swarming/deterministic_build",
    use_python3 = True,
)

build_recipe(
    name = "recipe:swarming/staging",
    use_python3 = True,
)

build_recipe(
    name = "recipe:tricium_clang_tidy_wrapper",
    use_python3 = True,
)

build_recipe(
    name = "recipe:tricium_metrics",
)

build_recipe(
    name = "recipe:tricium_oilpan",
    use_python3 = True,
)

build_recipe(
    name = "recipe:tricium_simple",
    use_python3 = True,
)

build_recipe(
    name = "recipe:webrtc/chromium_ios",
)
