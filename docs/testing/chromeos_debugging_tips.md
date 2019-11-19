# Chrome OS Debugging Instructions
Chrome on Chrome OS is tested using a handful of frameworks, each of which
you'll find running on Chrome's CQ and waterfalls. If you're investigating
failures in these tests, below are some tips for debugging and identifying the
cause.

*** note

This doc outlines tests running in true Chrome OS environments (ie: on virtual
machines or real devices). [linux-chromeos] tests, on the other hand, can be
debugged like any other linux test.
***

## Tast

[Tast] is Chrome OS's integration testing framework. Since Chrome itself is
instrumental to the Chrome OS system, it's equally important that we run some
of these integration tests on Chrome's waterfalls. If you find one of these
tests failing (likely in the `chrome_all_tast_tests` step), you can:

- **Inspect the failed test's log snippet**: There should be a log link for
each failed test with failure information. eg: For this [failed build], opening
the [platform.Histograms] log link contains stack traces and error messages.

- **View browser & system logs**: A common cause of failure on Chrome's builders
are browser crashes. When this happens, each test's log snippets will simply
contain warnings like "[Chrome probably crashed]". To debug these crashes,
navigate to the test's Isolated output, most likely listed in the build under
the test step's [shard #0 isolated out] link. From there, view the various
`log/chrome/...` or `log/ui/...` text files and you should find some with
browser [crashes and stack traces].

To disable a test on Chrome's builders, the preferred method is to add the
`informational` attribute to the test's definition (see [Tast attributes] for
more info). Note that this requires a full Chrome OS checkout. If that's not an
option, or if it needs to be disabled ASAP, you can add it to the list of
disabled tests for the step's GN target. For example, to disable a test in the
`chrome_all_tast_tests` step, add it to [this list].

## Telemetry

>TODO: Add instructions for debugging telemetry failures.

## GTest

>TODO: Add instructions for debugging GTest failures.

## Rerunning these tests locally

>TODO: Add instructions for rerunning these tests locally.


[linux-chromeos]: https://chromium.googlesource.com/chromium/src/+/master/docs/chromeos_build_instructions.md
[Tast]: https://chromium.googlesource.com/chromiumos/platform/tast/+/HEAD/README.md
[failed build]: https://ci.chromium.org/p/chromium/builders/ci/chromeos-kevin-rel/14102
[platform.Histograms]: https://logs.chromium.org/logs/chromium/buildbucket/cr-buildbucket.appspot.com/8904949911599004400/+/steps/chrome_all_tast_tests_on_ChromeOS/0/logs/Deterministic_failure:_platform.Histograms__status_FAILURE_/0
[Chrome probably crashed]: https://logs.chromium.org/logs/chromium/buildbucket/cr-buildbucket.appspot.com/8905974915785988832/+/steps/chrome_all_tast_tests__retry_shards_with_patch__on_ChromeOS/0/logs/Deterministic_failure:_ui.ChromeLogin__status_FAILURE_/0
[shard #0 isolated out]: https://isolateserver.appspot.com/browse?namespace=default-gzip&hash=fd1f6d76b076f07cc98fa7b2e0c0097f35c51cd0
[crashes and stack traces]: https://isolateserver.appspot.com/browse?namespace=default-gzip&digest=993d58ff48bb08071d951bd8e103fa5a3c03efb1&as=chrome_20190805-044653
[Tast attributes]: https://chromium.googlesource.com/chromiumos/platform/tast/+/HEAD/docs/test_attributes.md
[this list]: https://codesearch.chromium.org/chromium/src/chromeos/BUILD.gn?rcl=7b0393a9091fd02edc9ae773739124f7be5a0782&l=242
