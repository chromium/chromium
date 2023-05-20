# Special Case URLs

Several types of URLs lead to special case behavior in Chromium and are worth
considering as new features are built.

[TOC]


## about:blank

`about:blank` may sound like the simplest document in a browser, but it is
actually a huge source of corner cases and confusion:

 * **It may not be empty.** The creator of an `about:blank` document may inject
   content into it, using `document.write` or other DOM APIs like
   `document.body.innerHTML`.
 * **It may inherit an origin.** Navigating to `about:blank` in the address bar
   has a unique, opaque origin, but an `about:blank` iframe or popup created by
   another document will inherit that document's origin. (Caveat: Chromium's
   process model uses the initiator of the navigation to determine which process
   it belongs in, but Blink currently uses the parent's origin even if the
   initiator is not the parent, which we would like to fix in
   [issue 585649](https://crbug.com/585649). Blink also aliases the origin, so a
   modification of `document.domain` in the parent unexpectedly affects the
   `about:blank` document as well.)
 * **It may change its URL.** When another document uses `document.open` or
   `document.write` on an `about:blank` document, the `about:blank` document
   inherits `location.href` from the other document. However, this type of URL
   change does not occur for other DOM APIs (e.g., `document.body.innerHTML`
   modifications), and the browser process does not yet learn about the URL
   update at all (and thus the URL in the address bar does not change).
 * **It may have URL parameters.** Some pages navigate `about:blank` documents
   to fragments (e.g., `about:blank#foo`) or query parameters to communicate
   with scripts that have been injected into the document. For this reason, we
   recommend using `GURL::IsAboutBlank` to detect `about:blank` documents rather
   than comparing directly against `kAboutBlankURL`.
 * **It may be considered dangerous.** An `about:blank` document may inherit an
   origin of a broken HTTPS document or an origin initially blocked by Safe
   Browsing, resulting in potentially surprising address bar security
   indicators.
 * **It is present as the initial empty document of every frame.** While most
   users will not see `about:blank` in the address bar very often, it is created
   extremely frequently. Each main frame and subframe starts on `about:blank`
   until the first document commits, and many real pages create `about:blank`
   iframes to inject script code or other content into them.
 * **It may or may not commit.** Surprisingly, only some of the above cases
   generate navigation commit events in the browser process. The initial empty
   document will commit if the iframe or `window.open` call has no URL or
   `"about:blank"` itself, but if the iframe or `window.open` call use a real
   (potentially slow) URL, there will be no commit for the initial empty
   document.
 * **It may or may not stay in session history.** In most cases, the initial
   empty document is replaced in session history when the first non-blank URL
   commits, such that you cannot go back to the `about:blank` NavigationEntry.
   This is not true if a window is created with `window.open("about:blank")`,
   though, in which case the NavigationEntry is preserved.


## about:srcdoc

This URL commits when an iframe is created with a `srcdoc` parameter to define
its contents. The contents can only be defined by the parent (or a same-origin
document with access to the parent), and the document inherits its parent's
origin.


## iframe sandbox

When an iframe has a `sandbox` attribute (which does not include
`allow-same-origin`), it can load its content from a URL but the document has an
opaque origin, rather than the origin of the URL. For this reason, it is
important for most security checks to look at the origin rather than the URL
(see [Origin vs URL](security/origin-vs-url.md)).


## chrome: and os: URLs

`chrome:` URLs are used for privileged pages that are part of Chromium, such as
`chrome://settings`. Similarly, `os:` URLs are privileged pages that are part of
ChromiumOS. Web pages are not allowed to navigate to them, to reduce the risk of
privilege escalation attacks. Note that there are a subset of `chrome:` URLs
that are used for debug commands, described under [Debug URLs](#debug-urls)
below.


## Debug URLs

Chromium supports a series of "debug URLs" listed at the bottom of
`chrome://chrome-urls`, such as `chrome://crash`. These are used to crash, hang,
exit, or perform other debug actions. Like `javascript:` URLs, these URLs
represent a command rather than a destination, and they do not go through the
normal navigation flow or commit at all. Like the other
[chrome: URLs](#chrome_urls) discussed above, web pages are not allowed to
navigate to them.


## javascript: URLs

Navigating to a `javascript:` URL is essentially evaluating a JavaScript
expression in the target document, rather than navigating to a new document. As
a result, the Same Origin Policy only allows navigating same-origin documents to
`javascript:` URLs. A `javascript:` URL will never commit to session history.
However, if it evaluates to a string (e.g., `javascript:"foo"`), then the
contents of the document will be replaced with the string, similar to a
`document.write("foo")` statement. (This does create a new Document in Blink,
though, while document.write does not.)


## `chrome-error://chromewebdata`

When Chromium navigates to an error page, it commits as
`chrome-error://chromewebdata`. This URL is not displayed to the user (in favor
of the URL that failed or was blocked). Note that this error URL is not stored
in the NavigationEntry, but error pages can also be detected using the
`url_is_unreachable` bit on the commit params.
