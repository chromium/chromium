# iOS Test Runner
iOS test runner is a python script responsible for setting up test environment, executing iOS tests,
organize and report test results to resultdb, and cleaning up test environment at the end.

There are mainly 4 types of tests we run on the waterfall using the script:

1. Unit tests on iOS simulators (SimulatorTestRunner).
2. EG tests on iOS simulators (SimulatorParallelTestRunner).
3. Unit tests on physical iOS devices (DeviceTestRunner).
4. EG tests on physical iOS devices (DeviceXcodeTestRunner).

## Overall Workflow

Below are brief summaries for some of the major workflow steps when test runner is being invoked:

1. **Install Xcode and iOS runtime sdk (for simulator runs)** -- we use Xcode command lines to run tests so Xcode is required in every run.
2. **Sharding** -- each swarming task has a 60 minute hard limit execution time, so we are required to shard some of our test suite runs into multiple shards, to avoid timeout. Furthermore, sharding can improve tests run time as they can be distributed to multiple shards and run in parallel.
3. **Device/Simulator creation** -- if the tests will be run on simulator, an iOS simulator will be created. If the tests will be run a physical iOS device attached to the Mac host, its udid will be recorded for later usage.
4. **Pre-run set up** -- some cleanup steps will be performed on the simulator/device (such as content erase, app uninstallation, etc) to ensure a clean test environment.
5. **Test run** -- run tests through xcode-build commands, tests result parsing/reporting, and retries if needed.
6. **Tear down** -- extract test data to cas output such as logs, screenshots/videos, crash reports. Perform cleanup steps such as deleting simulator to ensure a clean environment for the next test run.


## Testing Instructions
Please make sure to **ALWAYS** write sufficient unit tests to cover any new changes you have made to the scripts.

Besides unit tests, there are also a few ways you can test out your changes, listed below:

### Try jobs
The easiest way is to upload your cl, and run a try job.
Usually **chromium/ios-simulator** or **chromium/ios-simulator-full-configs** are sufficient.

**INTERNAL ONLY:** if you have made changes related to physical devices, then you can also
run try jobs on **chrome/iphone-device** or **chrome/ipad-device**.

The cons of running try jobs are that they always perform a series of steps to build chrome, run a bunch of unit tests + EG tests and are very time consuming. If you are experimenting with new code and need to test out changes fast and iteratively, the next two ways below are recommended.

### Running locally (physical iOS devices not supported at the moment b/328982421)
You can also execute test runner locally to get immediate results. Make sure you have your test app already built and Xcode downloaded on your laptop.

**Note**: in order to run unit tests, you also need to add `enable_run_ios_unittests_with_xctest = true` under `[gn_args]` in your `~/.setup-gn` file before building your test targets

1. first cd into `local_runner/` folder
2. configure your test runner args in .txt format. Test args will vary slightly depending on the type of tests you are trying to run. If you are unsure, you can always copy one of the txt file from example_args folder and edit to match your needs.

	**Required args**:
	- *path*: path to your build dir. For example: `/Users/yueshe/bling/src/out/Debug-iphonesimulator`
	- *xcode\_path*: path to your local xcode that you want to run the test with, e.g. `/Application/Xcode.app`
	- *mac\_toolchain\_cmd*: path to your mac\_toolchain, usually in your depot\_tools dir, e.g. `/Users/yueshe/depot_tools/mac_toolchain`
	- *target*: Test app name under src/out/Debug-iphonesimulator/ (for running on simulators) or src/out/Debug-iphoneos/ (for running on devices), e.g. `ios_chrome_ui_eg2tests_module-Runner.app`

	**Test specific args**:
	- *host\_app*: ONLY REQUIRED if you are running EG Tests, e.g. `ios_chrome_eg2tests.app`
	- *platform*: ONLY REQUIRED if you are running on simulators, e.g. `"iPad Pro (12.9-inch) (5th generation)"`. Note that quotation is needed.
	- *version*: ONLY REQUIRED if you are running on simulators, e.g. `17.4`

	**Optional args**:
	- *gtest\_filter*: List of tests that you would like to include in the test run, separated by `:`. By default the test runner will run all the test cases in your test app. Having a filter is highly recommended for running EG tests to save your test execution time. E.g. `"AddressBarPreferenceTestCase:SearchEngineTestCase"` (quotes required)

3. After you have created your own .txt arg file, run the command `./local_run.sh [your arg file name].txt`
(If you don't have Xcode installed, it might ask you to confirm whether you would like test runner to install it)
Pick the type of tests you would like to run, and it should start running...

4. At the end of the run, it should create a folder that looks like `run_[a random number]/`. The folders contains a bunch of relevant test result files such as test screenshots, logs, xcrun files, etc... just like what you would see in the CAS Output of a swarming run. Please be careful to not include those files in your cl.


### Running on a swarming bot
Still WIP, instructions coming soon... However, it will most likely utilize the script `src/tools/run-swarmed.py`, and have a very similar running process from above. E.g. `./run-swarmed.py < [your arg file name].txt`
