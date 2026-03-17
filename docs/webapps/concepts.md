## Web Apps - Concepts

### Manifest, or WebManifest

This refers to the document described by the [appmanifest](https://www.w3.org/TR/appmanifest/) spec, with some extra features described by [manifest-incubations](https://wicg.github.io/manifest-incubations/index.html). This document describes metadata and developer configuration of an installable web app.

For code representations of the manifest see [the list](/chrome/browser/web_applications/docs/manifest_representations.md).

### Manifest Link

A manifest link is something that looks like this in a html document:

```html
<link rel="manifest" href="manifest.webmanifest">
```

This link ties the manifest to the document, and subsequently used in the spec algorithms defined in [appmanifest](https://www.w3.org/TR/appmanifest/) or [manifest-incubations](https://wicg.github.io/manifest-incubations/index.html) to describe the webapp and determine if it is installable.

### Installable

If a document or page is considered "installable", then the user agent can create some form of installed web app for that page. To be installable, [web_app::CanCreateWebApp](https://source.chromium.org/search?q=f:web_app_utils.h%20CanCreateWebApp) must return true, where:

- The user profile must allow webapps to be installed
- The web contents of the page must not be crashed
- The last navigation on the web contents must not be an error (like a 404)
- The url must be `http, https`, or `chrome-extension`

This is different from [promotable](#promotable) below, which determines if Chrome will promote installation of the page.

### Promotable

A document is considered "promotable" if it fulfills a set of criteria. This criteria may change to further encourage a better user experience for installable web apps. There are also a few optional checks that depend on the promotability checker. This general criteria as of 2022/09/08:

- _The document contains a manifest link_.
- The linked manifest can be processed [according](https://www.w3.org/TR/appmanifest/#processing) to the spec and is valid.
- The processed manifest contains the fields:
  - `name`
  - `start_url`
  - `icons` with at least one icon with a valid response that is a parsable image.
  - `display` field that is not `"browser`"
- "Serviceworker check": The `start_url` is 'controlled' (can be served by) a [serviceworker](https://developers.google.com/web/ilt/pwa/introduction-to-service-worker) with a fetch handler. **Optionally turned off**
  - Note: This is expected to be removed in Q4 2022.
- _"Engagement check": The user has engaged with, or interacted with, the page or origin a certain amount (currently at least one click and some seconds on the site). **Optionally turned off**_

Notes:

- Per spec, the document origin and the `start_url` origin must match.
- Per spec, the `start_url` origin does not have to match the `manifest_url` origin
- The `start_url` could be different from the `document_url`.

### Manifest id

The `id` specified in the manifest represents the identity of the web app. The manifest id is processed following the algorithm described in [appmanifest specification](https://www.w3.org/TR/appmanifest/#id-member) to produce the app's identity. In the web app system, the app's [identifier](https://www.w3.org/TR/appmanifest/#dfn-identity) is [hashed](https://source.chromium.org/search?q=f:web_app_helpers.h%20GenerateAppIdFromManifestId) to be stored to [WebApp->app_id()](https://source.chromium.org/search?q=f:web_app.h%20WebApp::app_id).

If a manifest is discovered during any sort of page load, then the update process is initiated for that manifest. If it resolves to an `app_id` that is installed, then it will perform an update. See [documentation](/chrome/browser/web_applications/docs/manifest_update_process.md) for more information.

### Scope

Scope refers to the prefix that a WebApp controls. All paths at or nested inside of a WebApp's scope are thought of as "controlled" or "in-scope" of that WebApp. This is a simple string prefix match. For example, if `scope` is `/my-app`, then the following will be "in-scope":

- `/my-app/index.html`
- `/my-app/sub/dir/hello.html`
- `/my-app-still-prefixed/index.html` (Note: if the scope was `/`, then this would not be out-of-scope)

And the following will be "out-of-scope":

- `/my-other-app/index.html`
- `/index.html`

### Display Mode

The `display` of a web app determines how the developer would like the app to look like to the user. See the [spec](https://www.w3.org/TR/appmanifest/#display-modes) for how the `display` member is processed in the manifest and what the display modes mean.

### Isolated Web Apps

See [this document](/chrome/browser/web_applications/docs/isolated_web_apps.md) for more information.
