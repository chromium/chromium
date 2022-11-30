# Blink Clipboard Overview

Blink exposes the clipboard to users, to allow them to share content between
a web application and a native or web application. Users most commonly interact
with this via "ctl+c" / "ctl+v" or the context menu's "copy", "cut", and "paste"
buttons. That said, Blink does also expose some APIs for use by sites.

## APIs

A few different clipboard APIs are exposed:
* `document.execCommand('copy')`
* `DataTransfer`
* Async Clipboard API

### Overviews

#### `document.execCommand('copy')`

`document.execCommand('copy')` can be used to copy content from HTML documents,
like text, HTML, and images. This API is deprecated and has inconsistent support
between browsers, and is mostly only supported for legacy reasons. It is also
synchronous, which means large copies or pastes will cause jank and freeze the
browser. Therefore, while we continue to maintain this API for legacy reasons,
we recommend sites against using this API.

`document.execCommand('copy')` is:
* specified in the
  [execCommand](https://w3c.github.io/editing/docs/execCommand/#the-copy-command)
  specification.
* documented for web developers in
  [MDN](https://developer.mozilla.org/en-US/docs/Web/API/Document/execCommand).

#### `DataTransfer`

`DataTransfer` is a mature, flexible API with strong user gesture guarantees,
that Chrome inherited when it forked from WebKit, and was likely based on IE's
implementation at the time. It supports both clipboard and drag-and-drop.

`DataTransfer` is:
* specified in the
  [HTML](https://html.spec.whatwg.org/multipage/dnd.html#the-datatransfer-interface).
  specification.
* documented for web developers in the
  [MDN](https://developer.mozilla.org/en-US/docs/Web/API/DataTransfer).

#### Async Clipboard API

The Async Clipboard API is a relatively new, programmatic API that aims to be
the primary API for new clipboard functionality. It allows for much larger
clipboard payloads due to its asynchronous behavior, and includes some newer
protections like permissions policy.

The Async Clipboard API is:
* specified in the
  [Clipboard API and events](https://w3c.github.io/clipboard-apis/)
  specification.
* documented for web developers in
  [MDN](https://developer.mozilla.org/en-US/docs/Web/API/Clipboard_API).

### Common topics

#### Sanitization

As documented in the
[mojo barrier's](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/public/mojom/clipboard/clipboard.mojom)
"security note" and "privacy note", sanitization is an important part of
safeguarding the user's:
* security and system integrity on write
* privacy and personally identifiable information (PII) on read

Unfortunately, sanitization often reduces fidelity by stripping away important
metadata or other information, like CSS `<style>` tags and `<script>` tags in
HTML. While this is done to protect the integrity of the underlying operating
system, the reduced fidelity can harm the productivity of web applications by
reducing the set of features they can adopt.

Sanitization is commonly accomplished by sending untrusted web-originated
payloads through encoders and decoders trusted to safely remove malicious
content, as seen in the Async Clipboard API's
[`ClipboardWriter`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/modules/clipboard/clipboard_writer.h).

Currently:
* `document.execCommand('copy')` aggressively sanitizes clipboard content,
  including HTML, where `<style>`, `<meta>`, `<script>`, etc are all sanitized.
  This is especially important for this API because it handles content actively
  on the HTML document.
* The Async Clipboard API sanitizes relatively aggressively, like
  `document.execCommand('copy')`, for safety.
* `DataTransfer` does *not* sanitize most clipboard content, including HTML.
  This was likely done in the past for parity with Internet Explorer, and
  sanitization cannot be added now without breaking compatibility with sites.

#### Asynchronous Behavior

The Async Clipboard API is the only asynchronous clipboard API currently
available to websites. It's recommended for all new use cases, because the API
surface allows sites, especially those copying or pasting large payloads
(ex. image editing applications) to copy or paste without blocking the
browser process and freezing users screens until the operation is complete.
Currently, much of the clipboard infrastructure is synchronous, so this benefit
is mostly only realized during sanitization, which is done in a background
thread in the renderer. This asynchronous behavior can be improved by improving
[promise support](https://crbug.com/1014310) or
[improvements in asynchronicity](https://crbug.com/443355) of the browser
process clipboard implementation.

In contrast, all other clipboard APIs are completely synchronous, and a very
large clipboard payload will freeze the renderer both during sanitization,
and while passing information around, until the operation is complete. As their
API surfaces are synchronous, improvements in asynchronicity of clipboard
infrastructure will be unlikely to improve these other APIs' freezing of the
renderer on large reads/writes.

#### User Gesture

`document.execCommand('copy')` and the Async Clipboard API do not require user
gesture, whereas DataTransfer requires a strict user gesture in the form of a
`copy` or `paste` event. It would be web-incompatible to require user gesture,
but requiring it could greatly secure the clipboard and improve specification
compliance. This is tracked and discussed in greater detail in
[this bug](https://crbug.com/1230211).

#### API Recommendations

Generally, the Async Clipboard API is recommended for new use-cases, as it's
the most modern, asynchronous, and a generally new API.
