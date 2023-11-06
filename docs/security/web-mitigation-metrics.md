# Web Mitigation Metrics

The web platform offers a number of tools to web developers which enable
mitigation of a few important threats. In order to understand how these are
being used in the wild, and evaluate our success at promulgating them, we
collect some usage statistics; this document outlines those counters and
points to some helpful graphs.

## Content Security Policy

We believe that a carefully-crafted [Content Security Policy][csp] can help
protect web applications from injection attacks that would otherwise lead to
script execution. [Strict CSP][strict-csp] is a reasonable approach, one which
we'd like to encourage.

[csp]: https://w3c.github.io/webappsec-csp/
[strict-csp]: https://csp.withgoogle.com/docs/strict-csp.html

In order to understand CSP's use in the wild, we can look at a few counters that
give some insight into the percentage of Chrome users' page views that use CSP
in a given way:

*   `kContentSecurityPolicy`
    ([graph](https://chromestatus.com/metrics/feature/timeline/popularity/15))
    tracks the overall usage of `Content-Security-Policy` headers. Likewise,
    `kContentSecurityPolicyReportOnly`
    ([graph](https://chromestatus.com/metrics/feature/timeline/popularity/16))
    tracks the report-only variant.

To get a feel for the general quality of policies in the wild, we want to
evaluate how closely developers are hewing to the strictures of Strict CSP.
We've boiled that down to three categories:

*   Does the policy reasonably restrict [`object-src`][object-src]? The only
    "reasonable" restriction, unfortunately, is `object-src 'none'`.
    `kCSPWithReasonableObjectRestrictions` and
    `kCSPROWithReasonableObjectRestrictions` track that directive value in
    enforced and report-only modes respectively.

*   Does the policy reasonably restrict `base-uri` in order to avoid malicious
    redirection of relative URLs? `base-uri 'none'` and `base-uri 'self'` are
    both appropriate, and are tracked via `kCSPWithReasonableBaseRestrictions`
    and `kCSPROWithReasonableBaseRestrictions` in enforced and report-only modes
    respectively.

*   Does the policy avoid using a list of hosts or schemes (which [research has
    shown to be mostly ineffective at mitigating attack][csp-is-dead])?
    `kCSPWithReasonableScriptRestrictions` and
    `kCSPROWithReasonableScriptRestrictions` track the policies whose
    [`script-src`][script-src] directives rely upon cryptographic nonces and/or
    hashes rather than lists of trusted servers, and which also avoid relying
    upon `'unsafe-inline'`.

Policies that sufficiently restrict all of the directives above (`object-src`,
`base-uri`, and `script-src`) are tracked via `kCSPWithReasonableRestrictions`
and `kCSPROWithReasonableRestrictions`. This is the baseline we'd like pages
generally to meet, and a number we hope we can drive up over time.

We're also tracking a higher bar, which includes all the restrictions above,
but also avoids relying upon `'strict-dynamic'`, via
`kCSPWithBetterThanReasonableRestrictions` and
`kCSPROWithBetterThanReasonableRestrictions`.

[object-src]: https://w3c.github.io/webappsec-csp/#directive-object-src
[base-uri]: https://w3c.github.io/webappsec-csp/#directive-base-uri
[script-src]: https://w3c.github.io/webappsec-csp/#directive-script-src
[csp-is-dead]: https://research.google/pubs/pub45542/

#### Embedded Enforcement

`kIFrameCSPAttribute` records the overall usage of the `csp` attribute on
`<iframe>` elements, which enables pages to enforce a policy on documents
they embed.

## Trusted Types

[Trusted Types][tt] gives page authors a means to protect their sites against
cross-site scripting attacks. In order to understand real-world Trusted Types
usage we obtain the following usage counts:

* General use:`kTrustedTypesEnabled`, `kTrustedTypesEnabledEnforcing`, and
  `kTrustedTypesEnabledReportOnly`. The first tells us (relative to all page
  loads) how many pages have any form of Trusted Types enabled, while the other
  two allow us to determine which percentage of pages run in enforcing or
  report-only mode (or both).

* Tracking specific features: `kTrustedTypesPolicyCreated` tracks
  creation of all Trusted Types policies, `kTrustedTypesDefaultPolicyCreated`
  notes whether a "default" policy has been created. `kTrustedTypesAllowDuplicates`
  records whether an 'allow-duplicates' keyword has been used.

* Error tracking: `kTrustedTypesAssignmentError` tracks whether Trusted Types
  has blocked a string assignment.

[tt]: https://github.com/w3c/webappsec-trusted-types/

## Cross Origin Isolation policies

Cross Origin Isolation policies refer to a number of header based policies that
developers can send to enforce specific rules about how their content can be
embedded, opened from, etc. It is also used to gate certain APIs that would be
otherwise too powerful to use in a post-Spectre world.

[Cross-Origin-Resource-Policy][corp] restricts a resource to only be fetched by
"same-origin" or "same-site" pages.

* "success": The CORP check passes successfully.
* "same-origin violation": "same-origin" is specified on a cross-origin
  response.
* "same-origin violation with COEP involvement": No CORP header
  is specified but that is treated as "same-origin" because the initiator
  context enables Cross-Origin Embedder Policy (see below), and the response
  comes from cross-origin.
* "same-site violation": "same-site" is specified on a cross-site response.

[Cross-Origin-Opener-Policy][coop] is used to restrict the usage of window
openers. Pages can choose to restrict this relation to same-origin pages with
similar COOP value, same-origin unless they are opening popups or put no
restriction by default.

* Usage of COOP is tracked via:
  - `kCrossOriginOpenerPolicySameOrigin`
  - `kCrossOriginOpenerPolicySameOriginAllowPopups`
  * `kCoopAndCoepIsolated`
They correspond respectively to the values: "same-origin",
"same-origin-allow-popups" and "same-origin" used conjointly with COEP.

* Usage of COOP in report-only mode is tracked symmetrically via:
  - `kCrossOriginOpenerPolicySameOriginReportOnly`
  - `kCrossOriginOpenerPolicySameOriginAllowPopupsReportOnly`
  * `kCoopAndCoepIsolatedReportOnly`

* We track how often same-origin documents are present in two pages with
  different COOP values via `kSameOriginDocumentsWithDifferentCOOPStatus`. We
  might restrict synchronous access between those in order to allow COOP
  "same-origin-allow-popups" to enable crossOriginIsolated when used in
  conjunction with COEP.

[Cross-Origin-Embedder-Policy][coep] is used to restrict the embedding of
subresources to only those that have explicitly opted in via
[Cross-Origin-Resource-Policy].

* Usage of COEP is tracked via:
  - `kCrossOriginEmbedderPolicyCredentialless`
  - `kCrossOriginEmbedderPolicyRequireCorp`.

* Usage of COEP in report-only mode is tracked symmetrically via:
  - `kCrossOriginEmbedderPolicyCredentiallessReportOnly`
  - `kCrossOriginEmbedderPolicyRequireCorpReportOnly`.

* Usage of COEP in SharedWorker is tracked via:
  - `kCoepNoneSharedWorker`,
  - `kCoepRequireCorpSharedWorker`
  - `kCoepCredentiallessSharedWorker`.

Note that some APIs having precise timers or memory measurement are enabled only
for pages that set COOP to "same-origin" and COEP to "require-corp".

* We track such pages via `kCoopAndCoepIsolated`.


[corp]: https://developer.mozilla.org/en-US/docs/Web/HTTP/Cross-Origin_Resource_Policy_(CORP)
[coep]: https://wicg.github.io/cross-origin-embedder-policy/
[coop]: https://gist.github.com/annevk/6f2dd8c79c77123f39797f6bdac43f3e

## Sanitizer API

[The Sanitizer API][sanitizer] provides a browser-maintained "ever-green", safe,
and easy-to-use library for user input sanitization as part of the general web
platform.

* Sanitizer creation: `kSanitizerAPICreated` and
  `kSanitizerAPIDefaultConfiguration` tell us how many Sanitizers are
  created and how many Sanitizers are created without custom configurations.
* Sanitizer method: `kSanitizerAPIToFragment`, `kSanitizerAPISanitizeFor`,
  and `kSanitizerAPIElementSetSanitized` measure which API entry point has been
  called.
* `kSanitizerAPIActionTaken` shows how many times a sanitize action has been
  performed while calling the Sanitizer APIs. (That is, on how many sanitizer
  calls did the sanitizer remove nodes from the input sets.)
* Input type: `kSanitizerAPIFromString`, `kSanitizerAPIFromDocument` and
  `kSanitizerAPIFromFragment` tell us what kind of input people are using.

[sanitizer]: https://wicg.github.io/sanitizer-api/

## Private Network Access

[Private Network Access][pna] helps to prevent the user agent from
inadvertently enabling attacks on devices running on a user's local intranet,
or services running on the user's machine directly.

* Use of PNA in workers tracked via:
  - `kPrivateNetworkAccessFetchesWorkerScript`
  - `kPrivateNetworkAccessWithWorker`

* `kPrivateNetworkAccessNullIpAddress` is an experimental use counter for
  accesses to the 0.0.0.0 IP address (and the corresponding `[::]` IPv6 address).
  These can be used to access localhost on MacOS and Linux and bypass Private
  Network Access checks. We intent to block all such requests. See
  https://crbug.com/1300021 and https://github.com/whatwg/fetch/issues/1117.

[pna]: https://wicg.github.io/private-network-access/
