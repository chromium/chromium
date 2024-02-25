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

- **Inspect the failed test's log snippet**: There should be a log snippet for
each failed test in the `Test Results` tab in the build UI. eg: For this
[failed build], clicking on the `policy.IncognitoModeAvailability` expands to
include stack traces and error messages.

- **View browser & system logs**: A common cause of failure on Chrome's builders
are browser crashes. When this happens, each test's log snippets will simply
contain warnings like "[Chrome probably crashed]". To debug these crashes,
expand the list of attached artifacts for the test by clicking the `Artifacts`
link under the failed test in the `Test Results` tab. There you'll find an
extended log for the test under `log.txt`. Additionally, you can find system
logs included in that list. To find a system log for a particular test, match
the timestamps printed in the test's log with the timestamps present in the
system log filename. For instance, the previous `example.ChromeFixture` failure
matches the [chrome/chrome_20210920-051805] browser log, which contains the
culprit Chrome crash and backtrace.

- **Symbolizing a browser crash dump**: See [below](#symbolizing-a-crash-dump).

### Disabling a test

If you are a Chrome Sheriff, please read the sheriff documentation
[here](http://go/chrome-sheriff-tast) before disabling any tests.

Tast tests are run under both Chrome's builders and CrOS's builders. They can be
disabled either completely (in all builders), or in Chrome's builders alone. The
latter should be used only for changes which are not expected to occur on CrOS's
builders.

- **Disabling in all builders**: If you have a full CrOS checkout, you can add
the `informational` [attribute] to the test's definition. (You may be able to
bypass the need for a full CrOS checkout by using the `Edit code` button in
codesearch UI, but this flow is unverified.) This can take time (ie: many hours)
to land and propagate onto Chrome's builders. So if you need the test disabled
ASAP, consult the next option.
- **Disabling in only Chrome's builders**: You can add the test to the list of
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

If you are running a locally compiled [Simple Chrome] binary on a device or VM,
you can can build `minidump_stackwalk` and download the
`/home/chronos/crash/chrome*.dmp` file.
```
autoninja -C out/Release minidump_stackwalk dump_syms

rsync -r -e "ssh -p 9222" root@localhost:/home/chronos/crash /tmp
```

For a crash on a bot, download both the task's input files (this provides the
symbols and the symbolizing tools) as well as the task's output results (this
provides the crash reports). See the commands listed under the *Reproducing the
task locally* section on the task page. For example, to download them for
[this task](https://chrome-swarming.appspot.com/task?id=5cc272e0a839b311), `cd`
into a tmp directory and run:
```
cipd install "infra/tools/luci/cas/\${platform}" -root bar
./bar/cas login
./bar/cas download -cas-instance projects/chrome-swarming/instances/default_instance -digest 1ad29e201e4ae7e3056a8b17935edbcd62fb54befdfeba221f2e82e54f150c86/812 -dir foo

cipd install "infra/tools/luci/swarming/\${platform}" -root bar
./bar/swarming login
./bar/swarming collect -S chrome-swarming.appspot.com -output-dir=foo 5cc272e0a839b311
```

Generate the breakpad symbols by pointing the `generate_breakpad_symbols.py` script to
your local binary, or the downloaded input build dir:
```
cd foo
vpython3 components/crash/content/tools/generate_breakpad_symbols.py --symbols-dir symbols --build-dir out/Release/ --binary out/Release/chrome --platform chromeos
```

That will generate the symbols in the `symbols/` dir. Then to symbolize a Chrome
crash report (either in the tasks's output, or the `/tmp/crash` dir):
```
./out/Release/minidump_stackwalk 5cc272e0a839b311/crashes/chrome.20220816.214251.44917.24579.dmp symbols/
```


### Running a test locally

To run a Tast test the same way it's ran on Chrome's builders:

- Decide which Chrome OS device type or VM to test on.

- Build Chrome via the [Simple Chrome] workflow for that board.

- Deploy your Chrome to the device via the [deploy_chrome.py] tool.

- Finally, run the Tast test on the device via the `cros_run_test` tool under
  `//third_party/chromite/bin/`. eg:
  `cros_run_test --device $DEVICE_IP --tast login.Chrome`. See [here] for more
  info on cros_run_test.

## Telemetry

>TODO: Add instructions for debugging telemetry failures.

## GTest

>TODO: Add instructions for debugging GTest failures.

## Rerunning these tests locally

>TODO: Add instructions for rerunning these tests locally.


[linux-chromeos]: https://chromium.googlesource.com/chromium/src/+/HEAD/docs/chromeos_build_instructions.md
[Tast]: https://chromium.googlesource.com/chromiumos/platform/tast/+/HEAD/README.md
[failed build]: https://ci.chromium.org/ui/p/chromium/builders/ci/chromeos-kevin-rel/37300/test-results
[Chrome probably crashed]: https://luci-milo.appspot.com/ui/inv/build-8835572137562508161/test-results?q=example.ChromeFixture
[chrome/chrome_20210920-051805]: https://luci-milo.appspot.com/ui/artifact/raw/invocations/task-chromium-swarm.appspot.com-561bed66572a9411/artifacts/chrome%2Fchrome_20210920-051805
[attribute]: https://chromium.googlesource.com/chromiumos/platform/tast/+/HEAD/docs/test_attributes.md
[this list]: https://codesearch.chromium.org/chromium/src/chromeos/tast_control.gni
[Chrome uprev]: https://chromium.googlesource.com/chromiumos/docs/+/HEAD/chrome_commit_pipeline.md#the-chrome-os-commit-pipeline-for-chrome-changes
[Simple Chrome]: https://chromium.googlesource.com/chromiumos/docs/+/HEAD/simple_chrome_workflow.md
[deploy_chrome.py]: https://chromium.googlesource.com/chromiumos/docs/+/HEAD/simple_chrome_workflow.md#Deploying-Chrome-to-the-device
[here]: https://chromium.googlesource.com/chromiumos/docs/+/HEAD/cros_vm.md#in-simple-chrome
