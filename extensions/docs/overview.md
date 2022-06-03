# Extensions Overview

This document is an overview of extensions concepts, terms, and architecture.
The target audience is engineers working on extension support code in the
browser, *not* extension authors. See also the [public extensions
documentation].

## Glossary

* **Extension**: a bundle of HTML, CSS, and Javascript that can interact with
  web contents and some parts of the browser UI. There are many types of
  extensions; see [extension types] for more details. Each extension has an
  **id**, which is a unique string identifying that extension.
* **Manifest**: a JSON file stored inside the extension bundle which describes
  the extension, the capabilities it needs, and many other things about it. The
  [manifest file format] doc gives a user-facing overview.
* **Action**: an action is one of the ways an extension can expose
  functionality to a user. There are three types: [page actions],
  [browser actions], and regular actions. Actions add buttons to the toolbar or
  to the extension menu.
* **Permission**: the ability of an extension to access a specific API. Most
  things extensions can do are controlled by permissions. See [permissions]
  for more details.
* **Extension renderer**: because extensions are logically their own web
  environments, each extension may have a renderer process that hosts its
  content. These renderers are annotated in the task manager as
  "Extension: Name".
* **Component extension**: an extension that is part of the browser. Component
  extensions are not user-visible, generally can't be configured or disabled,
  and may have special permissions. A component extension is so named because it
  is logically a *component of the browser* that happens to be an extension.

## Key Classes

### [extensions::Extension]

An instance of class Extension represents a single installed extension.
Instances of this class are mostly immutable, and are often passed around as
`scoped_refptr<const Extension>`.

### [extensions::Manifest]

An instance of this class represents the manifest of an Extension. An Extension
has (and owns) exactly one Manifest. The Manifest contains the parsed JSON data
from an extension's manifest.json.

### [extensions::ExtensionSystem]

An instance of this class manages much of the state needed to manipulate and use
extensions. It controls many subsidiary services and controllers. It also
contains an [extensions::ExtensionService], which is a mostly-historical grab
bag that is still used for many operations.

### [extensions::Extension::ManifestData] and [extensions::ManifestHandler]

TODO(ellyjones): Move this to a separate manifest.md doc?

These two cooperating classes allow for manifest parsing to be modular. An
instance of ManifestHandler receives the manifest and parses data out of it as
desired, attaching data to the Extension in question via
`extensions::Extension::SetManifestData`. If you were adding a new field to the
manifest, you would:

1. Create a new ManifestData subclass to store whatever you need to store
   per-Extension
2. Create a new ManifestHandler subclass to parse an Extension's manifest
3. Register the class from (2) in `extensions::RegisterChromeManifestHandlers`
4. Grab the ManifestData out of the Extension via
   `extensions::Extension::GetManifestData` as needed. Conventionally, one adds
   a static method to the ManifestData subclass from (1) to retrieve it from an
   Extension.

## Sync

TODO(ellyjones): How does extension sync work?

## Extension Process Model

TODO(ellyjones): Write some words!

[browser actions]: https://developer.chrome.com/extensions/browserAction
[extension types]: extension_and_app_types.md
[manifest file format]: https://developer.chrome.com/extensions/manifest
[page actions]: https://developer.chrome.com/extensions/pageAction
[permissions]: permissions.md
[public extensions documentation]: https://developer.chrome.com/extensions

[extensions::Extension::ManifestData]: https://cs.chromium.org/chromium/src/extensions/common/extension.h
[extensions::ExtensionService]: https://cs.chromium.org/chromium/src/chrome/browser/extensions/extension_service.h
[extensions::ExtensionSystem]: https://cs.chromium.org/chromium/src/extensions/browser/extension_system.h
[extensions::Extension]: https://cs.chromium.org/chromium/src/extensions/common/extension.h
[extensions::ManifestHandler]: https://cs.chromium.org/chromium/src/extensions/common/manifest_handler.h
[extensions::Manifest]: https://cs.chromium.org/chromium/src/extensions/common/manifest.h
