## Web Apps - Concepts

### Manifest, or WebManifest

This refers to the document described by the [appmanifest][2] spec, with some extra features described by [manifest-incubations][3]. This document describes metadata and developer configuration of an installable web app.

For code representations of the manifest see [the list][4].

### Manifest Link

A manifest link is something that looks like this in a html document:

```html
<link rel="manifest" href="manifest.webmanifest">
```

This link ties the manifest to the document, and subsequently used in the spec algorithms defined in [appmanifest][2] or [manifest-incubations][3] to describe the webapp and determine if it is installable.

### Installable

If a document or page is considered "installable", then the user agent can create some form of installed web app for that page. To be installable, [web_app::CanCreateWebApp][5] must return true, where:

- The user profile must allow webapps to be installed
- The web contents of the page must not be crashed
- The last navigation on the web contents must not be an error (like a 404)
- The url must be `http, https`, or `chrome-extension`

This is different from [promotable][6] below, which determines if Chrome will promote installation of the page.

### Promotable

A document is considered "promotable" if it fulfills a set of criteria. This criteria may change to further encourage a better user experience for installable web apps. There are also a few optional checks that depend on the promotability checker. This general criteria as of 2022/09/08:

- _The document contains a manifest link_.
- The linked manifest can be processed [according][7] to the spec and is valid.
- The processed manifest contains the fields:
  - `name`
  - `start_url`
  - `icons` with at least one icon with a valid response that is a parsable image.
  - `display` field that is not `"browser`"
- "Serviceworker check": The `start_url` is 'controlled' (can be served by) a [serviceworker][8] with a fetch handler. **Optionally turned off**
  - Note: This is expected to be removed in Q4 2022.
- _"Engagement check": The user has engaged with, or interacted with, the page or origin a certain amount (currently at least one click and some seconds on the site). **Optionally turned off**_

Notes:

- Per spec, the document origin and the `start_url` origin must match.
- Per spec, the `start_url` origin does not have to match the `manifest_url` origin
- The `start_url` could be different from the `document_url`.

### Manifest id

The `id` specified in the manifest represents the identity of the web app. The manifest id is processed following the algorithm described in [appmanifest specification][9] to produce the app's identity. In the web app system, the app's [identifier][10] is [hashed][11] to be stored to [WebApp->app_id()][12].

If a manifest is discovered during any sort of page load, then the update process is initiated for that manifest. If it resolves to an `app_id` that is installed, then it will perform an update. See [documentation][20] for more information.

### Scope

Scope refers to the prefix that a WebApp controls. All paths at or nested inside of a WebApp's scope are thought of as "controlled" or "in-scope" of that WebApp. This is a simple string prefix match. For example, if `scope` is `/my-app`, then the following will be "in-scope":

- `/my-app/index.html`
- `/my-app/sub/dir/hello.html`
- `/my-app-still-prefixed/index.html` (Note: if the scope was `/`, then this would not be out-of-scope)

And the following will be "out-of-scope":

- `/my-other-app/index.html`
- `/index.html`

### Display Mode

The `display` of a web app determines how the developer would like the app to look like to the user. See the [spec][13] for how the `display` member is processed in the manifest and what the display modes mean.

### User Display Mode

In addition to the developer-specified [`display`][14], the user can specify how they want a WebApp to be displayed, with the only option being whether to "open in a window" or not. Internally, this is expressed in the same display mode enumeration type as [`display`][14], but only the `kStandalone` and `kBrowser` values are used to specify "open in a window" and "do not open in a window", respectively.

#### Effective Display Mode

The pseudocode to determine the ACTUAL display mode a WebApp is displayed is:

```js
if (user_display_mode == kStandalone)
  return developer_specified_display_mode;
else
  return kBrowser; // Open in a tab.
```

#### Open-in-window

This refers to the user specifying that a WebApp should open in the developer specified display mode.

#### Open-in-browser-tab

