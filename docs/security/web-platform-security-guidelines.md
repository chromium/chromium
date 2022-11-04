# Web Platform Security guidelines

[TOC]

## Introduction
The Open Web Platform (OWP) is a fast evolving platform, with new features
continuously expanding the scope of what the platform can do. It is also a
particularly rich target for would-be attackers. In this context, all new
features should be reviewed with particular care when it comes to their
security implications. The goal of this document is to help feature teams go
through the Security part of the S&P review process which ensures that their
features meet the security requirements expected of a new Web Platform feature.

The guidelines in this document provide insight on how the Security teams think
about the security implications of new Web Platform features. They are here to
help feature teams think about security early on when designing their APIs. As
new threats and new mitigations arise, we will update this document to reflect
our updated recommendations.

Currently, this document is divided into three sets of guidelines: security
boundaries, integration with security APIs and security UX. We have written
those based on the Web Platform Security team experience of conducting security
reviews, in partnership with other security teams at Google. This is based on
concerns that have come up in security reviews, and a few items that we
envision could be problematic. 

The goal of this document is not to provide a checklist, where if every item in
the list is checked a feature can be considered secure. If you find that your
feature cannot meet some of the security guidelines on this list, please reach
out to the Web Platform Security team earlier rather than later, and we can
work together on how to support your feature’s needs in a secure manner.

## Guidelines

### General guidelines

<a name="TOC-safe-api-guidelines"></a>
#### Safe API design

> Prefer simple APIs.

* It is easier for developers to use higher-level and well laid out APIs. Try
to make the easy thing the safe thing in new APIs.
* If an API really needs potentially risky knobs, they should be well
documented and ideally named to explicitly call out their risk (e.g., the
subtle property in WebCrypto -- although this naming could be even more
explicit).

<a name="TOC-design-with-the-web-ecosystem-in-mind"></a>
#### Design with the web ecosystem in mind

> Consider how your feature will interact with the whole web ecosystem. In
particular, consider the interactions with workers (ServiceWorkers,
SharedWorkers, DedicatedWorkers), Fenced Frames and with the back/forward
cache.

