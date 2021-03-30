# Creating a new builder

This doc describes how to set up a new builder on LUCI. It's focused
on chromium builders, but parts may be applicable to other projects.

[TOC]

## TL;DR

For a typical chromium builder using the chromium recipe,
you'll need to acquire a host and then land **three** CLs:

1. in [infradata/config][16], modifying `chromium.star`.
2. in [chromium/tools/build][17], modifying the chromium\_tests
   configuration.
3. in [chromium/src][18], modifying all of the following:
    1. LUCI service configurations in `//infra/config`
    2. Compile configuration in `//tools/mb`
    3. Test configuration in `//testing/buildbot`

## Pick a name and a master

Your new builder's name should follow the [chromium builder naming scheme][3].

We still use master names to group builders in a variety of places (even
though buildbot itself is largely deprecated). FYI builders should use
`chromium.fyi`, while other builders should mostly use `chromium.$OS`.

> **Note:** If you're creating a try builder, its name should match the
> name of the CI builder it mirrors.

## Obtain a host

When you're setting up a new builder, you'll need a host to run it. For CQ try
bots, you'll likely need a large number of hosts to handle the load in parallel.
For CI / waterfall builders or manually triggered try builders, you'll typically
only need a single host.

To acquire the hosts, please file a [capacity bug][1] (internal) and describe
the amount needed, along with any specialized hardware that's required (e.g.
mac hardware, attached mobile devices, a specific GPU, etc.).

## Register hardware with swarming

Once your resource request has been approved and you've obtained the hardware,
you'll need to associate it with your new builder in swarming. You can do so by
modifying the relevant swarming instance's configuration.

This configuration is written in Starlark, and then used to generate Protobuf
files which are also checked in to the repo. Chromium's configuration is in
[`chromium.star`][4] (internal only).

If you're simply using a generic GCE bot, find the stanza corresponding to
the OS and size that you want, and increment the number of bots allocated for
that configuration. For example:

```diff
    # os:Ubuntu-16.04, cpu:x86-64
    chrome.gce_xenial(
        prefix = 'luci-chromium-ci-xenial-8',
        zone = 'us-central1-b',
        disk_gb = 400,
        lifetime = time.week,
-       amount = 20,
+       amount = 21,
    )
```

If you've been given a specific hostname, instead add an entry for your bot
name to be mapped to that hostname. For example:

```diff
+   # os:Ubuntu-16.04, cpu:x86-64
+   'Linux Tests': 'swarm1234-c4',
```

## Recipe configuration

Recipes tell your builder what to do. Many require some degree of
per-builder configuration outside of the chromium repo, though the
specifics vary. The recipe you use depends on what you want your
builder to do.

For typical chromium compile and/or test builders, the chromium and
chromium\_trybot recipes should be sufficient.

To configure a chromium CI builder, you'll want to add a config block
to the file in [recipe\_modules/chromium\_tests][5] corresponding
to your new builder's master name. The format is somewhat in flux
and is not very consistent among the different masters, but something
like this should suffice:

``` py
'your-new-builder': {
  'chromium_config': 'chromium',
  'gclient_config': 'chromium',
  'chromium_apply_config': ['mb', 'ninja_confirm_noop'],
  'chromium_config_kwargs': {
    'BUILD_CONFIG': 'Release', # or 'Debug', as appropriate
    'TARGET_BITS': 64, # or 32, for some mobile builders
  },
  'testing': {
    'platform': '$PLATFORM', # one of 'mac', 'win', or 'linux'
  },

  # Optional: where to upload test results. Valid values include:
  #   'public_server' for test-results.appspot.com
  #   'staging_server' for test-results-test.appspot.com
  #   'no_server' to disable upload
  'test_results_config': 'public_server',

  # There are a variety of other options; most of them are either
  # unnecessary in most cases or are deprecated. If you think one
  # may be applicable, please reach out or ask your reviewer.
}
```

For chromium try builders, you'll also want to set up mirroring.
You can do so by adding your new try builder to [trybots.py][21].

A typical entry will just reference the matching CI builder, e.g.:

``` py
TRYBOTS = freeze({
  # ...

  'tryserver.chromium.example': {
    'builders': {
      # If you want to build and test the same targets as one
      # CI builder, you can just do this:
      'your-new-builder': simple_bot({
        'mastername': 'chromium.example',
        'buildername': 'your-new-builder'
      }),

      # If you want to build the same targets as one CI builder
      # but not test anything, you can do this:
      'your-new-compile-builder': simple_bot({
        'mastername': 'chromium.example',
        'buildername': 'your-new-builder',
      }, analyze_mode='compile'),

      # If you want to build and test the same targets as a builder/tester
      # CI pair, you can do this:
      'your-new-tester': simple_bot({
        'mastername': 'chromium.example',
        'buildername': 'your-new-builder',
        'tester': 'your-new-tester',
      }),

      # If you want to mirror multiple try bots, please reach out.
    },
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

Each master has a function (sometimes multiple) defined that can be used to
define a builder that runs with that mastername and sets master-specific
defaults. Find the block of builders defined using the appropriate function and
add a new definition, which may be as simple as:

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
at most: one corresponding to its master, and possibly the main
console.

##### CI builders

The sequence of CI builds for a builder corresponds to a linear history of
revisions in the repository, and the console takes advantage of that, allowing
you to compare what revisions are in what builds for different builders in the
console.

```starlark
luci.console_view(
    name = '$MASTER_NAME',
    ...
    entries = [
        ...
        luci.console_view(
            builder = '$BUCKET_NAME/$BUILDER_NAME',

            # A builder's category is a pipe-delimited list of strings
            # that determines how a builder is grouped on a console page.
            # N>=0
            category = '$GROUP1|$GROUP2|...|$GROUPN',

           # A builder's short name is a string up to three characters
           # long that lets someone uniquely identify it among builders
           # in the same category.
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
    name = '$MASTER_NAME',
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

## Questions? Feedback?

If you're in need of further assistance, if you're not sure about
one or more steps, or if you found this documentation lacking, please
reach out to infra-dev@chromium.org or [file a bug][19]!

[1]: http://go/file-chrome-resource-bug
[3]: https://bit.ly/chromium-build-naming
[4]: https://luci-config.appspot.com/#/services/chromium-swarm
[5]: https://chromium.googlesource.com/chromium/tools/build/+/HEAD/recipes/recipe_modules/chromium_tests
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
[21]: https://chromium.googlesource.com/chromium/tools/build/+/HEAD/recipes/recipe_modules/chromium_tests/trybots.py
[22]: /infra/config/main.star
[23]: /infra/config/subprojects/chromium
[24]: /infra/config/lib/builders.star
