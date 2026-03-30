# Permissions Policy Security Framework

This document outlines the security framework for Permissions Policies.
It is intended to help security researchers and AI agents understand the context around Permissions Policy security and evaluate whether a finding constitutes an actionable vulnerability.

## Goals

Permissions Policies are intended to:

* Provide embedders with control over feature availability in embedded contexts.
* Set good defaults for the availability of new features while maintaining backwards compatibility for existing ones.

## Concerns

Permissions Policies should prohibit the following:

* Exposure of cross-origin behavior - A violation of the policy by an embedded context should not be observable by the embedder.
* Exposure of embedding policy - While an embedded page may infer information on the embedder based on the enforced policy, an embedder should not know anything about the policy of an embedded page beyond the limits the embedder itself set.

## Known Limitations

Permissions Policy implementation faces these current setbacks:

* [Renderer process trust](https://crbug.com/40126948) - The current policy parsing architecture depends on the renderer process to parse the final policy due to legacy design considerations (though this final policy is limited by restrictions imposed by parent-frame renderers).
Ideally this parsing would be moved back into the browser (or even network) process, but is not considered a vulnerability as sensitive information (e.g., location) have additional gating in the browser process via permission prompt UX.
* [Shared/Service worker policy support](https://crbug.com/497494112) - The current shared/service worker architecture does not support inheriting the permissions policy of the parent context at creation time as there is no definitive 'owning' context to inherit from.
Ideally some sort of limitation would be enforced (even for Shared Workers despite the lack of a clear owning context), but this is not considered a vulnerability as sensitive information (e.g., location) must be fetched from a context with user-facing rendering and gated via permission prompt UX.

## Reporting Vulnerabilities

To report a security issue, please follow the [Chromium security bug reporting guidelines](https://www.chromium.org/Home/chromium-security/reporting-security-bugs/).
When filing your report, please tag the [Permissions Policy component](https://issues.chromium.org/issues?q=status:open%20componentid:1456181&s=created_time:desc).

We recognize issues as vulnerabilities only when they violate general security principles or the goals/concerns outlined above, and are not listed in the known limitations.
