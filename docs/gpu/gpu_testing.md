# GPU Testing

This set of pages documents the setup and operation of the GPU bots and try
servers, which verify the correctness of Chrome's graphically accelerated
rendering pipeline.

[TOC]

## Overview

The GPU bots run a different set of tests than the majority of the Chromium
test machines. The GPU bots specifically focus on tests which exercise the
graphics processor, and whose results are likely to vary between graphics card
vendors.

Most of the tests on the GPU bots are run via the [Telemetry framework].
Telemetry was originally conceived as a performance testing framework, but has
proven valuable for correctness testing as well. Telemetry directs the browser
to perform various operations, like page navigation and test execution, from
external scripts written in Python. The GPU bots launch the full Chromium
browser via Telemetry for the majority of the tests. Using the full browser to
execute tests, rather than smaller test harnesses, has yielded several
advantages: testing what is shipped, improved reliability, and improved
performance.

[Telemetry framework]: https://github.com/catapult-project/catapult/tree/master/telemetry

A subset of the tests, called "pixel tests", grab screen snapshots of the web
page in order to validate Chromium's rendering architecture end-to-end. Where
necessary, GPU-specific results are maintained for these tests. Some of these
tests verify just a few pixels, using handwritten code, in order to use the
same validation for all brands of GPUs.

The GPU bots use the Chrome infrastructure team's [recipe framework], and
specifically the [`chromium`][recipes/chromium] and
[`chromium_trybot`][recipes/chromium_trybot] recipes, to describe what tests to
execute. Compared to the legacy master-side buildbot scripts, recipes make it
easy to add new steps to the bots, change the bots' configuration, and run the
tests locally in the same way that they are run on the bots. Additionally, the
`chromium` and `chromium_trybot` recipes make it possible to send try jobs which
add new steps to the bots. This single capability is a huge step forward from
the previous configuration where new steps were added blindly, and could cause
failures on the tryservers. For more details about the configuration of the
bots, see the [GPU bot details].

[recipe framework]: https://chromium.googlesource.com/external/github.com/luci/recipes-py/+/main/doc/user_guide.md
[recipes/chromium]:        https://chromium.googlesource.com/chromium/tools/build/+/main/scripts/slave/recipes/chromium.py
[recipes/chromium_trybot]: https://chromium.googlesource.com/chromium/tools/build/+/main/scripts/slave/recipes/chromium_trybot.py
[GPU bot details]: gpu_testing_bot_details.md

The physical hardware for the GPU bots lives in the Swarming pool\*. The
Swarming infrastructure ([new docs][new-testing-infra], [older but currently
more complete docs][isolated-testing-infra]) provides many benefits:

*   Increased parallelism for the tests; all steps for a given tryjob or
    waterfall build run in parallel.
*   Simpler scaling: just add more hardware in order to get more capacity. No
    manual configuration or distribution of hardware needed.
*   Easier to run certain tests only on certain operating systems or types of
    GPUs.
*   Easier to add new operating systems or types of GPUs.
*   Clearer description of the binary and data dependencies of the tests. If
    they run successfully locally, they'll run successfully on the bots.

(\* All but a few one-off GPU bots are in the swarming pool. The exceptions to
the rule are described in the [GPU bot details].)

The bots on the [chromium.gpu.fyi] waterfall are configured to always test
top-of-tree ANGLE. This setup is done with a few lines of code in the
[tools/build workspace]; search the code for "angle".

These aspects of the bots are described in more detail below, and in linked
pages. There is a [presentation][bots-presentation] which gives a brief
overview of this documentation and links back to various portions.

<!-- XXX: broken link -->
[new-testing-infra]: https://github.com/luci/luci-py/wiki
[isolated-testing-infra]: https://www.chromium.org/developers/testing/isolated-testing/infrastructure
[chromium.gpu]: https://ci.chromium.org/p/chromium/g/chromium.gpu/console
[chromium.gpu.fyi]: https://ci.chromium.org/p/chromium/g/chromium.gpu.fyi/console
[tools/build workspace]: https://source.chromium.org/chromium/chromium/tools/build/+/HEAD:recipes/recipe_modules/chromium_tests/builders/chromium_gpu_fyi.py
[bots-presentation]: https://docs.google.com/presentation/d/1BC6T7pndSqPFnituR7ceG7fMY7WaGqYHhx5i9ECa8EI/edit?usp=sharing

## Fleet Status

Please see the [GPU Pixel Wrangling instructions] for links to dashboards
showing the status of various bots in the GPU fleet.

[GPU Pixel Wrangling instructions]: http://go/gpu-pixel-wrangler#fleet-status

## Using the GPU Bots

Most Chromium developers interact with the GPU bots in two ways:

1.  Observing the bots on the waterfalls.
2.  Sending try jobs to them.

The GPU bots are grouped on the [chromium.gpu] and [chromium.gpu.fyi]
waterfalls. Their current status can be easily observed there.

To send try jobs, you must first upload your CL to the codereview server. Then,
either clicking the "CQ dry run" link or running from the command line:

```sh
git cl try
```

Sends your job to the default set of try servers.

The GPU tests are part of the default set for Chromium CLs, and are run as part
of the following tryservers' jobs:

*   [linux-rel], formerly on the `tryserver.chromium.linux` waterfall
*   [mac-rel], formerly on the `tryserver.chromium.mac` waterfall
*   [win-rel], formerly on the `tryserver.chromium.win` waterfall

