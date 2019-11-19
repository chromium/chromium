# Content API

`//content/public` is the API exposed to embedders of the [content
module](/content/README.md).

## Motivation
- isolate developers working on Chrome from inner workings of content
- make the boundary between content and chrome clear to developers and other
  embedders

## Design
In general, we follow the design of the [Blink Public
API](/third_party/blink/public/README.md). This makes it easier for people
who're already familiar with it, and also keeps things consistent.

- `//content/public` should contain only interfaces, enums, structs and (rarely)
  static functions.
  - An exception is `//content/public/test`. We allow concrete classes that
    chrome test classes derive from or use in here.
- While we don't allow old-style Chrome IPC `_messages.h` files in
  `//content/public`, we do allow `.mojom` files (see
  [discussion](https://groups.google.com/a/chromium.org/forum/#!searchin/chromium-mojo/cross-module/chromium-mojo/ZR2YlRV7Uxs/Ce-h_AaWCgAJ)).
  If a mojom is only used inside content, it should be in
  `//content/common`. If it's an interface that is implemented or called by
  content's embedder, then it belongs in `//content/public/common`.
- In general, if there is a struct or enum which is only used by an interface,
  they are put in the same file, but when the struct/enum is used in other
  places or if it's pretty big, then it should be in its own file.
- All code under `//content` should be in the `"content"` namespace.
- Interfaces that content implements usually should be pure abstract, because
  usually there's only one implementation. These should not be implemented
  outside of content.  (i.e., content will freely assume that it can cast to
  its implementation(s)).
- Interfaces that embedders implement, especially ones which are used in tests
  or are observer-style and have many implementations, should have default
  (empty) implementations.
- Prefer enum classes over enum types. For enum types, the value should start
  with the name of the type, i.e.,  `PAGE_TRANSITION_LINK` in the
  `content::PageTransition` enum.
- content implementation code should use other implementations directly and
  not go through the interface (i.e., code in `//content/renderer` should use
  `RenderViewImpl` instead of `content::RenderView`).
- It's acceptable to put implementation files that hold constructors/destructors
  of interfaces/structs which might have member variables. For structs, this
  covers initializing member variables. For interfaces (i.e.
  `RenderViewObserver`) this might cover things like automatic
  registration/unregistration. Normally we would put this small code in headers,
  but because of the clang checks against putting code in headers, we're forced
  to put it in .cc files (we don't want to make a clang exception for the
  `content/public` directory since that would lead to confusion).
- When code in chrome implements an interface from content, usually the
  convention is to prefix the implementation with "Chrome" (i.e.
  `ChromeContentBrowserClient` derives from `content::ContentBrowserClient`).
- Only expose methods in the public API that embedders need. If a method is only
  used by other code in content, it belongs in `foo_impl.h` and not `foo.h`.
- Methods in the API should be there because either content is calling out to
  its embedder, or the embedder is calling to content. There shouldn't be any
  methods which are used to call from the embedder to the embedder.
- All classes/structs/enums in the public API must be used by embedders and
  content. i.e. if the chrome layer uses a struct but content doesn't know about
  it, it doesn't belong in `//content/public` but instead some module that's
  higher level.
- We avoid single-method delegate interfaces, and in those case we use
  callbacks.
- Don't add the `const` identifier to interfaces. For interfaces implemented by
  the embedder, we can't make assumptions about what the embedder needs to
  implement it. For interfaces implemented by content, the implementation
  details doesn't have to be exposed.
- Observer interfaces (i.e. `WebContentsObserver`, `RenderFrameObserver`,
  `RenderViewObserver`) should only have void methods. This is because otherwise
  the order that observers are registered would matter, and we don't want that.
  The only exception is `OnMessageReceived()`, which is fine since only one
  observer class handles each particular IPC, so ordering doesn't make a
  difference.
