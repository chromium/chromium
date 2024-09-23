## Web Apps

### What are web apps?

Simply put web apps are sites that the user installs onto their machine mimicking a native app installed on their respective operating system.

#### User entry points

Sites that meet our install promotion requirements will have an install prompt appear in the omnibox on the right. Users can also install any site they like via `Menu > More tools > Create shortcut`....

- Example site: https://developers.google.com/

Users can see all of their web apps on chrome://apps (viewable on non-ChromeOS).

#### Developer interface

Sites customize how their installed site integrates at the OS level using a [web app manifest][2]. See developer guides for in depth overviews:

- https://web.dev/progressive-web-apps/
- https://web.dev/codelab-make-installable/

#### Presentation

See [https://tinyurl.com/dpwa-architecture-public][3] for presentation slides.

### Terms & Phrases

See [Web Apps - Concepts][4].

### Debugging

Use [chrome://web-app-internals][5] to inspect internal web app state. For Chromium versions prior to M93 use [chrome://internals/web-app][6].

To test the behavior of the web app itself, Chrome DevTools Protocol can be used. See [Instruction of using PWA via CDP][59].

### Documentation Guidelines

- Markdown documentation (files like this):
  - Contains information that can't be documented in class-level documentation.
  - Answers questions like: What is the goal of a group of classes together? How does a group of classes work together?
  - Explains concepts that are used across different files.
  - Should be unlkely to become out-of-date.
    - Any source links should link to a codesearch 'search' page and not the specific line number.
    - Avoid implementation details.
- Class-level documentation (documentation in header files):
  - Answers questions like: Why does this class exist? What is the responsibility of this class? If this class involves a process with stages, what are those stages / steps?
  - Should be updated actively when that given file is changed.
- Documentation inside of methods
  - Should explain the "why" of code if it is not clear.
  - Should be avoided otherwise.

### What makes up Chromium's implementation?

The task of turning websites into "apps" in the user's OS environment has many parts to it. Before going into the parts, here is where they live:

![](webappprovider_component_ownership.jpg)

See source [here][7].

- The `WebAppProvider` core system lives on the `Profile` object.
- The `WebAppUiManagerImpl` also lives on the `Profile` object (to avoid deps issues).
- The `AppBrowserController` (typically `WebAppBrowserController` for our interests) lives on the `Browser` object.
- The `WebAppTabHelper` lives on the `WebContents` object.

While most on-disk storage is done in the [`WebAppSyncBridge`][8], the system also sometimes uses the `PrefService`. Most of these prefs live on the `Profile` (`profile->GetPrefs()`), but some prefs are in the global browser prefs (`g_browser_process->local_state()`).

Presentation: [https://tinyurl.com/dpwa-architecture-public][3]

Older presentation: [https://tinyurl.com/bmo-public][9]

### Architecture Philosophy

There are a lot of great guidelines within Chromium

- [Style guides][45]
- [Dos and Don'ts][47]
- etc.

Other than general guidance of minimal complexity and having single-responsibility classes, some goals of our system:

- Tests should operate on the [public interface][48] as much as possible. Refactors to the internal system should not involve fixing / modifying tests.
- [External dependencies][49] should be behind fake-able interfaces, allowing unit & browser tests to swap these out. However, internal parts of our system should not be mocked out or faked - this tightly couples the internal implementation to our tests. If it is impossible to trigger a condition with the public interface, then that condition should be removed (or the public interface improved).
  - Here is a nice [presentation][44] about testing that might clarify our approach.

### Public Interface

This public interface should (and will) be improved, however this is the basic state as of 2022/11/09:

- `WebAppCommandScheduler`. Internally this schedules `WebAppCommand`s to do safe operations on the system.
  - This already includes a variety of operations like installation, launching, etc.
- Observers like the `AppRegistrarObserver` or `WebAppInstallManagerObserver`. However, users of these MUST NOT modify the web app system in the observation call - this can cause race conditions and weird re-entry bugs.
- Items exposed from the locks given to commands or callbacks:
  - `WebAppRegistrar`
  - Writing to the database using `ScopedRegistryUpdate` and the `WebAppSyncBridge`.
  - Pref reading & writing
  - `WebAppIconManager` supports icon fetch for a given web app.
  - etc - see the documentation on the lock for more guidance.

Some parts of the system that are used within commands:

- `WebAppUrlLoader` & `WebAppDataRetriever` are used in commands, but this interface could be improved & does not have a formal factory yet.
- `WebAppInstallFinalizer` is used in commands and could be improved.

### External Dependencies

The goal is to have all of these behind an abstraction that has a fake to allow easy unit testing of our system. Some of these dependencies are behind a nice fake-able interface, and some are not (yet).

- **Extensions** - Some of our code still talks to the extensions system, specifically the `PreinstalledWebAppManager`.
- **`content::WebContents`**
  - **`WebAppUrlLoader`** - load a given url in a `WebContents`. Faked by `FakeWebAppUrlLoader`.
  - **`WebAppDataRetriever`** - retrieve installability information, the manifest, or icons from a **`WebContents`**. Faked by `FakeWebAppDataRetriever`.
  - Misc:
    - Navigation completion in WebAppTabHelper, used to kick off update commands.
    - Listening to destruction.
    - IsPrimaryMainFrame() and other filtering.
    - etc
- **OS Integration**: Each OS integration has fairly custom code on each OS to do the operation. This is difficult to coordinate and test. Currently the `OsIntegrationManger` manages this, which has a fake version.
- **Sync system**
  - There is a tight coupling between our system and the sync system through the WebAppSyncBridge.
  - Faking this is easy and is handled by the `FakeWebAppProvider`.
- **UI**: There are parts of the system that are coupled to UI, like showing dialogs, determining information about app windows, etc. These are put behind the `WebAppUiManager`, and faked by the `FakeWebAppUiManager`.
- **Policy**: Our code depends on the policy system setting it's policies in appropriate prefs for us to read. Because we just look at prefs, we don't need a "fake" here.

### Databases / sources of truth

These store data for our system. Some of it is per-web-app, and some of it is global.

- **`WebAppRegistrar`**: This attempts to unify the reading of much of this data, and also holds an in-memory copy of the database data (in WebApp objects).
- **`WebAppDatabase`** / **`WebAppSyncBridge`**: This stores the web_app.proto object in a database, which is the preferred place to store information about a web app.
- **Icons on disk**: These are managed by the `WebAppIconManager` and stored on disk in the user's profile.
- **Prefs**: The `PrefService` is used to store information that is either global, or needs to persist after a web app is uninstalled. Most of these prefs live on the `Profile` (`profile->GetPrefs()`), but some prefs are in the global browser prefs (`g_browser_process->local_state()`). Some users of prefs:
  - AppShimRegistry
  - UserUninstalledPreinstalledWebAppPrefs
- **OS Integration**: Various OS integration requires storing state on the operating system. Sometimes we are able to read this state back, sometimes not.

None of this information should be accessed without an applicable 'lock' on the system.

### Managers

These are used to encapsulate common responsibilities or in-memory state that needs to be stored.

### Commands

Commands are used to encapsulate operations in the system, and use Locks to ensure that your operation has isolation from other operations.

- If you need to change something in the WebAppProvider system, you should probably use a command.
- Commands talk to the system using locks they are granted. The locks should offer access to "managers" that the commands can use.
- Commands expose a `ToDebugValue()` method that is logged on completion and exposed in the chrome://web-app-internals. This can be very helpful for debugging and bug reports.

Note: There are DVLOGs in the `WebAppCommandManager` that can be helpful.

### Locks / `WebAppLockManager`

Locks allow operations to receive appropriate protections for what they are doing. For example, an `AppLock` will guarantee that no one is modifying, installing, or uninstalling that AppId while it is granted.

Locks contain assessors that allow the user to access parts of the web app system. This is the safest way to read from the system.

Note: There are DVLOGs in the `WebAppLockManager` that can be helpful.

### OS Integration

Anything that involves talking to the operating system. Usually has to do with adding, modifying, or removing the os entity that we register for the web app.

## Deep Dives

- [/docs/webapps/installation_pipeline.md][34]
- [/docs/webapps/manifest_representations.md][35]
- [/docs/webapps/integration-testing-framework.md][11]
- [/docs/webapps/os_integration.md][50]
- [/docs/webapps/manifest_update_process.md][51]
- [/docs/webapps/isolated_web_apps.md][52]
- [/docs/webapps/webui_web_app.md][54]

## How To Use

See the [public interface][48] section about which areas are generally "publicly available".

The system is generally unit-test-compabible through the [`FakeWebAppProvider`][40], which is created by default in the `TestingProfile`. Sometimes tests require using the [`AwaitStartWebAppProviderAndSubsystems`][41] function in the setup function of the test to actually start the web app system & wait for it to complete startup. See [testing.md][58] for more information.

There is a long-term goal of having the system be easily fake-able for customers using it. The best current 'public interface' distinction of the system is the `WebAppCommandScheduler`.

To access or change information about a web app:

- Obtain a lock from the `WebAppLockManager`, or (preferably) create a command with the relevant lock description.
- When the lock is obtained (or the command is started with the lock), use the lock to access the data you need.
  - Generally, you should use the `WebAppRegistrar` to get the data you need. This unifies many of our data sources into one place.
- If changing data, change the data depending on the source of truth.
  - For information in the database, use a `ScopedRegistryUpdate`.
  - Otherwise use the relevant manager / helper to modify the data.
  - Some things expect "observers" to be notified. That integration is currently in `WebAppSyncBridge`, but can be pulled out.

Other guides:

- [/docs/webapps/why-is-this-test-failing.md][36]
- [/docs/webapps/how-to-create-webapp-integration-tests.md][37]

## Debugging

### chrome://web-app-internals

This page allows you to see all of the internal information about the WebAppProvider system, including a truncated log of the debug information of the last run commands.

It is often very useful to ask users to attach a copy of this page in bug reports.

The integration tests will print out the [contents of this page][57] if a test fails, which can help debug that failure as well.

### DVLOGs

The codebase has a number of useful DVLOGs:
- web_app_command_manager.cc: These will log various state changes of commands and the debug values on completion.
- web_app_lock_manager.cc: This will log lock requests and any 'held or pending' lock holders for the requested lock at the time of the request. This does not necessarily mean those locks are blocking (as they may be shared locks) so you will have to look at the request locations to determine the status.

## Testing

Please see [testing.md][58].

## Relevant Classes

#### [`WebAppProvider`][12]

This is a per-profile object housing all the various web app subsystems. This is the "main()" of the web app implementation where everything starts.

#### [`WebApp`][13]

This is the representation of an installed web app in RAM. Its member fields largely reflect all the ways a site can configure their [web app manifest][2] plus miscellaneous internal bookkeeping and user settings.

#### [`WebAppRegistrar`][14]

This is where all the [`WebApps`][13] live in memory, and what many other subsystems query to look up any given web app's fields. Mutations to the registry have to go via ScopedRegistryUpdate or [WebAppSyncBridge][16].

Accessing the registrar should happen through a Lock. If you access it through the `WebAppProvider`, then know that you are reading uncommitted (and thus unsafe) data.

Why is it full of `GetAppXYZ()` getters for every field instead of just returning a `WebApp` reference? This is primarily done because the value may depend on multiple sources of truth. For example, whether the app should be run on OS login depends on both the user preference (stored in our database) and the administrator's policy (stored separately & given to us in-memory using prefs) Historically this was originally done because WebApps used be stored both in our database and extensions, and this served to unify the two.

#### [`WebAppSyncBridge`][16]

This is "bridge" between the WebAppProvider system's in-memory representation of web apps and the sync system's database representation (along with sync system functionality like add/remove/modify operations). This integration is a little complex and deserves it's own document, but it basically: _Stores all WebApps into a database and updates the database if any fields change_. Updates the system when there are changes from the sync system. _Installs new apps, uninstalls apps the user uninstalled elsewhere, updates metadata like user display mode preference, etc_. Tells the sync system if there are local changes (installs, uninstalls, etc).

There is also a slide in a presentation [here][18] which illustrates how this system works, but it may be out of date.

Note: This only stores per-web-app data, and that data will be deleted if the web app is uninstalled. To store data that persists after uninstall, or applies to a more general scope than a single web app, then the `PrefService` can be used, either on the `Profile` object (per-profile data, `profile->GetPrefs()`) or on the browser process `(`g_browser_process->local_state()``). Example of needing prefs: Storing if an app was previously installed as a preinstalled app in the past. Information is needed during chrome startup before profiles are loaded. A feature needs to store global data - e.g. "When was the last time we showed the in-product-help banner for any webapp?"

#### [`ExternallyManagedAppManager`][19]

This is for all installs that are not initiated by the user. This includes [preinstalled apps][20], [policy installed apps][21] and [system web apps][22].

These all specify a set of [install URLs][23] which the `ExternallyManagedAppManager` synchronises the set of currently installed web apps with.

#### [`WebAppInstallFinalizer`][24]

This is the tail end of the installation process where we write all our web app metadata to [disk][25] and deploy OS integrations (like [desktop shortcuts][26] and [file handlers][27] using the [`OsIntegrationManager`][28].

This also manages the uninstallation process.

#### [`WebAppUiManager`][29]

Sometimes we need to query window state from chrome/browser/ui land even though our BUILD.gn targets disallow this as it would be a circular dependency. This [abstract class][30] + [impl][31] injects the dependency at link time (see [`WebAppUiManager::Create()`'s`][32] `declaration and definition locations`).

#### [`AppShimRegistry`][33]

On Mac OS we sometimes need to reason about the state of installed PWAs in all profiles without loading those profiles into memory. For this purpose, `AppShimRegistry` stores the needed information in Chrome's "Local State" (global preferences). The information stored here includes:

  - All profiles a particular web app is installed in.
  - What profiles a particular web app was open in when it was last used.
  - What file and protocol handlers are enabled for a web app in each profile it is installed in.

This information is used when launching a web app (to determine what profile or profiles to open the web app in), as well as when updating an App Shim (to make sure all file and protocol handlers for the app are accounted for).

[2]: https://www.w3.org/TR/appmanifest/
[3]: https://tinyurl.com/dpwa-architecture-public
[4]: concepts.md
[5]: chrome://web-app-internals
[6]: chrome://internals/web-app
[7]: https://docs.google.com/drawings/d/1TqUF2Pqh2S5qPGyA6njQWxOgSgKQBPePKPIH_srGeRk/edit?usp=sharing
[8]: #webappsyncbridge
[9]: https://tinyurl.com/bmo-public
[11]: integration-testing-framework.md
[12]: /chrome/browser/web_applications/web_app_provider.h
[13]: /chrome/browser/web_applications/web_app.h
[14]: /chrome/browser/web_applications/web_app_registrar.h
[16]: /chrome/browser/web_applications/web_app_sync_bridge.h
[18]: https://docs.google.com/presentation/d/e/2PACX-1vQxYZoCyhZ4xHS4pVuBC9YoE0O-QpW2Wj3scl6jtr3TEYheeod5Ch4b7OVEQEj_Hc6PM1RBGzovug3C/pub?start=false&loop=false&delayms=3000&slide=id.g59d9cb05b6_6_5
[19]: /chrome/browser/web_applications/externally_managed_app_manager.h
[20]: /chrome/browser/web_applications/preinstalled_web_app_manager.h
[21]: /chrome/browser/web_applications/policy/web_app_policy_manager.h
[22]: /chrome/browser/ash/system_web_apps/system_web_app_manager.h
[23]: /chrome/browser/web_applications/external_install_options.h
[24]: /chrome/browser/web_applications/web_app_install_finalizer.h
[25]: /chrome/browser/web_applications/web_app_database.h
[26]: /chrome/browser/web_applications/os_integration/web_app_shortcut.h
[27]: /chrome/browser/web_applications/os_integration/web_app_file_handler_manager.h
[28]: /chrome/browser/web_applications/os_integration/os_integration_manager.h
[29]: /chrome/browser/ui/web_applications/web_app_ui_manager_impl.h
[30]: /chrome/browser/web_applications/web_app_ui_manager.h
[31]: /chrome/browser/ui/web_applications/web_app_ui_manager_impl.h
[32]: https://source.chromium.org/search?q=WebAppUiManager::Create
[33]: /chrome/browser/web_applications/app_shim_registry_mac.h
[34]: installation_pipeline.md
[35]: manifest_representations.md
[36]: why-is-this-test-failing.md
[37]: how-to-create-webapp-integration-tests.md
[38]: /chrome/browser/ui/web_applications/web_app_browsertest.cc
[40]: /chrome/browser/web_applications/test/fake_web_app_provider.h
[41]: https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/web_applications/test/web_app_install_test_utils.cc;l=40?q=AwaitStartWebAppProviderAndSubsystems&ss=chromium
[44]: https://www.youtube.com/watch?v=EZ05e7EMOLM
[45]: /styleguide/styleguide.md
[47]: /styleguide/c++/c++-dos-and-donts.md
[48]: #public-interface
[49]: #external-dependencies
[50]: os_integration.md
[51]: manifest_update_process.md
[52]: isolated_web_apps.md
[54]: webui_web_app.md
[57]: https://source.chromium.org/search?q=WebAppInternalsHandler::BuildDebugInfo
[58]: testing.md
[59]: cdp-integration.md