* Some features might need to be restricted in workers. For example, we have
restricted access to features like camera/microphone/geolocation because we
have no UI surface with which to explain the implication of an API to users.
* Fenced Frames have particular privacy requirements that might require
disabling a feature inside them. For example, CSP EE required particular
integration as enforcing a CSP on a Fenced Frame would provide a
communication channel with the Fenced Frame embedder.
* The interaction of the feature with the back/forward cache might be a
security concern. For example, audio and video capture should only be allowed
in pages currently shown to the users, and not in pages located in the
back/forward cache. See also the [non-fully
active](https://www.w3.org/TR/security-privacy-questionnaire/#non-fully-active)
part of the W3C security and privacy questionnaire.

<a name="TOC-enterprise-policies"></a>
#### Enterprise policies

> New enterprise policies are not allowed to bypass existing web security
policies/protections. Enterprise policies can only be added to bypass newly
introduced security restrictions, to maintain compatibility of existing
enterprise web apps. Enterprise policies should only ever bypass policy
decisions made by the browser, and not policies requested by websites.

* When the browser introduces a new security restriction, such as gating Shared
Array Buffers behind crossOriginIsolation, it is ok to introduce an
enterprise policy to bypass that restriction in order to maintain
compatibility of existing enterprise web apps.
* It is not ok to create an enterprise policy to bypass longstanding security
restrictions that should be supported by existing apps. For example, it would
not be ok to introduce an enterprise policy to bypass the same-origin policy.
* Enterprise policies should only apply to new security restrictions introduced
to the browser. Enterprise policies should not be used to relax security
policies requested by the website themselves. For example, it would not be ok
to introduce an enterprise policy that bypasses CSP.
* See the [Chrome Security
FAQ](https://chromium.googlesource.com/chromium/src/+/master/docs/security/faq.md#Are-enterprise-admins-considered-privileged)
for more information on enterprise policies.

### Security boundaries

<a name="TOC-security-boundaries"></a>
#### Security boundaries

> The security team maintains several security boundaries in the WebPlatform:
origin, site, secure contexts, cross-origin isolated contexts. Before
introducing a new security boundary to support your design, discuss it with
the Web Platform Security team to ensure it's equally enforceable.

* Maintaining a security boundary is complex, and might not even be possible
(e.g. origins and Spectre). An API relying on a new form of security boundary
should be thoroughly discussed with the Security team to check if the
boundary is enforceable and the security guarantees can be met. For example,
it is impossible to create an iframe that is fully isolated from its parent,
due to the risk of Spectre attacks on platforms that do not support Site
Isolation (low-end Android).

<a name="TOC-the-origin-boundary"></a>
#### The origin boundary

> The origin is the security boundary we aim to defend. We may make diverge
> from that (in both directions) in some cases, but those ought to be done in
> consultation with security folks.

* The origin is the primary security boundary of the web, as per the
[Same-origin
policy](https://developer.mozilla.org/en-US/docs/Web/Security/Same-origin_policy).
* Note that unlike privacy, security is
concerned with same-site but cross-origin interactions.

* Maintaining a security boundary is complex, and might not even be possible
(e.g. origins and Spectre). An API relying on a new form of security boundary
should be thoroughly discussed with the Security team to check if the
boundary is enforceable and the security guarantees can be met. For example,
it is impossible to create an iframe that is fully isolated from its parent,
due to the risk of Spectre attacks on platforms that do not support Site
Isolation (low-end Android).

<a name="TOC-encryption"></a>
#### Encryption

> Prefer secure contexts for new features.

* Any data sent over HTTP can be observed by others on the network and opens
users to on-path attackers.
* The same-origin policy the web security model is built upon is easily abused
without cryptographical authentication of the servers we are talking to.
* See the [Chrome Security
FAQ](https://chromium.googlesource.com/chromium/src/+/main/docs/security/faq.md#why-are-some-web-platform-features-only-available-in-https-page_loads)
for more information.

<a name="TOC-timer-resolution"></a>
#### Timer resolution

> Explicit timers' granularity must be limited based on a context's
cross-origin isolation status. Currently, isolated contexts can support
timers coarsened to at least 5 microseconds, while unisolated contexts must
coarsen timers to 100 microseconds or more. If an API allows the creation of
timers with a precision higher than allowed in unisolated contexts, it should
be restricted to crossOriginIsolated contexts.

* High resolution timers open users to timing attacks such as Spectre. This is
why their precision should be limited.
* In crossOriginIsolated contexts, cross-origin resources are either loaded
without credentials, or they opt into being embedded cross-origin into a
context where they could potentially be read by their embedder. This means
that cross-origin resources in a crossOriginIsolated context are either ok
with a Spectre attack (opt-in model), or of no interest to an attacker
(credentialless model). Because of this, we allow higher precision timers in
crossOriginIsolated contexts. 
* APIs that can be used to create timers (e.g. SharedArrayBuffers) that are
more precise than timers available in cross-origin isolated contexts should
be gated behind crossOriginIsolation, in order to avoid introducing high
resolution timers to the platform.

<a name="TOC-accessing-data-of-cross-origin-subresources"></a>
#### Accessing data of cross-origin subresources

> New APIs that can access data from cross-origin subresources should be gated
> behind an appropriate mechanism depending on their surface:
> * CORS/TAO for access to a single resource.
> * crossOriginIsolation for access to the agent cluster.
> * crossOriginIsolation + frame opt-in mechanism for access to the whole page.

* This kind of API bypasses the same-origin policy, which is the base of the
Web security model. This is only acceptable if the cross-origin resources opt
into sharing this data.
* CORS or TAO are appropriate when divulging information about a single
resource, e.g. load timings for a single resource.
* crossOriginIsolation should be used when the API can divulge information
about cross-origin resources located in the same agent cluster (roughly, the
API is scoped to same-origin documents). For example, an API that measures
the memory taken by all same-origin documents and their subresources.
* If the API can divulge information from the resources located in the whole
page, we will need an opt-in from documents outside the agent cluster, on top
of crossOriginIsolation. For example, an API that streams a video of the page
or takes a screenshot of the page. Alternatively, it might be acceptable to
gate the API behind user interaction with a sufficiently informative UI
element.

<a name="TOC-side-channels"></a>
#### Side channels

> Any new form of cross-origin communication or API should be carefully
> considered when it comes to side-channel attack risks.

* Cross-origin communication channels and APIs can be abused to leak data from
cross-origin resources. Any new addition should be carefully reviewed to
assess the amount of data exposed. Note that unlike privacy, security is
concerned with same-site but cross-origin communication and APIs.

<a name="TOC-implementation-concerns"></a>
#### Implementation concerns

> Any new API whose implementation is particularly risky (e.g. requires new
> parsers, involves new codecs, requires particular isolation) should see the
> implementation reviewed in detail, in conjunction with the wider Chrome
> Security team.

* New parsers and codecs are particularly risky pieces of code that are exposed
to attacker-controlled inputs. Their implementations are subjected to
particular rules (see the [rule of
two](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/security/rule-of-2.md)),
and they must be fuzzed.
* Proper isolation is hard to deploy, is heavily dependent on implementation
and might face constraints on some platforms (e.g. low-end Android devices).

<a name="TOC-user-activation"></a>
#### User activation

> Consider requiring a feature be gated behind user activation if its UX could
> be abused by a document the user did not interact with.

* Several features should only be available if the user chooses to interact
with the document. If they could be abused otherwise, consider gating them
behind user activation. For example, fullscreen is gated behind user
activation, as it could be used to trick the user into believing they are on
another page by mimicking the Chrome UI.
* This helps match the platform with the user’s mental model.
* Features which are security sensitive will likely need aditional protection
beyond user activation. User activation is not a security boundary, it is a
way to protect users from abusive UX behavior from sites.

<a name="TOC-navigation-and-document-lifetime"></a>
#### Navigation and Document lifetime

> Any feature that impacts the lifetime of documents or that modifies
> navigation is likely to have far-reaching security implications. Please
> discuss the implications with the Web Platform Security team as soon as
> possible. 

* Modifying the navigation stack could cause URL spoofing attacks.
* Modifying the navigation stack or the document lifetime could result in wrong
origin or security policies being applied to the document.

### Integration with security policies

<a name="TOC-document-load"></a>
#### Document load

> All changes to how a document is loaded should ensure they uphold the following
> security policies:
> [XFO](https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/X-Frame-Options),
> [CSP](https://developer.mozilla.org/en-US/docs/Web/HTTP/CSP),
> [COOP](https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Cross-Origin-Opener-Policy),
> [COEP](https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Cross-Origin-Embedder-Policy),
> and [Private Network access](https://wicg.github.io/private-network-access/).

* XFO, CSP, COEP and Private Network access can block document load. This
should be respected.
* COOP can trigger a browsing context group switch which should be respected as
well.
* CSP, COEP and Private Network access are computed when loading a document and
may apply to all of the document resources. Failure to integrate properly
with them could result in the policies being bypassed.

<a name="TOC-subresource-load"></a>
#### Subresource load

> All changes to how a resource is loaded should uphold the following
> security policies:
> [CSP](https://developer.mozilla.org/en-US/docs/Web/HTTP/CSP),
> [COEP](https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Cross-Origin-Embedder-Policy),
> [CORP](https://developer.mozilla.org/en-US/docs/Web/HTTP/Cross-Origin_Resource_Policy_(CORP)),
> [CORS](https://developer.mozilla.org/en-US/docs/Web/HTTP/CORS),
> [CORB](https://fetch.spec.whatwg.org/#corb),
> [SRI](https://developer.mozilla.org/en-US/docs/Web/Security/Subresource_Integrity)
> and [Private Network access](https://wicg.github.io/private-network-access/).

* All of the policies above may block unsafe resource loads and must be
properly applied to any subresource load.

<a name="TOC-code-execution"></a>
#### Code execution

> New ways of executing JavaScript code should integrate with CSP script-src.

* New ways of executing JavaScript should defend themselves against XSS attacks
by supporting CSP.

> New ways of executing code should consider the cross-site scripting (XSS)
> risks. If any new risk is identified, the new API should integrate with CSP
> script-src and/or Trusted Types and/or Sanitizer.

* New ways of executing code should consider whether they are open to XSS
vulnerabilities. If they are, they should defend themselves against attacks
by supporting CSP and/or Trusted Types and/or Sanitizer.
* For example, the [import map
proposal](https://github.com/WICG/import-maps/issues/105) had to be spelled
as an extension to the
`<script>` tag to avoid inadvertently creating CSP bypasses.

<a name="TOC-CORS"></a>
#### CORS

> New types of elements should require CORS when loading resources.

* CORS allows an origin to control how its authenticated data is embedded in
other origins. In an ideal world, all cross-origin authenticated requests
would require CORS, but this is impossible for compatibility reasons.
However, new elements should not add a new source of non CORS requests to the
platform. 
* Elements that are allowed to make cross-origin authenticated requests without
CORS introduce a hole in the platform that may be exploited in a MIME
mismatch attack to bypass CORS protections for resources that are normally
loaded through Fetch or into elements that require CORS. 

<a name="TOC-mime-types"></a>
#### MIME types

> New resource types should require strict MIME type matching, and avoid
> relying upon [sniffing](https://mimesniff.spec.whatwg.org/).

* Mismatches between a resource's asserted MIME type and the way it's used by
the browser can cause security issues. For example, browsers currently
attempt to execute practically anything via `<script>` tags because of
widespread mislabeling of script resources as `text/html` or text/plain`,
which can expose those resources to side-channel attacks like Spectre, and
more direct XSSI attacks.
[CORB](https://chromium.googlesource.com/chromium/src/+/HEAD/services/network/cross_origin_read_blocking_explainer.md)
mitigates some, but not all, of these risks.
* New resource types should avoid these risks entirely by specifying clear MIME
types, and accepting only those resources that assert themselves to be of the
proper type.

### Security UX

<a name="TOC-iframes"></a>
#### Iframes

> A document element should not be allowed to draw outside its frame.

* A document drawing over its embedding frame allows it to perform clickjacking
attacks and should never be allowed.
* Drawing over the browser-controlled UX surface allows to perform all sorts of
attacks. See the browser-controlled surface guideline below for more
information.

<a name="TOC-browser-controlled-surface"></a>
#### Browser-controlled surface

> Browser-controlled surfaces should not be drawn over. Converting a
> browser-controlled UX surface into a content-controlled surface can only be
> considered in specific cases and must be gated behind appropriate mechanisms.

* Drawing over the omnibox allows to perform URL spoof attacks and should never
be allowed. It can also lead to displaying incorrect SSL state information to
the user.
* It is possible to consider converting some of the browser-controlled surfaces
into content-controlled surfaces in cases such as installed PWAs. This should
still be gated behind explicit signals from the user, such as a permission
grant or other in-context UI affordances that allow the user to toggle
between modes.
* Browser-controlled surfaces include the top browser chrome, but also the
fullscreen disclosure bubble, Payment Handler dialogs, permission dialogs,
etc.

<a name="TOC-site-identity-and-security-indicators"></a>
#### Site identity and security indicators

> Communicating site identity and security indicators should only be done
> through browser-controlled UI.

* Security state and site identity are state tracked by the browser and it is
difficult (at best) to show this information inside the content area in a
trustworthy way. See the [Chrome Security
FAQ](https://chromium.googlesource.com/chromium/src/+/master/docs/security/faq.md#Certificates-Connection-Indicators)
for more details.
* Interstitials (e.g., SSL error pages) are shown in the content area but are
browser controlled committed navigations.
* Other security displays overlaid on the content area (e.g., autofill
warnings) should be handled with care and should be controlled by the
browser.

<a name="TOC-browser-controlled-ui"></a>
#### Browser-controlled UI

> Changes to browser-controlled UI or new features that will require new
> browser-controlled UI should go through Chrome Browser security review.

* Browser-controlled UI has many considerations around spoofing, abuse, etc.
* New UI needs to be reviewed by various cross-functional groups (not just
security).
* This is especially true when the UI involves the user making a decision or
the UI communicates site identity to the user in any way.

<a name="TOC-permissions"></a>
#### Permissions

> Powerful new capabilities should in most cases integrate with
> Permissions-Policy and only be accessible to top-level frames by default.
> The Permissions Team should be brought in early to consult on new
> permissions.

* Permissions often require users to make a trust decision about a site. In
Chrome, the only visible site identity is the top-level frame (whose origin
is shown in the Omnibox).
* User confusion about subframe origins was a motivation for permission
delegation, where top-level contexts must explicitly delegate their
permissions to subframes, allowing the user to only have to reason about
top-level frames.

<a name="TOC-mixed-content"></a>
#### Mixed content

> New features should not be able to relax or work around mixed content
> restrictions.

* Chrome now upgrades or blocks all mixed content (insecure resources or
connections embedded in secure contexts). This greatly simplifies Chrome’s
security state model.
* Allowing new features to bypass these security restrictions can undermine
other security features in Chrome which assume no mixed content (such as
HTTPS-First Mode).

<a name="TOC-foreground-background-execution"></a>
#### Foreground/background execution

> Consider whether a new feature might be abusable or confusing to a user if a
> site can use it while in the background. Ensure that the user has sufficient
> context for something triggering and won’t be caught by surprise.

* This will often co-occur with “Browser-controlled UI” (see above), but it is
good to think about whether an API should be restricted to foreground tabs
only if they have a risk of being surprising to the user when used by a page.
* For example, we restrict HTML fullscreen to foreground contexts (and display
a disclosure UI). APIs that trigger permission prompts should only show the
prompts when the tab is in the foreground.

