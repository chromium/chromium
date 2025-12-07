# Bounce Tracking Mitigations (BTM)

This directory contains the code for Chromium's Bounce Tracking Mitigation (BTM)
feature.
BTM aims to mitigate the privacy impact of "bounce tracking," a technique used
to track users across websites without relying on third-party cookies (3PCs).

For historical reasons, some of the code in this directory is also used to
support mitigations against using popups to simulate 3PCs.

## What is bounce tracking?

Bounce tracking involves redirecting users through a tracker website, often
without their knowledge or interaction.
This allows the tracker to set or access first-party cookies, effectively
circumventing third-party cookie restrictions and user privacy preferences.

## How does BTM work?

BTM detects potential bounce tracking by analyzing website behavior, including:

- Short dwell times on a website before redirecting.
- Programmatic redirects (as opposed to user-initiated ones).
- Writing to storage (cookies, etc.) before redirecting.

If BTM determines that a website is likely involved in bounce tracking and
there's no indication of legitimate user interaction with the site, it
automatically deletes the site's storage (eTLD+1) after a brief grace period.

### Goals of BTM

- **Reduce cross-site tracking:** Limit the ability of bounce trackers to
  identify and track users across different contexts.
- **Protect user privacy:** Prevent bounce tracking from circumventing
  third-party cookie restrictions.
- **Maintain compatibility:** Avoid disrupting legitimate use cases like
  federated logins and payment flows that rely on redirects.
- **Adaptability:** Mitigate tracking by short-lived domains that may evade
  traditional blocklist-based approaches.

### Non-Goals

- **Replacing third-party cookie blocking:** BTM is primarily designed for
  environments where third-party cookies are already restricted.
- **Mitigating tracking by sites with significant first-party activity:** BTM
  focuses on incidental parties (sites without meaningful user interaction) and
  may not be effective against sites with substantial first-party engagement.

## Further Reading

- BTM Spec: https://privacycg.github.io/nav-tracking-mitigations/#bounce-tracking-mitigations
- BTM Explainer: https://github.com/privacycg/nav-tracking-mitigations/blob/main/bounce-tracking-explainer.md
- Chrome Status Listings:
  - Original BTM Feature: https://chromestatus.com/feature/5705149616488448
  - Stateless Bounce Extension: https://chromestatus.com/feature/6299570819301376
