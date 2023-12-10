# Content Browser Test Tips

A random collection of useful things to know when writing browser tests.

[TOC]

## Executing JavaScript

If the test needs to use the return value of the script, use `EvalJs()`:

```c++
  // Works with numerical types...
  EXPECT_EQ(0, EvalJs(shell(), "1 * 0");

  // ... string types ...
  EXPECT_EQ("BODY", EvalJs(shell(), "document.body.tagName");

  // and booleans too. Note the explicit use of EXPECT_EQ() instead of
  // EXPECT_TRUE() or EXPECT_FALSE(); this is intentional, and the latter
  // will not compile.
  EXPECT_EQ(false, EvalJs(shell(), "2 + 2 == 5"));
```

Like many other test helpers (e.g. the navigation helpers), the first argument
accepts `RenderFrameHost`, `WebContents`, and other types.

```c++
  // Executes in the main frame.
  EXPECT_EQ(true, EvalJs(shell()->web_contents(), "window.top == window"));

  // Also executes in the main frame.
  EXPECT_EQ(true, EvalJs(shell(), "window.top == window"));

  // Executes in the first child frame of the main frame.
  EXPECT_EQ(
      false,
      EvalJs(ChildFrameAt(shell()->web_contents()->GetPrimaryMainFrame(), 0),
             "window.top == window"));
```

Otherwise, simply use `ExecJs()`:

```c++
  EXPECT_TRUE(ExecJs("console.log('Hello world!')"));
```

Note that these helpers block until the script completes. For async
execution, use `ExecuteScriptAsync()`.

Finally, `JsReplace()` provides a convenient way to build strings for script
execution:

```c++
  EXPECT_EQ("00", EvalJs(JsReplace("$1 + $2", 0, "0")));
```

## Simulating Input

A wide range of methods are provided to simulate input such as clicks, touch,
mouse moves and so on. Many reside in
https://source.chromium.org/chromium/chromium/src/+/main:content/public/test/browser_test_utils.h.

When using input in tests, be aware that the renderer drops all input
received when the main frame is not being updated or rendered immediately
after load. There are three ways, in order of preference, to ensure that
the input will be processed. Use these when your test input seems to be having
no effect:

* Use the 'WaitForHitTestData method' from
  `content/public/test/hit_test_region_observer.h`

* Include visible text in the web contents you are interacting with.

* Add 'blink::switches::kAllowPreCommitInput' as a command line flag.

## Cross-origin navigations

For cross-origin navigations, it is to simplest to configure all hostnames to
resolve to `127.0.0.1` in tests, using a snippet like this:

```c++
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }
```

After that, `EmbeddedTestServer::GetURL()` can be used to generate navigable
URLs with the specific origin:

```c++
  const GURL& url_a = embedded_test_server()->GetURL("a.com", "/title1.html");
  const GURL& url_b = embedded_test_server()->GetURL("b.com", "/empty.html");
```

Most test resources are located in `//content/test/data`, e.g. navigating to
`GetURL("a.com", "/title1.html")` will serve `//content/test/data/title1.html`
as the content.

## Browser-initiated navigation to a specific origin

> **Note:** using arbitrary hostnames requires the [host resolver to
> be correctly configured][host-resolver-config].

`NavigateToURL()` begins and waits for the navigation to complete, as if the
navigation was browser-initiated, e.g. from the omnibox:

```c++
  GURL url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
```

> **Note**: NavigateToURL() allows subframes to be targetted, but outside of history
> navigations, subframe navigations are generally _renderer-initiated_.

## Renderer-initiated navigation to a specific origin

> **Note:** using arbitrary hostnames requires the [host resolver to
> be correctly configured][host-resolver-config].

`NavigateToURLFromRenderer()` begins and waits for the navigation to complete,
as if the navigation was renderer-initiated, e.g. by setting `window.location`:

```c++
  // Navigates the main frame.
  GURL url_1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(shell()->web_contents(), url_1));

  // Navigates the main frame too.
  GURL url_2(embedded_test_server()->GetURL("b.com", "/page_with_iframe.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(shell()->web_contents(), url_2));

  // Navigates the first child frame.
  GURL url_3(embedded_test_server()->GetURL("a.com", "/empty.html"));
  EXPECT_TRUE(
      NavigateToURLFromRenderer(
          ChildFrameAt(shell()->web_contents()->GetPrimaryMainFrame(), 0),
          url_3));
```

## Dynamically generating a page with iframes

> **Note:** using arbitrary hostnames requires the [host resolver to
> be correctly configured][host-resolver-config].

[`cross_site_iframe_factory.html`][cross-site-iframe-factory] is a helper that
makes it easy to generate a page with an arbitrary frame tree by navigating to
a URL. The query string to the URL allows configuration of the frame tree, the
origin of each frame, and a number of other options:

```c++
  GURL url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(a),c,example.com)"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
```

Will generate a page with:

```
Main frame with origin `a.com`
  ├─ Child frame #1 with origin `b.com`
  │    └─ Grandchild frame with origin `a.com`
  ├─ Child frame #2 with origin `c.com`
  └─ Child frame #3 with origin `example.com`
```

## Embedding an `<iframe>` with a specific origin

> **Note:** using arbitrary hostnames requires the [host resolver to
> be correctly configured][host-resolver-config].

Sometimes, a test page may need to embed a cross-origin URL. This is
problematic for pages that contain only static HTML, as the embedded test
server runs on a randomly selected port. Instead, static HTML can use the
cross-site redirector to generate a cross-origin frame:

```html
<!-- static_iframe.html -->
<html>
  <body>
    <iframe src="/cross-site/b.com/title1.html">
  </body>
</iframe>
```

**Important**: the cross-site redirector is not enabled by default.
Override `SetUpOnMainThread()` to configure it like this:

```c++
  void SetUpOnMainThread() override {
    ...
    SetupCrossSiteRedirector(embedded_test_server());
    ...
  }
```

## Simulating a slow load

Navigates to a page that takes 60 seconds to load.

```c++
  GURL url(embedded_test_server()->GetURL("/slow?60");
  EXPECT_TRUE(NavigateToURL(shell(), url));
```

The embedded test server also registers [other default
handlers][test-server-default-handlers] that may be useful.

## Simulating a failed navigation

Note that this is distinct from a navigation that results in an HTTP error,
since those navigations still load arbitrary HTML from the server-supplied error
page; a failed navigation is one that results in committing a Chrome-supplied
error page, i.e. `RenderFrameHost::IsErrorDocument()` returns `true`.

```c++
  GURL url = embedded_test_server()->GetURL("/title1.html");
  std::unique_ptr<URLLoaderInterceptor> url_interceptor =
      URLLoaderInterceptor::SetupRequestFailForURL(url, net::ERR_DNS_TIMED_OUT);
  EXPECT_FALSE(NavigateToURLFromRenderer(shell()->web_contents(), url));
```

[host-resolver-config]: README.md#Cross_origin-navigations
[cross-site-iframe-factory]: https://source.chromium.org/chromium/chromium/src/+/main:content/test/data/cross_site_iframe_factory.html
[test-server-default-handlers]: https://source.chromium.org/chromium/chromium/src/+/main:net/test/embedded_test_server/default_handlers.cc
