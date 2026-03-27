# Chrome DevTools Protocol (CDP) Security Framework

This document outlines the security goals, boundaries, and threat model for the Chrome DevTools Protocol (CDP) and the DevTools Frontend. It is intended to help security researchers and AI agents understand the context around DevTools security and evaluate whether a finding constitutes an actionable vulnerability.

## DevTools Security Goals

### DevTools Frontend (and its extensions)
* **Opening DevTools should have no side-effects** (except a few well-defined cases, e.g., loading source maps) so that it doesn't leak user data over the network.
* **The DevTools Renderer is considered trusted.** We enforce that it has no direct network access (especially to load images) to protect this boundary. Because the DevTools renderer is inherently trusted, **a report that requires an attacker to already have access to a DevTools renderer is not considered a vulnerability.**

### Chrome DevTools Protocol (CDP)
* Access is explicitly controlled and authorized.
* Access is limited by the scope of attached targets (e.g., related targets like subframes or workers).
* Access is distinguished between CDP domains.
* Access to browser-level features and internal WebUI should be requested explicitly and never granted across a trust boundary (i.e., to a Chrome extension).
* It should **never** be possible to:
  * Run an executable file.
  * Interact with non-web parts of Chrome.
  * Interact with any other software outside of the browser.
  * Invoke the CDP implementation in the browser without a client attached.

## Evaluating Potential Vulnerabilities

When assessing a potential security issue, please consider the following:

1. **Initial Access Requires Trust:** All CDP and DevTools components require a specific, non-trivial user action to be active (e.g., user opens DevTools, installs a Chrome Extension, or runs Chrome with a debugging port or pipe).
2. **Trusted DevTools Renderer:** If a bug allows an attacker who already has access to the DevTools renderer to perform privileged actions, this is an expected behavior of the system, not a security bug.
3. **Data Access Outside Boundaries:** Providing access to data outside the security boundary (e.g., local filesystem paths) is acceptable if we can guarantee that the user *already has access to it*.
4. **Memory Corruption:** Memory corruption in the page renderer via DevTools/CDP is no more severe than memory corruption via regular JavaScript. However, memory corruption in the browser or DevTools renderer itself is a high-priority issue.
5. **Social Engineering vs. Security:** Tricking a user into performing unsafe actions (e.g., pasting malicious code) is classified as a UX or social engineering problem, not a security vulnerability.
6. **AI-produced Output:** External links produced by LLM models in DevTools are not rendered clickable to minimize data exfiltration risk. Non-clickable links do not constitute an increased security risk and are evaluated the same way as social engineering issues.
7. **Detecting DevTools:** We make a best effort approach to prevent websites from detecting whether DevTools is open, but do not prioritize this. There are many subtle ways to detect DevTools, and while in many cases we can find workarounds, we avoid getting into an arms race that provides little user value.

For internal reference, see the full security model document at `go/chrome-devtools-security-model`.

## Reporting Vulnerabilities

To report a security issue, please follow the [Chromium security bug reporting guidelines](https://www.chromium.org/Home/chromium-security/reporting-security-bugs/). When filing your report, please mention that the issue is related to the Chrome DevTools Protocol or DevTools Frontend.

We recognize issues as vulnerabilities only when they violate the security goals and boundaries outlined above. Issues that have a security impact only when an attacker has already bypassed the DevTools renderer trust boundary or relies on social engineering are generally not treated as actionable security vulnerabilities.
