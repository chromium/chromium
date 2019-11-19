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

#### Buildbucket

Buildbucket is responsible for taking a build scheduled by a user or
an agent and translating it into a swarming task. Its configuration
includes things like:

  * ACLs for scheduling and viewing builds
  * Swarming dimensions
  * Recipe name and properties

Buildbucket's configuration schema is [here][7].
Chromium's buildbucket configuration is [here][8].

A typical chromium builder won't need to configure much. Adding a
`builders` entry to the appropriate bucket
(`luci.chromium.ci` for CI / waterfall, `luci.chromium.try` for try)
with the new builder's name, the mixin containing the appropriate
master name, and perhaps one or two dimensions should be sufficient,
e.g.:

``` sh
buckets {
  name: "luci.chromium.ci"
  ...

  swarming {
    ...

    builders {
      name: "your-new-builder"

      # To determine what you should include here, look for an
      # existing mixin containing
      #
      #   recipe {
      #     properties: "mastername:$MASTER_NAME"
      #   }
      #
      mixins: "$MASTER_NAME_MIXIN"

      # If you're running a bunch of bots on GCE, you probably don't
      # want those bots to be keyed by buildername. Rather, they should
      # share the large pool with all the other bots using similar hardware.
      # To enable this, use the builderless mixin:
      mixins: builderless

      # Add other mixins and dimensions as necessary. You will
      # usually at least want an os dimension configured, so if
      # none of your included mixins have one, consider adding one.
    }
  }
}
```

#### Milo

Milo is responsible for displaying builders and build histories on a
set of consoles. Its configuration includes the definitions of those
consoles.

Milo's configuration schema is [here][9].
Chromium's milo configuration is [here][10].

A typical chromium builder should be added to one or two consoles
at most: one corresponding to its master, and possibly the main
console, e.g.

``` sh
consoles {
  ...
  name: "$MASTER_NAME"
  ...
  builders {
    name: "buildbucket/$BUCKET_NAME/$BUILDER_NAME"

    # A builder's category is a pipe-delimited list of strings
    # that determines how a builder is grouped on a console page.
    category: "$LARGE_GROUP|$MEDIUM_GROUP|$SMALL_GROUP"

    # A builder's short name is a string up to three characters
    # long that lets someone uniquely identify it among builders
    # in the same category.
    short_name: "$ID"
  }
}
```

#### Scheduler (CI / waterfall builders only)

The scheduler is responsible for triggering CI / waterfall builders.

Scheduler's configuration schema is [here][11].
Chromium's scheduler configuration is [here][12].

A typical chromium builder will need a job configuration. A chromium
builder that's triggering on new commits or on a regular schedule
(as opposed to being triggered by a parent builder) will also need
a trigger entry.

``` sh
trigger {
  id: "master-gitiles-trigger"

  ...

  # Adding your builder to the master-gitiles-trigger
  # will cause your builder to be triggered on new commits
  # to chromium's master branch.
  triggers: "your-new-ci-builder"
}

job {
  id: "your-new-ci-builder"

  # acl_sets should either be
  #  - "default" for builders that are triggered by the scheduler
  #     (i.e. anything triggering on new commits or on a cron)
  #  - "triggered-by-parent-builders" for builders that are
  #    triggered by other builders
  acl_sets: "default"

  buildbucket: {
    server: "cr-buildbucket.appspot.com"
    bucket: "luci.chromium.ci"
    builder: "your-new-ci-builder"
  }
}
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
[5]: https://chromium.googlesource.com/chromium/tools/build/+/master/scripts/slave/recipe_modules/chromium_tests
[6]: /infra/config
[7]: https://luci-config.appspot.com/schemas/projects:cr-buildbucket.cfg
[8]: /infra/config/cr-buildbucket.cfg
[9]: http://luci-config.appspot.com/schemas/projects:luci-milo.cfg
[10]: /infra/config/luci-milo.cfg
[11]: https://chromium.googlesource.com/infra/luci/luci-go/+/master/scheduler/appengine/messages/config.proto
[12]: /infra/config/luci-scheduler.cfg
[13]: /tools/mb/README.md
[14]: /tools/mb/docs/user_guide.md#the-mb_config_pyl-config-file
[15]: /testing/buildbot/README.md
[16]: https://chrome-internal.googlesource.com/infradata/config
[17]: https://chromium.googlesource.com/chromium/tools/build
[18]: /
[19]: https://g.co/bugatrooper
[20]: https://chromium.googlesource.com/infra/luci/luci-py/+/master/appengine/swarming/proto/bots.proto
[21]: https://chromium.googlesource.com/chromium/tools/build/+/master/scripts/slave/recipe_modules/chromium_tests/trybots.py
