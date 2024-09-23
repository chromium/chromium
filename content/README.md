# Content module

## High-level overview
The "content" module is located in `src/content`, and is the core code needed to
render a page using a multi-process sandboxed browser. It includes all the web
platform features (i.e. HTML5) and GPU acceleration. It does not include Chrome
features, e.g. extensions/autofill/spelling etc.

## Motivation
As the Chromium code has grown, features inevitably hooked into the wrong
places, causing layering violations and dependencies that shouldn't exist. It's
been hard for developers to figure out what the "best" way is because the APIs
(when they existed) and features were together in the same directory. To avoid
this happening, and to add a clear separation between the core pieces of the
code that render a page using a multi-process browser, consensus was reached to
move the core Chrome code into `src/content` ([content not
chrome](http://blog.chromium.org/2008/10/content-not-chrome.html) :) ).

## content vs chrome
`content` should only contain code that is required to implement the web
platform. Generally, a feature belongs in this category if and only if all of
the following are true:

- Its launch is tracked on the <https://chromestatus.com/> dashboard.
- It has an associated spec.
- It is going through the [feature development
  lifecycle](https://www.chromium.org/blink/launching-features).

In contrast, many features that are common to modern web browsers do not satisfy
these criteria and thus, are not implemented in `content`. A non-exhaustive
list:

- Extensions
- NaCl
- SpellCheck
- Autofill
- Sync
- Safe Browsing
- Translate

Instead, these features are implemented in `chrome`, while `content` only
provides generic extension points that allow these features to subscribe to the
events they require. Some features will require adding new extension points: for
more information, see [How to Add New Features (without bloating
RenderView/RenderViewHost/WebContents)](https://www.chromium.org/developers/design-documents/multi-process-architecture/how-to-add-new-features).

Finally, there are a number of browser features that require interaction with
online services supplied by the vendor, e.g. from the above list, Safe Browsing,
Translate, Sync, and Autofill all require various network services to function.
The `chrome` layer is the natural place to encapsulate that vendor-specific
integration behavior. For the rare cases where a web platform feature
implemented in `content` has a dependency on a network service (e.g. the network
location service used by Geolocation), `content` should provide a way for the
embedder to inject an endpoint (e.g. `chrome` might provide the service URL to
use). The `content` module itself must remain generic, with no hardcoded
vendor-specific logic.

## Architectural Diagram
![Chrome browser depends on content, which as a whole depends on Chromium's
  low-level libraries and on the constituent parts of
  //content.](./architecture.png)

See an older diagram at: https://www.chromium.org/developers/content-module.

The diagram illustrates the layering of the different modules. A module can
include code directly from lower modules. However, a module can not include code
from a module that is higher than it.  This is enforced through DEPS rules.
Modules can implement embedder APIs so that modules lower than them can call
them. Examples of these APIs are the WebKit API and the Content API.

## Content API
The [Content API](public/README.md) is how code in content can indirectly call
Chrome. Where possible, Chrome features try to hook in by filtering IPCs and
listening to events per [How to Add New Features (without bloating
RenderView/RenderViewHost/WebContents)](https://www.chromium.org/developers/design-documents/multi-process-architecture/how-to-add-new-features).
When there isn't enough context (i.e.  callback from WebKit) or when the
callback is a one-off, we have a `ContentClient` interface that the embedder
(Chrome) implements. `ContentClient` is available in all processes. Some
processes also have their own callback API as well, i.e.
`ContentBrowserClient/ContentRendererClient/ContentPluginClient`.

## Status and Roadmap
The current status is `content` doesn't depend on chrome at all (see the meta
[bug](https://bugs.chromium.org/p/chromium/issues/detail?id=76697) and all bugs
it depends on). We now have a basic browser built on top of `content`
("`content_shell`") that renders pages using `content` on all platforms. This
allow developers working on the web platform and core code to only have to
build/test content, instead of all of chrome.

We have a separate target for `content`'s unit tests in `content_unittests`, and
integration tests in `content_browsertests`.

`content` is built as a separate dll to speed up the build.

We've created an API around `content`, similar to our Blink API. This isolates
embedders from content's inner workings, and makes it clear to people working on
content which methods are used by embedders.

## Content OWNERS
Top-level `content` OWNERS are reviewers who are qualified to review changes
across all of `content` and are responsible for its architecture. In general,
`content` subdirectories will have specific owners who are the experts in
reviewing that code, and top-level owners will defer to subdirectory owners as
needed. For large architectural changes to `content`, all owners should loop in
content-owners@chromium.org to give others a chance to post suggestions. This
applies to changes large enough to warrant a design doc.

To become a content/OWNER, candidates are expected to show substantial
contributions to `content` in recent past that demonstrate knowledge of the core
architecture and design principles, including both the browser process side and
the renderer side.  To become a top-level owner, please follow the following
process:

1. Become an owner in a few `content` subdirectories and establish yourself as
   an expert reviewer in those areas.

2. Find 1-2 current top-level owners who can become your "sponsors" for an owner
   nomination. Work with them to (1) review your technical changes in `content`
   to gain trust in your technical work and (2) shadow-review `content` changes
   that you also review to gain trust in you as a reviewer. Once ready, your
   sponsors will nominate you for ownership by sending an email to
   the current top-level owners.

A typical nomination includes:
- Projects that you worked on that involved `content`, and which concepts they
  covered.
- Some representative CLs contributed and/or reviewed. This can also include
  aggregate statistics, e.g. via `git shortlog -s --author=<username>
  content/browser`.
- Significant improvements to documentation of the above concepts

For reference, a top-level `content` OWNER is expected to be familiar with most
(but not necessarily all) of the following core parts of `content`:
- [Navigation](https://chromium.googlesource.com/chromium/src/+/main/docs/navigation.md)
- [Process model](https://chromium.googlesource.com/chromium/src/+/main/docs/process_model_and_site_isolation.md)
- [Session history](https://chromium.googlesource.com/chromium/src/+/main/docs/session_history.md)
- Loading, interactions with the network stack
- Manipulating pages, documents, frames, and frame trees.
- [MPArch](https://chromium.googlesource.com/chromium/src/+/main/docs/frame_trees.md)
  concepts like inner FrameTrees, primary vs non-primary pages.
- How `content` interacts with compositing and input handling.
- Mojo interfaces between `content/browser` and `content/renderer` and/or
  `blink/renderer`.
- Security checks (e.g., `ChildProcessSecurityPolicy`).
- [content/public APIs](public/README.md): rules for adding them, common APIs
  like `WebContentsObserver`, `ContentBrowserClient`, and `NavigationThrottle`.
- Know that there are `content` embedders beyond //chrome (e.g., Android Webview).
- DEPS rules, what should and should not depend on //content.

Correspondingly, a top-level `content` OWNER is typically familiar with most of
the following core `content` classes:
- `Render(Frame|FrameProxy|Process|Widget|View)Host`
- `Render(Frame|Widget|Thread)`
- `WebContents` and `WebContentsObserver`
- `FrameTree` and `FrameTreeNode`
- `RenderFrameHostManager`
- `NavigationHandle` and `NavigationRequest`, their ownership and lifetime
- `Page` vs `RenderFrameHost` vs blink's `Document`,
  `RenderDocumentHostUserData`/`NavigationHandleUserData`, and associated
  lifetime issues.
- `SiteInstance` and `BrowsingInstance`, `SiteInfo`
- `NavigationController`, `NavigationEntry` vs `FrameNavigationEntry`
- `ChildProcessSecurityPolicy`
- `BrowserContext`, `StoragePartition`
- `ContentBrowserClient`

## Further documentation

* [Bluetooth](browser/bluetooth/README.md)
* [content/browser/renderer_host](browser/renderer_host/README.md)
* [Frame trees](https://chromium.googlesource.com/chromium/src/+/main/docs/frame_trees.md)
* [Navigation](https://chromium.googlesource.com/chromium/src/+/main/docs/navigation.md)
* [Process model](https://chromium.googlesource.com/chromium/src/+/main/docs/process_model_and_site_isolation.md)
* [RenderDocument](https://chromium.googlesource.com/chromium/src/+/main/docs/render_document.md)
* [Session history](https://chromium.googlesource.com/chromium/src/+/main/docs/session_history.md)
