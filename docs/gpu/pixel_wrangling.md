# GPU Bots & Pixel Wrangling

![](images/wrangler.png)

(December 2017: presentation on GPU bots and pixel wrangling: see [slides].)

GPU Pixel Wrangling is the process of keeping various GPU bots green. On the
GPU bots, tests run on physical hardware with real GPUs, not in VMs like the
majority of the bots on the Chromium waterfall.

[slides]: https://docs.google.com/presentation/d/1sZjyNe2apUhwr5sinRfPs7eTzH-3zO0VQ-Cj-8DlEDQ/edit?usp=sharing

[TOC]

## Fleet Status

*   [Chrome GPU Fleet Status](http://vi/chrome-infra/Projects/gpu)

(Sorry, this link is Google internal only.)

These graphs show 1 day of activity by default. The drop-down boxes at the top
allow viewing of longer durations.

See [this CL](http://cl/238562533) for an example of how to update these graphs.

## GPU Bots' Waterfalls

The waterfalls work much like any other; see the [Tour of the Chromium Buildbot
Waterfall] for a more detailed explanation of how this is laid out. We have
more subtle configurations because the GPU matters, not just the OS and release
v. debug. Hence we have Windows Nvidia Release bots, Mac Intel Debug bots, and
so on. The waterfalls we’re interested in are:

*   [Chromium GPU]
    *   Various operating systems, configurations, GPUs, etc.
*   [Chromium GPU FYI]
    *   These bots run less-standard configurations like Windows with AMD GPUs,
        Linux with Intel GPUs, etc.
    *   These bots build with top of tree ANGLE rather than the `DEPS` version.
    *   The [ANGLE tryservers] help ensure that these bots stay green. However,
        it is possible that due to ANGLE changes these bots may be red while
        the chromium.gpu bots are green.
    *   The [ANGLE Wrangler] is on-call to help resolve ANGLE-related breakage
        on this watefall.
    *   To determine if a different ANGLE revision was used between two builds,
        compare the `got_angle_revision` buildbot property on the GPU builders
        or `parent_got_angle_revision` on the testers. This revision can be
        used to do a `git log` in the `third_party/angle` repository.
*   [Chromium SwANGLE]
    *   These bots run GPU tests on top of ANGLE's GLES implementation running
        on top of SwiftShader's Vulkan implementation purely in software.
        Regressions should mostly be handled by the [ANGLE Wrangler], but some
        failures fall into Pixel Wrangler's domain, for example, WebGL failures
        due to Chromium-side and WebGL-side changes on
        linux-swangle-chromium-x64, mac-swangle-chromium-x64 and
        win-swangle-chromium-x86 bots.

<!-- TODO(kainino): update link when the page is migrated -->
[Tour of the Chromium Buildbot Waterfall]: http://www.chromium.org/developers/testing/chromium-build-infrastructure/tour-of-the-chromium-buildbot
[Chromium GPU]: https://ci.chromium.org/p/chromium/g/chromium.gpu/console?reload=120
[Chromium GPU FYI]: https://ci.chromium.org/p/chromium/g/chromium.gpu.fyi/console?reload=120
[Chromium SwANGLE]: https://ci.chromium.org/p/chromium/g/chromium.swangle/console?reload=120
[ANGLE tryservers]: https://build.chromium.org/p/tryserver.chromium.angle/waterfall
[ANGLE Wrangler]: https://chromium.googlesource.com/angle/angle/+/master/infra/ANGLEWrangling.md

## Test Suites

The bots run several test suites. The majority of them have been migrated to
the Telemetry harness, and are run within the full browser, in order to better
test the code that is actually shipped. As of this writing, the tests included:

*   Tests using the Telemetry harness:
    *   The WebGL conformance tests: `webgl_conformance_integration_test.py`
    *   A Google Maps test: `maps_integration_test.py`
    *   Context loss tests: `context_lost_integration_test.py`
    *   Depth capture tests: `depth_capture_integration_test.py`
    *   GPU process launch tests: `gpu_process_integration_test.py`
    *   Hardware acceleration validation tests:
        `hardware_accelerated_feature_integration_test.py`
    *   Pixel tests validating the end-to-end rendering pipeline:
        `pixel_integration_test.py`
    *   Stress tests of the screenshot functionality other tests use:
        `screenshot_sync_integration_test.py`
*   `angle_unittests`: see `src/third_party/angle/src/tests/BUILD.gn`
*   drawElements tests (on the chromium.gpu.fyi waterfall): see
    `src/third_party/angle/src/tests/BUILD.gn`
*   `gles2_conform_test` (requires internal sources): see
    `src/gpu/gles2_conform_support/BUILD.gn`
*   `gl_tests`: see `src/gpu/BUILD.gn`
*   `gl_unittests`: see `src/ui/gl/BUILD.gn`
*   `rendering_representative_perf_tests` (on the chromium.gpu.fyi waterfall):
    see `src/chrome/test/BUILD.gn`

And more. See
[`src/testing/buildbot/README.md`](../../testing/buildbot/README.md)
and the GPU sections of `test_suites.pyl` and `waterfalls.pyl` for the
complete description of bots and tests.

Additionally, the Release bots run:

*   `tab_capture_end2end_tests:` see
    `src/chrome/browser/extensions/api/tab_capture/tab_capture_apitest.cc` and
    `src/chrome/browser/extensions/api/cast_streaming/cast_streaming_apitest.cc`

### More Details

More details about the bots' setup can be found on the [GPU Testing] page.

[GPU Testing]: https://sites.google.com/a/chromium.org/dev/developers/testing/gpu-testing

## Wrangling

### Prerequisites

1.  Ideally a wrangler should be a Chromium committer. If you're on the GPU
pixel wrangling rotation, there will be an email notifying you of the upcoming
shift, and a calendar appointment.
    *   If you aren't a committer, don't panic. It's still best for everyone on
        the team to become acquainted with the procedures of maintaining the
        GPU bots.
    *   In this case you'll upload CLs to Gerrit to perform reverts (optionally
        using the new "Revert" button in the UI), and might consider using
        `Tbr:` to speed through trivial and urgent CLs. In general, try to send
        all CLs through the commit queue.
    *   Contact bajones, kainino, kbr, vmiura, zmo, or another member of the
        Chrome GPU team who's already a committer for help landing patches or
        reverts during your shift.
1.  Apply for [access to the bots].
1.  You may want to install the [Flake linker] extension, which adds several useful features to the bot build log pages.
    *   Links to Chromium flakiness dashboard from build result pages, so you can see all failures for a single test across the fleet.
    *   Automatically hides green build steps so you can see the failure immediately.
    *   Turns build log links into deep links directly to the failure line in the log.

[access to the bots]: https://sites.google.com/a/google.com/chrome-infrastructure/golo/remote-access?pli=1
[Flake linker]: https://chrome.google.com/webstore/detail/flake-linker/boamnmbgmfnobomddmenbaicodgglkhc

### How to Keep the Bots Green

1.  Watch for redness on the tree.
    1.  [Sheriff-O-Matic] now has support for all the
        [GPU Bots' Waterfalls](#GPU-Bots_Waterfalls) under the
        [Chromium GPU][Sheriff-O-Matic] tab!
    1.  The bots are expected to be green all the time. Flakiness on these bots
        is neither expected nor acceptable.
    1.  If a bot goes consistently red, it's necessary to figure out whether a
        recent CL caused it, or whether it's a problem with the bot or
        infrastructure.
    1.  If it looks like a problem with the bot (deep problems like failing to
        check out the sources, the isolate server failing, etc.) notify the
        Chromium troopers and file a P1 bug with labels: Infra\>Labs,
        Infra\>Troopers and Internals\>GPU\>Testing. See the general [tree
        sheriffing page] for more details.
    1.  Otherwise, examine the builds just before and after the redness was
        introduced. Look at the revisions in the builds before and after the
        failure was introduced.
    1.  **File a bug** capturing the regression range and excerpts of any
        associated logs. Regressions should be marked P1. CC engineers who you
        think may be able to help triage the issue. Keep in mind that the logs
        on the bots expire after a few days, so make sure to add copies of
        relevant logs to the bug report.
    1.  Use the `Hotlist=PixelWrangler` label to mark bugs that require the
        pixel wrangler's attention, so it's easy to find relevant bugs when
        handing off shifts.
    1.  Study the regression range carefully. Use drover to revert any CLs
        which break the chromium.gpu bots. Use your judgment about
        chromium.gpu.fyi, since not all bots are covered by trybots. In the
        revert message, provide a clear description of what broke, links to
        failing builds, and excerpts of the failure logs, because the build
        logs expire after a few days.
    1.  If the failure is one that you believe should have been caught by an
        optional GPU trybot, you can use the script at
        [`//content/test/gpu/trim_culprit_cls.py`][trim culprit cls] to help
        trim down the blamelist by finding out which CLs passed said trybot
        before submission. See the documentation at the top of the script for
        example usage, etc.
1.  Make sure the bots are running jobs.
    1.  Keep an eye on the console views of the various bots.
    1.  Make sure the bots are all actively processing jobs. If they go offline
        for a long period of time, the "summary bubble" at the top may still be
        green, but the column in the console view will be gray.
    1.  Email the Chromium troopers if you find a bot that's not processing
        jobs.
1.  Make sure the GPU try servers are in good health.
    1.  The GPU try servers are no longer distinct bots on a separate
        waterfall, but instead run as part of the regular tryjobs on the
        Chromium waterfalls. The GPU tests run as part of the following
        tryservers' jobs:
        1.  `[linux-rel]` on the [luci.chromium.try] waterfall
        1.  `[mac-rel]` on the [luci.chromium.try] waterfall
        1.  `[win7-rel]` on the [luci.chromium.try] waterfall
    1.  The best tool to use to quickly find flakiness on the tryservers is the
        new [Chromium Try Flakes] tool. Look for the names of GPU tests (like
        maps_pixel_test) as well as the test machines (e.g. mac-rel). If you
        see a flaky test, file a bug like [this one](http://crbug.com/444430).
        Also look for compile flakes that may indicate that a bot needs to be
        clobbered. Contact the Chromium sheriffs or troopers if so.
    1.  Glance at these trybots from time to time and see if any GPU tests are
        failing frequently. **Note** that test failures are **expected** on
        these bots: individuals' patches may fail to apply, fail to compile, or
        break various tests. Look specifically for patterns in the failures. It
        isn't necessary to spend a lot of time investigating each individual
        failure. (Use the "Show: 200" link at the bottom of the page to see
        more history.)
    1.  If the same set of tests are failing repeatedly, look at the individual
        runs. Examine the swarming results and see whether they're all running
        on the same machine. (This is the "Bot assigned to task" when clicking
        any of the test's shards in the build logs.) If they are, something
        might be wrong with the hardware. Use the [Swarming Server Stats] tool
        to drill down into the specific builder.
    1.  If you see the same test failing in a flaky manner across multiple
        machines and multiple CLs, it's crucial to investigate why it's
        happening. [crbug.com/395914](http://crbug.com/395914) was one example
        of an innocent-looking Blink change which made it through the commit
        queue and introduced widespread flakiness in a range of GPU tests. The
        failures were also most visible on the try servers as opposed to the
        main waterfalls.
1.  Check if any pixel test failures are actual failures or need to be
    rebaselined.
    1. For a given build failing the pixel tests, look for either:
        1. One or more links named `gold_triage_link for <test name>`. This will
           be the case if there are fewer than 10 links. If the test was run on
           a trybot, the link will instead be named
           `triage_link_for_entire_cl for <test name>` (the weird naming comes
           with how the recipe processes and displays links).
        1. A single link named
           `Too many artifacts produced to link individually, click for links`.
           This will be the case if there are 10 or more links.
    1. In either case, follow the link(s) to the triage page for the image the
       failing test produced.
        1. If the test was run on a trybot, all the links will point to the same
           page, which will be the triage page for every untriaged image
           produced by the CL being tested.
    1. Ensure you are signed in to the Gold server the links take you to (both
       @google.com and @chromium.org accounts work).
    1. Triage images on those pages (typically by approving them, but you can
       mark them as negative if it is an image that should not be produced). In
       the case of a negative image, a bug should be filed on
       [crbug](https://crbug.com) to investigate and fix the cause of that
       particular image being produced, as future occurrences of it will cause
       the test to fail. Such bugs should include the `Internals>GPU>Testing`
       component and whatever component is suitable for the type of failing
       test (likely `Blink>WebGL` or `Blink>Canvas`). The test should also be
       marked as failing or skipped(see the item below on updating the
       Telemetry-based test expectations) so that the test failure doesn't show
       up as a builder failure. If the failure is consistent, prefer to skip
       instead of mark as failing so that the failure links don't pile up. If
       the failure occurs on the trybots, include the change to the
       expectations in your CL.
    1. Additional, less common triage steps for the pixel tests can be found in
       [this section][gold less common failures] of the GPU Gold documentation.
1.  Update Telemetry-based test expectations if necessary.
    1.  Most of the GPU tests are run inside a full Chromium browser, launched
        by Telemetry, rather than a Gtest harness. The tests and their
        expectations are contained in [src/content/test/gpu/gpu_tests/test_expectations] . See
        for example <code>[webgl_conformance_expectations.txt]</code>,
        <code>[gpu_process_expectations.txt]</code> and
        <code>[pixel_expectations.txt]</code>.
    1.  See the header of the file a list of modifiers to specify a bot
        configuration. It is possible to specify OS (down to a specific
        version, say, Windows 7 or Mountain Lion), GPU vendor
        (NVIDIA/AMD/Intel), and a specific GPU device.
    1.  The key is to maintain the highest coverage: if you have to disable a
        test, disable it only on the specific configurations it's failing. Note
        that it is not possible to discern between Debug and Release
        configurations.
    1.  Mark tests failing or skipped, which will suppress flaky failures, only
        as a last resort. It is only really necessary to suppress failures that
        are showing up on the GPU tryservers, since failing tests no longer
        close the Chromium tree.
    1.  Please read the section on [stamping out flakiness] for motivation on
        how important it is to eliminate flakiness rather than hiding it.
    1. For failures of rendering_representative_perf_tests please refer to its
    [instructions on updating expectations][rendering_representative_perf_tests].
    1. It's always better to have the CL reviewed properly, but for urgent
       suppressions when no reviewer is available, it is possible to rubber
       stamp the CL via adding `rubber-stamper@appspot.gserviceaccount.com` as
       your reviewer, in addition to the regular reviewer.
1.  For the remaining Gtest-style tests, use the [`DISABLED_`
    modifier][gtest-DISABLED] to suppress any failures if necessary.

[Sheriff-O-Matic]: https://sheriff-o-matic.appspot.com/chromium.gpu
[trim culprit cls]: https://source.chromium.org/chromium/chromium/src/+/master:content/test/gpu/trim_culprit_cls.py
[tree sheriffing page]: https://sites.google.com/a/chromium.org/dev/developers/tree-sheriffs
[linux-rel]: https://ci.chromium.org/p/chromium/builders/luci.chromium.try/linux-rel
[luci.chromium.try]: https://ci.chromium.org/p/chromium/g/luci.chromium.try/builders
[mac-rel]: https://ci.chromium.org/p/chromium/builders/luci.chromium.try/mac-rel
[tryserver.chromium.mac]: https://ci.chromium.org/p/chromium/g/tryserver.chromium.mac/builders
[win7-rel]:
https://ci.chromium.org/p/chromium/builders/luci.chromium.try/win7-rel
[tryserver.chromium.win]: https://ci.chromium.org/p/chromium/g/tryserver.chromium.win/builders
[Chromium Try Flakes]: http://chromium-try-flakes.appspot.com/
<!-- TODO(kainino): link doesn't work, but is still included from chromium-swarm homepage so not removing it now -->
[Swarming Server Stats]: https://chromium-swarm.appspot.com/stats
[gold less common failures]: gpu_pixel_testing_with_gold.md#Triaging-Less-Common-Failures
[Chrome Internal GPU Pixel Wrangling Instructions]: https://sites.google.com/a/google.com/client3d/documents/chrome-internal-gpu-pixel-wrangling-instructions
[src/content/test/gpu/gpu_tests/test_expectations]: https://chromium.googlesource.com/chromium/src/+/master/content/test/gpu/gpu_tests/test_expectations
[webgl_conformance_expectations.txt]: https://chromium.googlesource.com/chromium/src/+/master/content/test/gpu/gpu_tests/test_expectations/webgl_conformance_expectations.txt
[gpu_process_expectations.txt]: https://chromium.googlesource.com/chromium/src/+/master/content/test/gpu/gpu_tests/test_expectations/gpu_process_expectations.txt
[pixel_expectations.txt]: https://chromium.googlesource.com/chromium/src/+/master/content/test/gpu/gpu_tests/test_expectations/pixel_expectations.txt
[stamping out flakiness]: gpu_testing.md#Stamping-out-Flakiness
[gtest-DISABLED]: https://github.com/google/googletest/blob/master/googletest/docs/AdvancedGuide.md#temporarily-disabling-tests
[rendering_representative_perf_tests]: ../testing/rendering_representative_perf_tests.md#Updating-Expectations

### When Bots Misbehave (SSHing into a bot)

1.  See the [Chrome Internal GPU Pixel Wrangling Instructions] for information
    on ssh'ing in to the GPU bots.

[Chrome Internal GPU Pixel Wrangling Instructions]: https://sites.google.com/a/google.com/client3d/documents/chrome-internal-gpu-pixel-wrangling-instructions

### Reproducing WebGL conformance test failures locally

1.  From the buildbot build output page, click on the failed shard to get to
    the swarming task page. Scroll to the bottom of the left panel for a
    command to run the task locally. This will automatically download the build
    and any other inputs needed.
2.  Alternatively, to run the test on a local build, pass the arguments
    `--browser=exact --browser-executable=/path/to/binary` to
    `content/test/gpu/run_gpu_integration_test.py`.
    Also see the [telemetry documentation].

[telemetry documentation]: https://cs.chromium.org/chromium/src/third_party/catapult/telemetry/docs/run_benchmarks_locally.md

## Modifying the GPU Pixel Wrangling Rotation

You may find yourself needing to modify the current rotation. Whether to extend
the rotation, or if scheduling conflicts arise.

For scheduling conflicts you can swap your shift with another wrangler. A good
approach is to look at the rotation calendar, finding someone with nearby dates
to yours. Reach out to them, as they will often be willing to swap.

To actually modify the rotation:
See the [Chrome Internal GPU Pixel Wrangling Instructions] for information.

[Chrome Internal GPU Pixel Wrangling Instructions]: https://sites.google.com/a/google.com/client3d/documents/chrome-internal-gpu-pixel-wrangling-instructions