[linux-rel]: https://ci.chromium.org/p/chromium/builders/luci.chromium.try/linux-rel?limit=100
[mac-rel]:   https://ci.chromium.org/p/chromium/builders/luci.chromium.try/mac-rel?limit=100
[win-rel]:   https://ci.chromium.org/p/chromium/builders/luci.chromium.try/win-rel?limit=100

Scan down through the steps looking for the text "GPU"; that identifies those
tests run on the GPU bots. For each test the "trigger" step can be ignored; the
step further down for the test of the same name contains the results.

It's usually not necessary to explicitly send try jobs just for verifying GPU
tests. If you want to, you must invoke "git cl try" separately for each
tryserver master you want to reference, for example:

```sh
git cl try -b linux-rel
git cl try -b mac-rel
git cl try -b win7-rel
```

Alternatively, the Gerrit UI can be used to send a patch set to these try
servers.

Three optional tryservers are also available which run additional tests. As of
this writing, they ran longer-running tests that can't run against all Chromium
CLs due to lack of hardware capacity. They are added as part of the included
tryservers for code changes to certain sub-directories.

*   [linux_optional_gpu_tests_rel] on the [luci.chromium.try] waterfall
*   [mac_optional_gpu_tests_rel]   on the [luci.chromium.try]   waterfall
*   [win_optional_gpu_tests_rel]   on the [luci.chromium.try]   waterfall

[linux_optional_gpu_tests_rel]: https://ci.chromium.org/p/chromium/builders/luci.chromium.try/linux_optional_gpu_tests_rel
[mac_optional_gpu_tests_rel]:   https://ci.chromium.org/p/chromium/builders/luci.chromium.try/mac_optional_gpu_tests_rel
[win_optional_gpu_tests_rel]:   https://ci.chromium.org/p/chromium/builders/luci.chromium.try/win_optional_gpu_tests_rel
[luci.chromium.try]:            https://ci.chromium.org/p/chromium/g/luci.chromium.try/builders

Tryservers for the [ANGLE project] are also present on the
[tryserver.chromium.angle] waterfall. These are invoked from the Gerrit user
interface. They are configured similarly to the tryservers for regular Chromium
patches, and run the same tests that are run on the [chromium.gpu.fyi]
waterfall, in the same way (e.g., against ToT ANGLE).

