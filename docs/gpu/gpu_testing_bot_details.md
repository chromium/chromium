# GPU Bot Details

This page describes in detail how the GPU bots are set up, which files affect
their configuration, and how to both modify their behavior and add new bots.

[TOC]

## Overview of the GPU bots' setup

Chromium's GPU bots, compared to the majority of the project's test machines,
are physical pieces of hardware. When end users run the Chrome browser, they
are almost surely running it on a physical piece of hardware with a real
graphics processor. There are some portions of the code base which simply can
not be exercised by running the browser in a virtual machine, or on a software
implementation of the underlying graphics libraries. The GPU bots were
developed and deployed in order to cover these code paths, and avoid
regressions that are otherwise inevitable in a project the size of the Chromium
browser.

The GPU bots are utilized on the [chromium.gpu] and [chromium.gpu.fyi]
waterfalls, and various tryservers, as described in [Using the GPU Bots].

[chromium.gpu]: https://ci.chromium.org/p/chromium/g/chromium.gpu/console
[chromium.gpu.fyi]: https://ci.chromium.org/p/chromium/g/chromium.gpu.fyi/console
[Using the GPU Bots]: gpu_testing.md#Using-the-GPU-Bots

All of the physical hardware for the bots lives in the Swarming pool, and most
of it in the chromium.tests.gpu Swarming pool. The waterfall bots are simply
virtual machines which spawn Swarming tasks with the appropriate tags to get
them to run on the desired GPU and operating system type. So, for example, the
[Win10 x64 Release (NVIDIA)] bot is actually a virtual machine which spawns all
of its jobs with the Swarming parameters:

[Win10 x64 Release (NVIDIA)]: https://ci.chromium.org/p/chromium/builders/ci/Win10%20x64%20Release%20%28NVIDIA%29

```json
{
    "gpu": "10de:2184",
    "os": "Windows-10",
    "pool": "chromium.tests.gpu"
}
```

Since the GPUs in the Swarming pool are mostly homogeneous, this is sufficient
to target the pool of Windows 10-like NVIDIA machines. (There are a few Windows
7-like NVIDIA bots in the pool, which necessitates the OS specifier.)

Details about the bots can be found on [chromium-swarm.appspot.com] and by
using `src/tools/luci-go/swarming`, for example `swarming bots`.
If you are authenticated with @google.com credentials you will be able to make
queries of the bots and see, for example, which GPUs are available.

[chromium-swarm.appspot.com]: https://chromium-swarm.appspot.com/

The waterfall bots run tests on a single GPU type in order to make it easier to
see regressions or flakiness that affect only a certain type of GPU.

The tryservers like `win-rel` which include GPU tests, on the other hand, run
tests on more than one GPU type. As of this writing, the Windows tryservers ran
tests on NVIDIA and Intel GPUs; the Mac tryservers ran tests on Intel and AMD
GPUs. The way these tryservers' tests are specified is simply by *mirroring*
how one or more waterfall bots work. This is an inherent property of the
[`chromium_trybot` recipe][chromium_trybot.py], which was designed to eliminate
differences in behavior between the tryservers and waterfall bots. Since the
tryservers mirror waterfall bots, if the waterfall bot is working, the
tryserver must almost inherently be working as well.

[chromium_trybot.py]: https://chromium.googlesource.com/chromium/tools/build/+/main/recipes/recipes/chromium_trybot.py

There are some GPU configurations on the waterfall backed by only one machine,
or a very small number of machines in the Swarming pool. A few examples are:

