# How to create WebApp Integration Tests

Please see the the [Integration Testing Framework document][integration-testing-framework] for more information about specifics.

## 1. Familiarize yourself with the test generation, building, and running process.

- Run `chrome/test/webapps/generate_framework_tests_and_coverage.py` and verify nothing is outputted to the console.
- Build the tests by building the targets `browser_tests` and `sync_integration_tests`
  - (e.g. `autoninja -C out/Release browser_tests sync_integration_tests`)
- Run the generated tests, using the filter `--gtest_filter=WebAppIntegration*`:
  - Note: These will take a long time! No need to run them all, this is just so you know how to run them if you need to.
  - `testing/run_with_dummy_home.py testing/xvfb.py out/Release/browser_tests --gtest_filter=WebAppIntegration*`
  - `testing/run_with_dummy_home.py testing/xvfb.py out/Release/sync_integration_tests --gtest_filter=WebAppIntegration*`

## 2. Determine what actions are needed for new critical user journeys

The goal of this step is to put all critical user journeys for the feature into the [critical user journeys file][cuj-spreadsheet] and any new actions/enums into their respective files ([actions][cuj-actions-sheet], [enums][cuj-enums-sheet]).

Steps:
1. Explore the existing [actions][cuj-actions] and [critical user journeys][cuj-spreadsheet] to get familiar with the existing support.
2. Draft, possibly just in english, what all of the critical user journeys are.
  * Tip: Try not to 'collapse' multiple user journeys into one. It's easier to catalog them this way, and the testing framework script will collapse journeys for you during test generation.
