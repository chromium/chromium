# Setting up a new builder

This doc describes how to set up a new builder on LUCI. It's focused on Chromium
builders, but parts may be applicable to other projects. If you find terms
you're not familiar with in this doc, consult the infra [glossary][30].

[TOC]

## TL;DR

For a typical chromium builder using the chromium recipe, you'll need to file a
bug for tracking purposes, acquire a host, and then land **two** CLs:

1. in [infradata/config][16], modifying either [ci.star][31] for CI bots or
   [try.star][32] for trybots.
1. in [chromium/src][18], modifying the builder's declaration in `//infra/config`
    to define the following:
    1. LUCI service declaration for the builder including the inputs to the
        builder's recipe
    1. Compile configuration
    1. Targets configuration

## Background

There are two kinds of builders: "try builders" (also known as pre-submit
builders, which test patches before they land) and "CI builders" (also
known as post-submit builders, which test configurations on the committed
code). "CQ builders" are try builders that the
[CQ (Commit Queue)](cq.md) will run on every CL when it is being submitted;
non-CQ try builders are called "optional" try builders.

Try builders normally pick up their configuration from a "mirrored" (i.e.,
matching) CI builder (the mapping is set in the `mirrors` field in the try
builder's definition) and run the same set of tests/compile the same set of
targets. However, they can be configured to use slightly different GN args
(usually to enable DCHECKs on release builders) and (rarely) to run different
tests or run them with different flags. [ We enable dchecks on the release
try builders as a compromise between the speed of a release builder and the
coverage of a debug builder]. Note that differences between the try builders and
the CI builders can cause changes to land that break the CI builders, which is
unfortunate, but a known tradeoff we make.

Every try builder must mirror a CI builder, to help identify when failures
are specific to a given patch, or happening more generally, and, if the latter,
that some sheriff is looking at the failures.

Every CI builder must normally also have a mirrored try builder, so that you
can test changes that will affect the CI builder before landing. The only time
you would set up a CI builder that didn't have a matching try builder should be
if you couldn't set one up for some reason (e.g., we don't have enough capacity
for both, or some other limitation of the infrastructure).

All CQ builders *must* have mirrored CI builders.

## Pick a name and a builder group

Your new builder's name should follow the [chromium builder naming scheme][3].

Builders are put into builder groups, with the group acting as part of the key
used for looking up configuration for the builder in various places. Builders
are also grouped within Milo UI pages according to the builder group. Builder
groups are somewhat arbitrary, but there are some builder groups with
significance:

* `chromium.$OS` - These are builder groups for builders that provide testing
  coverage for a specific OS. These builders are watched by the main sheriff
  rotation so they must be in a state where builds generally succeed.
* `chromium` - This is a builder group for builders that produce archived builds
  for each OS. These builders are watched by the main sheriff rotation.
* `chromium.fyi` - This is a catch-all builder group for FYI builders (builders
  that do not have a formal sheriff rotation). Avoid using this, instead add
  to/create an OS-specific FYI builder group if you are testing an OS-specific
  configuration (e.g. `chromium.android.fyi`) or a feature/team-specific builder
  group (e.g. `chromium.updater`).

