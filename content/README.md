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
As discussed above, `content` should only have the core code needed to render a
page. Chrome features use APIs that are provided by `content` to filter IPCs and
get notified of events that they require. [How to Add New Features (without
bloating
RenderView/RenderViewHost/WebContents)](https://www.chromium.org/developers/design-documents/multi-process-architecture/how-to-add-new-features)
describes how to do this.

As an example, here's a (non-exhaustive) list of features that are Chrome only,
and so are not in content. This means that `content` code shouldn't have to know
anything about them, only providing generic APIs that they can be built upon.
- Extensions
- NaCl
- SpellCheck
- Autofill
- Sync
- Prerendering
- Safe Browsing
- Translate

As the list above shows, even browser features that are common to modern
browsers are not in `content`. The dividing line is that `src/content` only has
code that is required to implement the web platform. Features that aren't
covered by web specs should live in `src/chrome`. If a feature is being
implemented and the team foresees that it would be a spec, it should still go in
`src/chrome`. Once it has a spec, then it can move to `src/content`.

Where code interacts with online network services that must be supplied by the
vendor, the favored approach is to fully implement that feature outside of the
`content` module. E.g. from the list above Safe Browsing, Translate, Sync and
Autofill require various network services to function, and the `chrome` layer is
the natural place to encapsulate that behavior. For those few cases where we
need to make network requests using code in the content module in order to
implement generic HTML5 features (e.g. the network location service for
Geolocation), the embedder must fully define the the endpoint to connect to,
typically it might do this by injecting the service URL. We do not want any such
policy coded into the `content` module at all, again to keep it generic.

## Architectural Diagram
TODO: Draw a modern diagram.

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

`content` is build at a separate dll to speed up the build.

We've created an API around `content`, similar to our WebKit API. This isolates
embedders from content's inner workings, and makes it clear to people working on
content which methods are used by embedders.

## Further documentation

* [Bluetooth](browser/bluetooth/README.md)