<!-- XXX: update this list -->
*   [Mac Pro Release (AMD)](https://ci.chromium.org/p/chromium/builders/ci/Mac%20Pro%20FYI%20Release%20(AMD))
*   [Linux Release (AMD RX 5500 XT)](https://ci.chromium.org/p/chromium/builders/ci/Linux%20FYI%20Release%20(AMD%20RX%205500%20XT))

There are a couple of reasons to continue to support running tests on a
specific machine: it might be too expensive to deploy the required multiple
copies of said hardware, the hardware pool may have naturally died over time, or
the configuration might not be reliable enough to begin scaling it up.

## Adding a new isolated test to the bots

Adding a new test step to the bots requires that the test run via an isolate.
Isolates describe both the binary and data dependencies of an executable, and
are the underpinning of how the Swarming system works. See the [LUCI]
documentation for background on [Isolates] and [Swarming]. Note that with the
transition towards less Chromium-specific tools, you may see terms such as
"CAS inputs" instead of "isolate". These newer systems are functionally
identical to the older ones from a user's perspective, so the terms can be
safely interchanged.

[LUCI]: https://github.com/luci/luci-py
[Isolates]: https://github.com/luci/luci-py/blob/master/appengine/isolate/doc/README.md
[Swarming]: https://github.com/luci/luci-py/blob/master/appengine/swarming/doc/README.md

### Adding a new isolate

1.  Define your target using the `template("test")` template in
    [`src/testing/test.gni`][testing/test.gni]. See `test("gl_tests")` in
    [`src/gpu/BUILD.gn`][gpu/BUILD.gn] for an example. For a more complex
    example which invokes a series of scripts which finally launches the
    browser, see `telemetry_gpu_integration_test` in
    [`chrome/test/BUILD.gn`][chrome/test/BUILD.gn].
2.  Add an entry to
    [`src/testing/buildbot/gn_isolate_map.pyl`][gn_isolate_map.pyl] that refers
    to your target. Find a similar target to yours in order to determine the
    `type`. The type is referenced in [`src/tools/mb/mb.py`][mb.py].

[testing/test.gni]:     https://chromium.googlesource.com/chromium/src/+/main/testing/test.gni
[gpu/BUILD.gn]:         https://chromium.googlesource.com/chromium/src/+/main/gpu/BUILD.gn
[chrome/test/BUILD.gn]: https://chromium.googlesource.com/chromium/src/+/main/chrome/test/BUILD.gn
[gn_isolate_map.pyl]:   https://chromium.googlesource.com/chromium/src/+/main/testing/buildbot/gn_isolate_map.pyl
[mb.py]:                https://chromium.googlesource.com/chromium/src/+/main/tools/mb/mb.py

At this point you can build and upload your isolate to the isolate server.

See [Isolated Testing for SWEs] for the most up-to-date instructions. These
instructions are a copy which show how to run an isolate that's been uploaded
to the isolate server on your local machine rather than on Swarming.

[Isolated Testing for SWEs]: https://www.chromium.org/developers/testing/isolated-testing/for-swes

If `cd`'d into `src/`:

1.  `./tools/mb/mb.py isolate //out/Release [target name]`
    *   For example: `./tools/mb/mb.py isolate //out/Release angle_end2end_tests`
1.  `./tools/luci-go/isolate batcharchive -cas-instance chromium-swarm out/Release/[target name].isolated.gen.json`
    *   For example: `./tools/luci-go/isolate batcharchive -cas-instance chromium-swarm out/Release/angle_end2end_tests.isolated.gen.json`
See the section below on [isolate server credentials](#Isolate-server-credentials).

### Adding your new isolate to the tests that are run on the bots

See [Adding new steps to the GPU bots] for details on this process.

[Adding new steps to the GPU bots]: gpu_testing.md#Adding-new-steps-to-the-GPU-Bots

## Relevant files that control the operation of the GPU bots

In the [`chromium/src`][chromium/src] workspace:

*   [`src/testing/buildbot`][src/testing/buildbot]:
    *   [`chromium.gpu.json`][chromium.gpu.json] and
        [`chromium.gpu.fyi.json`][chromium.gpu.fyi.json] define which steps are
        run on which bots. These files are autogenerated. Don't modify them
        directly!
    *   [`waterfalls.pyl`][waterfalls.pyl],
        [`test_suites.pyl`][test_suites.pyl], [`mixins.pyl`][mixins.pyl],
        [`test_suite_exceptions.pyl`][test_suite_exceptions.pyl], and
        [`buildbot_json_magic_substitutions.py`][buildbot_substitutions.py]
        define the configuration for the autogenerated json files above.
        Run [`generate_buildbot_json.py`][generate_buildbot_json.py] to
        generate the json files after you modify these pyl files.
    *   [`generate_buildbot_json.py`][generate_buildbot_json.py]
        *   The generator script for all the waterfalls, including
            `chromium.gpu.json` and `chromium.gpu.fyi.json`.
        *   See the [README for generate_buildbot_json.py] for documentation
            on this script and the descriptions of the waterfalls and test
            suites.
        *   When modifying this script, don't forget to also run it, to
            regenerate the JSON files. Don't worry; the presubmit step will
            catch this if you forget.
        *   See [Adding new steps to the GPU bots] for more details.
    *   [`gn_isolate_map.pyl`][gn_isolate_map.pyl] defines all of the isolates'
        behavior in the GN build.
*   [`src/tools/mb/mb_config.pyl`][mb_config.pyl]
    *   Defines the GN arguments for all of the bots.
*   [`src/infra/config`][src/infra/config]:
    *   Definitions of how bots are organized on the waterfall,
        how builds are triggered, which VMs or machines are used for the
        builder itself, i.e. for compilation and scheduling swarmed tasks
        on GPU hardware, and underlying build configuration details, including
        which CI bots are mirrored on which trybots. The build config/mirroring
        information was previously in the [`tools/build`][tools/build] repo, but
        all GPU-related configurations have been migrated to be fully src-side.
        See
        [README.md](https://chromium.googlesource.com/chromium/src/+/main/infra/config/README.md)
        in this directory for up to date information.

[chromium/src]:                         https://chromium.googlesource.com/chromium/src/
[src/testing/buildbot]:                 https://chromium.googlesource.com/chromium/src/+/main/testing/buildbot
[src/infra/config]:                     https://chromium.googlesource.com/chromium/src/+/main/infra/config
[chromium.gpu.json]:                    https://chromium.googlesource.com/chromium/src/+/main/testing/buildbot/chromium.gpu.json
[chromium.gpu.fyi.json]:                https://chromium.googlesource.com/chromium/src/+/main/testing/buildbot/chromium.gpu.fyi.json
[gn_isolate_map.pyl]:                   https://chromium.googlesource.com/chromium/src/+/main/testing/buildbot/gn_isolate_map.pyl
[mb_config.pyl]:                        https://chromium.googlesource.com/chromium/src/+/main/tools/mb/mb_config.pyl
[generate_buildbot_json.py]:            https://chromium.googlesource.com/chromium/src/+/main/testing/buildbot/generate_buildbot_json.py
[mixins.pyl]:                           https://chromium.googlesource.com/chromium/src/+/main/testing/buildbot/mixins.pyl
[waterfalls.pyl]:                       https://chromium.googlesource.com/chromium/src/+/main/testing/buildbot/waterfalls.pyl
[test_suites.pyl]:                      https://chromium.googlesource.com/chromium/src/+/main/testing/buildbot/test_suites.pyl
[test_suite_exceptions.pyl]:            https://chromium.googlesource.com/chromium/src/+/main/testing/buildbot/test_suite_exceptions.pyl
[buildbot_substitutions.py]:            https://chromium.googlesource.com/chromium/src/+/main/testing/buildbot/buildbot_json_magic_substitutions.py
[tools/build]:         https://chromium.googlesource.com/chromium/tools/build/
[README for generate_buildbot_json.py]: ../../testing/buildbot/README.md

In the [`infradata/config`][infradata/config] workspace (Google internal only,
sorry):

*   [`gpu.star`][gpu.star]
    *   Defines a `chromium.tests.gpu` Swarming pool which contains all of the
        specialized hardware, except some hardware shared with Chromium:
        for example, the Windows and Linux NVIDIA
        bots, the Windows AMD bots, and the MacBook Pros with NVIDIA and AMD
        GPUs. New GPU hardware should be added to this pool.
    *   Also defines the GCEs, Mac VMs and Mac machines used for CI builders
        on GPU and GPU.FYI waterfalls and trybots.
*   [`pools.cfg`][pools.cfg]
    *   Defines the Swarming pools for GCEs and Mac VMs used for manually
        triggered trybots.

[infradata/config]:                https://chrome-internal.googlesource.com/infradata/config
[gpu.star]:                        https://chrome-internal.googlesource.com/infradata/config/+/main/configs/chromium-swarm/starlark/bots/chromium/gpu.star
[chromium.star]:                   https://chrome-internal.googlesource.com/infradata/config/+/main/configs/chromium-swarm/starlark/bots/chromium/chromium.star
[pools.cfg]:                       https://chrome-internal.googlesource.com/infradata/config/+/main/configs/chromium-swarm/pools.cfg
[main.star]:                       https://chrome-internal.googlesource.com/infradata/config/+/main/main.star
[vms.cfg]:                         https://chrome-internal.googlesource.com/infradata/config/+/main/configs/gce-provider/vms.cfg

## Walkthroughs of various maintenance scenarios

This section describes various common scenarios that might arise when
maintaining the GPU bots, and how they'd be addressed.

### How to add a new test or an entire new step to the bots

This is described in [Adding new tests to the GPU bots].

[Adding new tests to the GPU bots]: https://chromium.googlesource.com/chromium/src/+/main/docs/gpu/gpu_testing.md#Adding-New-Tests-to-the-GPU-Bots

### How to set up new virtual machine instances

The tests use virtual machines to build binaries and to trigger tests on
physical hardware. VMs don't run any tests themselves. There are 3 types of
bots:

* Builders - these bots build test binaries, upload them to storage and trigger
  tester bots (see below). Builds must be done on the same OS on which the
  tests will run, except for Android tests, which are built on Linux.
* Testers - these bots trigger tests to execute in Swarming and merge results
  from multiple shards. 2-core Linux GCEs are sufficient for this task.
* Builder/testers - these are the combination of the above and have same OS
  constraints as builders. All trybots are of this type, while for CI bots
  it is optional.

The process is:

1. Follow [go/request-chrome-resources](go/request-chrome-resources) to get
   approval for the VMs. Use `GPU` project resource group.
   See this [example ticket](http://crbug.com/1012805).
   You'll need to determine how many VMs are required, which OSes, how many
   cores and in which swarming pools they will be (see below for different
   scenarios).
    * If setting up a new GPU hardware pool, some VMs will also be needed
      for manual trybots, usually 2 VMs as of this writing.
    * Additional action is needed for Mac VMs, the GPU resource owner will
      assign the bug to Labs to deploy them. See this
      [example ticket](http://crbug.com/964355).
1. Once GCE resource request is approved / Mac VMs are deployed, the VMs need
   to be added to the right Swarming pools in a CL in the
   [`infradata/config`][infradata/config] (Google internal) workspace.
    1. GCEs for Windows CI builders and builder/testers should be added to
       `luci-chromium-gpu-ci-win10-8` group in [`gpu.star`][gpu.star].
    1. GCEs for Linux and Android CI builders and builder/testers should be added to
       `luci-chromium-gpu-ci-xenial-8` group in [`gpu.star`][gpu.star].
    1. VMs for Mac CI builders and builder/testers should be added to
       `builderfull_gpu_ci_bots` group in [`gpu.star`][gpu.star].
       [Example](https://chrome-internal-review.googlesource.com/c/infradata/config/+/1166889).
    1. GCEs for CI testers for all OSes should be added to
       `luci-chromium-gpu-ci-xenial-2` group in [`gpu.star`][gpu.star].
       [Example](https://chrome-internal-review.googlesource.com/c/infradata/config/+/2016410).
    1. GCEs and VMs for CQ and optional CQ GPU trybots for should be added to
       a corresponding `gpu_try_bots` group in [`gpu.star`][gpu.star].
       [Example](https://chrome-internal-review.googlesource.com/c/infradata/config/+/1561384).
       These trybots are "builderful", i.e. these GCEs can't be shared among
       different bots. This is done in order to limit the number of concurrent
       builds on these bots (until [crbug.com/949379](crbug.com/949379) is
       fixed) to prevent oversubscribing GPU hardware.
       `win_optional_gpu_tests_rel` is an exception, its GCEs come from
       `luci-chromium-try-win10-*-8` groups in
       [`chromium.star`][chromium.star], see
       [CL](https://chrome-internal-review.googlesource.com/c/infradata/config/+/1708723).
       This can cause oversubscription to Windows GPU hardware, however,
       Chrome Infra insisted on making this bot builderless due to frequent
       interruptions they get from limiting the number of concurrent builds on
       it, see discussion in
       [CL](https://chromium-review.googlesource.com/c/chromium/src/+/1775098).
    1. GCEs and VMs for manual GPU trybots should be added to a corresponding
       pool in "Manually-triggered GPU trybots" in [`gpu.star`][gpu.star].
       If adding a new pool, it should also be added to
       [`pools.cfg`][pools.cfg].
       [Example](https://chrome-internal-review.googlesource.com/c/infradata/config/+/2433332).
       This is a different mechanism to limit the load on GPU hardware,
       by having a small pool of GCEs which corresponds to some GPU hardware
       resource, and all trybots that target this GPU hardware compete for
       GCEs from this small pool.
    1. Run [`main.star`][main.star] to regenerate
       `configs/chromium-swarm/bots.cfg` and `configs/gce-provider/vms.cfg`.
       Double-check your work there.
       Note that previously [`vms.cfg`][vms.cfg] had to be edited manually.
       Part of the difficulty was in choosing a zone. This should soon no
       longer be necessary per [crbug.com/942301](http://crbug.com/942301),
       but consult with the Chrome Infra team to find out which of the
       [zones](https://cloud.google.com/compute/docs/regions-zones/) has
       available capacity. This also can be checked on viceroy
       [dashboard](https://viceroy.corp.google.com/chrome_infra/Quota/chrome?duration=7d).
    1. Get this reviewed and landed. This step associates the VM or pool of VMs
       with the bot's name on the waterfall for "builderful" bots or increases
       swarmed pool capacity for "builderless" bots.
       Note: CR+1 is not sticky in this repo, so you'll have to ping for
       re-review after every change, like rebase.

### How to add a new tester bot to the chromium.gpu.fyi waterfall

When deploying a new GPU configuration, it should be added to the
chromium.gpu.fyi waterfall first. The chromium.gpu waterfall should be reserved
for those GPUs which are tested on the commit queue. (Some of the bots violate
this rule – namely, the Debug bots – though we should strive to eliminate these
differences.) Once the new configuration is ready to be fully deployed on
tryservers, bots can be added to the chromium.gpu waterfall, and the tryservers
changed to mirror them.

In order to add Release and Debug waterfall bots for a new configuration,
experience has shown that at least 4 physical machines are needed in the
swarming pool. The reason is that the tests all run in parallel on the Swarming
cluster, so the load induced on the swarming bots is higher than it would be
if the tests were run strictly serially.

With these prerequisites, these are the steps to add a new (swarmed) tester bot.
(Actually, pair of bots -- Release and Debug. If deploying just one or the
other, ignore the other configuration.) These instructions assume that you are
reusing one of the existing builders, like [`GPU FYI Win Builder`][GPU FYI Win
Builder].

1.  Work with the Chrome Infrastructure Labs team to get the (minimum 4)
    physical machines added to the Swarming pool. Use
    [chromium-swarm.appspot.com] or `src/tools/luci-go/swarming bots`
    to determine the PCI IDs of the GPUs in the bots. (These instructions will
    need to be updated for Android bots which don't have PCI buses.)

    1.  Make sure to add these new machines to the chromium.tests.gpu Swarming
        pool by creating a CL against [`gpu.star`][gpu.star] in the
        [`infradata/config`][infradata/config] (Google internal) workspace.
        Git configure your user.email to @google.com if necessary. Here is one
        [example CL](https://chrome-internal-review.googlesource.com/913528)
        and a
        [second example](https://chrome-internal-review.googlesource.com/1111456).

    1.  Run [`main.star`][main.star] to regenerate
        `configs/chromium-swarm/bots.cfg`. Double-check your work there.

1.  Allocate new virtual machines for the bots as described in [How to set up
    new virtual machine
    instances](#How-to-set-up-new-virtual-machine-instances).

1.  Create a CL in the Chromium workspace which does the following. Here's an
    [example CL](https://chromium-review.googlesource.com/c/chromium/src/+/1752291).
    1.  Adds the new machines to [`waterfalls.pyl`][waterfalls.pyl] directly or
        to [`mixins.pyl`][mixins.pyl], referencing the new mixin in
        [`waterfalls.pyl`][waterfalls.pyl].
        1.  The swarming dimensions are crucial. These must match the GPU and
            OS type of the physical hardware in the Swarming pool. This is what
            causes the VMs to spawn their tests on the correct hardware. Make
            sure to use the chromium.tests.gpu pool, and that the new machines
            were specifically added to that pool.
        1.  Make triply sure that there are no collisions between the new
            hardware you're adding and hardware already in the Swarming pool.
            For example, it used to be the case that all of the Windows NVIDIA
            bots ran the same OS version. Later, the Windows 8 flavor bots were
            added. In order to avoid accidentally running tests on Windows 8
            when Windows 7 was intended, the OS in the swarming dimensions of
            the Win7 bots had to be changed from `win` to
            `Windows-2008ServerR2-SP1` (the Win7-like flavor running in our
            data center). Similarly, the Win8 bots had to have a very precise
            OS description (`Windows-2012ServerR2-SP0`).
        1.  If you're deploying a new bot that's similar to another existing
            configuration, please search around in
            [`test_suite_exceptions.pyl`][test_suite_exceptions.pyl] for
            references to the other bot's name and see if your new bot needs
            to be added to any exclusion lists. For example, some of the tests
            don't run on certain Win bots because of missing OpenGL extensions.
        1.  Run [`generate_buildbot_json.py`][generate_buildbot_json.py] to
            regenerate `src/testing/buildbot/chromium.gpu.fyi.json`.
    1. Updates [`chromium.gpu.star`][chromium.gpu.star] or
       [`chromium.gpu.fyi.star`][chromium.gpu.fyi.star] and its related
       generated files [`cr-buildbucket.cfg`][cr-buildbucket.cfg],
       [`luci-scheduler.cfg`][luci-scheduler.cfg], and
       [`luci-milo.cfg`][luci-milo.cfg]:
        *   Use the appropriate definition for the type of the bot being added,
            for example, `ci.gpu_fyi_thin_tester()` should be used for all CI
            tester bots on GPU FYI waterfall.
        *   Make sure to set `triggered_by` property to the builder which
            triggers the testers (like `'GPU Win FYI Builder'`).
        *   Include a `ci.console_view_entry` for the builder's
            `console_view_entry` argument. Look at the short names and
            categories to try and come up with a reasonable organization.
        *   Make sure to set the `serialize_tests` property to `True` in the
            builder config. This is specified for waterfall bots but not trybots
            and helps avoid overloading the physical hardware. Additionally,
            if the bot is configured as a split builder/tester pair, ensure that
            the tester's builder config matches the parent builder and the
            tester is marked as being triggered by the parent builder.
    1.  Run `main.star` in [`src/infra/config`][src/infra/config] to update the
        generated files. Double-check your work there.
    1.  If you were adding a new builder, you would need to also add the new
        machine to [`src/tools/mb/mb_config.pyl`][mb_config.pyl].

1. If the number of physical machines for the new bot permits, you should also
   add a manually-triggered trybot at the same time that the CI bot is added.
   This is described in [How to add a new manually-triggered trybot].

While the above instructions assume that an existing parent builder will be
be used, a new one can be set up by doing the same steps, but also adding the
new parent builder at the same time. There are plenty of existing parent
builder/child tester bots that you can use as a reference.

[How to add a new manually-triggered trybot]: https://chromium.googlesource.com/chromium/src/+/main/docs/gpu/gpu_testing_bot_details.md#How-to-add-a-new-manually_triggered-trybot

[chromium.gpu.star]:     https://chromium.googlesource.com/chromium/src/+/main/infra/config/subprojects/ci/chromium.gpu.star
[chromium.gpu.fyi.star]: https://chromium.googlesource.com/chromium/src/+/main/infra/config/subprojects/ci/chromium.gpu.fyi.star
[cr-buildbucket.cfg]:    https://chromium.googlesource.com/chromium/src/+/main/infra/config/generated/cr-buildbucket.cfg
[luci-scheduler.cfg]:    https://chromium.googlesource.com/chromium/src/+/main/infra/config/generated/luci-scheduler.cfg
[luci-milo.cfg]:         https://chromium.googlesource.com/chromium/src/+/main/infra/config/generated/luci-milo.cfg
[GPU FYI Win Builder]:   https://ci.chromium.org/p/chromium/builders/luci.chromium.ci/GPU%20FYI%20Win%20Builder

### How to remove an existing bot from the chromium.gpu.fyi waterfall

Basically, one needs to follow
[How to add a new tester bot to the chromium.gpu.fyi waterfall](#how-to-add-a-new-tester-bot-to-the-chromium_gpu_fyi-waterfall)
step in reverse.
To prevent bot failures during deletion process, pause the bot on
https://luci-scheduler.appspot.com/.

### How to start running tests on a new GPU type on an existing try bot

Let's say that you want to cause the `win-rel` try bot to run tests on
CoolNewGPUType in addition to the types it currently runs (as of this writing
only NVIDIA). To do this:

1.  Make sure there is enough hardware capacity using the available tools to
    report utilization of the Swarming pool.
1.  Deploy Release and Debug testers on the `chromium.gpu` waterfall, following
    the instructions for the `chromium.gpu.fyi` waterfall above. Make sure
    the flakiness on the new bots is comparable to existing `chromium.gpu` bots
    before proceeding.
1.  Create a CL in the [`chromium/src`][chromium/src] workspace that adds the
    new Release tester to `win-rel`'s `mirrors` list. Rerun
    `infra/config/main.star`.
1.  Once the above CL lands, the commit queue will **immediately** start
    running tests on the CoolNewGPUType configuration. Be vigilant and make
    sure that tryjobs are green. If they are red for any reason, revert the CL
    and figure out offline what went wrong.

### How to add a new manually-triggered trybot

Manually-triggered trybots are needed for investigating failures on a GPU type
which doesn't have a corresponding CQ trybot (due to lack of GPU resources).
Even for GPU types that have CQ trybots, it is convenient to have
manually-triggered trybots as well, since the CQ trybot often runs on more than
one GPU type, or some test suites which run on CI bot can be disabled on CQ
trybot (when the CQ bot has
[no CI equivalent](https://chromium.googlesource.com/chromium/src/+/main/docs/gpu/gpu_testing_bot_details.md#how-to-add-a-new-try-bot-that-runs-a-subset-of-tests-or-extra-tests)).
Thus, all CI bots in `chromium.gpu` and `chromium.gpu.fyi` have corresponding
manually-triggered trybots, except a few which don't have enough hardware
to support it. A manually-triggered trybot should be added at the same time
a CI bot is added.

Here are the steps to set up a new trybot which runs tests just on one
particular GPU type. Let's consider that we are adding a manually-triggered
trybot for the Win7 NVIDIA GPUs in Release mode. We will call the new bot
`gpu-fyi-try-win7-nvidia-rel-64`.

1.  If there already exist some manually-triggered trybot which runs tests on
    the same group of machines (i.e. same GPU, OS and driver), the new trybot
    will have to share the VMs with it. Otherwise, create a new pool of VMs for
    the new hardware and allocate the VMs as described in
    [How to set up new virtual machine instances](#How-to-set-up-new-virtual-machine-instances),
    following the "Manually-triggered GPU trybots" instructions.

1.  Create a CL in the Chromium workspace which does the following. Here's a
    [reference CL](https://chromium-review.googlesource.com/c/chromium/src/+/2191276)
    exemplifying the new "GCE pool per GPU hardware pool" way.
    1.  Updates [`gpu.try.star`][gpu.try.star] and its related generated file
        [`cr-buildbucket.cfg`][cr-buildbucket.cfg]:
        *   Add the new trybot with the right `builder` define and VMs pool.
            For `gpu-fyi-try-win7-nvidia-rel-64` this would be
            `gpu_win_builder()` and `luci.chromium.gpu.win7.nvidia.try`.
        *   Add the relevant CI bots to the new trybot's `mirrors` list. If the
            CI tester has a parent builder, the parent should be in the list as
            well.
    1.  Run `main.star` in [`src/infra/config`][src/infra/config] to update the
        generated files. Double-check your work there.
    1.  Adds the new trybot to [`src/tools/mb/mb_config.pyl`][mb_config.pyl]
        and [`src/tools/mb/mb_config_buckets.pyl`][mb_config_buckets.pyl].
        Use the same mixin as does the builder for the CI bot this trybot
        mirrors, in case of `gpu-fyi-try-win7-nvidia-rel-64` this is
        `GPU FYI Win x64 Builder` and thus `gpu_fyi_tests_release_trybot`.
    1.  Get this CL reviewed and landed.

At this point the new trybot should automatically show up in the
"Choose tryjobs" pop-up in the Gerrit UI, under the
`luci.chromium.try` heading, because it was deployed via LUCI. It
should be possible to send a CL to it.

(It should not be necessary to modify buildbucket.config as is
mentioned at the bottom of the "Choose tryjobs" pop-up. Contact the
chrome-infra team if this doesn't work as expected.)

[gpu.try.star]:                https://chromium.googlesource.com/chromium/src/+/main/infra/config/subprojects/gpu.try.star
[luci.chromium.try.star]:      https://chromium.googlesource.com/chromium/src/+/main/infra/config/consoles/luci.chromium.try.star
[tryserver.chromium.win.star]: https://chromium.googlesource.com/chromium/src/+/main/infra/config/consoles/tryserver.chromium.win.star


### How to add a new try bot that runs a subset of tests or extra tests

Several projects (ANGLE, Dawn) run custom tests using the Chromium recipes. They
use try bot bot configs that run subsets of Chromium or additional slower tests
that can't be run on the main CQ.

These trybots are a little different because they do not mirror any waterfall
bots.

Let's say the `android_optional_gpu_tests_rel` bot did not exist yet and you
wanted to add it. The process is similar to adding a CI bot, but modifying
slightly different files.

1.  Allocate new virtual machines for the bots as described in
    [How to set up new virtual machine instances](#How-to-set-up-new-virtual-machine-instances).
1.  Make sure there is enough hardware capacity using the available tools to
    report utilization of the Swarming pool.
1.  Create a CL in the Chromium workspace the does the following.
    1.  Add your new bot `android_optional_gpu_tests_rel` to the
        tryserver.chromium.android waterfall in
        [`waterfalls.pyl`][waterfalls.pyl].
        [Here][android_optional_waterfalls_pyl] is an explicit example using the
        real bot.
    1.  Re-run
        [`src/testing/buildbot/generate_buildbot_json.py`][generate_buildbot_json.py]
        to regenerate the JSON files.
    1.  Add the bot definition to the relevant tryserver `.star` file. In this
        example, that would be `tryserver.chromium.android.star`.
        [Here][android_optional_tryserver_star] is an explicit example using the
        real bot.
    1.  Run `main.star` in [`src/infra/config`][src/infra/config] to update the
        generated files: [`luci-milo.cfg`][luci-milo.cfg],
        [`luci-scheduler.cfg`][luci-scheduler.cfg],
        [`cr-buildbucket.cfg`][cr-buildbucket.cfg]. Double-check your work
        there.
    1.  Update [`src/tools/mb/mb_config.pyl`][mb_config.pyl]
        to include `android_optional_gpu_tests_rel`.
1.  After your CL lands you should be able to find and run
    `android_optional_gpu_tests_rel` on CLs using Choose Trybots in Gerrit.

[android_optional_waterfalls_pyl]: https://chromium.googlesource.com/chromium/src/+/024e15a6bb8b2e74ba3a5782831be6a1c11ddf43/testing/buildbot/waterfalls.pyl#6665
[android_optional_tryserver_star]: https://chromium.googlesource.com/chromium/src/+/b05a42bd3d0e84d55392ae984a69946e56203c71/infra/config/subprojects/chromium/try/tryserver.chromium.android.star#703


### How to test and deploy a driver and/or OS update

Let's say that you want to roll out an update to the graphics drivers or the OS
on one of the configurations like the Linux NVIDIA bots. In order to verify
that the new driver or OS won't destabilize Chromium's commit queue,
it's necessary to run the new driver or OS on one of the waterfalls for a day
or two to make sure the tests are reliably green before rolling out the driver
or OS update. To do this:

1.  Make sure that all of the current Swarming jobs for this OS and GPU
    configuration are targeted at the "stable" version of the driver and the OS
    in [`waterfalls.pyl`][waterfalls.pyl] and [`mixins.pyl`][mixins.pyl].
1.  File a `Build Infrastructure` bug, component `Infra>Labs`, to have ~4 of
    the physical machines already in the Swarming pool upgraded to the new
    version of the driver or the OS.
1.  If an "experimental" version of this bot doesn't yet exist, follow the
    instructions above for [How to add a new tester bot to the chromium.gpu.fyi
    waterfall](#How-to-add-a-new-tester-bot-to-the-chromium_gpu_fyi-waterfall)
    to deploy one. However, you do not need to request additional GCE resources
    since there should be enough spare capacity in the GPU builderless pool to
    handle them. Additionally, ensure that the bot definition in
    [`chromium.gpu.fyi.star`][chromium.gpu.fyi.star] includes a `list_view`
    argument specifying `chromium.gpu.experimental`.
1.  If an "experimental" version does already exist, re-add it to its default
    console in [`chromium.gpu.fyi.star`][chromium.gpu.fyi.star] by uncommenting
    its `console_view_entry` argument and unpause it in the [luci scheduler].
1.  Have this experimental bot target the new version of the driver or the OS
    in [`waterfalls.pyl`][waterfalls.pyl] and [`mixins.pyl`][mixins.pyl].
    [Sample CL][sample driver cl].
1.  Hopefully, the new machine will pass the pixel tests. If it doesn't, then
    it'll be necessary to follow the instructions on
    [updating Gold baselines (step #4)][updating gold baselines].
1.  Watch the new machine for a day or two to make sure it's stable.
1.  When it is, add the experimental driver/OS to the `_stable` mixin using the
    swarming OR operator `|`. For example:

    ```
    'win10_intel_hd_630_stable': {
      'swarming': {
        'dimensions': {
          'gpu': '8086:5912-26.20.100.7870|8086:5912-26.20.100.8141',
          'os': 'Windows-10',
          'pool': 'chromium.tests.gpu',
        },
      },
    }
    ```

    This will cause tests triggered using the `_stable` mixin to run on either
    the old stable dimension or the experimental/new stable dimension.

    **NOTE** There is a hard cap of 8 combinations in swarming, so you can only
    use the OR operator in up to 3 dimensions if each dimension only has two
    options. More than two options per dimension is allowed as long as the total
    number of combinations is 8 or less.
1.  After it lands, ask the Chrome Infrastructure Labs team to roll out the
    driver update across all of the similarly configured bots in the swarming
    pool.
1.  If necessary, update pixel test expectations and remove the suppressions
    added above.
1.  Remove the old driver or OS version from the `_stable` mixin, leaving just
    the new stable version.
1.  Clean up the "experimental" version of the bot by pausing it in the
    [luci scheduler] and commenting out its `console_view_entry` argument in
    [`chromium.gpu.fyi.star`][chromium.gpu.fyi.star].

Note that we leave the experimental bot in place. We could reclaim it, but it
seems worthwhile to continuously test the "next" version of graphics drivers as
well as the current stable ones.

[sample driver cl]: https://chromium-review.googlesource.com/c/chromium/src/+/1726875
[updating gold baselines]: http://go/gpu-pixel-wrangler-info#how-to-keep-the-bots-green
[luci scheduler]: https://luci-scheduler.appspot.com/

## Credentials for various servers

Working with the GPU bots requires credentials to various services: the isolate
server, the swarming server, and cloud storage.

### Isolate server credentials

To upload and download isolates you must first authenticate to the isolate
server. From a Chromium checkout, run:

*   `./src/tools/luci-go/isolate login`

This will open a web browser to complete the authentication flow. A @google.com
email address is required in order to properly authenticate.

To test your authentication, find a hash for a recent isolate. Consult the
instructions on [Running Binaries from the Bots Locally] to find a random hash
from a target like `gl_tests`. Then run the following:

[Running Binaries from the Bots Locally]: https://www.chromium.org/developers/testing/gpu-testing#TOC-Running-Binaries-from-the-Bots-Locally

### Swarming server credentials

The swarming server uses the same `auth.py` script as the isolate server. You
will need to authenticate if you want to manually download the results of
previous swarming jobs, trigger your own jobs, or run `swarming.py reproduce`
to re-run a remote job on your local workstation. Follow the instructions
above, replacing the service with `https://chromium-swarm.appspot.com`.

### Cloud storage credentials

Authentication to Google Cloud Storage is needed for a couple of reasons:
uploading pixel test results to the cloud, and potentially uploading and
downloading builds as well, at least in Debug mode. Use the copy of gsutil in
`depot_tools/third_party/gsutil/gsutil`, and follow the [Google Cloud Storage
instructions] to authenticate. You must use your @google.com email address and
be a member of the Chrome GPU team in order to receive read-write access to the
appropriate cloud storage buckets. Roughly:

1.  Run `gsutil config`
2.  Copy/paste the URL into your browser
3.  Log in with your @google.com account
4.  Allow the app to access the information it requests
5.  Copy-paste the resulting key back into your Terminal
6.  Press "enter" when prompted for a project-id (i.e., leave it empty)

At this point you should be able to write to the cloud storage bucket.

Navigate to
<https://console.developers.google.com/storage/chromium-gpu-archive> to view
the contents of the cloud storage bucket.

[Google Cloud Storage instructions]: https://developers.google.com/storage/docs/gsutil
