## **DIPS (Detect/Delete Incidental Party State)**

This directory contains the code for Chromium's DIPS (Detect/Delete Incidental Party State) feature, known externally as Bounce Tracking Mitigations (BTM). DIPS aims to mitigate the privacy impact of "bounce tracking", a technique used to track users across websites without relying on third-party cookies.

**What is bounce tracking?**

Bounce tracking involves redirecting users through a tracker website, often without their knowledge or interaction. This allows the tracker to set or access first-party cookies, effectively circumventing third-party cookie restrictions and user privacy preferences.

**How does DIPS work?**

DIPS detects potential bounce tracking by analyzing website behavior, such as:

- Short dwell times on a website before redirecting.
- Programmatic redirects (as opposed to user-initiated ones).
- Writing to storage (cookies, etc.) before redirecting.

If DIPS determines that a website is likely involved in bounce tracking, and there's no indication of legitimate user interaction with the site, it automatically deletes the site's storage (eTLD+1).

**Goals of DIPS:**

- **Reduce cross-site tracking:*- Limit the ability of bounce trackers to identify and track users across different contexts.
- **Protect user privacy:*- Prevent bounce tracking from circumventing third-party cookie restrictions.
- **Maintain compatibility:*- Avoid disrupting legitimate use cases like federated logins and payment flows that rely on redirects.
- **Adaptability:*- Mitigate tracking by short-lived domains that may evade traditional blocklist-based approaches.

**Non-Goals:**

- **Replacing third-party cookie blocking:*- DIPS is primarily designed for environments where third-party cookies are already restricted.
- **Mitigating tracking by sites with significant first-party activity:*- DIPS focuses on incidental parties (sites without meaningful user interaction) and may not be effective against sites with substantial first-party engagement.

**Further Reading:**

- Explainer: https://github.com/privacycg/nav-tracking-mitigations/blob/main/bounce-tracking-explainer.md
