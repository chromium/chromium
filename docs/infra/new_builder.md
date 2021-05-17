# Setting up a new builder

This doc describes how to set up a new builder on LUCI. It's focused
on Chromium builders, but parts may be applicable to other projects.

[TOC]

## TL;DR

For a typical chromium builder using the chromium recipe, you'll need to file a
bug for tracking purposes, acquire a host, and then land **three** CLs:

1. in [infradata/config][16], modifying `chromium.star`.
2. in [chromium/tools/build][17], modifying the chromium\_tests\_builder\_config
   configuration.
3. in [chromium/src][18], modifying all of the following:
    1. LUCI service configurations in `//infra/config`
    2. Compile configuration in `//tools/mb`
    3. Test configuration in `//testing/buildbot`

## Background

There are two kinds of builders: "try builders" (also known as pre-submit
builders, which test patches before they land) and "CI builders" (also
known as post-submit builders, which test configurations on the committed
code). "CQ builders" are try builders that the
[CQ (Commit Queue)](cq.md) will run on every CL when it is being submitted;
non-CQ try builders are called "optional" try builders.

Try builders normally pick up their configuration from a "mirrored" (i.e.,
matching) CI builder (the mapping is set in [trybots.py][25] in the
chromium\_tests recipe configuration) and run the exact same things.  However,
they can be configured to use slightly different GN args (usually to enable
DCHECKs on release builders) and (rarely) to run different tests or run them
with different flags. [ We enable dchecks on the release builders as a
compromise between the speed of a release builder and the coverage of a
debug builder]. Note that differences between the try builders and the CI
builders can cause changes to land that break the CI builders, which is
unfortunate, but a known tradeoff we make.

Every try builder should mirror a CI builder, to help identify when failures
are specific to a given patch, or happening more generally, and, if the latter,
that some sheriff is looking at the failures.e

[ Sometimes it's okay to have an "optional" try builder that doesn't have a
matching CI builder, but make sure to discuss that on the bug you're using
for this work. ]

Every CI builder should normally also have a mirrored try builder, so that you
can test changes that will affect the CI builder before landing. The only time
you would set up a CI builder that didn't have a matching try builder should be
if you couldn't set one up for some reason (e.g., we don't have enough capacity
for both, or some other limitation of the infrastructure).

