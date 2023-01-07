# Post-Spectre Web Development

Contributors: Artur Janc, Camille Lamy, Charlie Reis, Jun Kokatsu, Mike West.
Patches and corrections welcome!

Last Updated: Mar. 12th, 2021

*** note
**Note**: This document has been [adopted][cfc] by the W3C's Web Application Security
Working Group; [https://w3c.github.io/webappsec-post-spectre-webdev/][ED] is
proceeding under that group's aegis, and this document will redirect there
once the working group is satisfied with its recommendations.
***

[cfc]: https://lists.w3.org/Archives/Public/public-webappsec/2021Feb/0007.html
[ED]: https://w3c.github.io/webappsec-post-spectre-webdev/

[TOC]

## Introduction

In early 2018, [Spectre][spectre] made it clear that a foundational security
boundary the web aimed to maintain was substantially less robust than expected.
This revelation has pushed web browsers to shift their focus from the
platform-level origin boundary to an OS-level process boundary. Chromium's
[threat model][post-spectre-rethink], for instance, now asserts that "active
web content … will be able to read any and all data in the address space of the
process that hosts it". This  shift in thinking imposes a shift in development
practice, both for browser vendors, and for web developers. Browsers need to
align the origin boundary with the process boundary through fundamental
refactoring projects (for example, Chromium's [Site Isolation][site-isolation],
and Firefox's [Project Fission][project-fission]). Moreover, browsers must
provide web developers with tools to mitigate risk in the short term, and should
push the platform towards safe default behaviors in the long term. The bad news
is that this is going to be a lot of work, much of it falling on the shoulders
of web developers. The good news is that a reasonable set of mitigation
primitives exists today, ready and waiting for use.

This document will point to a set of mitigations which seem promising, and
provide concrete recommendations for web developers responsible for protecting
users' data.

[spectre]: https://spectreattack.com/
[post-spectre-rethink]: https://chromium.googlesource.com/chromium/src/+/main/docs/security/side-channel-threat-model.md
[site-isolation]: https://www.chromium.org/Home/chromium-security/site-isolation
[project-fission]: https://wiki.mozilla.org/Project_Fission


### Threat Model

Spectre-like side-channel attacks inexorably lead to a model in which active web
content (JavaScript, WASM, probably CSS if we tried hard enough, and so on) can
read any and all data which has entered the address space of the process which
hosts it. While this has deep implications for user agent implementations'
internal hardening strategies (stack canaries, ASLR, etc), here we'll remain
focused on the core implication at the web platform level, which is both simple
and profound: any data which flows into a process hosting a given origin is
legible to that origin. We must design accordingly.

In order to determine the scope of data that can be assumed accessible to an
attacker, we must make a few assumptions about the normally-not-web-exposed
process model which the user agent implements. The following seems like a good
place to start:

1.  User agents are capable of separating the execution of a web origin's code
    into a process distinct from the agent's core. This separation enables the
    agent itself to access local devices, fetch resources, broker cross-process
    communication, and so on, in a way which remains invisible to any process
    potentially hosting untrusted code.

2.  User agents are able to make decisions about whether or not a given resource
    should be delivered to a process hosting a given origin based on
    characteristics of both the request and the response (headers, etc).

3.  User agents can consistently separate top-level, cross-origin windows into
    distinct processes. They cannot consistently separate same-site or
    same-origin windows into distinct processes given the potential for
    synchronous access between the windows.

4.  User agents cannot yet consistently separate framed origins into processes
    distinct from their embedders' origin.
    
    Note: Though some user agents support [out-of-process frames][oopif], no
    agent supports it consistently across a broad range of devices and
    platforms. Ideally this will change over time, as the frame boundary *must*
    be one we can eventually consider robust.

With this in mind, our general assumption will be that an origin gains access to
any resource which it renders (including images, stylesheets, scripts, frames,
etc). Likewise, embedded frames gain access to their ancestors' content.

This model is spelled out in more detail in both Chromium's
[Post-Spectre Threat Model Rethink][post-spectre-rethink], and in Artur Janc's
[Notes on the threat model of cross-origin isolation][coi-threat-model].

[oopif]: https://www.chromium.org/developers/design-documents/oop-iframes
[coi-threat-model]: https://arturjanc.com/coi-threat-model.pdf


### Mitigation TL;DR

1.  **Decide when (not!) to respond to requests** by examining incoming headers,
    paying special attention to the `Origin` header on the one hand, and various
    `Sec-Fetch-` prefixed headers on the other, as described in the article
    [Protect your resources from web attacks with Fetch Metadata][fetch-metadata].

2.  **Restrict attackers' ability to load your data as a subresource** by
    setting a [cross-origin resource policy][corp] (CORP) of `same-origin`
    (opening up to `same-site` or `cross-origin` only when necessary).

3.  **Restrict attackers' ability to frame your data as a document** by opt-ing
    into framing protections via `X-Frame-Options: SAMEORIGIN` or CSP's more
    granular `frame-ancestors` directive (`frame-ancestors 'self'
    https://trusted.embedder`, for example).

4.  **Restrict attackers' ability to obtain a handle to your window** by setting
    a [cross-origin opener policy][coop] (COOP). In the best case, you can
    default to a restrictive `same-origin` value, opening up to
    `same-origin-allow-popups` or `unsafe-none` only if necessary.

5.  **Prevent MIME-type confusion attacks** and increase the robustness of
    passive defenses like [cross-origin read blocking][corb] (CORB) /
    [opaque response blocking][orb] (ORB) by setting correct `Content-Type`
    headers, and globally asserting `X-Content-Type-Options: nosniff`.

[fetch-metadata]: https://web.dev/fetch-metadata/
[corp]: https://resourcepolicy.fyi/
[coop]: https://docs.google.com/document/d/1Ey3MXcLzwR1T7aarkpBXEwP7jKdd2NvQdgYvF8_8scI/edit
[corb]: https://chromium.googlesource.com/chromium/src/+/main/services/network/cross_origin_read_blocking_explainer.md
[orb]: https://github.com/annevk/orb


## Practical Examples

### Subresources

Resources which are intended to be loaded into documents should protect
themselves from being used in unexpected ways. Before walking through strategies
for specific kinds of resources, a few headers seem generally applicable:

1.  Sites should use Fetch Metadata to make good decisions about
    [when to serve resources][fetch-metadata]. In order to ensure that decision
    sticks, servers should explain its decision to the browser by sending a
    `Vary` header containing `Sec-Fetch-Dest, Sec-Fetch-Mode, Sec-Fetch-Site`.
    This ensures that the server has a chance to make different decisions for
    requests which will be *used* differently.

2.  Subresources should opt-out of MIME type sniffing by sending an
    `X-Content-Type-Options` header with a value of `nosniff`. This increases
    the robustness of MIME-based checks like [cross-origin read blocking][corb]
    and [opaque response blocking][orb], and mitigates some well-known risks
    around type confusion for scripts.

3.  Subresources are intended for inclusion in a given context, not as
    independently navigable documents. To mitigate the risk that navigation to a
    subresource causes script execution or opens an origin up to attack in some
    other way, servers can assert the following set of headers which
    collectively make it difficult to meaningfully abuse a subresource via
    navigation:

    *   Use the `Content-Security-Policy` header to assert the `sandbox`
        directive. This ensures that these resources remain inactive if
        navigated to directly as a top-level document. No scripts will execute,
        and the resource will be pushed into an "opaque" origin.

        Note: Some servers deliver `Content-Disposition: attachment;
        filename=file.name` to obtain a similar effect. This was valuable to
        mitigate vulnerabilies in Flash, but the `sandbox` approach seems to
        more straightforwardly address the threats we care about today.

    *   Asserting the [`Cross-Origin-Opener-Policy`][coop] header with a value
        of `same-origin` prevents cross-origin documents from retaining a handle
        to the resource's window if it's opened in a popup.

    *   Sending the `X-Frame-Options` header with a value of `DENY` prevents the
        resource from being framed.

Most subresources, then, should contain the following block of headers, which
you'll see repeated a few times below:    

```http
Content-Security-Policy: sandbox
Cross-Origin-Opener-Policy: same-origin
Vary: Sec-Fetch-Dest, Sec-Fetch-Mode, Sec-Fetch-Site
X-Content-Type-Options: nosniff
X-Frame-Options: DENY
```

With these generic protections in mind, let's sift through a few scenarios to
determine what headers a server would be well-served to assert:

#### Static Subresources

By their nature, static resources contain the same data no matter who requests
them, and therefore cannot contain interesting information that an attacker
couldn't otherwise obtain. There's no risk to making these resources widely
available, and value in allowing embedders to robustly debug, so something like
the following response headers could be appropriate:

```http
Access-Control-Allow-Origin: *
Cross-Origin-Resource-Policy: cross-origin
Timing-Allow-Origin: *
Content-Security-Policy: sandbox
Cross-Origin-Opener-Policy: same-origin
Vary: Sec-Fetch-Dest, Sec-Fetch-Mode, Sec-Fetch-Site
X-Content-Type-Options: nosniff
X-Frame-Options: DENY
```

CDNs are the canonical static resource distribution points, and many use the
pattern above. Take a look at the following common resources' response headers
for inspiration:

* <a href="https://stackpath.bootstrapcdn.com/bootstrap/4.5.2/js/bootstrap.min.js">`https://stackpath.bootstrapcdn.com/bootstrap/4.5.2/js/bootstrap.min.js`</a>
* <a href="https://cdnjs.cloudflare.com/ajax/libs/jquery/3.5.1/jquery.min.js">`https://cdnjs.cloudflare.com/ajax/libs/jquery/3.5.1/jquery.min.js`</a>
* <a href="https://cdn.jsdelivr.net/npm/jquery@3.5.1/dist/jquery.min.js">`https://cdn.jsdelivr.net/npm/jquery@3.5.1/dist/jquery.min.js`</a>
* <a href="https://ssl.google-analytics.com/ga.js">`https://ssl.google-analytics.com/ga.js`</a>

Similarly, application-specific static resource servers are a good place to look
for this practice. Consider:

* <a href="https://static.xx.fbcdn.net/rsrc.php/v3/y2/r/zVvRrO8pOtu.png">`https://static.xx.fbcdn.net/rsrc.php/v3/y2/r/zVvRrO8pOtu.png`</a>
* <a href="https://www.gstatic.com/images/branding/googlelogo/svg/googlelogo_clr_74x24px.svg">`https://www.gstatic.com/images/branding/googlelogo/svg/googlelogo_clr_74x24px.svg`</a>

#### Dynamic Subresources

Subresources that contain data personalized to a given user are juicy targets
for attackers, and must be defended by ensuring that they're loaded only in
ways that are appropriate for the data in question. A few cases are well worth
considering:

1.  Application-internal resources (private API endpoints, avatar images,
    uploaded data, etc.) should not be available to any cross-origin requestor.
    These resources should be restricted to usage as a subresource in
    same-origin contexts by sending a [`Cross-Origin-Resource-Policy`][corp]
    header with a value of `same-origin`:
    
    ```http
    Cross-Origin-Resource-Policy: same-origin
    Content-Security-Policy: sandbox
    Cross-Origin-Opener-Policy: same-origin
    Vary: Sec-Fetch-Dest, Sec-Fetch-Mode, Sec-Fetch-Site
    X-Content-Type-Options: nosniff
    X-Frame-Options: DENY
    ```

    This header will prevent cross-origin attackers from loading the resource as
    a response to a `no-cors` request.

    For example, examine the headers returned when requesting endpoints like the
    following:
    
    *   <a href="https://myaccount.google.com/_/AccountSettingsUi/browserinfo">`https://myaccount.google.com/_/AccountSettingsUi/browserinfo`</a>
    *   <a href="https://twitter.com/i/api/1.1/branch/init.json">`https://twitter.com/i/api/1.1/branch/init.json`</a>
    *   <a href="https://www.facebook.com/ajax/webstorage/process_keys/?state=0">`https://www.facebook.com/ajax/webstorage/process_keys/?state=0`</a>

2.  Personalized resources intended for cross-origin use (public API endpoints,
    etc) should carefully consider incoming requests' properties before
    responding. These endpoints can only safely be enabled by requiring CORS,
    and choosing the set of origins for which a given response can be exposed by
    setting the appropriate access-control headers, for example:

    ```http
    Access-Control-Allow-Credentials: true
    Access-Control-Allow-Origin: https://trusted.example
    Access-Control-Allow-Methods: POST
    Access-Control-Allow-Headers: ...
    Access-Control-Allow-...: ...
    Cross-Origin-Resource-Policy: same-origin
    Content-Security-Policy: sandbox
    Cross-Origin-Opener-Policy: same-origin
    Vary: Sec-Fetch-Dest, Sec-Fetch-Mode, Sec-Fetch-Site
    X-Content-Type-Options: nosniff
    X-Frame-Options: DENY
    ```

    Note: The `Cross-Origin-Resource-Policy` header is only processed for
    requests that are _not_ using CORS for access control ("`no-cors`
    requests"). Sending `Cross-Origin-Resource-Policy: same-origin` is
    therefore not harmful, and works to ensure that `no-cors` usage isn't
    accidentally allowed.
    
    For example, examine the headers returned when requesting endpoints like
    the following:
    
    *   <a href="https://api.twitter.com/1.1/jot/client_event.json">`https://api.twitter.com/1.1/jot/client_event.json`</a>
    *   <a href="https://play.google.com/log?format=json&hasfast=true">`https://play.google.com/log?format=json&hasfast=true`</a>
    *   <a href="https://securepubads.g.doubleclick.net/pcs/view">`https://securepubads.g.doubleclick.net/pcs/view`</a>
    *   <a href="https://c.amazon-adsystem.com/e/dtb/bid">`https://c.amazon-adsystem.com/e/dtb/bid`</a>

3.  Personalized resources that are intended for cross-origin `no-cors`
    embedding, but which don't intend to be directly legible in that context
    (avatar images, authenticated media, etc). These should enable cross-origin
    embedding via `Cross-Origin-Resource-Policy`, but _not_ via CORS access
    control headers:

    ```http
    Cross-Origin-Resource-Policy: cross-origin
    Content-Security-Policy: sandbox
    Cross-Origin-Opener-Policy: same-origin
    Vary: Sec-Fetch-Dest, Sec-Fetch-Mode, Sec-Fetch-Site
    X-Content-Type-Options: nosniff
    X-Frame-Options: DENY
    ```

    Note, that this allows the resource to be used by any cross-origin document.
    That's reasonable for some use cases, but requiring CORS, and opting-in a
    small set of origins via appropriate access-control headers is a possible
    alternative for some resources. This approach will give those contexts
    trivial access to the resource's bits, so the granularity is a tradeoff.
    Still, considering this case to be the same as the "personalized resources
    intended for cross-origin use" isn't unreasonable.

    If we implemented more granular bindings for CORP headers (along the lines
    of `Cross-Origin-Resource-Policy: https://trusted.example`), we could avoid
    this tradeoff entirely. That's proposed in
    [whatwg/fetch#760](https://github.com/whatwg/fetch/issues/670).


    For example:

    *   <a href="https://lh3.google.com/u/0/d/1JBUaX1xSOZRxBk5bRNZWgnzyJoCQC52TIRokACBSmGc=w512">`https://lh3.google.com/u/0/d/1JBUaX1xSOZRxBk5bRNZWgnzyJoCQC52TIRokACBSmGc=w512`</a>


### Documents {#documents}

#### Fully-Isolated Documents

Documents that require users to be signed-in almost certainly contain
information that shouldn't be revealed to attackers. These pages should take
care to isolate themselves from other origins, both by making _a priori_
decisions about [whether to serve the page at all][fetch-metadata], and by
giving clients careful instructions about how the page can be used once
delivered. For instance, something like the following set of response headers
could be appropriate:

```http
Cross-Origin-Opener-Policy: same-origin
Cross-Origin-Resource-Policy: same-origin
Vary: Sec-Fetch-Dest, Sec-Fetch-Mode, Sec-Fetch-Site
X-Content-Type-Options: nosniff
X-Frame-Options: SAMEORIGIN
```

Note: Documents which need to make use of APIs that require full cross-origin
isolation (such as `SharedArrayBuffer`), will also need to serve a
[`Cross-Origin-Embedder-Policy`][coep] header, as outlined in
[Making your website "cross-origin isolated" using COOP and COEP][coop-coep].

Account settings pages, admin panels, and application-specific documents are all
good examples of resources which would benefit from as much isolation as
possible. For real-life examples, consider:

*   <a href="https://myaccount.google.com/">`https://myaccount.google.com/`</a>

[coep]: https://wicg.github.io/cross-origin-embedder-policy/
[coop-coep]: https://web.dev/coop-coep/


#### Documents Expecting to Open Cross-Origin Windows

Not every document that requires sign-in can be fully-isolated from the rest of
the internet. It's often the case that partial isolation is a better fit.
Consider sites that depend upon cross-origin windows for federated workflows
involving payments or sign-in, for example. These pages would generally benefit
from restricting attackers' ability to embed them, or obtain their window
handle, but they can't easily lock themselves off from all such vectors via
`Cross-Origin-Opener-Policy: same-origin` and `X-Frame-Options: DENY`. In these
cases, something like the following set of response headers might be
appropriate:

```http
Cross-Origin-Opener-Policy: same-origin-allow-popups
Cross-Origin-Resource-Policy: same-origin
Vary: Sec-Fetch-Dest, Sec-Fetch-Mode, Sec-Fetch-Site
X-Content-Type-Options: nosniff
X-Frame-Options: SAMEORIGIN
```

The only difference between this case and the "Fully-Isolated" case above is the
`Cross-Origin-Opener-Policy` value. `same-origin` will break the opener
relationship between the document and any cross-origin window, regardless of who
opened whom. `same-origin-allow-popups` will break cross-origin opener
relationships initiated by a cross-origin document's use of `window.open()`, but
will allow the asserting document to open cross-origin windows that retain an
opener relationship.


#### Documents Expecting Cross-Origin Openers

Federated sign-in forms and payment providers are clear examples of documents
which intend to be opened by cross-origin windows, and require that relationship
to be maintained in order to facilitate communication via channels like
`postMessage()` or navigation. These documents cannot isolate themselves
completely, but can prevent themselves from being embedded or fetched
cross-origin. Three scenarios are worth considering:

1.  Documents that only wish to be opened in cross-origin popups could loosen
    their cross-origin opener policy by serving the following headers:
    
    ```http
    Cross-Origin-Resource-Policy: same-origin
    Cross-Origin-Opener-Policy: unsafe-none
    Vary: Sec-Fetch-Dest, Sec-Fetch-Mode, Sec-Fetch-Site
    X-Content-Type-Options: nosniff
    X-Frame-Options: SAMEORIGIN
    ```

    For example:

    *   > TODO: Find some links.

2.  Documents that only wish to be framed in cross-origin contexts could loosen
    their framing protections by serving the following headers:

    ```http
    Cross-Origin-Resource-Policy: same-origin
    Cross-Origin-Opener-Policy: same-origin
    Vary: Sec-Fetch-Dest, Sec-Fetch-Mode, Sec-Fetch-Site
    X-Content-Type-Options: nosniff
    X-Frame-Options: ALLOWALL
    ```

    
    Note that this allows embedding by any cross-origin documents. That's
    reasonable for some widgety use cases, but when possible, a more secure
    alternative would specify a list of origins which are allowed to embed the
    document via the `frame-ancestors` CSP directive. That is, in addition to
    the `X-Frame-Options` header above, the following header could also be
    included to restrict the document to a short list of trusted embedders:

    ```http
    Content-Security-Policy: frame-ancestors https://trusted1.example https://trusted2.example
    ```

    For example:

    *   > TODO: Find some links.

3.  Documents that support both popup and framing scenarios need to loosen both
    their cross-origin opener policies and framing protections by combining the
    recommendations above, serving the following headers:

    ```http
    Cross-Origin-Resource-Policy: same-origin
    Cross-Origin-Opener-Policy: unsafe-none
    Vary: Sec-Fetch-Dest, Sec-Fetch-Mode, Sec-Fetch-Site
    X-Content-Type-Options: nosniff
    X-Frame-Options: ALLOWALL
    ```

    For example:

    *   > TODO: Find some links.

## Implementation Considerations

### Explicitly Setting Headers with Default Values

Several recommendations above suggest that developers would be well-served to
set headers like `X-Frame-Options: ALLOWALL` or `Cross-Origin-Opener-Policy:
unsafe-none` on responses. These map to the web's status quo behavior, and seem
therefore superfluous. Why should developers set them?

The core reason is that these defaults are poor fits for today's threats, and we
ought to be working to change them. Proposals like [Embedding Requires Opt-In][embedding]
and [COOP By Default][coop-by-default] suggest that we should shift the web's
defaults away from requiring developers to opt-into more secure behaviors by
making them opt-out rather than opt-in. This would place the configuration cost
on those developers whose projects require risky settings.

This document recommends setting those less-secure header values explicitly, as
that makes it more likely that we'll be able to shift the web's defaults in the
future.

[embedding]: https://github.com/mikewest/embedding-requires-opt-in
[coop-by-default]: https://github.com/mikewest/coop-by-default

## Acknowledgements

This document relies upon a number of excellent resources that spell out much of
the foundation of our understanding of Spectre's implications for the web, and
justify the mitigation strategies we currently espouse. The following is an
incomplete list of those works, in no particular order:

*   Chris Palmer's
    [Isolating Application-Defined Principles](https://noncombatant.org/application-principals/)

*   Charlie Reis'
    [Long-Term Web Browser Mitigations for Spectre](https://docs.google.com/document/d/1dnUjxfGWnvhQEIyCZb0F2LmCZ9gio6ogu2rhMGqi6gY/edit)

*   Anne van Kesteren's
    [A Spectre-Shaped Web](https://docs.google.com/presentation/d/1sadl7jTrBIECCanuqSrNndnDr82NGW1yyuXFT1Dc7SQ/edit#slide=id.p),
    [Safely Reviving Shared Memory](https://hacks.mozilla.org/2020/07/safely-reviving-shared-memory/),
    and [Opaque Resource Blocking](https://github.com/annevk/orb).

*   Artur Janc and Mike West's [How do we stop spilling the beans across origins?](https://www.arturjanc.com/cross-origin-infoleaks.pdf)

*   Mike West's [Cross-Origin Embedder Policy explainer](https://wicg.github.io/cross-origin-embedder-policy/)

*   Charlie Reis and Camille Lamy's [Cross-Origin Opener Policy explainer](https://docs.google.com/document/d/1Ey3MXcLzwR1T7aarkpBXEwP7jKdd2NvQdgYvF8_8scI/edit)

*   Artur Janc, Charlie Reis, and Anne van Kesteren's [COOP and COEP Explained](https://docs.google.com/document/d/1Ey3MXcLzwR1T7aarkpBXEwP7jKdd2NvQdgYvF8_8scI/edit)

*   Artur Janc's [Notes on the threat model of cross-origin isolation](https://arturjanc.com/coi-threat-model.pdf)
