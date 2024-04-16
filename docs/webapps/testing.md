# [Web Apps](../README.md) - Testing


Please read [Testing In Chromium][13] for general guidance on writing tests in chromium.

The following tests are expected for writing code in this system:

* Unit tests
* Browser tests
* Integration tests

### Known Issues

- Unit tests currently cannot rely on `WebContents` functionality, as that is not built as part of unit test frameworks. Instead, they must use the `FakeWebAppUrlLoader` or `FakeWebAppDataRetriever` classes.
  - Note: This should be fixed in early 2023. See [bug][1] to make it easier to install apps in unit tests that require a web contents and [bug][2] to improve the `WebContents` dependency and current helper classes to allow the WebAppProvider system to not directly depend on `WebContents`.
- Installing web apps before the WebAppProvider system starts can be cumbersome.
  - Browser tests can use the `PRE_` test functionality to set up any state.
  - Unit tests must either load a static profile directory saved in the test data, or create a test-only way to explicitly delay the desired subsystem from starting.

## Unit tests

Unit tests have the following benefits:

* They are very efficient.
* They run on all relevant CQ trybots.
* They will always be supported by the [code coverage][8] framework.

Unit tests are the fastest tests to execute and are expected to be used to test most cases, especially error cases. They are usually built on the `WebAppTest` base class, and use the `FakeWebAppProvider` to customize (or not) the [dependencies][3] of the `WebAppProvider` system.

Notes

- UI elements do not work in unit tests, and the appropriate fakes must be used (see [External Dependencies][3]).
- If one of the external dependencies of the system cannot be faked out yet or the feature is tightly coupled to this, then it might make sense to use a browser test instead (or make that dependency fake-able).
- Please use the [`WebAppTest`][4] base class if possible.
- Unit tests based on `WebAppTest` print a snapshot of chrome://web-app-internals to console on test failures. This can be a powerful debugging tool. The command line flag `--disable-web-app-internals-log` can be used to disable this feature.

## Browser tests

With improved web app test support, most of the components should using unittests to cover the detailed test cases.

Creating an integration test (using the integration framework) should satisfy the need for end-to-end tests for major use-cases of your feature. However, you may need to create one due to:

- The unittest framework doesn’t support certain needs.
- You need end-to-end test, but using integration test framework has too much overhead.

Browser tests are much more expensive to run, as they run a fully functional browser. These tests are usually only created to test functionality that requires multiple parts of the system to be running or dependencies like the Sync service to be fully running and functional. It is good practice to have browsertests be as true-to-user-action as possible, to make sure that as much of our stack is exercised.

An example set of browser tests are in [`web_app_browsertest.cc`][6]. Please use the [`WebAppBrowserTestBase`][5] base class.

Notes

- Browser tests based on `WebAppBrowserTestBase` print a snapshot of chrome://web-app-internals to console on test failures. This can be a powerful debugging tool. The command line flag `--disable-web-app-internals-log` can be used to disable this feature.

## Integration tests

We have a custom integration testing framework that we use due to the complexity of our use-cases. See [integration-testing-framework.md][7] for more information.

**It is a good idea to think about your integration tests early & figure out your CUJs with the team. Having your CUJs and integration tests working early greatly speeds up development & launch time.**

Notes

- Integration tests using `WebAppIntegrationTestDriver` print a snapshot of chrome://web-app-internals to console on test failures. This can be a powerful debugging tool. The command line flag `--disable-web-app-internals-log` can be used to disable this feature.

## Testing OS integration

It is very common to test OS integration. By default, OS integration is suppressed if the test extends [`WebAppTest`][4]  or  [`WebAppBrowserTestBase`][5].

End-to-end OS integration testing is facilitated using the [`OsIntegrationTestOverride`][9]. If OS integration CAN be tested in an automated way, this class will do so. If not, the existence of this override will stub-out the OS integration at the lowest level to test as much of our code as possible.

## `Fake*` classes

A class that starts with `Fake` is meant to completely replace a component of the system. They inherit from a base class with virtual methods, and allow a test to specify custom behavior or checks. The component should seem to be working correctly to other system components, but with behavior that is defined by a test.