> **Note:** not every CI builder that should have a matching try builder
> currently does, unfortunately (see
> [crbug.com/709214](https://crbug.com/709214)).  Also, figuring out what the
> corresponding builders are is harder than it should be, you have to look at
> [trybots.py][25] for the mapping (embedded in the code).

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

To acquire the hosts, please file a [capacity bug][1] (internal) and describe
the amount needed, along with any specialized hardware that's required (e.g.
mac hardware, attached mobile devices, a specific GPU, etc.).

See [infradata docs][4] (internal) for information on how to register
the hardware to be used by your builder.

## Recipe configuration

Recipes tell your builder what to do. Many require some degree of
per-builder configuration outside of the chromium repo, though the
specifics vary. The recipe you use depends on what you want your
builder to do.

For typical chromium compile and/or test builders, the chromium and
chromium\_trybot recipes should be sufficient.

To configure a chromium CI builder, you'll want to add a config block to the
file in [recipe\_modules/chromium\_tests\_builder\_config][5] corresponding to
your new builder's builder group. The format is somewhat in flux and is not very
consistent among the different builder groups, but something like this should
suffice:

``` py
'your-new-builder': builder_spec.BuilderSpec.create(
  chromium_config='chromium',
  gclient_config='chromium',
  chromium_apply_config=['mb', 'ninja_confirm_noop'],
  chromium_config_kwargs={
    'BUILD_CONFIG': 'Release', # or 'Debug', as appropriate
    'TARGET_BITS': 64, # or 32, for some mobile builders
  },
  simulation_platform='$PLATFORM',  # one of 'mac', 'win', or 'linux'

  # There are a variety of other options; most of them are either unnecessary in
  # most cases. If you think one may be applicable, please reach out or ask your
  # reviewer.
)
```

For chromium try builders, you'll also want to set up mirroring.
You can do so by adding your new try builder to [trybots.py][21].

A typical entry will just reference the matching CI builder, e.g.:

``` py
TRYBOTS = try_spec.TryDatabase.create({
  # ...

  'tryserver.chromium.example': {
      # If you want to build and test the same targets as one
      # CI builder, you can just do this:
      'your-new-builder': try_spec.TrySpec.create_for_single_mirror(
          builder_group='chromium.example',
          buildername='your-new-builder',
      ),

      # If you want to build the same targets as one CI builder
      # but not test anything, you can do this:
      'your-new-compile-builder': try_spec.TrySpec.create_for_single_mirror(
          builder_group='chromium.example',
          buildername='your-new-builder',
          analyze_mode='compile',
      ),

      # If you want to build and test the same targets as a builder/tester
      # CI pair, you can do this:
      'your-new-tester': try_spec.TrySpec.create_for_single_mirror(
          builder_group='chromium.example',
          buildername='your-new-builder',
          tester='your-new-tester',
      ),

      # If you want to mirror multiple try bots, please reach out.
    },

  # ...
})
```

## Chromium configuration

Lastly, you need to configure a variety of things in the chromium repo.
It's generally ok to land all of them in a single CL.

### LUCI services

LUCI services used by chromium are configured in [//infra/config][6].

The configuration is written in Starlark and used to generate Protobuf files
which are also checked in to the repo.

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

Each bucket has a corresponding `.star` file where the builders for the bucket
are defined.

Most builders are defined using the builder function from [builders.star][24]
(or some function that wraps it), which simplifies setting the most common
dimensions and properties and provides a unified interface for setting
module-level defaults.

A typical chromium builder won't need to configure much; module-level defaults
apply values that are widely used for the bucket (e.g. bucket and executable).

Each builder group has a function (sometimes multiple) defined that can be used
to define a builder that sets the `builder_group` property to the group and sets
group-specific defaults defaults. Find the block of builders defined using the
appropriate function and add a new definition, which may be as simple as:

```starlark
ci.linux_builder(
    name = '$BUILDER_NAME',
)
```

You can generate the configuration files and then view the added entry in
[cr-buildbucket.cfg][8] to make sure all properties and dimensions are set as
expected and adjust your definition as necessary.

#### Milo

Milo is responsible for displaying builders and build histories on a
set of consoles. Its configuration includes the definitions of those
consoles.

Chromium's milo Starlark configuration is intermixed with the
[builder definitions][23].
Chromium's generated milo configuration is [here][10].
Milo's configuration schema is [here][9].

Each console has a corresponding `.star` file that defines the console.

A typical chromium builder should be added to one or two consoles
at most: one corresponding to its builder group, and possibly the main
console.

##### CI builders

The sequence of CI builds for a builder corresponds to a linear history of
revisions in the repository, and the console takes advantage of that, allowing
you to compare what revisions are in what builds for different builders in the
console.

```starlark
luci.console_view(
    name = '$BUILDER_GROUP_NAME',
    ...
    entries = [
        ...
        luci.console_view(
            builder = '$BUCKET_NAME/$BUILDER_NAME',

            # A builder's category is a pipe-delimited list of strings
            # that determines how a builder is grouped on a console page.
            # N>=0
            category = '$CATEGORY1|$CATEGORY2|...|$CATEGORYN',

            # A builder's short name is the name that shows up in the column for
            # the builder in the consolew view.
            short_name = '$SHORT_NAME',
       ),
   ...
   ],
),
```

Both category and short_name can be omitted, but is strongly recommended that
all entries include short name.

##### Try builders

The sequence of try builders for a builder does not correspond to a linear
history of revisions. Consequently, the interface for the consoles is different,
as is the method of defining the console.

```starlark
luci.list_view(
    name = '$BUILDER_GROUP_NAME',
    entries = [
        ...
        '$BUCKET_NAME/$BUILDER_NAME',
        ...
    ],
),
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
modules for the `ci` bucket and its release branch counterparts are written such
that builders will be triggered by the appropriate poller by default. Setting
the `triggered_by` field on a builder will disable this default behavior.

##### Triggered by another builder

Builders that will be triggered by other builders (e.g. a builder compiles tests
and then triggers another builder to actually run the tests) call this out in
their own definition by setting the `triggered_by` field. For builders in the
`ci` bucket and its release branch counterparts, this will disable the default
behavior of being triggered by the poller.

```starlark
ci.linux_builder(
    name = '$BUILDER_NAME',
    triggered_by = ['$PARENT_BUILDER_NAME'],
)
```

##### Scheduled

Builders that need to run regularly but not in response to landed code can be
scheduled using the `schedule` field in their definition. For builders in the
`ci` bucket and its release branch counterparts, the `triggered_by` field should
be set to an empty list to disable the default behavior of being triggered by
the poller. See the documentation of the `schedule` field in the `Job` message
in the [scheduler schema][11].

```starlark
ci.builder(
    name = '$BUILDER_NAME',
    schedule = 'with 10m interval',
    triggered_by = [],
)
```

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

#### chromium & chromium\_trybot

The build and test configurations used by the main `chromium` and
`chromium_trybot` recipes are stored src-side:

* **Build configuration**: the gn configuration used by chromium
recipe builders is handled by [MB][13]. MB's configuration is documented
[here][14]. You only need to modify it if your new builder will be
compiling.

* **Test configuration**: the test configuration used by chromium
recipe builders is in a group of `.pyl` and derived `.json` files
in `//testing/buildbot`. The format is described [here][15].

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

## Questions? Feedback?

If you're in need of further assistance, if you're not sure about
one or more steps, or if you found this documentation lacking, please
reach out to infra-dev@chromium.org or [file a bug][19]!

[1]: http://go/file-chrome-resource-bug
[3]: https://bit.ly/chromium-build-naming
[4]: http://go/chromium-hardware
[5]: https://chromium.googlesource.com/chromium/tools/build/+/HEAD/recipes/recipe_modules/chromium_tests_builder_config
[6]: /infra/config
[7]: https://luci-config.appspot.com/schemas/projects:cr-buildbucket.cfg
[8]: /infra/config/generated/cr-buildbucket.cfg
[9]: http://luci-config.appspot.com/schemas/projects:luci-milo.cfg
[10]: /infra/config/generated/luci-milo.cfg
[11]: https://chromium.googlesource.com/infra/luci/luci-go/+/HEAD/scheduler/appengine/messages/config.proto
[12]: /infra/config/generated/luci-scheduler.cfg
[13]: /tools/mb/README.md
[14]: /tools/mb/docs/user_guide.md#the-mb_config_pyl-config-file
[15]: /testing/buildbot/README.md
[16]: https://chrome-internal.googlesource.com/infradata/config
[17]: https://chromium.googlesource.com/chromium/tools/build
[18]: /
[19]: https://g.co/bugatrooper
[20]: https://chromium.googlesource.com/infra/luci/luci-py/+/HEAD/appengine/swarming/proto/bots.proto
[21]: https://chromium.googlesource.com/chromium/tools/build/+/HEAD/recipes/recipe_modules/chromium_tests_builder_config/trybots.py
[22]: /infra/config/main.star
[23]: /infra/config/subprojects/chromium
[24]: /infra/config/lib/builders.star
[25]: https://source.chromium.org/chromium/chromium/tools/build/+/main:recipes/recipe_modules/chromium_tests_builder_config/trybots.py
[26]: /infra/config/generated/cq-builders.md
