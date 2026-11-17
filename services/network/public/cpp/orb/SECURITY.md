# Security Considerations for Opaque Response Blocking (ORB)

## Scope

This file applies to ORB in `//services/network/public/cpp/orb/`. See
[README.md](README.md) for an overview of ORB.

## Threat Model

ORB runs in the network service. For a cross-origin `no-cors` request, ORB
decides if the response body can go to the renderer process that made the
request. This protects the response from an attacker who has compromised that
renderer, or who can read its memory with a Spectre-style attack. See
[Threat Model And Defenses Against Compromised Renderers](https://chromium.googlesource.com/chromium/src/+/main/docs/security/compromised-renderers.md).

ORB decides mainly based on the response headers and the start of the response
body. Because of this, the protection of a response depends on how the website
serves it.

## Severity of ORB Bypasses

### Real-World Impact

An ORB bypass lets a renderer process receive a cross-origin response that ORB
should block. For such a bypass to be useful to an attacker, the victim website
must also serve a response that:

* contains user-private data,
* needs authentication, so that the attacker cannot get the same response from
  their own machine,
* is not protected by `SameSite` cookies, which the attacker cannot attach from
  a cross-site renderer process, and
* has a URL that the attacker can know in advance. Responses with user-private
  data often have unguessable URLs.

Without evidence that real, popular websites serve sensitive responses in a way
that would be vulnerable, these conditions are
[extreme mitigating factors](https://chromium.googlesource.com/chromium/src/+/main/docs/security/severity-guidelines.md#TOC-Low-severity).
Thus, rate an ORB bypass as Low severity (S3), unless the report shows such
evidence. If there is such evidence, the severity can be higher.

Background: comments in
[issue 560039872](https://crbug.com/560039872#comment18) and
[issue 563877891](https://crbug.com/563877891#comment6).

### HTML, XML, and JSON Sniffing

Many websites serve JavaScript and other subresources with an incorrect
`Content-Type`, for example `text/html`. To stay compatible with these
websites, by default ORB does not block a response only because it is labeled
as HTML, XML, JSON, or `text/plain`. Instead, ORB sniffs the start of the
response body, and blocks the response if it looks like HTML, XML, or JSON.

Sniffing is a best-effort heuristic. It does not find all HTML, XML, and JSON
responses. For example:

* It looks at only the first 1024 bytes of the response body.
* It does not recognize text in some encodings, for example UTF-16.
* If sniffing cannot decide, then ORB allows the response.

To more effectively protect a response, a website should send the correct
`Content-Type` and `X-Content-Type-Options: nosniff`. ORB then blocks responses
labeled as HTML, XML, JSON, or `text/plain`, without sniffing. This is the
recommendation in
[Post-Spectre Web Development](https://chromium.googlesource.com/chromium/src/+/main/docs/security/post-spectre-webdev.md).

Thus, a gap in sniffing (i.e., an HTML, XML, or JSON response that ORB allows
because sniffing does not recognize it) is Low severity (S3), like other ORB
bypasses. Websites can avoid such gaps in protection by adopting `nosniff`.

Background: comments in
[issue 497529290](https://crbug.com/497529290#comment3).

## Reporting

Before you report an ORB bug, read
[Security for Agents](https://chromium.googlesource.com/chromium/src/+/main/docs/security/security-for-agents.md).
