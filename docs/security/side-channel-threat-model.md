# Post-Spectre Threat Model Re-Think

Contributors: awhalley, creis, dcheng, jschuh, jyasskin, lukasza, mkwst, nasko,
palmer, tsepez. Patches and corrections welcome!

Last Updated: 29 May 2018

[TOC]

## Introduction

In light of [Spectre/Meltdown](https://spectreattack.com/), we needed to
re-think our threat model and defenses for Chrome renderer processes. Spectre is
a new class of hardware side-channel attack that affects (among many other
targets) web browsers. This document describes the impact of these side-channel
attacks and our approach to mitigating them.

> The upshot of the latest developments is that the folks working on this from
> the V8 side are increasingly convinced that there is no viable alternative to
> Site Isolation as a systematic mitigation to SSCAs [speculative side-channel
> attacks]. In this new mental model, we have to assume that user code can
> reliably gain access to all data within a renderer process through
> speculation. This means that we definitely need some sort of ‘privileged/PII
> data isolation’ guarantees as well, for example ensuring that password and
> credit card info are not speculatively loaded into a renderer process without
> user consent. — Daniel Clifford, in private email

In fact, any software that both (a) runs (native or interpreted) code from more
than one source; and (b) attempts to create a security boundary inside a single
address space, is potentially affected. For example, software that processes
document formats with scripting capabilities, and which loads multiple documents
from different sources into the same process, may need to take defense measures
similar to those described here.

### Problem Statement

#### Active Web Content: Renderer Processes

We must assume that *active web content* (JavaScript, WebAssembly, Native
Client, Flash, PDFium, …) will be able to read any and all data in the address
space of the process that hosts it. Multiple independent parties have developed
proof-of-concept exploits that illustrate the effectiveness and reliability of
Spectre-style attacks. The loss of cross-origin confidentiality inside a single
process is thus not merely theoretical.

The implications of this are far-reaching:

* An attacker that can exploit Spectre can bypass certain native code exploit
  mitigations, even without an infoleak bug in software.
   * ASLR
   * Stack canaries
   * Heap metadata canaries
   * Potentially certain forms of control-flow integrity
* We must consider any data that gets into a renderer process to have no
  confidentiality from any origins running in that process, regardless of the
  same origin policy.

Additionally, attackers may develop ways to read memory from other userland
processes (e.g. a renderer reading the browser’s memory). We do not include
those attacks in our threat model. The hardware, microcode, and OS must
re-establish the process boundary and the userland/kernel boundary. If the
underlying platform does not enforce those boundaries, there’s nothing an
application (like a web browser) can do.

#### GPU Process

Chrome’s GPU process handles data from all origins in a single process. It is
not currently practical to isolate different sites or origins into their own GPU
processes. (At a minimum, there are time and space efficiency concerns; we are
still trying to get Site Isolation shipped and are actively resolving issues
there.)

However, WebGL exposed high-resolution clocks that are useful for exploiting
Spectre. It was possible to temporarily remove some of them, and to coarsen
another, with minimal breakage of web compatibility, and so [that has been
done](https://bugs.chromium.org/p/chromium/issues/detail?id=808744). However, we
expect to reinstate the clocks on platforms where Site Isolation is on by
default. (See [Attenuating Clocks, below](#attenuating-clocks).)

We do not currently believe that, short of full code execution, an attacker can
control speculative execution inside the GPU process to the extent necessary to
exploit Spectre-like vulnerabilities. [As always, evidence to the contrary is
welcome!](https://www.google.com/about/appsecurity/chrome-rewards/index.html)

#### Nastier Threat Models

It is generally safest to assume that an arbitrary read-write primitive in the
renderer process will be available to the attacker. The richness of the
attack/API surface available in a rendering engine makes this plausible.
However, this capability is not a freebie the way Spectre is — the attacker must
actually find 1 or more bugs that enable the RW primitive.

Site Isolation (SI) gets us closer to a place where origins face in-process
attacks only from other origins in their `SiteInstance`, and not from any
arbitrary origin. (Origins that include script from hostile origins will still
be vulnerable, of course.) However, [there may be hostile origins in the same
process](#multiple-origins-within-a-siteinstance).

Strict origin isolation is not yet being worked on; we must first ship SI on by
default. It is an open question whether strict origin isolation will turn out to
be feasible.

## Defensive Approaches

These are presented in no particular order, with the exception that Site
Isolation is currently the best and most direct solution.

### Site Isolation

The first order solution is to simply get cross-origin data out of the Spectre
attacker’s address space. [Site
Isolation](https://www.chromium.org/Home/chromium-security/site-isolation) (SI)
more closely aligns the web security model (the same-origin policy) with the
underlying platform’s security model (separate address spaces and privilege
reduction).

SI still has some bugs that need to be ironed out before we can turn it on by
default, both on Desktop and on Android. As of May 2018 we believe we can turn
it on by default, on Desktop (but not Android yet) in M67 or M68.

On iOS, where Chrome is a WKWebView embedder, we must rely on [the mitigations
that Apple is
developing](https://webkit.org/blog/8048/what-spectre-and-meltdown-mean-for-webkit/).

All major browsers are working on some form of site isolation, and [we are
collaborating publicly on a way for sites to opt in to
isolation](https://groups.google.com/a/chromium.org/forum/#!forum/isolation-policy),
to potentially make implementing and deploying site isolation easier. (Chrome
Desktop’s Site Isolation will be on by default, regardless, in the M67 – M68
timeframe.)

#### Limitations

##### Incompleteness of CORB

Site Isolation depends on [cross-origin read
blocking](https://chromium.googlesource.com/chromium/src/+/master/content/browser/loader/cross_origin_read_blocking_explainer.md)
(CORB; formerly known as cross-site document blocking or XSDB) to prevent a
malicious website from pulling in sensitive cross-origin data. Otherwise, an
attacker could use markup like `<img src="http://example.com/secret.json">` to
get cross-origin data within reach of Spectre or other OOB-read exploits.

As of M65, CORB protects:

* HTML, JSON, and XML responses.
  Protection requires the resource to be served with the correct
  `Content-Type` header. [We recommend using `X-Content-Type-Options:
  nosniff`](https://www.chromium.org/Home/chromium-security/ssca).
* text/plain responses which sniff as HTML, XML, or JSON.

Today, CORB doesn’t protect:

* Responses without a `Content-Type` header.
* Particular content types:
   * `image/*`
   * `video/*`
   * `audio/*`
   * `text/css`
   * `font/*`
   * `application/javascript`
   * PDFs, ZIPs, and other unrecognized MIME types
* Responses to requests initiated from the Flash plugin.

Site operators should read and follow, where applicable, [our guidance for
maximizing CORB and other defensive
features](https://developers.google.com/web/updates/2018/02/meltdown-spectre).
(There is [an open bug to add a CORB evaluator to
Lighthouse](https://bugs.chromium.org/p/chromium/issues/detail?id=806070).)

##### Multiple Origins Within A `SiteInstance` {#multiple-origins-within-a-siteinstance}

A *site* is defined as the effective TLD + 1 DNS label (“eTLD+1”) and the URL
scheme. This is a broader category than the origin, which is the scheme, entire
hostname, and port number. All of these origins belong to the same site:

* https, www.example.com, 443
* https, www.example.com, 8443
* https, goaty-desktop.internal.example.com, 443
* https, compromised-and-hostile.unmaintained.example.com, 8443

Therefore, even once we have shipped SI on all platforms and have shaken out all
the bugs, renderers will still not be perfect compartments for origins. So we
will still need to take a multi-faceted approach to UXSS, memory corruption, and
OOB-read attacks like Spectre.

Note that we are looking into the possibility of disabling assignments to
`document.domain` (via [origin-wide](https://wicg.github.io/origin-policy)
application of [Feature Policy](https://wicg.github.io/feature-policy/) or the
like). This would open the possibility that we could isolate at the origin
level.

##### Memory Cost

With SI, Chrome tends to spawn more renderer processes, which tends to lead to
greater overall memory usage (conservative estimates seem to be about 10%). On
many Android devices, it is more than 10%, and this additional cost can be
prohibitive. However, each renderer is smaller and shorter-lived under Site
Isolation.

##### Plug-Ins

###### PDFium

Chrome uses different PPAPI processes per origin, for secure origins. (We
tracked this as [Issue
809614](https://bugs.chromium.org/p/chromium/issues/detail?id=809614).)

###### Flash

Click To Play greatly reduces the risk that Flash-borne Spectre (and other)
exploits will be effective at scale.  Additionally, the enterprise policies
[PluginsBlockedForUrls](https://cloud.google.com/docs/chrome-enterprise/policies/?policy=PluginsBlockedForUrls)
and
[PluginsAllowedForUrls](https://cloud.google.com/docs/chrome-enterprise/policies/?policy=PluginsAllowedForUrls)
can be combined to restrict Flash to specific websites.
Even so,
[we might want to consider teaching CORB about Flash flavour of CORS](https://crbug.com/816318).

##### All Frames In A `<webview>` Run In The Same Process

[`<webview>`s run in a separate renderer
process](https://developer.chrome.com/apps/tags/webview), but that single
process hosts all frames in the `<webview>` (even with Strict Site Isolation
enabled elsewhere in Chrome). Extra work is needed to fix this.

Mitigating factors:

* `<webview>` is available only to Web UI and Chrome Apps (which are deprecated
  outside of Chrome OS).
* `<webview>` contents are in a separate storage partition (separate from the
  normal profile and from the Chrome App using the `<webview>` tag). The Chrome
  App is also in an additional separate storage partition.

Chrome WebUI pages must not, and Chrome Apps should not, use `<webview>` for
hosting arbitrary web pages. They must only allow a single trustworthy page or
set of pages. The user already has to trust the Chrome App to do the right thing
(there is no Omnibox, for example) and only take the user to safe sites. If we
can’t enforce this programmatically, we may consider enforcing it through code
review.

##### Android `WebView`

Android `WebView`s run in their own process as of Android O, so the hosting
application gets protection from malicious web content. However, all origins are
run in the same `WebView` process.

### Ensure User Intent When Sending Data To A Renderer

Before copying sensitive data into a renderer process, we should somehow get the
person’s affirmative knowledge and consent. This has implications for all types
of form auto-filling: normal form data, passwords, payment instruments, and any
others. It seems like we are [currently in a pretty good place on that
front](https://bugs.chromium.org/p/chromium/issues/detail?id=802993), with one
exception: usernames and passwords get auto-filled into the shadow DOM, and then
revealed to the real DOM on a (potentially forged?) user gesture. These
credentials are origin-bound, however.

The [Credential Management
API](https://developer.mozilla.org/en-US/docs/Web/API/Credential_Management_API)
still poses a risk, exposing usernames/passwords without a gesture for the
subset of users who've accepted the auto-sign-in mechanism.

What should count as a secure gesture is a gesture on relevant, well-labeled
browser chrome, handled in the browser process. Tracking the gesture in the
renderer, that can be forged by web content that compromises the renderer, does
not suffice.

#### Challenge

We must enable a good user experience with autofill, payments, and passwords,
while also not ending up with a browser that leaks these super-important classes
of data. (A good password management experience is itself a key security goal,
after all.)

### Reducing Or Eliminating Speculation Gadgets

Exploiting Spectre requires that the attacker can find (in V8, Blink, or Blink
bindings), generate, or cause to be generated code ‘gadgets’ that will read out
of bounds when speculatively executed. By exerting more control over how we
generate machine code from JavaScript, and over where we place objects in memory
relative to each other, we can reduce the prevalence and utility of these
gadgets. The V8 team has been [landing such code generation
changes](https://bugs.chromium.org/p/chromium/issues/detail?id=798964)
continually since January 2018.

Of the known attacks, we believe it’s currently only feasible to try to mitigate
variant 1 with code changes in C++. We will need the toolchain and/or platform
support to mitigate other types of speculation attacks. We could experiment with
inserting `LFENCE` instructions or using
[Retpoline](https://support.google.com/faqs/answer/7625886) before calling into
Blink.

PDFium uses V8 for its JavaScript support. To the extent that we rely on V8
mitigations for Spectre defense, we need to be sure that PDFium uses the latest
V8, so that it gets the latest mitigations. In shipping Chrome/ium products,
PDFium uses the V8 that is in Chrome/ium.

#### Limitations

We don’t consider this approach to be a true solution; it’s only a mitigation.
We think we can eliminate many of the most obvious gadgets and can buy some time
for better defense mechanisms to be developed and deployed (primarily, Site
Isolation).

It is very likely impossible to eliminate all gadgets. As with [return-oriented
programming](https://en.wikipedia.org/wiki/Return-oriented_programming), a large
body of object code (like a Chrome renderer) is likely to contain so many
gadgets that the attacker has a good probability to craft a working exploit. At
some point, we may decide that we can’t stay ahead of attack research, and will
stop trying to eliminate gadgets.

Additionally, the mitigations typically come with a performance cost, and we may
ultimately roll some or all of them back. Some potential mitigations are so
expensive that it is impractical to deploy them.

### Attenuating Clocks {#attenuating-clocks}

Exploiting Spectre requires a clock. We don’t believe it’s possible to
eliminate, coarsen, or jitter all explicit and implicit clocks in the Open Web
Platform (OWP) in a way that is sufficient to fully resolve Spectre. ([Merely
enumerating all the
clocks](https://bugs.chromium.org/p/chromium/issues/detail?id=798795) is
difficult.) Surprisingly coarse clocks are still useful for exploitation.

While it sometimes makes sense to deprecate, remove, coarsen, or jitter clocks,
we don’t expect that we can get much long-term defensive value from doing so,
for several reasons:

* There are [many explicit and implicit clocks in the
  platform](https://bugs.chromium.org/p/chromium/issues/detail?id=798795)
* It is not always possible to coarsen or jitter them enough to slow or stop
  exploitation…
* …while also maintaining web platform compatibility and utility

In particular, [clock jitter is of extremely limited
utility](https://rdist.root.org/2009/05/28/timing-attack-in-google-keyczar-library/#comment-5485)
when defending against side channel attacks.

Many useful and legitimate web applications need access to high-precision
clocks, and we want the OWP to be able to support them.

### Gating Access To APIs That Enable Exploitation

**Note:** This section explores ideas but we are not currently planning on
implementing anything along these lines.

Although we want to support applications that necessarily need access to
features that enable exploitation, such as `SharedArrayBuffer`, we don’t
necessarily need to make the features available unconditionally. For example, a
third-party `iframe` that is trying to exploit Spectre is very different than a
WebAssembly game, in the top-level frame, that the person is actively playing
(and issuing many gestures to). We could programmatically detect engagement and
establish policies for when certain APIs and features will be available to web
content. (See e.g. [Feature Policy](https://wicg.github.io/feature-policy/).)

*Engagement* could be defined in a variety of complementary ways:

* High [site engagement
  score](https://www.chromium.org/developers/design-documents/site-engagement)
* High site popularity, search rank, or similar
* Frequent gestures on/interactions with the document
* Document is the top-level document
* Document is the currently-focused tab
* Site is bookmarked or added to the Home screen or Desktop

Additionally, we have considered the possibility of prompting the user for
permission to run certain exploit-enabling APIs, although there are problems:
warning fatigue, and the difficulty of communicating something accurate yet
comprehensible to people.

## Conclusion

For the reasons above, we now assume any active code can read any data in the
same address space. The plan going forward must be to keep sensitive
cross-origin data out of address spaces that run untrustworthy code, rather than
relying on in-process checks.