> **Note:** If you're creating a try builder, its name should match the name of
> the CI builder it mirrors. The builder group for the try builder should
> usually be the builder group of the CI builder appended to `tryserver.`.
> However, not every existing builder does this
> ([crbug.com/905879](https://crbug.com/905879)).

## Obtain a host

When you're setting up a new builder, you'll need a host to run it. For CQ try
bots, you'll likely need a large number of hosts to handle the load in parallel.
For CI / waterfall builders or manually triggered try builders, you'll typically
only need a single host.

To acquire the hosts, please file a [resource request][1] (internal) and
describe the amount needed, along with any specialized hardware that's required
(e.g. mac hardware, attached mobile devices, a specific GPU, etc.). Note that
even if there's hardware currently available for the new builder, a resource
request will still be needed if the footprint of the new builder equates to
at least 5 VMs or 50 CPU cores per hour. See
[go/estimating-bot-capacity](https://goto.google.com/estimating-bot-capacity)
for guidance on how many hosts to request.

See [infradata docs][4] (internal) for information on how to register
the hardware to be used by your builder.

## Chromium configuration

Lastly, you need to configure a variety of things in the chromium repo.
It's generally ok to land all of them in a single CL.

### Starlark

LUCI services used by chromium are configured in [//infra/config][6].

The configuration is written in Starlark and used to generate Protobuf files
which are also checked in to the repo. In addition to the LUCI services
configuration files, the starlark also generates per-builder files that are used
by the builder's executable.

Generating all of the LUCI services configuration files for the production
builders is done by executing [main.star][22] or running
`lucicfg generate main.star`.

#### Buildbucket

Buildbucket is responsible for taking a build scheduled by a user or
an agent and translating it into a swarming task. Its configuration
includes things like:

* ACLs for scheduling and viewing builds
* Swarming dimensions
* Recipe name and properties

Chromium's buildbucket Starlark configuration is [here][23].
Chromium's generated buildbucket configuration is [here][8].
Buildbucket's configuration schema is [here][7].

Each builder group has a corresponding `.star` file where the builders in the
group are defined. Module-level defaults specify values that are widely used
within the builder group, so most builders won't need to individually set all of
its details.

You can generate the configuration files and then view the added entry in
[cr-buildbucket.cfg][8] to make sure all properties and dimensions are set as
expected and adjust your definition as necessary.

#### GN build configuration

The GN configuration used by the chromium family of recipes is handled by
[MB][13] and is historically configured in a [config file][14]. However, for
new builders, it should be configured within the builder definition in
Starlark, by adding a `gn_args` field to the builder function call. For
example:

```starlark
ci.builder(
    name = "$BUILDER_NAME",
    gn_args = "$GN_CONFIG",
)
```

The value to the `gn_args` field can be one of the following 3 types:

* a call to the `gn_args.config` function provided by the
[gn_args starlark library][28]. For example:

    ```starlark
    try_.builder(
        name = "$TRY_BUILDER_NAME",
        gn_args = gn_args.config(
            configs = [
                "ci/$CI_BUILDER_NAME",
                "try_builder",
            ],
        ),
    )
    ```

    The `configs` argument of the `gn_args.config` function takes a list of
    GN config names.

* The name of a single GN config to use. For example:

    ```starlark
    try_.builder(
        name = "$TRY_BUILDER_NAME",
        gn_args = "ci/$CI_BUILDER_NAME",
    )
    ```

* A dict mapping phase names to the GN config to use for that phase. This is a
  little-used feature, primarily for builders that do run a recipe that does
  multiple compilations. For example:

    ```starlark
    builders.builder(
        name = "$BUILDER_NAME",
        gn_args = {
            "phase1": "release_builder",
            "phase2": gn_args.config(
                configs = [
                    "debug",
                    "shared",
                ],
            ),
        },
    )
    ```

    The value for each phase can be a GN config name or a call to the
    `gn_args.config` function, but can not be a dictionary.

##### Named configs

Named GN configs are created in one of 2 ways:

* A named `gn_args.config` declaration. These configs are located in the
  [gn_args.star][29] in the gn_args directory. The declarations have the same
  form as when using `gn_args.config` for setting a builder's config except that
  they appear at the top-level of the file and they include the `name` argument,
  which is the name that can be used to reference them in other configs.

  Avoid creating new configs that are simply the combination of the names of
  several existing GN configs unless the combined name is an actual concept. For
  example, you should avoid creating a GN config named `shared_debug`, which
  includes GN configs `shared` and `debug`; instead, you should directly specify
  `["shared", "debug"]` in the Starlark builder definition.

* When a builder sets the `gn_args` argument, there will be 1 or more
  named configs defined for the builder.

  When the `gn_args` argument is assigned a single config, either by name or a
  `gn_args.config` call, there will be a single equivalent config defined that
  has the builder's bucket-qualified name as its name (e.g. `ci/my_builder`).

  When the `gn_args` argument is a dict, there will be a named config defined
  for each phase equivalent to the config for that phase. The name will be
  composed of the builder's bucket-qualified name and the phase name joined with
  a `:` (e.g. `ci/my_builder:phase1`).

#### Milo

Milo is responsible for displaying builders and build histories on a
set of consoles. Its configuration includes the definitions of those
consoles.

Chromium's milo Starlark configuration is intermixed with the
[builder definitions][23].
Chromium's generated milo configuration is [here][10].
Milo's configuration schema is [here][9].

A typical chromium builder should be added to one or two consoles
at most: one corresponding to its builder group, and possibly the main
console.

##### CI builders

The sequence of CI builds for a builder corresponds to a linear history of
revisions in the repository, and the console takes advantage of that, allowing
you to compare what revisions are in what builds for different builders in the
console.

```starlark
consoles.console_view(
    name = "$BUILDER_GROUP_NAME",
    ...  # There is often an ordering argument that controls what order the
         # entries in the console are displayed
)

ci.builder(
    name = "$BUILDER_NAME",
    ...
    console_view = consoles.console_view_entry(
        # A builder's category is a pipe-delimited list of strings
        # that determines how a builder is grouped on a console page.
        # N>=0
        category = "$CATEGORY1|$CATEGORY2|...|$CATEGORYN",

        # A builder's short name is the name that shows up in the column for
        # the builder in the console view.
        short_name = "$SHORT_NAME",
    ),
)
```

Both category and short_name can be omitted, but is strongly recommended that
all entries include short name.

##### Try builders

The sequence of builds for a try builder does not correspond to a linear history
of revisions. Consequently, the interface for the consoles is different, as is
the method of defining the console. Try builders will by default be added to a
list view with the same name as its builder group and also to a console that
includes all try builders, so nothing usually needs to be done to update a
console when adding a builder to an existing builder group.

```starlark
consoles.list_view(
    name = "$BUILDER_GROUP_NAME",
)
```

#### Scheduler (CI / waterfall builders only)

The scheduler is responsible for triggering CI / waterfall builders.

Chromium's scheduler Starlark configuration is intermixed with the
[builder definitions][23].
Chromium's generated scheduler configuration is [here][12].
Scheduler's configuration schema is [here][11].

##### Poller

To trigger builders when changes are landed on a repo, a poller needs to be
defined. The poller defines the repo and refs to watch and triggers builders
when changes land on one of the watched refs.

Pollers are already defined for all of the active refs within chromium/src. The
modules for the `ci` bucket are written such that builders will be triggered by
the appropriate poller by default. Setting the `triggered_by` or `parent` field
on a builder will disable this default behavior.

##### Triggered by another builder

Builders that will be triggered by other builders (e.g. a builder compiles tests
and then triggers another builder to actually run the tests) call this out in
their own definition by setting the `parent` field, assuming that src-side
builder configs are being used and the standard triggering mechanism of the
chromium family of recipes is being used. If the builder configs are specified
in the recipe or a different triggering mechanism is being used, then the
`triggered_by` field should be set instead, taking a list with a reference to
the triggering builder. In either case, for builders in the `ci` bucket, this
will disable the default behavior of being triggered by the poller.

```starlark
ci.builder(
    name = "$BUILDER_NAME",
    parent = "ci/$PARENT_BUILDER_NAME",
)
```

##### Scheduled

Builders that need to run regularly but not in response to landed code can be
scheduled using the `schedule` field in their definition. For builders in the
`ci` bucket, the `triggered_by` field should be set to an empty list to disable
the default behavior of being triggered by the poller. See the documentation of
the `schedule` field in the `Job` message in the [scheduler schema][11].

```starlark
ci.builder(
    name = "$BUILDER_NAME",
    schedule = "with 10m interval",
    triggered_by = [],
)
```

#### CQ (try builders only)

CQ is responsible for launching try builders against CLs before they are
submitted to verify that they don't cause any breakages.

Chromium's CQ Starlark configuration is intermixed with the
[builder definitions][23].
Chromium's generated CQ configuration is [here][26].
CQ's configuration schema is [here][27].

##### Opt-in try builders

Opt-in try builders are not automatically added to any CQ attempts, they must be
requested using the Cq-Include-Trybots footer. By default, try builders will be
opt-in try builders.

##### CQ builders

CQ builders are automatically added to CQ attempts. They can be configured to
only be added on specific paths or to be triggered experimentally some
percentage of the time. Adding builders to the CQ has a substantial cost, so
doing so will require approval from a limited set of approvers. This is enforced
by OWNERS files, so no need to worry about accidentally doing so.

To add a builder to the CQ, add a `tryjob` value to the builder definition.

This will add the builder to all CQ attempts (except for CLs that only contain
files in some particular directories).

The starlark config files for builders are organized by builder_group. For
example, the linux builders are in
//infra/config/subprojects/chromium/try/tryserver.chromium.linux.star. These
files have default values set for all builders in each particular file.

###### Regular (non-Orchestrator) CQ builders

```starlark
try_.builder(
    name = "$BUILDER_NAME",
    tryjob = try_.job(),
)
```

###### Orchestrator CQ Builders

The Orchestrator pattern is an optimization from the old chromium_trybot CQ
builders, where compiles are triggered to run on separate beefier machines.
It consists of the chromium/orchestrator.py and chromium/compilator.py recipes.

Builders using the Orchestrator pattern use a dedicated pool of machines to run
their builds (often called builderful). The
Orchestrator builder uses 2 or 4 core bots and the Compilator builder uses a
beefier >=16 core bot. The Compilator builder name should always be the
orchestrator name + "-compilator", like linux-rel and linux-rel-compilator.

In //infra/config/subprojects/chromium/try/tryserver.chromium.linux.star:

```starlark
try_.orchestrator_builder(
    name = "linux-rel",
    compilator = "linux-rel-compilator",
    branch_selector = branches.STANDARD_MILESTONE,
    main_list_view = "try",
    use_clang_coverage = True,
    coverage_test_types = ["unit", "overall"],
    tryjob = try_.job(),
)

try_.compilator_builder(
    name = "linux-rel-compilator",
    branch_selector = branches.STANDARD_MILESTONE,
    main_list_view = "try",
)
```

In infradata/config/configs/chromium-swarm/bots/chromium/chromium.star:
(In the [infradata/config](https://chrome-internal.googlesource.com/infradata/config/) repo)

```starlark
try_bots({
    "linux-rel": chrome.gce_bionic(
        prefix = "linux-rel-orchestrator-2-core",
        zone = "us-central1-b",
        machine_type = "n1-standard-2",
        lifetime = time.week,
        amount = 80,
    ),
    "linux-rel-compilator": chrome.gce_bionic(
        prefix = "linux-rel-compilator-ssd-16-core",
        zone = "us-central1-b",
        machine_type = "n1-standard-16",
        lifetime = time.week,
        amount = 25,
        disk_gb = 100,
        // This enables local ssd usage for this bot
        scratch_disks = chrome.scratch_disks(count = 1, interface = "NVME"),
    ),
})
```

###### Experimental CQ builders

Sometimes as a way of testing new features for try builders or as a precursor to
adding a builder to the CQ, it will be added as an experimental CQ builder,
which will be triggered for some percentage of CQ attempts. Such builds will not
block the completion of the CQ attempt and its status will not be considered for
determining the status of the CQ attempt.

To add a builder to the CQ experimentally, add a `tryjob` value to the builder
definition that specifies `experiment_percentage`.

```starlark
try_.builder(
    name = "$BUILDER_NAME",
    tryjob = try_.job(
        experiment_percentage = 5,
    ),
)
```

###### Path-based CQ builders

Sometimes it will be determined that a try builder is too expensive or catches
too few errors to be added to all CQ attempts, but that it is effective at
catching errors introduced when certain files are changed. In that case, the try
builder can be added to the CQ only when those files are changed.

To add a builder to the CQ on a path basis, add a `tryjob` value to the builder
definition that specifies `location_regexp`.

```starlark
try_.builder(
    name = "$BUILDER_NAME",
    tryjob = try_.job(
        # ".+/[+]/" Matches the repo/+/ prefix of the gitiles file location
        location_regexp = ".+/[+]/path/with/affected/files",
    ),
)
```

#### Generic script runner

You can use [chromium/generic_script_runner][34] recipe to run a script in
chromium without running builds and tests. See also [these examples][35].

#### Common mistakes

##### Setting branch_selector

A value should only be passed to the `branch_selector` argument if the builder
should run against the branches. This is uncommon, see the [Branched
builders](#branched-builders) section for information on whether a builder
should be branched.

##### Setting tree_closing (CI builder)

The `tree_closing` argument should only be set to `True` if compile failures for
the builder should prevent additional changes from being landed. This should
generally be restricted to builders that are watched by a sheriffing rotation.

##### Setting main_console_view (CI builder)

A value should usually be passed to the `main_console_view` argument if the
builder is in one of the builder groups that is watched by the main chromium
sheriff rotation (*chromium*, *chromium.win*, *chromium.mac*, *chromium.linux*,
*chromium.chromiumos* and *chromium.memory*).

##### Setting cq_mirror_console_view (CI builder)

A value should only be passed to the `cq_mirrors_console_view` argument if the
builder is the mirror of a non-experimental try builder on the CQ.

### Recipe-specific configurations

#### chromium, chromium\_trybot & chromium/orchestrator

The chromium family of recipes reads certain types of configuration from the
source tree.

##### Builder configuration

The [chromium\_tests\_builder\_config][5] module now supports module properties
that can be used to specify the per-builder config as part of the builder's
properties. There is starlark code that handles setting the properties correctly
for capturing parent-child and mirroring relationships. Having the config
specified at the builder definition simplifies adding and maintaining builders
and removes the need to make a change to [chromium/tools/build][17]. Module
properties must be used for all related builders (triggered/triggering builders
and mirrored/mirroring builders).

For the old way of defining the builder config in the recipe see the section
titled "Recipe-based config".

###### CI builders

CI builders will specify the `builder_spec` argument which contains the same
information that a `BuilderSpec` defined in the recipe would, though not in the
same structure.

```starlark
ci.builder(
    name = "$BUILDER_NAME",
    bootstrap = True,
    builder_spec = builder_config.builder_spec(
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
    ),
...
)
```

If the CI builder only runs tests and is triggered by another builder, it should
set `execution_mode` to `builder_config.execution_mode.TEST` and specify the
triggering builder in the `triggered_by` field. The triggered_by field must be
set and it must contain exactly 1 element that is a reference to a builder that
also defines a `builder_spec`.

```starlark
ci.builder(
    name = "$BUILDER_NAME",
    bootstrap = True,
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.TEST,
        chromium_config = builder_config.execution_mode.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
    ),
    triggered_by = ["ci/$PARENT_BUILDER_NAME"],
    ...
)
```

###### Try builders

Most try builders will mirror 1 or more CI builders, this is done by specifying
the `mirrors` argument.

```starlark
try_.builder(
    name = "$BUILDER_NAME",
    bootstrap = True,
    mirrors = [
        "ci/$CI_BUILDER_NAME",
        "ci/$CI_TESTER_NAME",
    ],
)
```

Occasionally, a try builder will be needed that doesn't mirror any CI builders,
in this case the `builder_spec` argument is specified just as a CI builder
would.

##### Targets configuration

The targets configuration used by the chromium family is historically configured
in a group of `.pyl` and derived `.json` files in `//testing/buildbot`. The
format is described [here][15]. Builders that specify their builder configs
src-side can and should instead set their targets in the builders' starlark
declarations. Most builders in the chromium project have been updated to do so.

The starlark-based target configs are defined in terms of target bundles. Target
bundles are a collection of compile targets to build and a collection of tests
to run along with the specifics of how those tests should be run (arguments to
pass, shard count, swarming dimensions to run against, etc.).

```starlark
ci.builder(
    name = "$BUILDER_NAME",
    targets = "$BUNDLE_NAME",
)
```

The value to the `targets` field can be one of the following 3 types:

* A call to the `targets.bundle` function provided by the [targets starlark
  library][36]. For example:

    ```starlark
    ci.builder(
        name = "$BUILDER_NAME",
        targets = targets.bundle(
            additional_compile_targets = [
                "compile-target1",
                "compile-target2",
            ],
            targets = [
                "bundle1",
                "bundle2",
            ],
        ),
    )
    ```

* The name of a targets bundle. This is equivalent to a `targets.bundle` call
  with the single bundle passed to the `targets` argument. For example:

    ```starlark
    ci.builder(
        name = "$BUILDER_NAME",
        targets = "$BUNDLE_NAME",
    )
    ```

* A list where the elements are either of the above types. This is equivalent to
  a `target.bundle` call with the list passed to the `targets` argument. For
  example:

    ```starlark
    ci.builder(
        name = "$BUILDER_NAME",
        targets = [
            "bundle1",
            targets.bundle(
                targets = ["bundle2"],
                mixins = ["my-mixin"],
            ),
        ],
    )
    ```

#### Named target bundles

Named target bundles are created in one of 4 ways:

* When a test is declared, a targets bundle is created with the same name as the
  test containing only that test. These declarations are located in [tests.star][37].

* A named `targets.bundle` declaration. These configs are located in
  [bundles.star][38]. The declarations have the same form as when using
  `targets.bundle` for setting a builder's targets except that they appear at
  the top-level of the file and they include the `name` argument, which is the
  name that can be used to reference them in other configs.

* A legacy suite definition. Suites are defined in [basic_suites.star][39],
  [compound_suites.star][40] and [matrix_compound_suites.star][41]. An
  equivalent targets bundle with the same name as the suite is created.

* When a builder sets the `targets` argument, an equivalent targets bundle will
  be defined that has the builder's bucket-qualified name as its name
  (e/g/`ci/my_builder`).

### Branched builders

Active chromium branches have CI and CQ set up that is a subset of the
configuration for trunk. The exact subset depends on the stage of the branch
(beta/stable vs. a long-term channel). Most builders do not need to be branched;
on trunk we run tests for not-yet-supported features and configurations.
Generally, a builder should be branched if and only if one of the following is
true:

* The builder is a non-experimental try builder on the CQ (specifies a value for
  the `tryjob` argument that doesn't set `experiment_percentage`).
* The builder is a CI builder that is mirrored by a non-experimental try
  builders on the CQ.
* The builder is a CI builder that uploads build artifacts.

There are occasional exceptions where builders are or aren't branched such as
not branching a builder that runs tests on a very small set of machines: with
limited capacity, it would be overwhelmed with additional builds happening on
the branch.

## Testing your new builder

The builder must be registered before it can be tested with `led`. The best
practice is to create the builder with no gardening rotation, no tree closing,
and no `tryjob()` entry.

After the CL lands and the builder is registered it can be tested with
[`led`][33].

Once the builder is green then gardening rotation, tree closing, and/or tryjob
settings can be changed. The gardening rotation can be unset if there is a
module-level default with `sheriff_rotations = args.ignore_default(None)` and
tree closing can be disabled with `tree_closing = False`.

## Questions? Feedback?

If you're in need of further assistance, if you're not sure about
one or more steps, or if you found this documentation lacking, please
reach out to infra-dev@chromium.org or [file a bug][19]!

[1]: http://go/i-need-hw
[3]: https://bit.ly/chromium-build-naming
[4]: http://go/chromium-hardware
[5]: https://chromium.googlesource.com/chromium/tools/build/+/HEAD/recipes/recipe_modules/chromium_tests_builder_config
[6]: /infra/config
[7]: https://luci-config.appspot.com/schemas/projects:cr-buildbucket.cfg
[8]: /infra/config/generated/luci/cr-buildbucket.cfg
[9]: http://luci-config.appspot.com/schemas/projects:luci-milo.cfg
[10]: /infra/config/generated/luci/luci-milo.cfg
[11]: https://chromium.googlesource.com/infra/luci/luci-go/+/HEAD/scheduler/appengine/messages/config.proto
[12]: /infra/config/generated/luci/luci-scheduler.cfg
[13]: /tools/mb/README.md
[14]: /tools/mb/docs/user_guide.md#the-mb_config_pyl-config-file
[15]: /testing/buildbot/README.md
[16]: https://chrome-internal.googlesource.com/infradata/config
[17]: https://chromium.googlesource.com/chromium/tools/build
[18]: /
[19]: https://g.co/bugatrooper
[22]: /infra/config/main.star
[23]: /infra/config/subprojects/chromium
[24]: /infra/config/lib/builders.star
[25]: https://source.chromium.org/chromium/chromium/tools/build/+/main:recipes/recipe_modules/chromium_tests_builder_config/trybots.py
[26]: /infra/config/generated/luci/commit-queue.cfg
[27]: https://luci-config.appspot.com/schemas/projects:commit-queue.cfg
[28]: /infra/config/lib/gn_args.star
[29]: /infra/config/gn_args/gn_args.star
[30]: /docs/infra/glossary.md
[31]: https://chrome-internal.googlesource.com/infradata/config/+/refs/heads/main/configs/chromium-swarm/starlark/bots/chromium/ci/ci.star
[32]: https://chrome-internal.googlesource.com/infradata/config/+/refs/heads/main/configs/chromium-swarm/starlark/bots/chromium/try.star
[33]: chromium.googlesource.com/chromium/src/+/refs/heads/main/docs/infra/using_led.md
[34]: https://source.chromium.org/chromium/infra/infra_superproject/+/main:build/recipes/recipes/chromium/generic_script_runner.py
[35]: https://source.chromium.org/search?q=recipe:chromium%2Fgeneric_script_runner&sq=&ss=chromium%2Fchromium%2Fsrc:infra%2Fconfig%2Fsubprojects%2Fchromium%2F
[36]: /infra/config/lib/targets.star
[37]: /infra/config/targets/tests.star
[38]: /infra/config/targets/bundles.star
[39]: /infra/config/targets/basic_suites.star
[40]: /infra/config/targets/compound_suites.star
[41]: /infra/config/targets/matrix_compound_suites.star