# Component Extensions

## What are component extensions?

Component extensions are extensions that are built into and (typically) bundled
with the Chromium binary.  They don't have a visible UI presence: they don't
have actions in the toolbar and don't show up in the chrome://extensions page
(unless the user appends the `--show-component-extension-options` commandline
flag).  Users cannot disable them or, largely, interact with them.

## Component extensions philosophy

Component extensions should be viewed as fundamental parts of the Chromium
browser.  The best way to think of these is that the fact these features are
implemented with an extension is an _implementation detail_.  Just as we
consider the chrome://settings pages fundamental, core parts of the browser and
give them special privileges even though they are written in HTML, CSS, and
JavaScript, we consider component extensions to be a core part of the browser.

## When to use a component extension

Component extensions are used for core parts of the Chromium browser.  As such,
a component extension should _not_ be used for any feature that should be
removable or that isn't fundamentally part of the browser.  Instead, a
component extension should be used if it is the most appropriate technology.
**The "bar" for adding a component extension is the same as the bar for adding
a new feature to Chromium in C++.**

Reasons to use a component extension include:
*   Faster development.  For the same reason we use WebUI, component extensions
    can be attractive.  It is generally faster to write UI in HTML, CSS, and
    JavaScript than in C++.
*   Strong security model.  Extensions run in renderer processes (and, thanks to
    site isolation, typically do not share with any other extensions or
    websites).
*   Defined API pattern.  The extension API system is tried-and-true and is a
    relatively straightforward way to expose new capabilities to a renderer
    process based on fine-grained criteria (e.g., only exposing to a certain
    host).
*   Modularity.  Adding a component extension to Chrome helps to isolate code,
    and is good for experimentation of Chrome features that may be removed.

## Examples of component extensions

Over the years, there have been numerous component extensions.  Today, some
examples include the built-in PDF reader.  In the past, the bookmark manager,
"cast" functionality, and the U2F security key API (CryptoToken) were also
implemented as extensions.

There are also component platform apps; ChromeOS historically had many of
these.  Now, the general guidance is to instead implement these features as
[System Web Apps](/chrome/browser/ash/system_web_apps/README.md).

## Component extensions permissions

Since component extensions are a part of the Chromium browser, they do not have
user-facing permissions (the same as the rest of the browser doesn't have
user-facing permissions).  Component extensions can typically be auto-granted
any API permission, independent of their manifest (though this is undesired,
and ideally we'd require them to at least declare the permission in their
`manifest.json` file).

## Compared to default-installed extensions

Default-installed extensions are different than component extensions.
Default-installed extensions are added to the browser and can be added either
by the browser publisher (e.g. Google Docs Offline is included with Google
Chrome) or by the OEM (certain manufacturers add extensions to their
Chromebooks).

Default-installed extensions are _not_ the same as component extensions:
*   Default-installed extensions are shown in chrome://extensions
*   Default-installed extensions can be disabled or removed by the user
*   Default-installed extensions do not have inherent access to all APIs; they
    can only access APIs available in their manifest
