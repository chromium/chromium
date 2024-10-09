# Tools for migrating to starlark

This directory contains tools for migrating configuration to starlark from other
locations.

## Migrating tests to starlark

Make sure to [install
buildozer](https://github.com/bazelbuild/buildtools/blob/main/buildozer/README.md#installation)
and that it is on your path.

Commands assume the working directory is //infra/config.

1. Execute //infra/config/migration/migrate-targets.py.

    ```sh
    ./migration/migrate-targets.py $BUILDER_GROUP
    ```

    This will modify the starlark files so that it will generate per-builder
    targets spec files for all builders in the builder group that have their tests
    set in //testing/buildbot/waterfalls.pyl.

    To migrate a subset of builders from a builder group, instead run

    ```sh
    ./migration/migrate-targets.py $BUILDER_GROUP $BUILDER1 $BUILDER2 ... $BUILDER_N
    ```

    When migrating an entire builder group, it might be necessary to do this to
    migrate some builders in another builder group since all builders that are
    related via parent-child triggering or try-builder mirroring must be migrated
    in one CL.

    Executing this script may fail at the `./main.star` command because there is a
    test modification or removal for a test not present in any of the bundles
    within a builder, for example:
    `Error: attempting to remove test 'perfetto_unittests' that is not contained in the bundle`.

    This is most likely due to outdated configurations in //testing/buildbot,
    where an update in the test suites in //testing/buildbot/waterfalls.pyl was
    not reflected in //testing/buildbot/test_suite_exceptions.pyl. Remove any
    offending modifications or removals from
    //testing/buildbot/test_suite_exceptions.pyl, and rerun the script and those
    errors should go away.

    After updating starlark, the script will put the working copy into the state
    where git index contains the updated starlark and the new json files with
    the specs for each builder set to the same content that the testing/buildbot
    json files have for the builder. The unindexed new json files will have the
    content generated from the starlark, so `git diff` will show what is
    different between what generate_buildbot_json.py and what starlark generate.

1. Make any necessary manual changes to resolve the output of `git diff`.

    Differences could be due to:
    * Some manual changes are required that can't be handled by the migration
      script
      * The builder sets the isolated_scripts suite type with a suite containing
        gtests: the `"expand-as-isolated-script"` mixin needs to be applied to
        the suite; see
        [examples](https://source.chromium.org/search?q=%27%22expand-as-isolated-script%22%27%20-f:mixins.star).
        Note that if the builder was using both the gtest_tests and
        isolated_scripts suite types, then an anonymous bundle may need to be
        created to limit the scope that the mixin is applied to; see
        [examples](https://source.chromium.org/search?q=%27mixins%20%3D%20%22expand-as-isolated-script%22%27&sq=).
    * Some feature is not supported by starlark and/or the migration script
    * The difference doesn't actually have an effect (the
      targets-config-verifier try builder will pass in this case)

    If there are no differences, it's still possible that the migration will be
    rejected by the targets-config-verifier builder if the change is missing
    migrating some related builders.

1. Remove the entries for all migrated builders from
  //testing/buildbot/waterfalls.pyl and
  //testing/buildbot/test_suite_exceptions.pyl. In waterfalls.pyl, make sure to
  add breadcrumb comments to indicate the builders that are migrated at the top
  of the builder group. If the entire builder group is migrated, add the
  breadcrumb comment for the entire builder group near the top of the file.

1. If any builder group was fully migrated, manually remove the corresponding
  .json file from //testing/buildbot.

1. Execute //infra/config/migration/post-migrate-targets.py.

    ```sh
    ./migration/post-migrate-targets.py
    ```

    This will do the following:
    * Regenerate the .json files in //testing/buildbot.
    * Replace test suite definitions with bundle definitions for any test suites
      no longer referenced in //testing/buildbot.
    * Mark mixins and variants that are no longer referenced in
      //testing/buildbot so that they no longer generate pyl entries.
    * Mark isolates that are no longer referenced in //testing/buildbot so that
      they no longer trigger an error from //testing/buildbot/check.py.
    * Regenerate all //infra/config config files and sync necessary .pyl files
      to //testing/buildbot.

1. Some manual modifications may be necessary to the starlark files:

    * buildozer reformats the entire file, some of this is undesirable.
    * Top-level function calls added by the script should be reformatted so that
      each argument is on its own line (it's not possible to force this with
      buildozer).
    * Comments associated with the entries in test_suite_exceptions.pyl and
      waterfalls.pyl should be transferred to the newly added starlark
      declarations
    * Tests that are removed require a reason specified. The script puts in a
      reason string that will prevent the change from being submitted. This
      allows for confirming the generated files, without risking an undocumented
      removal. This may overlap with the previous bullet point since comments on
      the removal may serve as an adequate value for the reason field.
    * Comments associated with removed test suite declarations should be
      transferred to the corresponding bundle declarations.

## buildozer

The migration scripts make use of buildozer, which is a tool for making edits to
starlark files. It's geared towards modifying BUILD files but can be used for
other types of starlark files with some care. See installation
[instructions](https://github.com/bazelbuild/buildtools/blob/main/buildozer/README.md#installation)
to get buildozer.

Ideally all edits of a script could be done in a single invocation of buildozer
by using the -f flag to pass commands in a file, but command files don't
currently support values with escaped newlines which are necessary in order to
force values onto multiple lines when buildozer would normally put them on a
single line: calls with one argument, dicts with one item or lists with one
element. There is a [pull
request](https://github.com/bazelbuild/buildtools/pull/1296) to enable this
functionality waiting for review.
