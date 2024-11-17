# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/bootstrap.star", "POLYMORPHIC", "register_recipe_bootstrappability")
load("//lib/recipe_experiments.star", "register_recipe_experiments")

_RECIPE_NAME_PREFIX = "recipe:"

def _recipe_for_package(cipd_package):
    def recipe(
            *,
            name,
            cipd_version = None,
            recipe = None,
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
              In addition to a True or False value, POLYMORPHIC can be
              specified. This value will cause the builder's executable to be
              changed to the bootstrapper in properties-optional, polymorphic
              mode, which will by default not bootstrap any properties. On a
              per-run basis the $bootstrap/properties property can be set to
              bootstrap properties for different builders.
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
            use_python3 = True,
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
    name = "recipe:android/device_flasher",
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
    name = "recipe:binary_size_fuchsia_trybot",
)

build_recipe(
    name = "recipe:branch_configuration/tester",
    bootstrappable = True,
)

build_recipe(
    name = "recipe:build_size_trybot",
)

build_recipe(
    name = "recipe:chrome_build/build_perf",
    bootstrappable = True,
)

build_recipe(
    name = "recipe:chrome_build/build_perf_siso",
    bootstrappable = True,
)

build_recipe(
    name = "recipe:chrome_build/build_perf_developer",
    bootstrappable = True,
)

build_recipe(
    name = "recipe:chrome_build/build_perf_without_rbe",
    bootstrappable = True,
)

build_recipe(
    name = "recipe:celab",
)

build_recipe(
    name = "recipe:chromium",
    bootstrappable = True,
)

build_recipe(
    name = "recipe:chromium/builder_config_verifier",
)

build_recipe(
    name = "recipe:chromium/autosharder",
    bootstrappable = True,
)

build_recipe(
    name = "recipe:chromium/generic_script_runner",
)

build_recipe(
    name = "recipe:chromium/orchestrator",
    bootstrappable = True,
)

build_recipe(
    name = "recipe:chromium/compilator",
    bootstrappable = True,
)

build_recipe(
    name = "recipe:chromium/builder_cache_prewarmer",
    bootstrappable = True,
)

build_recipe(
    name = "recipe:chromium/gn_args_verifier",
)

build_recipe(
    name = "recipe:chromium/targets_config_verifier",
)

build_recipe(
    name = "recipe:chromium_licenses/scan",
)

build_recipe(
    name = "recipe:chromium_polymorphic/launcher",
)

build_recipe(
    name = "recipe:chromium_rr/orchestrator",
)

build_recipe(
    name = "recipe:chromium_rr/test_launcher",
    bootstrappable = POLYMORPHIC,
)

build_recipe(
    name = "recipe:chromium_3pp",
)

build_recipe(
    name = "recipe:chromium/fuzz",
    bootstrappable = True,
)

build_recipe(
    name = "recipe:chromium/mega_cq_launcher",
)

build_recipe(
    name = "recipe:chromium/universal_test_runner_test",
)

build_recipe(
    name = "recipe:chromium_clang_coverage_tot",
)

build_recipe(
    name = "recipe:chromium_fuzz_coverage",
)

build_recipe(
    name = "recipe:chrome_codeql_database_builder",
)

build_recipe(
    name = "recipe:chrome_codeql_query_runner",
)

build_recipe(
    name = "recipe:chromium_codesearch",
)

build_recipe(
    name = "recipe:chromium_expectation_files/expectation_file_scripts",
    bootstrappable = True,
)

build_recipe(
    name = "recipe:chromium_export_metadata",
)

build_recipe(
    name = "recipe:chromium_rts/create_model",
)

build_recipe(
    name = "recipe:chromium_rts/rts_analyze",
)

build_recipe(
    name = "recipe:chromium_trybot",
    bootstrappable = True,
)

build_recipe(
    name = "recipe:chromium_toolchain/package_clang",
)

build_recipe(
    name = "recipe:chromium_toolchain/package_rust",
)

build_recipe(
    name = "recipe:cronet",
)

build_recipe(
    name = "recipe:flakiness/reproducer",
)

build_recipe(
    name = "recipe:gofindit/chromium/single_revision",
    bootstrappable = POLYMORPHIC,
)

build_recipe(
    name = "recipe:gofindit/chromium/test_single_revision",
    bootstrappable = POLYMORPHIC,
)

build_recipe(
    name = "recipe:presubmit",
)

build_recipe(
    name = "recipe:reclient_config_deploy_check/tester",
)

build_recipe(
    name = "recipe:reclient_reclient_comparison",
)

build_recipe(
    name = "recipe:requires_testing_checker",
)

build_recipe(
    name = "recipe:reviver/chromium/runner",
    bootstrappable = POLYMORPHIC,
)

build_recipe(
    name = "recipe:swarming/deterministic_build",
)

build_recipe(
    name = "recipe:tricium_clang_tidy_wrapper",
)

build_recipe(
    name = "recipe:tricium_clang_tidy_orchestrator",
)

build_recipe(
    name = "recipe:tricium_metrics",
)

build_recipe(
    name = "recipe:tricium_oilpan",
)

build_recipe(
    name = "recipe:tricium_simple",
)

build_recipe(
    name = "recipe:webrtc/chromium_ios",
)
