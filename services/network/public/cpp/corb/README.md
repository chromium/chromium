# Cross-Origin Read Blocking / Opaque Resource Blocking (CORB/ORB)

tl;dr:

* CORB and ORB are related solutions to the same problem.
* One can consider CORB to be v1, and ORB to be v2.
* This directory implements both.
* Implementation and rollout will be in stages.
* For websites that correctly label the MIME types of their responses none of
  this matters.

# The Problem CORB/ORB Are Solving

Browsers enforce the [same-origin policy](https://developer.mozilla.org/en-US/docs/Web/Security/Same-origin_policy)
for many website-initiated fetches. However, in some contexts, the same origin
policy does not apply. For example, a page may load images from other origins.
The specification refers to these as requests with a ["no-cors" request mode](https://fetch.spec.whatwg.org/#concept-request-mode).

Meanwhile, the ["Spectre" family of attacks](https://chromium.googlesource.com/chromium/src/+/main/docs/security/side-channel-threat-model.md)
allow a website to read memory within an operating system process.
(Caveats apply and it's more complicated that that; but that's the gist.)

These, at its extreme, allow a near-complete circumvention of the "same-origin
policy": By first loading any resource one wishes to read via e.g. an `<img>`
tag, and then reading the result from its own memory via a "Spectre" gadget.
In this scenario, the image loading will fail and the site does not get
any programmatic access to the image content. (That is, to the bytes of the
resource, that the attacker pretended to be an image.) But because the
"image" has been brought into the renderer process (in order to parse it), and
because "Spectre" allows access to memory within the process, an attacking
page might get its contents after all.

Additionally, CORB/ORB serve as a "defense in depth" measure: They help
to improve [isolation](https://chromium.googlesource.com/chromium/src/+/main/docs/process_model_and_site_isolation.md) between origins and thus make it
harder [to exploit a compromised renderer](https://chromium.googlesource.com/chromium/src/+/main/docs/security/compromised-renderers.md).

## Security Properties

In brief, the security property we want from CORB/ORB is this: For any
"no-cors" request we want positive evidence that the response received
is of the intended data type.

Note:
* CORB/ORB has nothing to add for non-"no-cors" requests.
* We require this evidence before handing the resource data off to the renderer.
* For "positive evidence" we accept a conservative approximation.
  It's not okay to block a valid resource; but it might well be okay to
  occasionally let an inappropriate resource pass.
* A "correct" MIME type is good enough evidence.
* A "wrong" MIME type is good enough counter evidence.
* For historic reasons, browsers accept a number of resources with
  inappropriate MIME types. For these historically accepted MIME types,
  we require "content sniffing" to provide us with the desired evidence.

## A Note on Deprecations

Both CORB and ORB are [deprecations](https://developer.chrome.com/blog/deps-rems-101/)
at heart. That is: Formerly allowed behaviour would be newly disallowed and
blocked. This invariably causes issues for some well-meaning websites that,
often inadvertently, rely on the behaviour to be deprecated.
Any deprecation requires great care when deploying in order to minimize
unintended side-effects on the web ecosystem.

# The Solution(s)

## Cross-Origin Read Blocking (CORB)

[Cross-Origin Read Blocking](https://www.chromium.org/Home/chromium-security/corb-for-developers/)
identifies a number of resource types that should be protected, namely
HTML, XML (except SVG), and JSON, and blocks them from being loaded in
"no-cors" responses. None of the intended usages for these resource types
issue "no-cors" requests. In addition to MIME type checks, it also employs
"sniffers" for these formats.

The full details are more complicated. Here, we'll skip over details of
sniffing, error handling, and partial content responses (HTTP 206 responses).
Additional details can be found
[here](https://chromium.googlesource.com/chromium/src/+/main/services/network/cross_origin_read_blocking_explainer.md) and [here](https://www.chromium.org/Home/chromium-security/corb-for-developers/).

Note that this is a partial mismatch for our security requirements: Instead
of requiring positive evidence for the intended format, we instead picked a
handful of known-bad cases. Essentially, we look for negative evidence
for a request type mismatch, and ignore all the cases for which we don't have
such evidence.

## Opaque Resource Blocking (ORB)

[ORB](https://github.com/annevk/orb) is an alternative proposal to solve the
same problem. In Chromium we intend to replace CORB with ORB, so that in
the context of Chromium ORB could be considered as a "version 2" of CORB.

The fundamental difference between CORB and ORB is that CORB picked
specific type mismatches to disallow, while ORB enumerates the data formats
that we expect to occur in "no-cors" requests and blocks the rest.
This makes ORB a better fit for our security requirements. It also makes ORB
a much bigger risk for web compatibility.

## "ORB v0.1"

Since CORB/ORB are deprecations, great care must be taken to not
break legitimate web sites. Since no implementation of ORB exists
(as of 2022-05), we have no existing web compatibiltiy data and must thus
be careful in deploying ORB.

As a first step we are implementing a subset, dubbed "ORB v0.1". This differs
from ORB as proposed:

- It is more permissive with several resource types: `audio/*`, `video/*`,
  and some XML MIME types.
- It does not implement the JavaScript parsing steps. Instead it
  re-uses several sniffers from CORB:
  [HTML](https://chromium.googlesource.com/chromium/src/+/main/services/network/cross_origin_read_blocking_explainer.md#protecting-html),
  [XML](https://chromium.googlesource.com/chromium/src/+/main/services/network/cross_origin_read_blocking_explainer.md#protecting-xml), and
  [JSON and "XSSI-defeating prefixes"](https://chromium.googlesource.com/chromium/src/+/main/services/network/cross_origin_read_blocking_explainer.md#protecting-json).
- It will accept any response for which these heuristics do not deliver a
  verdict.
- CORB error handling is re-used: If a response is blocked, an empty response
  will be injected in its stead. (ORB proposes an error response.)

One unfortunate effect of postponing JSON and JavaScript sniffing is that
"ORB v0.1" does not yet achieve our goal of blocking any resource for which
we do not have positive evidence.

## ORB post v0.1

These plans are not settled. We will iterate towards a more complete ORB
implementation. Note that some ORB details are also not finalized yet.

# Appendix: Implementation and Spec Status

## CORB

* Implemented & shipped: Chrome M67
* Spec: Formerly https://fetch.spec.whatwg.org/#corb (removed [here](https://github.com/whatwg/fetch/commit/78f9bdd73e8c6893d629c2ce4a3bde7eb01cac59))

## "ORB v0.1"

* Spec: n/a. Follows the [ORB](https://github.com/annevk/orb) proposal, but
  with several differences, noted below.
* Implemented & shipped: [Intent here](https://groups.google.com/u/1/a/chromium.org/g/blink-dev/c/ScjhKz3Z6U4/m/kW5RjWamAQAJ)

## ORB

* Draft Spec: https://github.com/annevk/orb
* Implementation: Under discussion.

# Appendix: Differences between "ORB v0.1" and ORB

The main difference is that "ORB v0.1" is more permissive, to reduce web
compatibility risks. A more detailed write-up may be found
[here](https://docs.google.com/document/d/1qUbE2ySi6av3arUEw5DNdFJIKKBbWGRGsXz_ew3S7HQ/edit#heading=h.mptmm5bpjtdn).

Below are the tabulated differences between "ORB v0.1" and the ORB proposal.
Decisions are based on four factors:

* The response's MIME type,
* the response's "nosniff" header,
* several "sniffers" that heuristically examine the first 1KiB of data,
* and parsers for the full resource (ORB only).

| | "ORB v0.1" | ORB | Comment |
| --- | --- | --- | --- |
| MIME type: JavaScript | n/a (allow, unless it "sniffs" wrong. This follows from the rules below.) | **allow** (without sniffing) | Known-good "no-cors" MIME types.
| MIME type: HTML, JSON, XML, text/plain | **block** (if nosniff) | **block** (if nosniff) | MIME types that have historically been accepted in "no-cors" requests. We hope developers set the "nosniff" header.
| MIME type: zip + gzip, various MS office types; protobuf, text/csv | **block** | **block** | "Never sniff" MIME types. Not allowed in any "no-cors" requests.
| MIME type: audio/* or video/* | **allow** | **block** (unless it "sniffs" okay. This follows from the rules below. I'd expect most resources to "sniff" okay, though, so in practice these would likely be mostly allowed.) | "ORB v0.1" relaxes audio + video handling, and just lets all audio + video MIME types pass.
| MIME type: n/a (invalid/missing MIME type) | n/a (rules below apply. Would probably mostly allow.) | **allow** | |
| sniffs as audio/*, video/*, image/* | **allow** | **allow** | |
| JSON / JS handling | **block** if first 1KiB bytes sniff as <ul><li>HTML (but not MIME type: CSS),</li><li>XML (but not MIME type: SVG),</li><li>JS parser breaker,</li><li>or JSON.</li></ul> | **block** if "nosniff", **allow** if parses (!!) as JavaScript, **block** otherwise. (E.g. JSON or HTML or PDF won’t parse as JS.) | "ORB v0.1" makes a very deliberate decision to not parse a full response.
| default | **allow** | **block** | Because "ORB v0.1" can only "sniff" certain JS anti-patterns, it has to make the unfortunate decision to allow unknown content. We'd very much like to fix this, without the cost of a full parse.

The table omits:

* Blocked response handling: "ORB v0.1" injects empty responses instead of
  returning an error.
* Handling of range requests / partial content responses (HTTP 206 responses).
