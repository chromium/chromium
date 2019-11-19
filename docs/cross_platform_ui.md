# How to Write Cross-Platform UI (Native components)

[TOC]

Chrome runs on many platforms such as Desktop (Windows, Mac, Linux, and
ChromeOS), Android, iOS, and VR. These platforms have different UIs, written in
different languages. Chrome also has WebUI, which is browser UI implemented in
the HTML content area. This can be considered another UI toolkit.

## Separating out platform-independent business logic

For UI that appears on multiple platforms, it's best to share as much of the
business logic (model behavior backing the UI) as possible. The
platform-specific views should be as minimal and "dumb" as possible. One caveat:
sharing everything may not be feasible if there are performance or latency
concerns.

The cross-platform model will be in native C++ code, likely in the
//components/foobar directory. Model layer code should handle the business
logic, and be richly observable. Platform-specific views/controllers should
observe the model, update the UI, and mutate the model as needed. This model
should have tests.

Note that there can be model state that is applicable to only a subset of
platforms. For instance, mobile devices can change device rotation or layout. On
Mobile, the platform itself may provide extended text editing capabilities that
don't exist on Desktop.

## Lifetime Issues

Different platforms may have different UI lifetime semantics, so it's helpful to
keep the business logic stateless or with a flexible lifetime model. Android
Java object lifetimes are well-defined, but has gotchas for cross-platform code.
For instance: the Java side of Android is available immediately on startup, but
the C++ side needs to wait for the native library to be loaded. Having a native
object "own" a garbage-collected Java object or vice versa can also be tricky.

 1. Pure static functions are the easiest, if this works for your use case. In
    addition to being easy to bridge to Java, these are also easy to unit test,
    since they don't require setting up state.

    You can usually use pure static functions for simpler cases that don't
    involve state. These are easy to bridge to Java. See
    [`UrlFormatter`](https://cs.chromium.org/chromium/src/components/url_formatter/android/java/src/org/chromium/components/url_formatter/UrlFormatter.java)
    for an example of this.

 2. For objects that are associated with a specific `Profile`, use a
    `KeyedService` instance with
    [`BrowserContextKeyedServiceFactory`](https://cs.chromium.org/chromium/src/components/keyed_service/content/browser_context_keyed_service_factory.h).
    If your object depends on other `KeyedService` instances, there's a strong
    chance your object should also be a `KeyedService`. 

    These are also pretty easy to bridge to Java. In most cases, the Java code
    will need to pass a `Profile` object, so that the native `KeyedService` can
    be fetched.

    A neat trick is that the Java code can be all-static, even if the native
    code isn't, if every method also includes a `Profile` parameter.

 3. Objects tied to WebContents lifetimes are also commonly used, and can be
    implemented using a WebContentsObserver and WebContentsUserData.

 4. For lifetimes tied to UI instances, or other custom lifetimes, you will need
    to do custom lifetime management. For Java bridging advice, see the JNI
    documentation for details.

## More Advice on Bridging to Java

Bridging a code module to Java will have three parts:

 1. components/omnibox/browser/foobar.h/cc - Cross-platform code you already
    wrote and have unit tests for.

 2. chrome/browser/android/omnibox/foo_feature_android.h/cc - C++ side of the
    bridge. You might only need the cc file.

 3. chrome/android/java/src/org/chromium/chrome/browser/omnibox/FooFeature.java
    - Java side of the bridge.

See the [JNI README](../base/android/jni_generator/README.md) for more details.
