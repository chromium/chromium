# How to create WebApp Integration Tests

Please see the the [Integration Testing Framework document][integration-testing-framework] for more information about specifics.

## 1. Familiarize yourself with the test generation, building, and running process.

- Run `chrome/test/webapps/generate_framework_tests_and_coverage.py` and verify nothing is outputted to the console.
- Build the tests by building the targets `browser_tests` and `sync_integration_tests`
  - (e.g. `autoninja -C out/Release browser_tests sync_integration_tests`)
- Run the generated tests, using the filter `--gtest_filter=*WebAppIntegration_*`:
  - `testing/run_with_dummy_home.py testing/xvfb.py out/Release/browser_tests --gtest_filter=*WebAppIntegration_*`
  - `testing/run_with_dummy_home.py testing/xvfb.py out/Release/sync_integration_tests --gtest_filter=*WebAppIntegration_*`
  - These will take a long time! No need to run them all, this is just so you know how to run them if you need to.

## 2. Determine what actions are needed for new critical user journeys

At the end of this process, all critical user journeys be decomposed into existing and new user actions and enumerated in the [Critical User Journey Spreadsheet][cuj-spreadsheet]. However, for this step the goal is to create any new actions that those journeys might need. See all existing actions in the [Actions sub-sheet][cuj-actions-sheet], which should give a good idea of what actions are currently supported in the framework.

Given the existing actions:
1. Draft what our critical user journeys will be.
2. Figure out if any new actions (or sites) will need to be implemented for these new journeys

