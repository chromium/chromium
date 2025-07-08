## [Web Apps][2] - Installation

Installing a webapp can come from a variety of channels. This section serves to enumerate them all and show how they fit together in the installation pipeline.

### Flowchart

Here is a graphic of the installation flow:

![](webapp_installation_process.png)

[https://tinyurl.com/dpwa-installation-flowchart][4]

Note: The ExternallyManagedAppManager adds a few steps before this, and will sometimes (for placeholder apps) build a custom `WebAppInstallInfo` object to skip the 'build' steps.

### Installation Commands

There are a variety of [commands][5] used to install web apps. If introducing a new installation source, consider making a new command to isolate your operation (and prevent it from being complicated by other use-cases).

### Installation Sources

There are a variety of installation sources and expectations tied to those sources.

#### Omnibox install icon

User-initiated installation. To make the omnibox install icon visible, the document must: _Be promotable and installable_. NOT be inside of the scope of an installed WebApp with an effective [display mode display][6] mode that isn't `kBrowser`.

Triggers an install view that will show the name & icon to the user to confirm install. If the manifest also includes screenshots with a wide form-factor, then a more detailed install dialog will be shown.

This uses the [`FetchManifestAndInstallCommand`][7], providing just the `WebContents` of the installable page.

Fails if, after the user clicks : _After clicking on the install icon, the `WebContents` is no longer_ [_promotable_][8], _skipping engagement checks_. The user rejects the installation dialog.

#### 3-dot menu option "Install {App_Name}..."

User-initiated installation. To make the install menu option visible, the document must: _Be promotable and installable_. NOT be inside of the scope of an installed WebApp with an effective [display mode display][6] mode that isn't `kBrowser`.

Triggers an install view that will show the name & icon to the user to confirm install. If the manifest also includes screenshots with a wide form-factor, then a more detailed install dialog will be shown.

Calls [`FetchManifestAndInstallCommand`][7] with the `WebContents` of the installable page, and `fallback_behavior = FallbackBehavior::kUseFallbackInfoWhenNotInstallable`.

Fails if: _The user rejects the installation dialog_.

Notably, this option does not go through the same exact pathway as the [omnibox install icon][10], as it shares the call-site as the "Create Shortcut" method below. The main functional difference here is that if the site becomes no longer [promotable][8] in between clicking on the menu option and the install actually happening, it will not fail and instead fall back to a fake manifest and/or fake icons based on the favicon. Practically, this option doesn't show up if the site is not [promotable][8]. Should it share installation pathways as the the [omnibox install icon?][10] Probably, yes.

#### 3-dot menu option "Create Shortcut..."

User-initiated installation. This menu option is always available, except for internal chrome urls like chrome://settings.

Prompts the user whether the shortcut should "open in a window". If the user checks this option, then the resulting WebApp will have the [user display][11] set to `kStandalone` / open-in-a-window.


The document does not need to have a manifest for this install path to work. If no manifest is found, then a fake one is created with `start_url` equal to the document url, `name` equal to the document title, and the icons are generated from the favicon (if present).

Calls [`FetchManifestAndInstallCommand`][7] with the `WebContents` of the installable page, and `fallback_behavior = FallbackBehavior::kAllowFallbackDataAlways`.

Fails if: _The user rejects the shortcut creation dialog_.

#### Externally Managed Apps

These installations are more customizable than user installations, as these external app management surfaces need to specify all of the options up front (e.g. create shortcut on desktop, open in window, run on login, etc). The [`ExternalAppResolutionCommand`][14] is used for this purpose.

The general installation flow of an externally managed app is:

1. A call to [`ExternallyManagedAppProvider::SynchronizeInstalledApps`][16]
1. Finding all apps that need to be uninstalled and uninstalling them, find all
   apps that need to be installed and:
1. Enqueue an `ExternalAppResolutionCommand` for each app to start resolving
   what the final behavior should be.
1. Each command loads the url for the app.
1. If the url is successfully loaded, the command proceeds with a regular
   installation pipeline (fetching manifest, icons, etc).
1. If the url fails to fully load (usually a redirect if the user needs to sign
   in or corp credentials are not installed), and the external app manager
   specified a [placeholder app was required][18] then an `InstallPlaceholderJob`
   is used to install a placeholder app.

These placeholder apps are not meant to stay, and to replace them with the intended apps, the following occurs:

1. When a new installation is triggered for the same install URL, the
   `ExternalAppResolutionCommand` is given the ID of the existing placeholder
   app.
1. After the new app is successfully installed, the placeholder app is
   uninstalled.

#### Sync

When an app is installed via sync on a non-ChromeOS device, it is initially
installed in a "suggested" state (`InstallState::SUGGESTED_FROM_ANOTHER_DEVICE`)
without full OS integration. See [Installation State][23] in the concepts doc
for more details. On ChromeOS, synced apps are always fully installed with OS
integration.

##### Retry on startup

Sync installs have a few extra complications:

- They need to be immediately saved to the database & be installed eventually.
- Many are often queued up during a new profile sign-in, and it's not uncommon for the user to quit before the installation queue finishes.

Due to this, unlike other installs, a special [`WebApp::is_from_sync_and_pending_installation`][24] (protobuf variable is saved in the database. WebApps with this set to true are treated as not fully installed, and are often left out of app listings. This variable is reset back to `false` when the app is finished installing.

To handle the cases above, on startup when the database is loaded, any WebApp with `is_from_sync_and_pending_installation` of `true` will be re-installed inside of [`WebAppSyncBridge::MaybeInstallAppsFromSyncAndPendingInstallation`][25]

### Installation State Modifications

#### Triggering a full installation

On non-ChromeOS devices, an app can be in a "suggested" state (e.g. from sync)
or "installed without OS integration" (e.g. preinstalled). To trigger a full
installation with OS integration, the user can:
*   Install the app again via a normal installation flow (e.g. using the
    omnibox install icon).
*   Right-click the app on `chrome://apps` and select "Install".

Programmatically, this is handled by the [`InstallAppLocallyCommand`][26].

#### Creating Shortcuts


On non-ChromeOS devices, an app can be [not locally installed][23]. To become locally installed, the user can follow a normal install method (the install icon will show up), or they can interact with the app on `chrome://apps`.

The `chrome://apps` code is unique here, and instead of re-installing the app, it schedules an [`InstallAppLocallyCommand`][26] to set the app as locally installed and trigger OS integration.

#### Creating Shortcuts

Similarly to above, in `chrome://apps` the user can "Create Shortcuts..." for a web app. This should overwrite any shortcuts already created, and basically triggers OS integration to install shortcuts again via an [`OsIntegrationSynchronizeCommand`][27].

[2]: README.md
[4]: https://tinyurl.com/dpwa-installation-flowchart
[5]: https://source.chromium.org/search?q=f:install%20f:web_applications%2Fcommands&sq=&ss=chromium
[6]: concepts.md#effective-display-mode
[7]: https://source.chromium.org/search?q=FetchManifestAndInstallCommand&ss=chromium
[8]: concepts.md#promotable
[10]: #omnibox-install-icon
[11]: concepts.md#user-display-mode
[14]: https://source.chromium.org/search?q=ExternalAppResolutionCommand&sq=&ss=chromium%2Fchromium%2Fsrc
[15]: https://source.chromium.org/search?q=f:web_app_install_utils.h%20"ApplyParamsToFinalizeOptions"
[16]: https://source.chromium.org/search?q=ExternallyManagedAppProvider::SynchronizeInstalledApps
[17]: #flowchart
[18]: https://source.chromium.org/search?q=ExternalInstallOptions::install_placeholder
[21]: https://source.chromium.org/search?q=f:install_from_sync_command.h
[23]: concepts.md#installation-state
[24]: https://source.chromium.org/search?q=WebApp::is_from_sync_and_pending_installation
[25]: https://source.chromium.org/search?q=WebAppSyncBridge::MaybeInstallAppsFromSyncAndPendingInstallation
[26]: https://source.chromium.org/search?q=InstallAppLocallyCommand
[27]: https://source.chromium.org/search?q=OsIntegrationSynchronizeCommand