This refers to the user specifying that a WebApp should NOT open in a window, and thus the WebApp, if launched, will just be opened in a browser tab.

### App Management

Each app has one or more 'management source', specified by the [`WebAppManagement`][17] enumeration. This signifies the system that is 'managing' the install, AKA responsible for installing or uninstalling the app. Internally, the web app system will ensure that the app will only be uninstalled if there are no sources left in the app.

When a user installs an app, the `kSync` management source is specified, because user installs are considered 'managed' by the sync system (and installs will by synced to all devices). See the [`WebAppManagement`][17] enumeration for the description of other management sources.

Installation by certain sources can cause the app to no longer be "uninstallable" by the user. The method [`CanUserUninstallWebApp`][18] function determines if this is the case.

#### Placeholder app

There are some webapps which are managed by external sources - for example, the enterprise policy force-install apps, or the system web apps for ChromeOS. These are generally not installed by user interaction, and the WebAppProvider needs to install something for each of these apps.

Sometimes, the installation of these apps can fail because the install url is not reachable (usually a cert or login needs to occur, and the url is redirected). When this happens, the system [can][15] install a "placeholder" app, which is a fake application that, when launched, navigates to the install url of the application, given by the external app manager.

To resolve placeholder apps back into the intended installation, web contents (either in-(placeholder)-app or in the browser) are all listened to. If any web content successfully [navigates][16] to a placeholder app's `install_url`, then:

1. The placeholder app is uninstalled.
2. After uninstallation, the non-placeholder app is [installed][19].

### Locally Installed

When signing into a non-ChromeOS device, all web apps are installed but not **locally installed**. This means that OS integration is not triggered (so there are no platform shortcuts created), install icons will still show up for the app websites, and the app icon will appear grayed out on chrome://apps.

For an app to become locally installed, the user must do one of the following:

- Navigate to `chrome://apps`, find the grayed-out icon of the app, right click on it, and select "Install".
- Follow any of the normal installation routes to install that app (e.g. visit the app page in the browser and interact with the omnibox install icon)

This was done because on non-ChromeOS devices it was considered a bad user experience to fully install all of the profile's web apps (creating platform shortcuts, etc), as this might not be expected by the user.

### Isolated Web Apps

See [this document][21] for more information.

[2]: https://www.w3.org/TR/appmanifest/
[3]: https://wicg.github.io/manifest-incubations/index.html
[4]: manifest_representations.md
[5]: https://source.chromium.org/search?q=web_app::CanCreateWebApp
[6]: #promotable
[7]: https://www.w3.org/TR/appmanifest/#processing
[8]: https://developers.google.com/web/ilt/pwa/introduction-to-service-worker
[9]: https://www.w3.org/TR/appmanifest/#id-member
[10]: https://www.w3.org/TR/appmanifest/#dfn-identity
[11]: https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/web_applications/web_app_helpers.cc;l=69;drc=cafa646efbb6f668d3ba20ff482c1f729159ae97
[12]: https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/web_applications/web_app.h;l=43;drc=cafa646efbb6f668d3ba20ff482c1f729159ae97;bpv=1;bpt=1
[13]: https://www.w3.org/TR/appmanifest/#display-modes
[14]: #display-mode
[15]: https://source.chromium.org/search?q=ExternalInstallOptions::install_placeholder
[16]: https://source.chromium.org/search?q=WebAppTabHelper::ReinstallPlaceholderAppIfNecessary
[17]: https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/web_applications/web_app_constants.h;l=32?q=WebAppManagement&ss=chromium%2Fchromium%2Fsrc
[18]: https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/web_applications/web_app_utils.cc;l=481?q=CanUserUninstallWebApp&ss=chromium%2Fchromium%2Fsrc
[19]: https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/web_applications/externally_managed_app_install_task.cc;l=215?q=OnPlaceholderUninstalled&ss=chromium%2Fchromium%2Fsrc
[20]: manifest_update_process.md
[21]: isolated_web_apps.md