Please [contact the team](#contact-the-team) if you have any questions or trouble here.

### Example - File Handlers
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
- Action 3 & 4: User action to allow or deny the dialog, with a "mode" of "Remember" "NotRemember".

## 3. Create action implementation with 'manual' tests

The goal of this step is to implement the actions (or other changes) that were determined by the last step. The action should be tested to make sure there are no bugs and it works on all applicable platforms.

For details about how to implement actions, see [Creating Actions in the `WebAppIntegrationTestDriver`][creating-actions].

Implementing or changing actions is usually done in [`WebAppIntegrationTestDriver`](https://source.chromium.org/search?q=WebAppIntegrationTestDriver&ss=chromium). If the action only works with the sync system, then it may have to be implemented in the `TestDelegate` interface and then in the [`TwoClientWebAppsIntegrationTestBase`](https://source.chromium.org/search?q=TwoClientWebAppsIntegrationTestBase&sq=&ss=chromium). See [test partitioning][test-partitioning] for more information.

Adding or modifying sites would happen in the [test data directory](https://source.chromium.org/chromium/chromium/src/+/main:chrome/test/data/web_apps/).

After the action and/or other changes are implemented, one or more "manual" tests should be implemented to ensure that they are working properly. See [test partitioning][test-partitioning] for more information about these various files, but basically:
- Non-sync related tests are put in the [regular browsertests][regular-browsertests] (windows-mac-linux only example [here][regular-browsertests-wml]).
- Sync-related tests are put in the [sync partition][sync-browsertests-wml].

### Example - `UninstallFromList`

[This](https://chromium-review.googlesource.com/c/chromium/src/+/3252543) is an example CL of implementing an action. It:
1. Adds the action implementation to the [`WebAppIntegrationTestDriver`](https://source.chromium.org/search?q=WebAppIntegrationTestDriver&ss=chromium).
2. Creates a simple "manual" (not generated) test of the action which runs on all platforms.

## 4. Add the new Critical User Journeys to the [spreadsheet][cuj-spreadsheet], generate, and submit them.

Finally, now that the changes are implemented and tested, they can be used in generated critical user journey tests. Please work with someone on the team (see [contact the team](#contact-the-team)) to review the new tests being added here and to help make sure nothing is missed. Working together you will add those new tests (and possibly actions) to the [spreadsheet][cuj-spreadsheet] so that you can execute the following steps:

### 4.1. Download the new actions and user journeys.

This command will download the data from the [spreadsheet][cuj-spreadsheet] into the local checkout.

```bash
chrome/test/webapps/download_data_from_sheet.py
```

### 4.2 Mark the action as supported.

To have the script actually generate tests using the new actions, they must be marked as supported in the [supported actions file][supported-actions]. The support is specified by a symbol per platform:

- 🌕 - Full coverage - This means that the driver implements this action in a way that completely matches (or almost matches) the code paths that are used when the user triggers this action.
- 🌓 - Partial coverage - This means that the testing framework implements this action in a way that accomplishes the state change or check, but does not fully match the code path that is used when the user triggers this action.
- 🌑 - No coverage - This means the action is not supported.

After recording an action as supported here, the next step should generate new tests!

### 4.2. Generate test changes.

This command will output all changes that need to happen to the critical user journeys.

```bash
chrome/test/webapps/generate_framework_tests_and_coverage.py
```

The output should:
1. Generate a coverage report for the change in the [data directory][script-data-dir].
1. Print new tests that need to be added to the integration browsertest files.
2. Print out test ids that need to be removed.

After you make changes to the integration browsertests, please re-run the above command to verify that all of the changes were performed and no mistakes were made.

Possible issues / Things to know:
1. Tests being removed are often replaced by a test that has all the same actions, plus some new ones. Finding these & replacing these tests inline helps the diff look more reasonable.
2. Sometimes a test that is removed is currently disabled. If this is the case, please find the new test that is being added that has all of the same actions as the old test, and also disable it (please add the same comments, etc). Because the old test was failing, likely the new test will be failing too.

After all tests are added, `git cl format` is often required. It's a good idea to test all of the new tests locally if you can, and then after local verification a patch can be uploaded, the the trybots can be run, and a review can be requested from the team.

### 4.3 (optional) Disable failing tests

If the "manual" browsertest didn't catch a bug that is now failing for the generated tests and there is no obvious fix for the bug, it is OK to submit the new tests as disabled. To do this:
1. Try to figure out generally why the generated tests are failing, or what the problem is, and create a bug.
2. Mark the affected tests as disabled and add a comment referencing the bug.
3. Make sure to call `chrome/test/webapps/generate_framework_tests_and_coverage.py` again to update the coverage percentage.
4. Submit the patch with tests disabled.
5. Create a follow up patch to fix the action and re-enable the tests.

Why is this OK? Adding the generated tests can be a big pain, especially if others are modifying the tests as well. It is often better to get them compiling and submitted quickly with a few tests disabled instead of waiting until everything works.

### Example - `UninstallFromList`

[Here](https://chromium-review.googlesource.com/c/chromium/src/+/3252305) is an example CL of adding generated tests for the `UninstallFromList` action addition. During the development of this action, it was discovered that some of the critical user journeys were incorrect and needed updating - you can see this in the downloaded file changes.

## Contact the team

To contact the team for help, send an email to pwa-dev@chromium.org and/or post on #pwas on Chromium Slack.

[cuj-spreadsheet]: https://docs.google.com/spreadsheets/d/1d3iAOAnojp4_WrPky9exz1-mjkeulOJVUav5QYG99MQ/edit#gid=2008870403
[cuj-actions-sheet]: https://docs.google.com/spreadsheets/d/1d3iAOAnojp4_WrPky9exz1-mjkeulOJVUav5QYG99MQ/edit#gid=1864725389
[regular-browsertests]: https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/views/web_apps/web_app_integration_browsertest.cc
[regular-browsertests-wml]: https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/views/web_apps/web_app_integration_browsertest_mac_win_linux.cc
[sync-browsertests-wml]: https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/sync/test/integration/two_client_web_apps_integration_test_mac_win_linux.cc
[test-partitioning]: todo
[supported-actions]: ../../chrome/test/webapps/data/framework_supported_actions.csv
[script-data-dir]: ../../chrome/test/webapps/data/
[integration-testing-framework]: integration-testing-framework.md
[creating-actions]: integration-testing-framework.md#creating-actions-in-the-webappintegrationtestdriver