The most common pattern here is that the Fake will by default appear to work correctly, and a test can either specify it to return custom results, fail in specific ways, or simply check that it was used in the correct way.

An example is [fake_os_integration_manager.h][14], which pretends to successfully perform install, update, and uninstall operations on OS integration, but instead pretends to work and does simple bookkeeping for tests to check that it was called correctly.

## `Mock*` classes

A class that start with `Mock` is a [gmock][12] version of the class. This allows the user to have complete control of exactly what that class does, verify it is called exactly as expected, etc. These tend to be much more powerful to use than a `Fake`, as you can easily specify every possible case you might want to check, like which arguments are called and the exact calling order of multiple functions, even across multiple mocks. The downsides are:
* Mocks end up being very verbose to use, often at the expense of test readiability.
* Mocks require creating a mock class & learning how to use gmock.

These are generally not preferred to a "Fake".

## Tool: `FakeWebAppProvider`

The [`FakeWebAppProvider`][11] is basically a fake version of the WebAppProvider system, that uses the  [`WebAppProvider`][10] root class to set up subsystems and can be used to selectively set fake subsystems or shut them
down on a per-demand basis to test system shutdown use-cases.

By default, the `FakeWebAppProvider` will NOT start the `WebAppProvider` system, and it must be manually done so. This is usually done by calling  [`AwaitStartWebAppProviderAndSubsystems`][15].

## Common issue: Waiting

Many operations that happen at higher levels that the commands / scheduling system in the WebAppProvider require that tests wait for async operations to complete.

### Tabs & Browsers

* `AllBrowserTabAddedWaiter` - Waits for a tab to be added anywhere (works for both app browser and regular browser).
* `BrowserChangeObserver` - Waits for a browser to add or remove.

### Navigation & Loading

* `UrlLoadObserver` - Waits for given url to load anywhere.
* `content::TestNavigationObserver` - Waits for a navigation anywhere or in given WebContents. See StartWatchingNewWebContents to watch all web contents.
* `content::WebContentsObserver` - Can generally be used to wait for events on a given `content::WebContents`
  * To wait for `onload` to complete in a page, the `::DocumentOnLoadCompletedInPrimaryMainFrame` can be used if `WebContents::IsDocumentOnLoadCompletedInPrimaryMainFrame()` returns false.

### `WebAppProvider` commands

[`WebAppCommandManager::AwaitAllCommandsCompleteForTesting`][16] will wait for all commands to complete. This will mostly handle all tasks in the `WebAppProvider`.

## Common issue: External Dependency that isn't faked

Sometimes classes use a dependency that either doesn't work or isn't fake-able in our system.

1. Can you just not depend on that? The best way is to remove the dependency entirely if possible.
1. If there is a way to easily fake the dependency that is already supported, then do that next.
    - e.g. if it's a `KeyedService`, and the authors have a fake version you can use, then use that. See how it is used elsewhere.
1. Create a new interface for this new external dependency, put it on the `WebAppProvider`, and create a fake for it so that you can test with it faked.
1. If all else fails, use a browser test.

[1]: https://b/269618710
[2]: http://b/271124885
[3]: README.md#external-dependencies
[4]: https://source.chromium.org/search?q=web_app_test.h
[5]: https://source.chromium.org/search?q=WebAppBrowserTestBase
[6]: https://source.chromium.org/search?q=web_app_browsertest.cc
[7]: integration-testing-framework.md
[8]: ../testing/code_coverage.md
[9]: https://source.chromium.org/search?q=OsIntegrationTestOverride
[10]: https://source.chromium.org/search?q=WebAppProvider
[11]: https://source.chromium.org/search?q=FakeWebAppProvider
[12]: https://github.com/google/googletest/tree/HEAD/googlemock
[13]: ../testing/testing_in_chromium.md
[14]: https://source.chromium.org/search?q=FakeOsIntegrationManager
[15]: https://source.chromium.org/search?q=AwaitStartWebAppProviderAndSubsystems
[16]: https://source.chromium.org/search?q=AwaitAllCommandsCompleteForTesting
