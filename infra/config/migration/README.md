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

1. Regenerate the config files from starlark.

    ```sh
    ./main.star
    ```

    This should run without error and produce .json files for the migrated
    builders.

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

1. Remove the entries for all migrated builders from
  //testing/buildbot/waterfalls.pyl and
  //testing/buildbot/test_suite_exceptions.pyl. In waterfalls.pyl, make sure to
  add breadcrumb comments to indicate the builders that are migrated at the top
  of the builder group. If the entire builder group is migrated, add the
  breadcrumb comment for the entire builder group near the top of the file.

1. If any builder group was fully migrated, manually remove the corresponding
  .json file from //testing/buildbot.

1. Regenerate .json files in //testing/buildbot that have had builders removed
  by running

    ```sh
    ../../testing/buildbot/generate_buildbot_json.py
    ```

1. Test suites and mixins that are no longer referenced by
  //testing/buildbot/waterfalls.pyl require additional updates to the starlark.
  Suites need to be converted to bundles and mixins need to set
  `generate_pyl_entry=False`.

    To find the necessary updates, run

    ```sh
    ../../testing/buildbot/generate_buildbot_json.py --check
    ```

    Unused mixins will only be reported when there are no unused test suites, so
    you should run it until it produces no output.

1. Binaries that are no longer referenced by any builder in
  //testing/waterfalls.pyl should be added to the exclude list in
  //testing/buildbot/check.py. These binaries can be found by running

    ```sh
    ../../testing/buildbot/check.py
    ```

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
