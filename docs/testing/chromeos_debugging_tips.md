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
the [ui.WindowControl] log link contains stack traces and error messages.

- **View browser & system logs**: A common cause of failure on Chrome's builders
are browser crashes. When this happens, each test's log snippets will simply
contain warnings like "[Chrome probably crashed]". To debug these crashes,
navigate to the test's Isolated output, listed in the build under the test
step's [shard #0 isolated out] link. There you'll find expanded logs for every
test. For example, the [tests/ui.WindowControl/messages] log has more info
than its earlier snippet. Additionally, you can find system logs under
the `system_logs/` prefix. To find a system log for a particular test, match
the timestamps printed in the test's log with the timestamps present in the
system log filename. For instance, the previous `ui.WindowControl` failure
matches the [system_logs/chrome/chrome_20201029-195153] browser log, which
contains the culprit Chrome crash and backtrace.

- **Symbolizing a browser crash dump**: See [below](#symbolizing-a-crash-dump).

### Disabling a test

There a couple ways to disable a test on Chrome's builders:
- **With a full CrOS checkout**: If you have a full CrOS checkout, you can add
the `informational` [attribute] to the test's definition. (You may be able to
bypass the need for a full CrOS checkout by using the `Edit code` button in
codesearch UI, but this flow is unverified.) This can take time (ie: many hours)
to land and propagate onto Chrome's builders. So if you need the test disabled
ASAP, consult the next option.
- **With only a Chromium checkout**: You can also add the test to the list of
disabled tests for the step's GN target. For example, to disable a test in the
`chrome_all_tast_tests` step, add it to [this list]. **Note**: If the test is
failing consistently, and you only disable it here, it will likely start to fail
in the next [Chrome uprev] on CrOS's builders, which can lead to further
problems down the road. So please make sure you pursue the first option as well
in that case.

In both cases, please make sure a bug is filed for the test, and route it to
the appropriate owners.

### Symbolizing a crash dump

If a test fails due to a browser crash, there should be a Minidump crash report
present in the test's isolated out under the prefix `crashes/chrome...`. These
reports aren't very useful by themselves, but with a few commands you can
symbolize the report locally to get insight into what conditions caused Chrome
to crash.

To do so, first download both the task's input isolate (this provides the
symbols and the symbolizing tools) as well as the task's output isolate (this
provides the crash reports). See the commands listed under the *Reproducing the
task locally* section on the task page. For example, to download them for
[this task](https://chrome-swarming.appspot.com/task?id=506a01dd12c8a610), `cd`
into a tmp directory and run:
```
$CHROME_DIR/tools/luci-go/isolated download -I https://chrome-isolated.appspot.com --namespace default-gzip -isolated 64919fee8b02d826df2401544a9dc0f7dfa2172d -output-dir input
python $CHROME_DIR/tools/swarming_client/swarming.py collect -S chrome-swarming.appspot.com 506a01dd12c8a610 --task-output-dir output
```

Once both isolates have been fetched you must then generate the breakpad
symbols by pointing the `generate_breakpad_symbols.py` script to the input's
build dir:
```
python input/components/crash/content/tools/generate_breakpad_symbols.py --symbols-dir symbols --build-dir input/out/Release/ --binary input/out/Release/chrome
```

That will generate the symbols in the `symbols/` dir. Then to symbolize a Chrome
crash report present in the task's output (such as
`chrome.20201211.041043.31022.5747.dmp`):
```
./input/out/Release/minidump_stackwalk output/0/crashes/chrome.20201211.041043.31022.5747.dmp symbols/
```


### Running a test locally

To run a Tast test the same way it's ran on Chrome's builders:

- Decide which Chrome OS device type or VM to test on.

- Build Chrome via the [Simple Chrome] workflow for that board.

- Deploy your Chrome to the device via the [deploy_chrome.py] tool.

- Finally, run the Tast test on the device via the `cros_run_test` tool under
  `//third_party/chromite/bin/`. eg:
  `cros_run_test --device $DEVICE_IP --tast ui.ChromeLogin`. See [here] for more
  info on cros_run_test.

## Telemetry

>TODO: Add instructions for debugging telemetry failures.

## GTest

>TODO: Add instructions for debugging GTest failures.

## Rerunning these tests locally

>TODO: Add instructions for rerunning these tests locally.


[linux-chromeos]: https://chromium.googlesource.com/chromium/src/+/HEAD/docs/chromeos_build_instructions.md
[Tast]: https://chromium.googlesource.com/chromiumos/platform/tast/+/HEAD/README.md
[failed build]: https://ci.chromium.org/p/chromium/builders/ci/chromeos-kevin-rel/29791
[ui.WindowControl]: https://logs.chromium.org/logs/chromium/buildbucket/cr-buildbucket.appspot.com/8865053459542681936/+/steps/chrome_all_tast_tests_on_ChromeOS/0/logs/Deterministic_failure:_ui.WindowControl__status_FAILURE_/0
[Chrome probably crashed]: https://logs.chromium.org/logs/chromium/buildbucket/cr-buildbucket.appspot.com/8905974915785988832/+/steps/chrome_all_tast_tests__retry_shards_with_patch__on_ChromeOS/0/logs/Deterministic_failure:_ui.ChromeLogin__status_FAILURE_/0
[shard #0 isolated out]: https://isolateserver.appspot.com/browse?namespace=default-gzip&hash=3d35c273195f640c69b1cf0d15d19d9868e3f593
[tests/ui.WindowControl/messages]: https://isolateserver.appspot.com/browse?namespace=default-gzip&digest=baefbcfd24c02b3ada4617d259dc6b4220b413b9&as=messages
[system_logs/chrome/chrome_20201029-195153]: https://isolateserver.appspot.com/browse?namespace=default-gzip&digest=272166c85f190c336a9885f0267cbdea912e31da&as=chrome_20201029-195153
[attribute]: https://chromium.googlesource.com/chromiumos/platform/tast/+/HEAD/docs/test_attributes.md
[this list]: https://codesearch.chromium.org/chromium/src/chromeos/tast_control.gni
[Chrome uprev]: https://chromium.googlesource.com/chromiumos/docs/+/HEAD/chrome_commit_pipeline.md#the-chrome-os-commit-pipeline-for-chrome-changes
[Simple Chrome]: https://chromium.googlesource.com/chromiumos/docs/+/HEAD/simple_chrome_workflow.md
[deploy_chrome.py]: https://chromium.googlesource.com/chromiumos/docs/+/HEAD/simple_chrome_workflow.md#Deploying-Chrome-to-the-device
[here]: https://chromium.googlesource.com/chromiumos/docs/+/HEAD/cros_vm.md#in-simple-chrome