3. (optional) Draft what new actions (or sites) will need to be implemented for these new journeys.
4. [Contact the team](#contact-the-team) to schedule a small work session to turn your CUJs into entries in the files.

See the example [below](#example---file-handlers).

## 3. Create action implementation with 'manual' tests

The browsertest files are split into two sections, manual tests and script-generated tests. Before generating all of the new tests, the next step is to
1. Implement the new actions that were determined by the last step.
2. Include a simple 'manual' test to verify it is working correctly.

See the [example browsertest][regular-browsertests] file to see the manual tests at the top, written by the action authors.

For details about how to implement actions, see [Creating Actions in the `WebAppIntegrationTestDriver`][creating-actions]. Implementing or changing actions is usually done in [`WebAppIntegrationTestDriver`](https://source.chromium.org/search?q=WebAppIntegrationTestDriver&ss=chromium). If the action only works with the sync system, then it may have to be implemented in the `TestDelegate` interface and then in the [`WebAppIntegrationTestBase`](https://source.chromium.org/search?q=WebAppIntegrationTestBase&sq=&ss=chromium). The [dPWA team](#contact-the-team) should have informed you if there was anything specific you need to do here.

Before submitting, make sure to also [run the trybots on mac][running-mac-tests], as these are sometimes disabled on the CQ.

If in Step 2 above the team concluded a "Site" must be modified or created, these are located in the [test data directory](/chrome/test/data/web_apps/).

See the example [below](#example---uninstallfromlist).

### Why write a 'manual' test when they are about to ge generated?
Implementing an action is often flaky and uncovers bugs. If all of the tests are in the first CL and it has problems, this causes large reverts or (even worse) sheriffs to manually disable many tests over the next few days. To prevent this complexity, this first CL should only include a few manual tests to make sure that everything is working correctly before more tests are generated.

## 4. Add the new Critical User Journeys to the [file][cuj-spreadsheet], generate, and submit them.

Finally, now that the changes are implemented and tested, they can be used in generated critical user journey tests.

### 4.1. Mark the action as supported.

Add to (or modify) this [file][supported-actions] marking the new actions as supported.

To have the script actually generate tests using the new actions, they must be marked as supported in the [supported actions file][supported-actions]. The support is specified by a symbol per platform:

- 🌕 - Full coverage - This means that the driver implements this action in a way that completely matches (or almost matches) the code paths that are used when the user triggers this action.
- 🌓 - Partial coverage - This means that the testing framework implements this action in a way that accomplishes the state change or check, but does not fully match the code path that is used when the user triggers this action.
- 🌑 - No coverage - This means the action is not supported and any tests using this action will only be partially generated.

If the action you have implemented is not present in the [file][supported-actions], please add it.

### 4.2. Generate test changes.

This command will output all changes that need to happen to the critical user journeys.

```bash
chrome/test/webapps/generate_framework_tests_and_coverage.py
```

The output should:
1. Generate a coverage report for the change in the [data directory][script-data-dir].
2. Print new tests that need to be manually copied to the integration browsertest files.
3. Print out test ids that need to be removed.

Note:
1. The option `--delete-in-place` can be used to remove all tests that aren't disabled by sheriffs.
2. The option `--add-to-file` can be used to add new tests to existing test files. If the test file does not exist, the expected file names and tests will be printed out to the console. You will have to manually create the file, copy-and-paste the tests to the new file and add the file to the BUILD file.

After you make changes to the integration browsertests, please re-run the above command to verify that all of the changes were performed and no mistakes were made. If all looks right, the script will output nothing to console when run a second time.

Possible issues / Things to know:
1. Tests being removed are often replaced by a test that has all the same actions, plus some new ones. Finding these & replacing these tests inline helps the diff look more reasonable.
2. Sometimes a test that is removed is currently disabled. If this is the case, **please find the new test that is being added that has all of the same actions as the old test, and also disable it (please add the same comments, etc)**. Because the old test was failing, likely the new test will be failing too.

After all tests are added, `git cl format` is often required. It's a good idea to test all of the new tests locally if you can, and then after local verification a patch can be uploaded, the the trybots can be run, and a review can be requested from the team.

Before submitting, make sure to also [run the trybots on mac][running-mac-tests], as these are sometimes disabled on the CQ.

### 4.3. Run new tests locally.

It is recommended to run the new tests locally before testing them on trybots.

This command will to generate the gtest_filter for all the new and modified tests.

```bash
chrome/test/webapps/generate_gtest_filter_for_added_tests.py --diff-strategy <upstream|committed|staged|unstaged>
```
This script uses a default diff strategy that includes uncommitted, staged, and committed changes to the UPSTREAM. See the `--diff-strategy` option for more options here.

The output should print out the gtest_filter for any new (or modified) tests in `browser_tests` and `sync_integration_tests`.

The output format will be
```bash
browser_tests --gtest_filter=<test_name>

sync_integration_tests --gtest_filter=<test_name>
```

You can run the tests by adding the path to `browser_tests` or `sync_integration_tests` binaries.

### 4.4. (optional) Disable failing tests

If the "manual" browsertest didn't catch a bug that is now failing for the generated tests and there is no obvious fix for the bug, it is OK to submit the new tests as disabled. To do this:
1. Try to figure out generally why the generated tests are failing, or what the problem is, and create a bug.
2. Mark the affected tests as disabled and add a comment referencing the bug.
3. Make sure to call `chrome/test/webapps/generate_framework_tests_and_coverage.py` again to update the coverage percentage.
4. Submit the patch with tests disabled.
5. Create a follow up patch to fix the action and re-enable the tests.

Why is this OK? Adding the generated tests can be a big pain, especially if others are modifying the tests as well. It is often better to get them compiling and submitted quickly with a few tests disabled instead of waiting until everything works.

## Example - `UninstallFromList`

[This](https://chromium-review.googlesource.com/c/chromium/src/+/3252543) is an example CL of implementing an action. It:
1. Adds the action implementation to the [`WebAppIntegrationTestDriver`](https://source.chromium.org/search?q=WebAppIntegrationTestDriver&ss=chromium).
2. Creates a simple "manual" (not generated) test of the action which runs on all platforms.

[Here](https://chromium-review.googlesource.com/c/chromium/src/+/3252305) is an example CL of adding generated tests for the `UninstallFromList` action addition. During the development of this action, it was discovered that some of the critical user journeys were incorrect and needed updating - you can see this in the downloaded file changes.

## Example - File Handlers
The file handlers feature:
- Allows a web app to register itself as a file handler on the operating system.
- Launches the web app when a registered file is launched by the system.
- Protects the user by presenting them with a confirmation dialog on launch, allowing the user to confirm or deny the launch.

What critical user journeys will be needed? Generally:
- Verify that installing a site with file handlers registers that site as a file handler on the system.
- Verify that launching a registered file will trigger the user confirmation dialog
- Verify that each user choice causes the correct behavior to happen.
  - "allow without remember" will launch the app. If the file is launched again, the user should be presented with the dialog again.
  - "allow with remember" will launch the app. If the file is launched again, the user should NOT be presented with the dialog.
  - "deny without remember" will NOT launch the app. If the file is launched again, the user should be presented with the dialog again.
  - "deny with remember" will NOT launch the app and unregister the app as a file handler. The operating system should no longer have the file registered with the web app.

The existing [actions][cuj-actions-sheet] already have a lot of support for installing, launching, checking if a window was created, etc. The following changes will have to happen:
- Modify an existing site (maybe Site B?), or create a new site (Site D? File Handler?), which handles a test file type in it's manifest.
  - Because multiple browsertests can be running at the same time on a trybot, this type will probably have to be uniquely generated per test to avoid conflicts.
- Action 1: detect if a file type is registered on an operating system.
  - This will be unique per operating system.
- Action 2: Launch the associated file.
  - Due to operating system complexities, it might be hard to have this be a true 'launch from the operating system'. It may have to be faked in our code, but we want it to be as close as possible to what would happen if the operating system actually launched the file.
- Action 3 & 4: User action to allow or deny the dialog, with an argument of "Remember" "NotRemember".

## Contact the team

To contact the team for help, send an email to pwa-dev@chromium.org and/or post on #pwas on Chromium Slack.

[cuj-spreadsheet]: /chrome/test/webapps/data/critical_user_journeys.md
[cuj-actions-sheet]: /chrome/test/webapps/data/actions.md
[cuj-enums-sheet]: /chrome/test/webapps/data/enums.md
[regular-browsertests]: https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/views/web_apps/web_app_integration_browsertest.cc
[regular-browsertests-wml]: https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/views/web_apps/web_app_integration_browsertest_mac_win_linux.cc
[sync-browsertests-wml]: https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/sync/test/integration/two_client_web_apps_integration_test_mac_win_linux.cc
[supported-actions]: ../../chrome/test/webapps/data/framework_supported_actions.csv
[script-data-dir]: ../../chrome/test/webapps/data/
[integration-testing-framework]: integration-testing-framework.md
[creating-actions]: integration-testing-framework.md#creating-action-implementations
[running-mac-tests]: integration-testing-framework.md#running-the-tests-on-mac
