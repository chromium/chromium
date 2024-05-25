# Compression Dictionary Transport

- Contact: <compression-dictionary-transport-experiment@chromium.org>
- Feedback: <https://github.com/WICG/compression-dictionary-transport/issues>
- Chrome status: <https://chromestatus.com/feature/5124977788977152>

## Introduction

This document provides an overview of the current implementation status of the
[**Compression Dictionary Transport**][explainer] feature in Chrome and
instructions on how to enable it.

Starting from version 117, Chrome experimentally supports Compression Dictionary
Transport feature. This feature adds support for using designated previous
responses, as an external dictionary for Brotli- or Zstandard-compressed HTTP
responses.

## Activation

This API can be enabled by chrome://flags or via [Origin Trial][ot-blog].

### chrome://flags

If you want to try Compression Dictionary Transport feature, you have to enable
both [chrome://flags/#enable-compression-dictionary-transport][flag] and
[chrome://flags/#enable-compression-dictionary-transport-backend][backend-flag].
The details about each flag are described [here][shared_dictionary_readme].

### Origin Trial

Origin Trial token can be obtained from [the Origin Trial console][ot-console].
You can enable this feature by setting `Origin-Trials` HTTP response header in
the page's HTML,

```http
Origin-Trial: YOUR_ORIGIN_TRIAL_TOKEN
```

or adding a meta tag in the &lt;header&gt; element.

```html
<meta http-equiv="origin-trial" content="YOUR_ORIGIN_TRIAL_TOKEN">
```

### Third-Party Origin Trial

This feature supports third-party Origin Trial. If you are serving third-party
scripts, you can enable this feature by adding a meta tag.

```javascript
(() => {
  const meta = document.createElement('meta');
  meta.httpEquiv = 'origin-trial';
  meta.content = 'YOUR_THIRD_PARTY_ORIGIN_TRIAL_TOKEN';
  document.head.appendChild(meta);
})()
```

Currently third-party Origin Trial doesn't support `Origin-Trial` HTTP
response header. See [this doc][third-party-ot-dd] for more details.

### Origin Trial and the backend feature

The origin trial token works only when the backend of this feature is enabled.
Currently, the backend is gradually being rolled out. So the backend may not be
enabled in your Chrome yet. If you want to try this feature using an Origin
Trial token, please enable the backend at
[chrome://flags/#enable-compression-dictionary-transport-backend][backend-flag].

#### Why the backend is not enabled for all Chrome yet?

When the backend is enabled, Chrome will check the dictionary database while
fetching resources. This may negatively affect Chrome's loading performance.
Therefore, we are conducting experiments to ensure that this does not cause
regressions before rolling it out to all users.

TODO(crbug.com/40255884): When we enable the backend for all Chrome, remove this
section.

## Feature detection

If you want to check whether the Compression Dictionary Transport feature is
enabled or not, you can try the following code:

```javascript
document.createElement('link').relList.supports('compression-dictionary')
```

If the code above returns true, the Compression Dictionary Transport feature is
enabled.

Note: Until M126, `dictionary` was used instead of `compression-dictionary`.

## Registering dictionaries

When Chrome receives a HTTP response with `use-as-dictionary: <options>` header,
and if the the response type is not opaque, Chrome registers the response as a
dictionary.

Chrome supports following options (see [the explainer][explainer] and [the
spec draft][httpbis-draft] for more details):

- **match**
  - The URL Pattern [URLPattern] to use for request matching.
- **match-dest**
  - List of request destinations for the dictionary to match.
- **id**
  - The dictionary ID.
  - This ID is sent to the server in a "Dictionary-ID" request header when the
    dictionary is available.
- **type**
  - Dictionary format.
  - This field is optional and defaults to `raw`.
  - Currently Chrome only supports `raw`. So if this field is set but it is not
    `raw`, Chrome doesn't register the response as a dictionary. This logic of
    checking **type** was [introduced at M118][type-option-cl].

Note: These options fields are expected to change when we officially launch this
feature, depending on the outcome of [the spec discussion][httpbis-draft].

## Dictionary lifetime

The lifetime of registered dictionary is calculated from the response's
[freshness](https://datatracker.ietf.org/doc/html/rfc9111#name-freshness).

While running the Origin Trial experiment, the max expiration time is
limited to 30 days. This limitation can be removed by enabling
[chrome://flags/#enable-compression-dictionary-transport][flag].

## Fetching dedicated dictionaries

Chrome fetches a dedicated dictionary when it detects
`<link rel="compression-dictionary" href="DICTIONARY_URL">` in the page, or
`Link: <DICTIONARY_URL>; rel="compression-dictionary"` HTTP response header is
set in the response of the page's HTML file.

Note: Until M126, `dictionary` was used instead of `compression-dictionary`.

## Using dictionaries

While fetching a HTTP request, Chrome match the request against the available
dictionary `match` URL patterns.

If a dictionary is available for the request, Chrome will add `dcb` and `dcz`in
the `Accept-Encoding` request header, as well as a
`Available-Dictionary: <SHA-256>` header with the hash of the dictionary.

## Supported compression scheme

Chrome 117.0.5857.0 introduced support for Shared Brotli, and Chrome
118.0.5952.0 adds support for Shared Zstandard.

Shared Zstandard can be enabled/disabled from
[chrome://flags/#enable-shared-zstd][shared-zstd-flag].

Until M126, `br-d` and `zstd-d` encoding was used. After M127, `dcb` and `dcz`
encodings are used. See [this spec
change](https://github.com/httpwg/http-extensions/pull/2784) for more details
about this change of the encoding.

## Supported HTTP protocol

From Chrome 121, Chrome may not use stored shared dictionares when the
connection is using HTTP/1 for non-localhost requests. From Chrome 125, Chrome
may not use stored shared dictionares when the connection is using HTTP/2 for
non-localhost requests.
Also Chrome may not use shared dictionares when the HTTPS connection's
certificate is not rooted by a well known root CA (eg: using a user installed
root certificate). This is for an investigation for an issue that some network
appliances are interfering with HTTPS traffic by inspecting encrypted responses
but failing to properly decode the shared dictionary encoded content.

If you want to use shared dictionaries with HTTP/1, please enable
[chrome://flags/#enable-compression-dictionary-transport-allow-http1][allow-http1-flag].
If you want to use shared dictionaries with HTTP/2, please enable
[chrome://flags/#enable-compression-dictionary-transport-allow-http2][allow-http2-flag].
Also if you want to use shared dictionaries over the HTTPS connection which
certificate is not rooted by a well known root CA, please disable
[chrome://flags/#enable-compression-dictionary-transport-require-known-root-cert][require-known-root-ca-flag].

## Debugging

### Managing registered dictionaries

Developers can manage the registered dictionaries in
[chrome://net-internals/#sharedDictionary][net-internals-sd].

### DevTools

Developers can check the related HTTP request and response headers
(`Use-As-Dictionary`, `Available-Dictionary`, `Accept-Encoding` and
`Content-Encoding`) using DevTools' Network tab.

## Known issues

- [crbug.com/1468156](crbug.com/1468156): `encodedBodySize` property and
  `transferSize` property of `PerformanceResourceTiming` for shared dictionary
  compressed response are wrong. Currently it returns as if the response is not
  compressed.

## Changes in M123

The following changes have been made to Chrome since M123 to keep up with the
changes in specifications.

- Changed Content-Encoding name "br-d" "zstd-d"
- Changed match to use URLPattern
- Added support for a server-provided dictionary id
- Stop using "expires" value of "Use-As-Dictionary" header, and use the cache
  expiration time calculated from the response's freshness instead.
- Removed support for hash negotiation and force use of sha-256
- Added the dictionary hash to the compressed response
- Dictionary hashes changed to sf-binary
- Use "Available-Dictionary" header instead of "Sec-Available-Dictionary"
- Added support for match-dest option

## Changes in M127

The following changes have been made to Chrome since M127 to keep up with the
changes in specifications.

- `compression-dictionary` rel attribute is used for HTML `link` element and
  HTTP `Link:` header instead of `dictionary`.
- Changed Content-Encoding to use "dcb" and "dcz".
  See [this spec change](https://github.com/httpwg/http-extensions/pull/2784).

## Demo sites

There are a few demo sites that you can use to test the feature:

- Shopping site demo (dynamic resources flow)
   <https://compression-dictionary-transport-shop-demo.glitch.me/>
- Three JS demo (static resources flow)
   <https://compression-dictionary-transport-threejs-demo.glitch.me/>

[explainer]: https://github.com/WICG/compression-dictionary-transport
[flag]: chrome://flags/#enable-compression-dictionary-transport
[backend-flag]: chrome://flags/#enable-compression-dictionary-transport-backend
[shared-zstd-flag]: chrome://flags/#enable-shared-zstd
[allow-http1-flag]: chrome://flags/#enable-compression-dictionary-transport-allow-http1
[allow-http2-flag]: chrome://flags/#enable-compression-dictionary-transport-allow-http2
[require-known-root-ca-flag]: chrome://flags/#enable-compression-dictionary-transport-require-known-root-cert
[shared_dictionary_readme]: ../../services/network/shared_dictionary/README.md#flags
[ot-blog]: https://developer.chrome.com/blog/origin-trials/
[ot-console]: https://developer.chrome.com/origintrials/#/trials/active
[third-party-ot-dd]: https://docs.google.com/document/d/1xALH9W7rWmX0FpjudhDeS2TNTEOXuPn4Tlc9VmuPdHA/edit#heading=h.bvw2lcb2dczg
[httpbis-draft]: https://datatracker.ietf.org/doc/draft-ietf-httpbis-compression-dictionary/
[net-internals-sd]: chrome://net-internals/#sharedDictionary
[type-option-cl]: https://chromiumdash.appspot.com/commit/169031f4af2cbdc529f48160f1df20b4ca8b6cc1
[URLPattern]: https://urlpattern.spec.whatwg.org/
