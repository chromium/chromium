# Using Web Bundles

This document is for web developers who want to create [Web Bundles](https://wicg.github.io/webpackage/draft-yasskin-wpack-bundled-exchanges.html) that can be loaded with Chromium's experimental implementation.

[TOC]

## Creating Web Bundles

Web Bundles can be created with the [`gen-bundle` tool in the WICG/webpackage repository](https://github.com/WICG/webpackage/tree/master/go/bundle).

To enable experimental support for Web Bundles in Chrome, enable `chrome://flags/#web-bundles` flag .

## Dealing with Common Problems in Unsigned Bundles

As of Chrome 79, only unsigned bundles are supported and they can only be loaded from local files. Such bundles are loaded as "untrusted" pages, where:

- Document's URL is set to the concatenation of bundle's URL and the document's inner URL, e.g. `file://path/to/wbn?https://example.com/article.html`.
- (As a consequence of the above) document's origin is set to an [opaque origin](https://html.spec.whatwg.org/multipage/origin.html#concept-origin-opaque), not the origin of the exchange's URL.
- Document's base URL is set to the exchange's URL, so that relative URL resolution will work correctly.

The following sections list some issues you may encounter when you create an unsigned bundle for a web site.

### Cannot access the origin's resources

Since documents loaded from unsigned bundles have opaque origins, bundled pages cannot access resources of the original site, such as cookies, local storages or service worker caches.

This is a fundamental limitation of unsigned bundles. Origin-signed bundles (not supported by any browser yet) will not have this limitation.

### CORS failures

In unsigned bundles, a document has a synthesized URL like `file://path/to/wbn?https://example.com/` but its subresources are requested with their original url, like `https://example.com/style.css`. That means, **all subresource requests are cross-origin**. So [CORS](https://developer.mozilla.org/en-US/docs/Web/HTTP/CORS) requests (e.g. fonts, module scripts) will fail if the response doesn’t have the `Access-Control-Allow-Origin` header, even when it was same-origin request in the original site.

A workaround is to inject the `Access-Control-Allow-Origin` header to the responses when generating bundles. To do that with `gen-bundle`, use `-headerOverride 'Access-Control-Allow-Origin: *'`.

### Cannot create web workers

Web worker is created by giving a URL of worker script to `new Worker()`, where the script URL **must be same-origin** (relative URLs are resolved with document's base URL). In unsigned bundles, this doesn't work because the document has an opaque origin.

A workaround to this issue is to create a worker via a blob URL. i.e. define a function like this:
```javascript
function newWorkerViaBlob(script) {
  const scriptURL = new URL(script, document.baseURI);
  const blob = new Blob(['importScripts("' + scriptURL + '");'],
                        {type: 'text/javascript'});
  return new Worker(window.URL.createObjectURL(blob));
}
```
and replace `new Worker(scriptURL)` with `newWorkerViaBlob(scriptURL)`.

Note that you may also have to fix `importScripts()` or `fetch()` from the worker script that use relative URLs, because the worker’s base URL is now `blob://...`.

### Other things that do not work in unsigned bundles
- Scripts using the value of `location.href` may not work (use `document.baseURI`'s value instead).
- Service workers (does not work in file://)
- History API (does not work in opaque origins)