If you find it necessary to try patches against other sub-repositories than
Chromium (`src/`) and ANGLE (`src/third_party/angle/`), please
[file a bug](http://crbug.com/new) with component Internals\>GPU\>Testing.

[ANGLE project]: https://chromium.googlesource.com/angle/angle/+/main/README.md
[tryserver.chromium.angle]: https://build.chromium.org/p/tryserver.chromium.angle/waterfall
[file a bug]: http://crbug.com/new

## Running the GPU Tests Locally

All of the GPU tests running on the bots can be run locally from a Chromium
build. Many of the tests are simple executables:

*   `angle_unittests`
*   `gl_tests`
*   `gl_unittests`
*   `tab_capture_end2end_tests`

Some run only on the chromium.gpu.fyi waterfall, either because there isn't
enough machine capacity at the moment, or because they're closed-source tests
which aren't allowed to run on the regular Chromium waterfalls:

*   `angle_deqp_gles2_tests`
*   `angle_deqp_gles3_tests`
*   `angle_end2end_tests`

The remaining GPU tests are run via Telemetry.  In order to run them, just
build the `telemetry_gpu_integration_test` target (or
`telemetry_gpu_integration_test_android_chrome` for Android) and then
invoke `src/content/test/gpu/run_gpu_integration_test.py` with the appropriate
argument. The tests this script can invoke are
in `src/content/test/gpu/gpu_tests/`. For example:

*   `run_gpu_integration_test.py context_lost --browser=release`
*   `run_gpu_integration_test.py webgl1_conformance --browser=release`
*   `run_gpu_integration_test.py webgl2_conformance --browser=release --webgl-conformance-version=2.0.1`
*   `run_gpu_integration_test.py maps --browser=release`
*   `run_gpu_integration_test.py screenshot_sync --browser=release`
*   `run_gpu_integration_test.py trace_test --browser=release`

The pixel tests are a bit special. See
[the section on running them locally](#Running-the-pixel-tests-locally) for
details.

The `--browser=release` argument can be changed to `--browser=debug` if you
built in a directory such as `out/Debug`. If you built in some non-standard
directory such as `out/my_special_gn_config`, you can instead specify
`--browser=exact --browser-executable=out/my_special_gn_config/chrome`.

If you're testing on Android, use `--browser=android-chromium` instead of
`--browser=release/debug` to invoke it. Additionally, Telemetry will likely
complain about being unable to find the browser binary on Android if you build
in a non-standard output directory. Thus, `out/Release` or `out/Debug` are
suggested when testing on Android.

If you are running on a platform that does not support multiple browser
instances at a time (Android or ChromeOS), it is also recommended that you pass
in `--jobs=1`. This only has an effect on test suites that have parallel test
support, but failure to pass in the argument for those tests on these platforms
will result in weird failures due to multiple test processes stepping on each
other. On other platforms, you are still free to specify `--jobs` to get more
or less parallelization instead of relying on the default of one test process
per logical core.

**Note:** The tests require some third-party Python packages. Obtaining these
packages is handled automatically by `vpython3`, and the script's shebang should
use vpython if running the script directly. Since shebangs are not used on
Windows, you will need to manually specify the executable if you are on a
Windows machine. If you're used to invoking `python3` to run a script, simply
use `vpython3` instead, e.g. `vpython3 run_gpu_integration_test.py ...`.

You can run a subset of tests with this harness:

*   `run_gpu_integration_test.py webgl1_conformance --browser=release
    --test-filter=conformance_attribs`

The exact command used to invoke the test on the bots can be found in one of
two ways:

1. Looking at the [json.input][trigger_input] of the trigger step under
   `requests[task_slices][command]`. The arguments after the last `--` are
   used to actually run the test.
1. Looking at the top of a [swarming task][sample_swarming_task].

In both cases, the following can be omitted when running locally since they're
only necessary on swarming:
* `testing/test_env.py`
* `testing/scripts/run_gpu_integration_test_as_googletest.py`
* `--isolated-script-test-output`
* `--isolated-script-test-perf-output`


[trigger_input]: https://logs.chromium.org/logs/chromium/buildbucket/cr-buildbucket.appspot.com/8849851608240828544/+/u/test_pre_run__14_/l_trigger__webgl2_conformance_d3d11_passthrough_tests_on_NVIDIA_GPU_on_Windows_on_Windows-10-18363/json.input
[sample_swarming_task]: https://chromium-swarm.appspot.com/task?id=52f06058bfb31b10

The Maps test requires you to authenticate to cloud storage in order to access
the Web Page Reply archive containing the test. See [Cloud Storage Credentials]
for documentation on setting this up.

[Cloud Storage Credentials]: gpu_testing_bot_details.md#Cloud-storage-credentials

### Bisecting ChromeOS Failures Locally

Failures that occur on the ChromeOS amd64-generic configuration are easy to
reproduce due to the VM being readily available for use, but doing so requires
some additional steps to the bisect process. The following are steps that can be
followed using two terminals and the [Simple Chrome SDK] to bisect a ChromeOS
failure.

1. Terminal 1: Start the bisect as normal `git bisect start`
   `git bisect good <good_revision>` `git bisect bad <bad_revision>`
1. Terminal 1: Sync to the revision that git spits out
   `gclient sync -r src@<revision>`
1. Terminal 2: Enter the Simple Chrome SDK
   `cros chrome-sdk --board amd64-generic-vm --log-level info --download-vm --clear-sdk-cache`
1. Terminal 2: Compile the relevant target (probably the GPU integration tests)
   `autoninja -C out_amd64-generic-vm/Release/ telemetry_gpu_integration_test`
1. Terminal 2: Start the VM `cros_vm --start`
1. Terminal 2: Deploy the Chrome binary to the VM
   `deploy_chrome --build-dir out_amd64-generic-vm/Release/ --device 127.0.0.1:9222`
   This will require you to accept a prompt twice, once because of a board
   mismatch and once because the VM still has rootfs verification enabled.
1. Terminal 1: Run your test on the VM. For GPU integration tests, this involves
   specifying `--browser cros-chrome --remote 127.0.0.1 --remote-ssh-port 9222`
1. Terminal 2: After determining whether the revision is good or bad, shut down
   the VM `cros_vm --stop`
1. Terminal 2: Exit the SKD `exit`
1. Terminal 1: Let git know whether the revision was good or bad
   `git bisect good`/`git bisect bad`
1. Repeat from step 2 with the new revision git spits out.

The repeated entry/exit from the SDK between revisions is to ensure that the
VM image is in sync with the Chromium revision, as it is possible for
regressions to be caused by an update to the image itself rather than a Chromium
change.

[Simple Chrome SDK]: https://chromium.googlesource.com/chromiumos/docs/+/HEAD/simple_chrome_workflow.md

### Telemetry Test Suites
The Telemetry-based tests are all technically the same target,
`telemetry_gpu_integration_test`, just run with different runtime arguments. The
first positional argument passed determines which suite will run, and additional
runtime arguments may cause the step name to change on the bots. Here is a list
of all suites and resulting step names as of April 15th 2021:

* `context_lost`
  * `context_lost_passthrough_tests`
  * `context_lost_tests`
  * `context_lost_validating_tests`
* `hardware_accelerated_feature`
  * `hardware_accelerated_feature_tests`
* `gpu_process`
  * `gpu_process_launch_tests`
* `info_collection`
  * `info_collection_tests`
* `maps`
  * `maps_pixel_passthrough_test`
  * `maps_pixel_test`
  * `maps_pixel_validating_test`
  * `maps_tests`
* `pixel`
  * `android_webview_pixel_skia_gold_test`
  * `egl_pixel_skia_gold_test`
  * `pixel_skia_gold_passthrough_test`
  * `pixel_skia_gold_validating_test`
  * `pixel_tests`
  * `vulkan_pixel_skia_gold_test`
* `power`
  * `power_measurement_test`
* `screenshot_sync`
  * `screenshot_sync_passthrough_tests`
  * `screenshot_sync_tests`
  * `screenshot_sync_validating_tests`
* `trace_test`
  * `trace_test`
* `webgl_conformance`
  * `webgl2_conformance_d3d11_passthrough_tests`
  * `webgl2_conformance_gl_passthrough_tests`
  * `webgl2_conformance_gles_passthrough_tests`
  * `webgl2_conformance_tests`
  * `webgl2_conformance_validating_tests`
  * `webgl_conformance_d3d11_passthrough_tests`
  * `webgl_conformance_d3d9_passthrough_tests`
  * `webgl_conformance_fast_call_tests`
  * `webgl_conformance_gl_passthrough_tests`
  * `webgl_conformance_gles_passthrough_tests`
  * `webgl_conformance_metal_passthrough_tests`
  * `webgl_conformance_swangle_passthrough_tests`
  * `webgl_conformance_tests`
  * `webgl_conformance_validating_tests`
  * `webgl_conformance_vulkan_passthrough_tests`

### Running the pixel tests locally

The pixel tests are a special case because they use an external Skia service
called Gold to handle image approval and storage. See
[GPU Pixel Testing With Gold] for specifics.

[GPU Pixel Testing With Gold]: gpu_pixel_testing_with_gold.md

TL;DR is that the pixel tests use a binary called `goldctl` to download and
upload data when running pixel tests.

Normally, `goldctl` uploads images and image metadata to the Gold server when
used. This is not desirable when running locally for a couple reasons:

1. Uploading requires the user to be whitelisted on the server, and whitelisting
everyone who wants to run the tests locally is not a viable solution.
2. Images produced during local runs are usually slightly different from those
that are produced on the bots due to hardware/software differences. Thus, most
images uploaded to Gold from local runs would likely only ever actually be used
by tests run on the machine that initially generated those images, which just
adds noise to the list of approved images.

Additionally, the tests normally rely on the Gold server for viewing images
produced by a test run. This does not work if the data is not actually uploaded.

The pixel tests contain logic to automatically determine whether they are
running on a workstation or not, as well as to determine what git revision is
being tested. This *should* mean that the pixel tests will automatically work
when run locally. However, if the local run detection code fails for some
reason, you can manually pass some flags to force the same behavior:

In order to get around the local run issues, simply pass the
`--local-pixel-tests` flag to the tests. This will disable uploading, but
otherwise go through the same steps as a test normally would. Each test will
also print out `file://` URLs to the produced image, the closest image for the
test known to Gold, and the diff between the two.

Because the image produced by the test locally is likely slightly different from
any of the approved images in Gold, local test runs are likely to fail during
the comparison step. In order to cut down on the amount of noise, you can also
pass the `--no-skia-gold-failure` flag to not fail the test on a failed image
comparison. When using `--no-skia-gold-failure`, you'll also need to pass the
`--passthrough` flag in order to actually see the link output.

Example usage:
`run_gpu_integration_test.py pixel --no-skia-gold-failure --local-pixel-tests
--passthrough`

If, for some reason, the local run code is unable to determine what the git
revision is, simply pass `--git-revision aabbccdd`. Note that `aabbccdd` must
be replaced with an actual Chromium src revision (typically whatever revision
origin/main is currently synced to) in order for the tests to work. This can
be done automatically using:
``run_gpu_integration_test.py pixel --no-skia-gold-failure --local-pixel-tests
--passthrough --git-revision `git rev-parse origin/main` ``

## Running Binaries from the Bots Locally

Any binary run remotely on a bot can also be run locally, assuming the local
machine loosely matches the architecture and OS of the bot.

The easiest way to do this is to find the ID of the swarming task and use
"swarming.py reproduce" to re-run it:

*   `./src/tools/luci-go/swarming reproduce -S https://chromium-swarm.appspot.com [task ID]`

The task ID can be found in the stdio for the "trigger" step for the test. For
example, look at a recent build from the [Mac Release (Intel)] bot, and
look at the `gl_unittests` step. You will see something like:

[Mac Release (Intel)]: https://ci.chromium.org/p/chromium/builders/luci.chromium.ci/Mac%20Release%20%28Intel%29/

```
Triggered task: gl_unittests on Intel GPU on Mac/Mac-10.12.6/[TRUNCATED_ISOLATE_HASH]/Mac Release (Intel)/83664
To collect results, use:
  swarming.py collect -S https://chromium-swarm.appspot.com --json /var/folders/[PATH_TO_TEMP_FILE].json
Or visit:
  https://chromium-swarm.appspot.com/user/task/[TASK_ID]
```

There is a difference between the isolate's hash and Swarming's task ID. Make
sure you use the task ID and not the isolate's hash.

As of this writing, there seems to be a
[bug](https://github.com/luci/luci-py/issues/250)
when attempting to re-run the Telemetry based GPU tests in this way. For the
time being, this can be worked around by instead downloading the contents of
the isolate. To do so, look into the "Reproducing the task locally" section on
a swarming task, which contains something like:

```
Download inputs files into directory foo:
# (if needed, use "\${platform}" as-is) cipd install "infra/tools/luci/cas/\${platform}" -root bar
# (if needed) ./bar/cas login
./bar/cas download -cas-instance projects/chromium-swarm/instances/default_instance -digest 68ae1d6b22673b0ab7b4427ca1fc2a4761c9ee53474105b9076a23a67e97a18a/647 -dir foo
```

Before attempting to download an isolate, you must ensure you have permission
to access the isolate server. Full instructions can be [found
here][isolate-server-credentials]. For most cases, you can simply run:

*   `./src/tools/luci-go/isolate login`

The above link requires that you log in with your @google.com credentials. It's
not known at the present time whether this works with @chromium.org accounts.
Email kbr@ if you try this and find it doesn't work.

[isolate-server-credentials]: gpu_testing_bot_details.md#Isolate-server-credentials

## Debugging a Specific Subset of Tests on a Specific GPU Bot

When a test exhibits flake on the bots, it can be convenient to run it
repeatedly with local code modifications on the bot where it is exhibiting
flake. One way of doing this is via swarming (see the below section). However, a
lower-overhead alternative that also works in the case where you are looking to
run on a bot for which you cannot locally build is to locally alter the
configuration of the bot in question to specify that it should run only the
tests desired, repeating as many times as desired. Instructions for doing this
are as follows (see the [example CL] for a concrete instantiation of these
instructions):

1. In testsuite_exceptions.pyl, find the section for the test suite in question
   (creating it if it doesn't exist).
2. Add modifications for the bot in question and specify arguments such that
   your desired tests are run for the desired number of iterations.
3. Run testing/buildbot/generate_buildbot_json.py and verify that the JSON file
   for the bot in question was modified as you would expect.
4. Upload and run tryjobs on that specific bot via "Choose Tryjobs."
5. Examine the test results. (You can verify that the tests run were as you
   expected by examining the test results for individual shards of the run
   of the test suite in question.)
6. Add logging/code modifications/etc as desired and go back to step 4,
   repeating the process until you've uncovered the underlying issue.
7. Remove the the changes to testsuite_exceptions.pyl and the JSON file if
   turning the CL into one intended for submission!

Here is an [example CL] that does this.

[example CL]: https://chromium-review.googlesource.com/c/chromium/src/+/3898592/4

## Running Locally Built Binaries on the GPU Bots

The easiest way to run a locally built test on swarming is the `tools/mb/mb.py`
wrapper. This handles compilation (if necessary), uploading, and task triggering
with a single command.

In order to use this, you will need:

* An output directory set up with the correct GN args you want to use.
  `out/Release` will be assumed for examples.
* The dimensions for the type of machine you want to test on. This can be
  grabbed from an existing swarming task, assuming you are trying to reproduce
  an issue that has occurred on the bots. These can be found in the `Dimensions`
  field just above the `CAS Inputs` field near the top of the swarming task's
  page.
* The arguments you want to run the test with. These can usually be taken
  directly from the swarming task, printed out after `Command:` near the top of
  the task output.

The general format for an `mb.py` command is:

```
tools/mb/mb.py run -s --no-default-dimensions \
-d dimension_key1 dimension_value1 -d dimension_key2 dimension_value2 ... \
out/Release target_name \
--
test_arg_1 test_arg_2 ...
```

**Note:** The test is executed from within the output directory, so any
relative paths passed in as test arguments need to be specified relative to
that. This generally means prefixing paths with `../../` to get back to the
Chromium src directory.

The command will compile all necessary targets, upload the necessary files to
CAS, and trigger a test task using the specified dimensions and test args. Once
triggered, a swarming task URL will be printed that you can look at and the
script will hang until it is complete. At this point, it is safe to kill the
script, as the task has already been queued.

### Concrete Example

Say we wanted to reproduce an issue happening on a Linux NVIDIA machine in the
WebGL 1 conformance tests. The dimensions for the failed task are:

```
gpu: NVIDIA GeForce GTX 1660 (10de:2184-440.100)
os: Ubuntu-18.04.5|Ubuntu-18.04.6
cpu: x86-64
pool: chromium.tests.gpu
```

and the command from the swarming task is:

```
Additional test environment:
    CHROME_HEADLESS=1
    GTEST_SHARD_INDEX=0
    GTEST_TOTAL_SHARDS=2
    LANG=en_US.UTF-8
Command: /b/s/w/ir/.task_template_vpython_cache/vpython/store/python_venv-rrcc1h3jcjhkvqtqf5p39mhf78/contents/bin/python3 \
  ../../testing/scripts/run_gpu_integration_test_as_googletest.py \
  ../../content/test/gpu/run_gpu_integration_test.py \
  --isolated-script-test-output=/b/s/w/io83bc1749/output.json \
  --isolated-script-test-perf-output=/b/s/w/io83bc1749/perftest-output.json \
  webgl1_conformance --show-stdout --browser=release --passthrough -v \
  --stable-jobs \
  --extra-browser-args=--enable-logging=stderr --js-flags=--expose-gc --use-gl=angle --use-angle=gl --use-cmd-decoder=passthrough --force_high_performance_gpu \
  --read-abbreviated-json-results-from=../../content/test/data/gpu/webgl1_conformance_linux_runtimes.json \
  --jobs=4
```

The resulting `mb.py` command to run an equivalent task with a locally built
binary would be:

```
tools/mb/mb.py run -s --no-default-dimensions \
  -d gpu 10de:2184-440.100 \
  -d os Ubuntu-18.04.5|Ubuntu-18.04.6 \
  -d cpu x86-64 \
  -d pool chromium.tests.gpu \
  out/Release telemetry_gpu_integration_test \
  -- \
  --isolated-script-test-output '${ISOLATED_OUTDIR}/output.json' \
  webgl1_conformance --show-stdout --browser=release --passthrough -v \
  --stable-jobs \
  --extra-browser-args="--enable-logging=stderr --js-flags=--expose-gc --use-gl=angle --use-angle=gl --use-cmd-decoder=passthrough --force_high_performance_gpu" \
  --read-abbreviated-json-results-from=../../content/test/data/gpu/webgl1_conformance_linux_runtimes.json \
  --jobs=4 \
  --total-shards=2 --shard-index=0
```

Here is a breakdown of what each component does and where it comes from:

* `run -s` - Tells `mb.py` to run a test target on swarming (as opposed to
  locally)
* `--no-default-dimensions` - `mb.py` by default assumes the dimensions for
  Linux GCEs that Chromium commonly uses for testing. Passing this in prevents
  those dimensions from being auto-added.
* `-d gpu 10de:2184-440.100` - Specifies the GPU model and driver version to
  target. This is pulled directly from the `gpu` dimension of the task. Note
  that the actual dimension starts with the PCI-e vendor ID - the human-readable
  string (`NVIDIA GeForce GTX 1660`) is just provided for ease-of-use within the
  swarming UI.
* `-d os Ubuntu-18.04.5|Ubuntu-18.04.6` - Specifies the OS to target. Pulled
  directly from the `os` dimension of the task. The use of `|` means that either
  specified OS version is acceptable.
* `-d cpu x86-64` - Specifies the CPU architecture in case there are other types
  such as ARM. Pulled directly from the `cpu` dimension of the task.
* `-d pool chromium.tests.gpu` - Specifies the hardware pool to use. Pulled
  directly from the `pool` dimension of the task. Most GPU machines are in
  `chromium.tests.gpu`, but some configurations are in `chromium.tests` due to
  sharing capacity with the rest of Chromium.
* `out/Release` - Specifies the output directory to use. Can usually be changed
  to whatever output directory you want to use, but this can have an effect on
  which args you need to pass to the test.
* `telemetry_gpu_integration_test` - Specifies the GN target to build.
* `--` - Separates arguments meant for `mb.py` from test arguments.
* `--isolated-script-test-output '${ISOLATED_OUTDIR}/output.json'` - Taken from
  the same argument from the swarming task, but with `${ISOLATED_OUTDIR}` used
  instead of a specific directory since it is random for every task. Note that
  single quotes are necessary on UNIX-style platforms to avoid having it
  evaluated on your local machine. The similar
  `--isolated-script-test-perf-output` argument present in the swarming test
  command can be omitted since its presence is just due to some legacy behavior.
* `webgl1_conformance` - Specifies the test suite to run. Taken directly from
  the swarming task.
* `--show-stdout --passthrough -v --stable-jobs` - Boilerplate arguments taken
  directly from the swarming task.
* `--browser=release` - Specifies the browser to use, which is related to the
  name of the output directory. `release` and `debug` will automatically map to
  `out/Release` and `out/Debug`, but other values would require the use of
  `--browser=exact` and `--browser-executable=path/to/browser`. This should end
  up being either `./chrome` or `.\chrome.exe` for Linux and Windows,
  respectively, since the path should be relative to the output directory.
* `--extra-browser-args="..."` - Extra arguments to pass to Chrome when running
  the tests. Taken directly from the swarming task, but double or single quotes
  are necessary in order to have the space-separated values grouped together.
* `--read-abbreviated-json-results-from=...` - Taken directly from the swarming
  task. Affects test sharding behavior, so only necessary if reproducing a
  specific shard (covered later), but does not negatively impact anything if
  unnecessarily passed in.
* `--jobs=4` - Taken directly from the swarming task. Affects how many tests are
  run in parallel.
* `--total-shards=2 --shard-index=0` - Taken from the environment variables of
  the swarming task. This will cause only the tests that ran on the particular
  shard to run instead of all tests from the suite. If specifying these, it is
  important to also specify `--read-abbreviated-json-results-from` if it is
  present in the original command, as otherwise the tests that are run will
  differ from the original swarming task. A possible alternative to this would
  be explicitly specify the tests you want to run using the appropriate argument
  for the target, in this case `--test-filter`.

## Moving Test Binaries from Machine to Machine

To create a zip archive of your personal Chromium build plus all of
the Telemetry-based GPU tests' dependencies, which you can then move
to another machine for testing:

1. Build Chrome (into `out/Release` in this example).
1. `vpython3 tools/mb/mb.py zip out/Release/ telemetry_gpu_integration_test out/telemetry_gpu_integration_test.zip`

Then copy telemetry_gpu_integration_test.zip to another machine. Unzip
it, and cd into the resulting directory. Invoke
`content/test/gpu/run_gpu_integration_test.py` as above.

This workflow has been tested successfully on Windows with a
statically-linked Release build of Chrome.

Note: on one macOS machine, this command failed because of a broken
`strip-json-comments` symlink in
`src/third_party/catapult/common/node_runner/node_runner/node_modules/.bin`. Deleting
that symlink allowed it to proceed.

Note also: on the same macOS machine, with a component build, this
command failed to zip up a working Chromium binary. The browser failed
to start with the following error:

`[0626/180440.571670:FATAL:chrome_main_delegate.cc(1057)] Check failed: service_manifest_data_pack_.`

In a pinch, this command could be used to bundle up everything, but
the "out" directory could be deleted from the resulting zip archive,
and the Chromium binaries moved over to the target machine. Then the
command line arguments `--browser=exact --browser-executable=[path]`
can be used to launch that specific browser.

See the [user guide for mb](../../tools/mb/docs/user_guide.md#mb-zip), the
meta-build system, for more details.

## Adding New Tests to the GPU Bots

The goal of the GPU bots is to avoid regressions in Chrome's rendering stack.
To that end, let's add as many tests as possible that will help catch
regressions in the product. If you see a crazy bug in Chrome's rendering which
would be easy to catch with a pixel test running in Chrome and hard to catch in
any of the other test harnesses, please, invest the time to add a test!

There are a couple of different ways to add new tests to the bots:

1.  Adding a new test to one of the existing harnesses.
2.  Adding an entire new test step to the bots.

### Adding a new test to one of the existing test harnesses

Adding new tests to the GTest-based harnesses is straightforward and
essentially requires no explanation.

As of this writing it isn't as easy as desired to add a new test to one of the
Telemetry based harnesses. See [Issue 352807](http://crbug.com/352807). Let's
collectively work to address that issue. It would be great to reduce the number
of steps on the GPU bots, or at least to avoid significantly increasing the
number of steps on the bots. The WebGL conformance tests should probably remain
a separate step, but some of the smaller Telemetry based tests
(`context_lost_tests`, `memory_test`, etc.) should probably be combined into a
single step.

If you are adding a new test to one of the existing tests (e.g., `pixel_test`),
all you need to do is make sure that your new test runs correctly via isolates.
See the documentation from the GPU bot details on [adding new isolated
tests][new-isolates] for the gn args and authentication needed to upload
isolates to the isolate server. Most likely the new test will be Telemetry
based, and included in the `telemetry_gpu_test_run` isolate.

[new-isolates]: gpu_testing_bot_details.md#Adding-a-new-isolated-test-to-the-bots

### Adding new steps to the GPU Bots

The tests that are run by the GPU bots are described by a couple of JSON files
in the Chromium workspace:

*   [`chromium.gpu.json`](https://chromium.googlesource.com/chromium/src/+/main/testing/buildbot/chromium.gpu.json)
*   [`chromium.gpu.fyi.json`](https://chromium.googlesource.com/chromium/src/+/main/testing/buildbot/chromium.gpu.fyi.json)

These files are autogenerated by the following script:

*   [`generate_buildbot_json.py`](https://chromium.googlesource.com/chromium/src/+/main/testing/buildbot/generate_buildbot_json.py)

This script is documented in
[`testing/buildbot/README.md`](https://chromium.googlesource.com/chromium/src/+/main/testing/buildbot/README.md). The
JSON files are parsed by the chromium and chromium_trybot recipes, and describe
two basic types of tests:

*   GTests: those which use the Googletest and Chromium's `base/test/launcher/`
    frameworks.
*   Isolated scripts: tests whose initial entry point is a Python script which
    follows a simple convention of command line argument parsing.

The majority of the GPU tests are however:

*   Telemetry based tests: an isolated script test which is built on the
    Telemetry framework and which launches the entire browser.

A prerequisite of adding a new test to the bots is that that test [run via
isolates][new-isolates]. Once that is done, modify `test_suites.pyl` to add the
test to the appropriate set of bots. Be careful when adding large new test steps
to all of the bots, because the GPU bots are a limited resource and do not
currently have the capacity to absorb large new test suites. It is safer to get
new tests running on the chromium.gpu.fyi waterfall first, and expand from there
to the chromium.gpu waterfall (which will also make them run against every
Chromium CL by virtue of the `linux-rel`, `mac-rel`, `win7-rel` and
`android-marshmallow-arm64-rel` tryservers' mirroring of the bots on this
waterfall – so be careful!).

Tryjobs which add new test steps to the chromium.gpu.json file will run those
new steps during the tryjob, which helps ensure that the new test won't break
once it starts running on the waterfall.

Tryjobs which modify chromium.gpu.fyi.json can be sent to the
`win_optional_gpu_tests_rel`, `mac_optional_gpu_tests_rel` and
`linux_optional_gpu_tests_rel` tryservers to help ensure that they won't
break the FYI bots.

## Debugging Pixel Test Failures on the GPU Bots

If pixel tests fail on the bots, the build step will contain either one or more
links titled `gold_triage_link for <test name>` or a single link titled
`Too many artifacts produced to link individually, click for links`, which
itself will contain links. In either case, these links will direct to Gold
pages showing the image produced by the image and the approved image that most
closely matches it.

Note that for the tests which programmatically check colors in certain regions of
the image (tests with `expected_colors` fields in [pixel_test_pages]), there
likely won't be a closest approved image since those tests only upload data to
Gold in the event of a failure.

[pixel_test_pages]: https://cs.chromium.org/chromium/src/content/test/gpu/gpu_tests/pixel_test_pages.py

## Updating and Adding New Pixel Tests to the GPU Bots

If your CL adds a new pixel test or modifies existing ones, it's likely that
you will have to approve new images. Simply run your CL through the CQ and
follow the steps outline [here][pixel wrangling triage] under the "Check if any
pixel test failures are actual failures or need to be rebaselined." step.

[pixel wrangling triage]: http://go/gpu-pixel-wrangler-info#how-to-keep-the-bots-green

If you are adding a new pixel test, it is beneficial to set the
`grace_period_end` argument in the test's definition. This will allow the test
to run for a period without actually failing on the waterfall bots, giving you
some time to triage any additional images that show up on them. This helps
prevent new tests from making the bots red because they're producing slightly
different but valid images from the ones triaged while the CL was in review.
Example:

```
from datetime import date

...

PixelTestPage(
  'foo_pixel_test.html',
  ...
  grace_period_end=date(2020, 1, 1)
)
```

You should typically set the grace period to end 1-2 days after the the CL will
land.

Once your CL passes the CQ, you should be mostly good to go, although you should
keep an eye on the waterfall bots for a short period after your CL lands in case
any configurations not covered by the CQ need to have images approved, as well.
All untriaged images for your test can be found by substituting your test name
into:

`https://chrome-gpu-gold.skia.org/search?query=name%3D<test name>`

**NOTE** If you have a grace period active for your test, then Gold will be told
to ignore results for the test. This is so that it does not comment on unrelated
CLs about untriaged images if your test is noisy. Images will still be uploaded
to Gold and can be triaged, but will not show up on the main page's untriaged
image list, and you will need to enable the "Ignored" toggle at the top of the
page when looking at the triage page specific to your test.

## Stamping out Flakiness

It's critically important to aggressively investigate and eliminate the root
cause of any flakiness seen on the GPU bots. The bots have been known to run
reliably for days at a time, and any flaky failures that are tolerated on the
bots translate directly into instability of the browser experienced by
customers. Critical bugs in subsystems like WebGL, affecting high-profile
products like Google Maps, have escaped notice in the past because the bots
were unreliable. After much re-work, the GPU bots are now among the most
reliable automated test machines in the Chromium project. Let's keep them that
way.

Flakiness affecting the GPU tests can come in from highly unexpected sources.
Here are some examples:

*   Intermittent pixel_test failures on Linux where the captured pixels were
    black, caused by the Display Power Management System (DPMS) kicking in.
    Disabled the X server's built-in screen saver on the GPU bots in response.
*   GNOME dbus-related deadlocks causing intermittent timeouts ([Issue
    309093](http://crbug.com/309093) and related bugs).
*   Windows Audio system changes causing intermittent assertion failures in the
    browser ([Issue 310838](http://crbug.com/310838)).
*   Enabling assertion failures in the C++ standard library on Linux causing
    random assertion failures ([Issue 328249](http://crbug.com/328249)).
*   V8 bugs causing random crashes of the Maps pixel test (V8 issues
    [3022](https://code.google.com/p/v8/issues/detail?id=3022),
    [3174](https://code.google.com/p/v8/issues/detail?id=3174)).
*   TLS changes causing random browser process crashes ([Issue
    264406](http://crbug.com/264406)).
*   Isolated test execution flakiness caused by failures to reliably clean up
    temporary directories ([Issue 340415](http://crbug.com/340415)).
*   The Telemetry-based WebGL conformance suite caught a bug in the memory
    allocator on Android not caught by any other bot ([Issue
    347919](http://crbug.com/347919)).
*   context_lost test failures caused by the compositor's retry logic ([Issue
    356453](http://crbug.com/356453)).
*   Multiple bugs in Chromium's support for lost contexts causing flakiness of
    the context_lost tests ([Issue 365904](http://crbug.com/365904)).
*   Maps test timeouts caused by Content Security Policy changes in Blink
    ([Issue 395914](http://crbug.com/395914)).
*   Weak pointer assertion failures in various webgl\_conformance\_tests caused
    by changes to the media pipeline ([Issue 399417](http://crbug.com/399417)).
*   A change to a default WebSocket timeout in Telemetry causing intermittent
    failures to run all WebGL conformance tests on the Mac bots ([Issue
    403981](http://crbug.com/403981)).
*   Chrome leaking suspended sub-processes on Windows, apparently a preexisting
    race condition that suddenly showed up ([Issue
    424024](http://crbug.com/424024)).
*   Changes to Chrome's cross-context synchronization primitives causing the
    wrong tiles to be rendered ([Issue 584381](http://crbug.com/584381)).
*   A bug in V8's handling of array literals causing flaky failures of
    texture-related WebGL 2.0 tests ([Issue 606021](http://crbug.com/606021)).
*   Assertion failures in sync point management related to lost contexts that
    exposed a real correctness bug ([Issue 606112](http://crbug.com/606112)).
*   A bug in glibc's `sem_post`/`sem_wait` primitives breaking V8's parallel
    garbage collection ([Issue 609249](http://crbug.com/609249)).
*   A change to Blink's memory purging primitive which caused intermittent
    timeouts of WebGL conformance tests on all platforms ([Issue
    840988](http://crbug.com/840988)).
*   Screen DPI being inconsistent across seemingly identical Linux machines,
    causing the Maps pixel test to flakily produce incorrectly sized images
    ([Issue 1091410](https://crbug.com/1091410)).

If you notice flaky test failures either on the GPU waterfalls or try servers,
please file bugs right away with the component Internals>GPU>Testing and
include links to the failing builds and copies of the logs, since the logs
expire after a few days. [GPU pixel wranglers] should give the highest priority
to eliminating flakiness on the tree.

[GPU pixel wranglers]: http://go/gpu-pixel-wrangler
