# Cross-Origin Read Blocking (CORB)

This document outlines Cross-Origin Read Blocking (CORB), an algorithm by which
dubious cross-origin resource loads may be identified and blocked by web
browsers before they reach the web page.
CORB reduces the risk of leaking sensitive data by keeping it further from
cross-origin web pages.  In most browsers, it keeps such data out of untrusted
script execution contexts.  In browsers with
[Site Isolation](https://www.chromium.org/Home/chromium-security/site-isolation),
it can keep such data out of untrusted renderer processes entirely, helping even
against side channel attacks.

[TOC]

## The problem

The same-origin policy generally prevents one origin from reading arbitrary
network resources from another origin. In practice, enforcing this policy is not
as simple as blocking all cross-origin loads: exceptions must be established for
web features, like `<img>` or `<script>` which can target cross-origin
resources for historical reasons, and for the CORS mechanism which allows some
resources to be selectively read across origins.

Certain types of content, however, can be shown to be incompatible with all of
the historically-allowed permissive contexts. JSON is one such type: a JSON
response will result in a decode error when targeted by the `<img>` tag, either
a no-op or syntax error when targeted by the `<script>` tag, and so on. The
only case where a web page can load JSON with observable consequences, is via
`fetch()` or `XMLHttpRequest`; and in those cases, cross-origin reads are
moderated by CORS.

By detecting and blocking loads of CORB-protected resources early -- that is,
before the response makes it to the image decoder or JavaScript parser stage --
CORB defends against side channel vulnerabilities that may be present in the
stages which are skipped.

## What attacks does CORB mitigate?

CORB mitigates the following attack vectors:

* Cross-Site Script Inclusion (XSSI)
  * XSSI is the technique of pointing the `<script>` tag at a target resource
    which is not JavaScript, and observing some side effects when the resulting
    resource is interpreted as JavaScript. An early example of this attack was
    discovered in 2006: by overwriting the JavaScript Array constructor, the
    contents of JSON lists could be intercepted as simply as:
      `<script src="https://example.com/secret.json">`.
    While the array constructor attack vector is fixed in current
    browsers, numerous similar exploits have been found and fixed in the
    subsequent decade. For example, see the
    [slides here](https://www.owasp.org/images/6/6a/OWASPLondon20161124_JSON_Hijacking_Gareth_Heyes.pdf).
  * CORB prevents this class of attacks, because a CORB-protected resource will
    be blocked from ever being delivered to a cross-site `<script>` element.
  * CORB is particularly valuable in absence of other XSSI defenses like
    [XSRF tokens](https://cheatsheetseries.owasp.org/cheatsheets/Cross-Site_Request_Forgery_Prevention_Cheat_Sheet.html#synchronizer-token-pattern)
    and/or
    [JSON security prefixes](https://docs.angularjs.org/api/ng/service/$http#json-vulnerability-protection).
    Additionally, the presence of XSSI defenses like
    [JSON security prefixes](https://docs.angularjs.org/api/ng/service/$http#json-vulnerability-protection)
    can also be used as a signal to the CORB algorithm that a resource should be
    CORB-protected.

* Speculative Side Channel Attack (e.g. Spectre).
  * For example, an attacker may use an
    `<img src="https://example.com/secret.json">`
    element to pull a cross-site secret
    into the process where the attacker's JavaScript runs, and then use a
    speculative side channel attack (e.g. [Spectre](https://spectreattack.com))
    to read the secret.
  * CORB can prevent this class of attacks when used in tandem with
    [Site Isolation](https://www.chromium.org/Home/chromium-security/site-isolation),
    by preventing the JSON resource from being present in the
    memory of a process hosting a cross-site page.

## How does CORB "block" a response?

When CORB decides that a response needs to be CORB-protected, the response is
modified as follows:
* The response body is replaced with an empty body.
* The response headers are removed.

> [lukasza@chromium.org] Chromium currently retains Access-Control-\* headers
> (this helps generate better error messages for CORS).

To be effective against speculative side-channel attacks, CORB blocking must
take place before the response reaches the process hosting the cross-origin
initiator of the request.  In other words, CORB blocking should prevent
CORB-protected response data from ever being present in the memory of the
process hosting a cross-origin website (even temporarily or for a short term).
This is different from the concept of
[filtered responses](https://fetch.spec.whatwg.org/#concept-filtered-response)
(e.g. [CORS filtered response](https://fetch.spec.whatwg.org/#concept-filtered-response-cors) or
[opaque filtered response](https://fetch.spec.whatwg.org/#concept-filtered-response-opaque))
which just provide a limited view into full data that remains stored in an
[internal response](https://fetch.spec.whatwg.org/#concept-internal-response)
and may be implemented inside the renderer process.

A CORB demo page
[is available here](https://anforowicz.github.io/xsdb-demo/index.html).

## What kinds of requests are CORB-eligible?

The following kinds of requests are CORB-exempt:

* [navigation requests](https://fetch.spec.whatwg.org/#navigation-request)
  or requests where the
  [request destination](https://fetch.spec.whatwg.org/#concept-request-destination)
  is "object" or "embed".
  Cross-origin `<iframe>`s, `<object>`s, and `<embed>`s create a separate
  security context and thus pose less risk for leaking the data.  In most
  browsers, this separate context means that a malicious page would have more
  trouble inferring the contents than from loading them into its own execution
  context and observing side effects (e.g., XSSI, style tags, etc).  In browsers
  with Site Isolation, this security context uses a separate process, keeping
  the data out of the malicious page's address space entirely.

> [lukasza@chromium.org] TODO: Figure out how
> [Edge's VM-based isolation](https://cloudblogs.microsoft.com/microsoftsecure/2017/10/23/making-microsoft-edge-the-most-secure-browser-with-windows-defender-application-guard/)
> works (e.g. if some origins are off-limits in particular renderers, then this
> would greatly increase utility of CORB in Edge).

* Download requests (e.g. requests where the
  [initiator](https://fetch.spec.whatwg.org/#concept-request-initiator)
  is "download").  In this case the data from the response is saved to disk
  (instead of being shared to a cross-origin context) and therefore wouldn't
  benefit from CORB protection.

> [lukasza@chromium.org] AFAIK, in Chrome a response to a download request never
> passes through memory of a renderer process.  Not sure if this is true in
> other browsers.

All other kinds of requests may be CORB-eligible.  This includes:
* [XHR](https://xhr.spec.whatwg.org/)
  and [fetch()](https://fetch.spec.whatwg.org/#dom-global-fetch)
* `ping`, `navigator.sendBeacon()`
* `<link rel="prefetch" ...>`
* Requests with the following
  [request destination](https://fetch.spec.whatwg.org/#concept-request-destination):
    - "image" requests like `<img>` tag, `/favicon.ico`, SVG's `<image>`,
      CSS' `background-image`, etc.
    - [script-like destinations](https://fetch.spec.whatwg.org/#request-destination-script-like)
      like `<script>`, `importScripts()`, `navigator.serviceWorker.register()`,
      `audioWorklet.addModule()`, etc.
    - "audio", "video" or "track"
    - "font"
    - "style"
    - "report" requests like CSP reports, NEL reports, etc.

The essential idea of CORB is to consider whether a particular resource might be
unsuitable for use in *every* context listed above. If every possible usage
would result in either a CORS error, a syntax/decoding error, or no observable
consequence, CORB ought to be able to block the cross-origin load without
changing the observable consequences of the load. Prior to CORB, details are
already suppressed from cross-origin errors, to prevent information leaks. Thus,
the observable consequences of such errors are already limited, and feasible to
preserve while blocking.

## What types of content are protected by CORB?

As discussed below, the following types of content are CORB-protected:
 * JSON
 * HTML
 * XML

These are each discussed in the following sections.

### Protecting JSON

JSON is a widely used data format on the web; support for JSON is built into the
web platform. JSON responses are very likely to contain user data worth
protecting. Additionally, unlike HTML or image formats, there are no legacy HTML
mechanisms (that is, predating CORS) which allow cross-origin embedding of JSON
resources.

Because the JSON syntax is derived from and overlaps with JavaScript, care must
be taken to handle the possibility of JavaScript/JSON polyglots.
CORB handles the following cases for JSON:
 * Non-empty JSON object literal: A non-empty JSON object
   (such as `{"key": "value"}`). This is precisely the subset of JSON syntax
   which is invalid JavaScript syntax -- the colon after the first string
   literal will generate a syntax error. CORB can protect these cases, even if
   labeled with a different Content-Type, by sniffing the response body.
 * Other JSON literals: The remaining subset of the JSON syntax (for example,
   `null` or `[1, 2, "3"]`) also happens to be valid JavaScript syntax. In
   particular, when evaluated as script, they are value expressions that should
   have no side effects. Thus, if they can be detected, they can be CORB-
   protected. Detection here is possible, but requires implementing a validator
   that understands the full JSON syntax:
    * If the response is not labeled with a JSON Content Type, CORB might detect
      these cases by buffering and validating the entire response body as
      JSON; the entire response must be considered because of the potential for
      a valid, side-effect-having JavaScript program like `[1, 2,
      "3"].map(...)`.
    * If the response is indeed labeled with a JSON Content Type, CORB may
      decide to sniff the response to confirm it is valid JSON, only up to a
      certain number of bytes. This would avoid buffering and parsing
      in an unbounded amount of memory.
 * JSON served with
   [an XSSI-defeating prefix](https://docs.angularjs.org/api/ng/service/$http#json-vulnerability-protection):
   As a mitigation for past browser
   vulnerabilities, many actual websites and frameworks employ a convention of
   prefixing their fetchable resources with a string designed to force a
   JavaScript error.
   These prefixes have not been standardized prior to CORB, but a few approaches
   seem prevalent:

    * The character sequence `)]}'` is built into
      [the angular.js framework](https://docs.angularjs.org/api/ng/service/$http),
      [the Java Spring framework](https://goo.gl/xP7FWn),
      and is observed in wide use on the google.com domain.
    * The character sequence `{} &&` was
      [historically built into the Java Spring framework](https://goo.gl/JYPFAv).
    * Infinite loops, such as `for(;;);`, are observed in wide use on the
      facebook.com domain.

   The presence of these recognized XSSI defenses is a
   strong signal to the CORB algorithm that a resource should be CORB-protected.
   As such, these prefixes should trigger CORB protection in almost every case,
   no matter what follows them. This is argued to be safe because:
     * [A JSON security prefix](https://docs.angularjs.org/api/ng/service/$http#json-vulnerability-protection)
       would cause a syntax error (or a hang) if present in a document served
       with a JavaScript MIME type such as `text/javascript`.
     * [JSON security prefixes](https://docs.angularjs.org/api/ng/service/$http#json-vulnerability-protection)
       are not known to collide with binary
       resources like images, videos or fonts (which typically require
       the first few bytes to be hardcoded to a specific sequence - for example
       `FF D8 FF` for image/jpeg).
     * Collisions with `text/css` stylesheets are theoretically possible, because
       it is possible to construct a file that begins with
       [a JSON security prefix](https://docs.angularjs.org/api/ng/service/$http#json-vulnerability-protection),
       but at the same parses fine as a stylesheet.
       `text/css` is therefore established as an exception, even though the
       practical likelihood of such a scenario seems low.
       See below for an example of such a stylesheet:
```css
)]}'
{}
h1 { color: red; }
```

JSON is also used by some web features. One example is `<link
rel="manifest">`, whose `href` attribute specifies a JSON manifest file.
Fortunately, this mechanism requires CORS when the manifest is specified cross-
origin, so its CORB treatment works identically to the rules applied to fetch().

> [nick@chromium.org] TODO: Is there a spec link for JSON being side-effect
> free when interpreted as script?

### Protecting HTML

HTML can be embedded cross-origin via `<iframe>` (as noted above),
but otherwise HTML documents can
only be loaded by fetch() and XHR, both of which require CORS. HTML sniffing is
already well-understood, so (unlike JSON) it is relatively easy to identify HTML
resources with high confidence.

One ambiguous polyglot case has been
identified that CORB needs to handle conservatively: HTML-style comments, which
are [part of the JavaScript syntax](https://www.ecma-international.org/ecma-262/8.0/index.html#sec-html-like-comments).
* CORB skips over HTML comment blocks when sniffing to
  confirm a HTML content type.  This means that (unlike in
  [normal HTML sniffing](https://mimesniff.spec.whatwg.org/#identifying-a-resource-with-an-unknown-mime-type))
  presence of "`<!--`" string doesn't immediately confirm that the sniffed resource is a
  HTML document - the HTML comment still has to be followed by a valid HTML tag.
* Additionally, after the end of a HTML comment, the CORB sniffer will skip all
  characters until a line terminating character.  This helps accomodate the
  [`SingleLineHTMLCloseComment`](https://www.ecma-international.org/ecma-262/8.0/index.html#prod-annexB-SingleLineHTMLCloseComment)
  rule which can consume
  [`SingleLineCommentChars`](https://www.ecma-international.org/ecma-262/8.0/index.html#prod-SingleLineCommentChars)
  _after_ the "`-->`" characters.

Examples of html/javascript polyglots which have been observed
in use on real websites:
```js
<!--/*--><html><body><script type="text/javascript"><!--//*/
var x = "This is both valid html and valid javascript";
//--></script></body></html>
```

```js
<!-- comment --> <script type='text/javascript'>
//<![CDATA[
var x = "This is both valid html and valid javascript";
//]]>--></script>
```


### Protecting XML

XML, like JSON, is a widely used data exchange format, and like HTML, is a
document format that's built into the web platform (notably via XmlHttpRequest).

Confirming an XML content-type via sniffing is more straightforward than JSON or
HTML: XML is signified by the pattern `<?xml`, possibly preceded by whitespace.

The only identified XML case that requires special treatment by CORB is
`image/svg+xml`, which is an image type. All other XML mime types are treated as
CORB-protected.

## Determining whether a response is CORB-protected

CORB decides whether a response needs protection (i.e. if a response is a JSON,
HTML or XML resource) based on the following:

* If the response contains `X-Content-Type-Options: nosniff` response header,
  then the response will be CORB-protected
  if its `Content-Type` header is one of the following:
  * [HTML MIME type](https://mimesniff.spec.whatwg.org/#html-mime-type)
  * [XML MIME type](https://mimesniff.spec.whatwg.org/#xml-mime-type)
    (except `image/svg+xml` which is CORB-exempt as described above)
  * [JSON MIME type](https://mimesniff.spec.whatwg.org/#json-mime-type)
  * `text/plain`

* If the response is a 206 response,
  then the response will be CORB-protected
  if its `Content-Type` header is one of the following:
  * [HTML MIME type](https://mimesniff.spec.whatwg.org/#html-mime-type)
  * [XML MIME type](https://mimesniff.spec.whatwg.org/#xml-mime-type)
    (except `image/svg+xml` which is CORB-exempt as described above)
  * [JSON MIME type](https://mimesniff.spec.whatwg.org/#json-mime-type)

* Otherwise, CORB attempts to sniff the response body:
  * [HTML MIME type](https://mimesniff.spec.whatwg.org/#html-mime-type)
    that sniffs as HTML is CORB-protected
  * [XML MIME type](https://mimesniff.spec.whatwg.org/#xml-mime-type)
    (except `image/svg+xml`) that sniffs as XML is CORB-protected
  * [JSON MIME type](https://mimesniff.spec.whatwg.org/#json-mime-type)
    that sniffs as JSON is CORB-protected
  * `text/plain` that sniffs as JSON, HTML or XML is CORB-protected
  * Any response (except `text/css`) that begins with
    [a JSON security prefix](https://docs.angularjs.org/api/ng/service/$http#json-vulnerability-protection)
    is CORB-protected

The sniffing is necessary to avoid blocking existing web pages that depend on
mislabeled cross-origin responses (e.g. on images served as `text/html`).
Without sniffing CORB would block around 16 times as many
responses.
* CORB will only sniff to *confirm* the classification based on the `Content-Type`
  header (i.e. if the `Content-Type` header is `text/json` then CORB will sniff
  for JSON and will not sniff for HTML or XML).
* If some syntax elements are shared between CORB-protected and
  non-CORB-protected MIME types, then these elements have to be ignored by CORB
  sniffing.  For example, when sniffing for (CORB-protected) HTML, CORB ignores
  and skips HTML comments, because
  [they can also be present](http://www.ecma-international.org/ecma-262/6.0/#sec-html-like-comments)
  in (non-CORB-protected) JavaScript.  This is different from the
  [HTML sniffing rules](https://mimesniff.spec.whatwg.org/#rules-for-identifying-an-unknown-mime-type),
  used in other contexts.
* Sniffing is a best-effort heuristic and for best security results, we
  recommend that web developers 1) mark responses with the correct `Content-Type`
  header and 2) opt out of sniffing by using the
  `X-Content-Type-Options: nosniff` header.

> [nick@chromium.org] This section needs a strong justification for why
> text/plain gets this special interpretation. Ideally data showing that
> text/plain is commonly used to serve HTML, JSON, or XML. Treatment of
> text/plain in our current implementation may actually be an artifact of
> an earlier prototype, which ran after standard mime sniffing, and may have
> seen 'text/plain' MIME types applied as a default MIME type when the response
> omitted a Content-Type header.

Note that the above means that the following responses are not CORB-protected:
* Responses labeled as `multipart/*`.
  This avoids having to parse the content types of the nested parts.
  We recommend not supporting multipart range requests for sensitive documents.
* Responses without a `Content-Type` header.
* Responses with a JavaScript MIME type such as `text/javascript`. This
  includes JSONP ("JSON with padding") which unlike JSON is meant to be read
  and executed in a cross-origin context.


## CORB and web compatibility

### Observable CORB impact on images

CORB should have no observable impact on `<img>` tags unless the image resource
is both 1) mislabeled with an incorrect, non-image, CORB-protected Content-Type
and 2) served with the `X-Content-Type-Options: nosniff` response header.

Examples:

* **Correctly-labeled HTML document**
  * Resource used in an `<img>` tag:
    * Body: an HTML document
    * `Content-Type: text/html`
    * No `X-Content-Type-Options` header
  * Expected behavior: **no observable difference**.
    The rendered image should be the same broken image when 1) attempting
    to render an html document as an image (without CORB) and 2) attempting to
    render an empty response as an image (when CORB blocks the response).
  * WPT test: `fetch/corb/img-html-correctly-labeled.sub.html`

* **Mislabeled image (with sniffing)**
  * Resource used in an `<img>` tag:
    * Body: an image
    * `Content-Type: text/html`
    * No `X-Content-Type-Options` header
  * Expected behavior: **no difference**.
    CORB will sniff that the response body is *not* actually a CORB-protected
    type and therefore will allow the response.
  * WPT test: `fetch/corb/img-png-mislabeled-as-html.sub.html`

* **Mislabeled image (nosniff)**
  * Resource used in an `<img>` tag:
    * Body: an image
    * `Content-Type: text/html`
    * `X-Content-Type-Options: nosniff`
  * Expected behavior: **observable difference**.
    Because of the `nosniff` header, CORB will have to rely on the
    `Content-Type` header.  Because this response is mislabeled (the body is an
    image, but the `Content-Type` header says that it is a html document), CORB
    will incorrectly classify the response as requiring CORB-protection.
  * WPT test: `fetch/corb/img-png-mislabeled-as-html-nosniff.tentative.sub.html`

In addition to the HTML `<img>` tag, the examples above should apply to other
web features that consume images - including, but not limited to:
* `/favicon.ico`
* SVG's `<image>`,
* `<link rel="preload" as="image" ...>` (see WPT test:
  `fetch/corb/preload-image-png-mislabeled-as-html-nosniff.tentative.sub.html`)
* `background-image` in stylesheets
* painting images onto (potentially tainted) HTML's `<canvas>`

> [lukasza@chromium.org] Earlier attempts to block nosniff images with
> incompatible MIME types
> [failed](https://github.com/whatwg/fetch/issues/395).
> We think that CORB will have more luck, because
> it will only block a subset of CORB-protected MIME types
> (e.g. it won't block `application/octet-stream` as quoted in a
> [Firefox bug](https://bugzilla.mozilla.org/show_bug.cgi?id=1302539))


### Observable CORB impact on multimedia

Audio and video resources should see similar impact as images, though 206
responses are more likely to occur for media.

### Observable CORB impact on scripts

CORB should have no observable impact on `<script>` tags except for cases where
a CORB-protected, non-JavaScript resource labeled with its correct MIME type is
loaded as a script - in these cases the resource will usually result in a syntax
error, but CORB-protected response's empty body will result in no error.

Examples:

* **Correctly-labeled HTML document**
  * Resource used in a `<script>` tag:
    * Body: an HTML document
    * `Content-Type: text/html`
    * No `X-Content-Type-Options` header
  * Expected behavior: **observable difference** via
    [GlobalEventHandlers.onerror](https://developer.mozilla.org/en-US/docs/Web/API/GlobalEventHandlers/onerror).
    Most CORB-protected resources would result in a syntax error when parsed as
    JavaScript.  The syntax error goes away after CORB blocks a response,
    because the empty body of the blocked response parses fine as JavaScript.
  * WPT test: `fetch/corb/script-html-correctly-labeled.tentative.sub.html`

> [lukasza@chromium.org] In theory, using a non-empty response in CORB-blocked
> responses might reintroduce the lost syntax error.  We didn't go down that
> path, because
> 1) using a non-empty response would be inconsistent with other parts of the
>    Fetch spec (like
>    [opaque filtered response](https://fetch.spec.whatwg.org/#concept-filtered-response-opaque)).
> 2) retaining the presence of the syntax error might require changing the
>    contents of a CORB-blocked response body depending on whether the original
>    response body would have caused a syntax error or not.  This would add
>    extra complexity that seems undesirable both for CORB implementors and for
>    web developers.

* **Mislabeled script (with sniffing)**
  * Resource used in a `<script>` tag:
    * Body: a proper JavaScript script
    * `Content-Type: text/html`
    * No `X-Content-Type-Options` header
  * Expected behavior: **no difference**.
    CORB will sniff that the response body is *not* actually a CORB-protected
    type and therefore will allow the response.  Note that CORB sniffing is
    resilient to the fact that some syntax elements are shared across HTML
    and JavaScript (e.g.
    [comments](http://www.ecma-international.org/ecma-262/6.0/#sec-html-like-comments)).
  * WPT test: `fetch/corb/script-js-mislabeled-as-html.sub.html`

* **Mislabeled script (nosniff)**
  * Resource used in a `<script>` tag:
    * Body: a proper JavaScript script
    * `Content-Type: text/html`
    * `X-Content-Type-Options: nosniff`
  * Expected behavior: **no observable difference**.
    Both with and without CORB, the script will not execute, because the
    `nosniff` response header response will cause the response to be blocked
    when its MIME type (`text/html` in the example) is not a
    [JavaScript MIME type](https://html.spec.whatwg.org/multipage/scripting.html#javaScript-mime-type)
    (this behavior is required by the
    [Fetch spec](https://fetch.spec.whatwg.org/#should-response-to-request-be-blocked-due-to-nosniff?)).
  * WPT test: `fetch/corb/script-js-mislabeled-as-html-nosniff.sub.html`

In addition to the HTML `<script>` tag, the examples above should apply to other
web features that consume JavaScript including
[script-like destinations](https://fetch.spec.whatwg.org/#request-destination-script-like)
like `importScripts()`, `navigator.serviceWorker.register()`,
`audioWorklet.addModule()`, etc.


### Observable CORB impact on stylesheets

CORB should have no observable impact on stylesheets.

Examples:

* **Anything not labeled as text/css**
  * Examples of resources used in a `<link rel="stylesheet" href="...">` tag:
    * Body: an HTML document OR a simple, valid stylesheet OR a polyglot
      HTML/CSS stylesheet
    * `Content-Type: text/html`
    * No `X-Content-Type-Options` header
  * Expected behavior: **no observable difference**.
    Even without CORB, such stylesheet examples will be rejected, because
    due to the
    [relaxed syntax rules](https://scarybeastsecurity.blogspot.dk/2009/12/generic-cross-browser-cross-domain.html)
    of CSS, cross-origin CSS requires a correct Content-Type header
    (restrictions vary by browser:
    [IE](http://msdn.microsoft.com/en-us/library/ie/gg622939%28v=vs.85%29.aspx),
    [Firefox](http://www.mozilla.org/security/announce/2010/mfsa2010-46.html),
    [Chrome](https://bugs.chromium.org/p/chromium/issues/detail?id=9877),
    [Safari](http://support.apple.com/kb/HT4070)
    (scroll down to CVE-2010-0051) and
    [Opera](http://www.opera.com/support/kb/view/943/)).
    This behavior is covered by
    [the HTML spec](https://html.spec.whatwg.org/C/#link-type-stylesheet)
    which 1) asks to only assume `text/css` Content-Type
    if the document embedding the stylesheet has been set to quirks mode and has
    the same origin and 2) only asks to run the steps for creating a CSS style
    sheet if Content-Type of the obtained resource is `text/css`.

  * WPT tests:
    `fetch/corb/style-css-mislabeled-as-html.sub.html`,
    `fetch/corb/style-html-correctly-labeled.sub.html`

* **Anything not labeled as text/css (nosniff)**
  * Resource used in a `<link rel="stylesheet" href="...">` tag:
    * Body: a simple stylesheet
    * `Content-Type: text/html`
    * `X-Content-Type-Options: nosniff`
  * Expected behavior: **no observable difference**.
    Both with and without CORB, the stylesheet will not load, because the
    `nosniff` response header response will cause the response to be blocked
    when its MIME type (`text/html` in the example) is not `text/css`
    (this behavior is required by the
    [Fetch spec](https://fetch.spec.whatwg.org/#should-response-to-request-be-blocked-due-to-nosniff?)).
  * WPT test: `fetch/corb/style-css-mislabeled-as-html-nosniff.sub.html`

* **Correctly-labeled stylesheet with a JSON security prefix**
  * Resource used in a `<link rel="stylesheet" href="...">` tag:
    * Body: a stylesheet that begins with
      [a JSON security prefix](https://docs.angularjs.org/api/ng/service/$http#json-vulnerability-protection)
    * `Content-Type: text/css`
    * No `X-Content-Type-Options` header
  * Expected behavior: **no difference**,
    because CORB sniffing for
    [JSON security prefixes](https://docs.angularjs.org/api/ng/service/$http#json-vulnerability-protection)
    is not performed for responses labeled as `Content-Type: text/css`.
  * WPT test: `fetch/corb/style-css-with-json-parser-breaker.sub.html`


### Observable CORB impact on other web platform features

CORB has no impact on the following scenarios:

* **[XHR](https://xhr.spec.whatwg.org/) and
  [fetch()](https://fetch.spec.whatwg.org/#dom-global-fetch)**
  * CORB has no observable effect, because [XHR](https://xhr.spec.whatwg.org/)
    and [fetch()](https://fetch.spec.whatwg.org/#dom-global-fetch) already apply
    same-origin policy to the responses (e.g. CORB should only block responses
    that would result in cross-origin XHR errors because of lack of CORS).

* **[Prefetch](https://html.spec.whatwg.org/C/#link-type-prefetch)**
  * CORB blocks response body from reaching a cross-origin renderer, but
    CORB doesn't prevent the response body from being cached by the browser
    process (and subsequently delivered into another, same-origin renderer
    process).

* **Tracking and reporting**
  * Various techniques try to check that a user has accessed some content
    by triggering a web request to a HTTP server that logs the user visit.
    The web request is frequently triggered by an invisible `img` element
    to a HTTP URI that usually replies either with a 204 or with a
    short HTML document.  In addition to the `img` tag, websites may use
    `style`, `script` and other tags to track usage.
  * CORB won't impact these techniques, as they don't depend on the actual
    content being returned by the HTTP server.  This also applies to other
    web features that don't care about the response: beacons, pings, CSP
    violation reports, etc.

* **Service workers**
  * Service workers may intercept cross-origin requests and artificially
    construct a response *within* the service worker (i.e. without crossing
    origins and/or security boundaries).  CORB will not block such responses.
  * When service workers cache actual cross-origin responses (e.g. in 'no-cors'
    request mode), the responses are 'opaque' and therefore CORB can block such
    responses without changing the service worker's behavior ('opaque' responses
    have a non-accessible body even without CORB).

* **Blob and File API**
  * Fetching cross-origin blob URLs is blocked even without CORB
    (see https://github.com/whatwg/fetch/issues/666).
  * WPT test: `script-html-via-cross-origin-blob-url.sub.html`
    (and also tests for navigation requests covered by the
    [commit here](https://github.com/mkruisselbrink/web-platform-tests/commit/9524a71919340eacc8aaa6e55ffe0b5aa72f9bfd)).

* **Content scripts and plugins**
  * These are not covered by CORB - CORB assumes that that appropriate security
    policies are enforced by some other mechanism for content scripts and
    plugins (e.g. Adobe Flash implements a CORS-like mechanism via
    [crossdomain.xml](https://www.adobe.com/devnet/articles/crossdomain_policy_file_spec.html)).

### Quantifying CORB impact on existing websites

CORB has been enabled in optional Site Isolation modes and field trials, and
Chromium has been instrumented to count how many CORB-eligible responses are
blocked.  (CORB-eligible responses exclude
[navigation requests](https://fetch.spec.whatwg.org/#navigation-request) and
downloads; see the "What kinds of requests are CORB-eligible?" section above.)
Our analysis of the initial data from Chrome Canary in February 2018 shows a low
upper bound on the number of cases observable to web pages, with possibilities
to further lower the bounds.

Overall, **0.961% of all CORB-eligible responses are blocked.**  However, over
half of these are empty responses already (i.e., actually have a
`Content-Length: 0` response header), and thus cause effectively no behavior
change (i.e., only
[non-safelisted](https://fetch.spec.whatwg.org/#cors-safelisted-response-header-name)
headers would be affected).  Note that if sniffing were omitted, almost 20% of
responses would be blocked, so sniffing is a clear necessity.

Looking closer, **0.456% of all CORB-eligible responses are non-empty and
blocked.**  However, most of these cases fall into the non-observable categories
described in the subsections above, such as HTML responses being delivered to
image tags as tracking pixels.

We can focus on two groups of blocked responses which may have observable
impact.

* **0.115% of all CORB-eligible responses might have been observably blocked due
  to a nosniff header or range request.**  This is specific to non-empty
  responses with a status code other than 204, requested from a context that
  doesn't otherwise ignore mislabeled nosniff content (e.g., as script tags
  would).  Within this group:
  * 95.16% of these are nosniff responses labeled as HTML requested by an image
    tag.  These are blocked and could possibly contain real images.  However, we
    expect many of these cases actually contained HTML and would not have
    rendered in the image tag anyway (as we observed in one case).

> [creis@chromium.org] We are considering lowering this bound further by
> sniffing these responses to confirm how many might contain actual images.

  * Another 3.76% of these are range requests for text/plain from a media
    context.  We have not yet found examples in practice, but we are considering
    allowing range request responses for text/plain to avoid disruption here.

* **0.014% of all CORB-eligible responses were invalid inputs to script tags**,
  since CORB sniffing revealed they were HTML, XML, or JSON.  Again, this is
  specific to non-empty responses that do not have a 204 status code.  These
  cases should have minimal risk of disruption in practice (e.g., more than half
  have error status codes and likely represent broken links), but it is
  technically possible to observe a difference based on whether a syntax error
  is reported.

These numbers of affected cases are sufficiently low to suggest that CORB is
promising from a web compatibility perspective.


## Appendix: Future work - protecting more resource types

The currently proposed version of CORB only protects JSON, HTML and XML
resources - other sensitive resources need to be protected in some other way.
One possible approach is to protect such resources via unguessable XSRF tokens
which are distributed via JSON (which is CORB-protected).

In the future CORB may be extended to protect additional resources as follows:

* **Covering more MIME types**.
  Instead of blocklisting HTML, XML, and JSON, CORB protection can be extended to
  all MIME types, except MIME types that are allowlisted as usable in `<img>`,
  `<audio>`, `<video>`, `<script>` and other similar elements that can be
  embedded cross-origin:
    * [JavaScript MIME type](https://html.spec.whatwg.org/C/#javascript-mime-type)
      like `text/javascript`, `application/javascript`, or `text/jscript`
    * `text/css`
    * [image types](https://mimesniff.spec.whatwg.org/#image-type) like types
      matching `image/*`
    * [audio or video types](https://mimesniff.spec.whatwg.org/#audio-or-video-type)
      like `audio/*`, `video/*` or `application/ogg`
    * `font/*` or one of legacy
      [font types](https://mimesniff.spec.whatwg.org/#font-type)
    * Other MIME types like
      `application/octet-stream`,
      [text/vtt](https://w3c.github.io/webvtt/#file-structure)

  This extension would offer CORB-protection to resources like PDFs or ZIP files.
  CORB would not perform confirmation sniffing for MIME types other than HTML,
  XML and JSON (since it is not practical to teach CORB sniffer about *all* the
  possible MIME types).  On the other hand, the value of confirmation sniffing
  for these other MIME types seems low, since mislabeling content as such
  types seems less likely than for example mislabeling as `text/html`.

> [lukasza@chromium.org] See also https://github.com/whatwg/fetch/issues/721

* **CORB opt-in header**.
  To protect resources that normally may be embedded cross-origin,
  a server might explicitly opt into CORB with a HTTP response header.
  This would make it possible to CORB-protect resources like
  images or JavaScript (including JSONP).

> [lukasza@chromium.org] Currently considered CORB opt-in signals include:
> - `From-Origin:` or `Cross-Origin-Resource-Policy:` header - see https://github.com/whatwg/fetch/issues/687
> - `Isolate-Me` header - see https://github.com/WICG/isolation

## Appendix: CORB and web standards

[The CORB section in the Fetch spec](https://fetch.spec.whatwg.org/#corb) covers
handling of `nosniff` and 206 responses since
[May 2018](https://github.com/whatwg/fetch/pull/686).

CORB confirmation sniffing is not standardized yet.

[Some aspects of CORB](https://github.com/whatwg/fetch/issues?utf8=%E2%9C%93&q=is%3Aissue+CORB+)
are under discussion and may evolve over time.

## Appendix: CORB implementation status

Tracking bugs:
- Chrome: https://crbug.com/268640 and https://crbug.com/802835 and https://www.chromestatus.com/feature/5629709824032768
- Edge: https://developer.microsoft.com/en-us/microsoft-edge/platform/issues/17382911/
- Firefox: https://bugzilla.mozilla.org/show_bug.cgi?id=1459357
- Safari/WebKit: https://bugs.webkit.org/show_bug.cgi?id=185331

Status of Web Platform Tests:
- [Experimental builds](https://master-dot-wptdashboard.appspot.com/results/fetch/corb?label=experimental)
- [Stable releases](https://master-dot-wptdashboard.appspot.com/results/fetch/corb)
