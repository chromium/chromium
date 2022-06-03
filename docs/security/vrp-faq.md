# Chrome Vulnerability Reward Program FAQ

[TOC]

## What are the differences between the vulnerability [categories](https://www.google.com/about/appsecurity/chrome-rewards/index.html#rewards) in the Chrome VRP?

We have several different classifications for security vulnerabilities that are
reported to us. More information about each category can be found below:

 * **Sandbox escape / Memory corruption in a non-sandboxed process**: a bug that
   allows malicious code to execute in a non-sandboxed process (like the browser
   process), or to circumvent the protections of the sandbox. (ex:
   https://crbug.com/1025067)
 * **Universal Cross Site Scripting (includes Site Isolation bypass)**: a flaw
   allowing an attacker to execute script in the context of any other origin,
   similar to how Cross Site Scripting can be leveraged against insecure
   websites. (ex: https://crbug.com/997190)
 * **Renderer RCE / memory corruption in a sandboxed process**: a bug that
   allows malicious code to be executed inside a renderer or other sandboxed
   process.  (ex: https://crbug.com/990897)
 * **Security UI Spoofing**: a situation in which an attacker gains an
   illegitimate advantage on a user interface surface. In Chrome this includes
   spoofing the displayed URL or creating fake permission prompts outside of the
   frame containing the site. (ex: https://crbug.com/1017564)
 * **User information disclosure**: unauthorized access to information that
   should be inaccessible to an attacker. (ex: https://crbug.com/989078)
 * **Web Platform Privilege Escalation**: a bug that allows a site to obtain a
   permission or capability that was not granted by a user, such as escaping an
   iframe sandbox or bypassing cross-origin checks.
 * **Exploitation Mitigation Bypass**: a bug which makes exploitation easier,
   such as an out of bounds read in a sandboxed process, or which bypasses
   security checks in Chrome. (ex:  https://crbug.com/1021457,
   https://crbug.com/979441)

User information disclosure, web platform privilege escalation and exploitation
mitigation bypasses exist on a continuum based on how harmful they are to users.

## What about rewards for Site Isolation?

Site Isolation vulnerabilities are no longer receiving special rewards and will
be categorized and rewarded as Universal Cross-site Scripting vulnerabilities.

[Site Isolation](https://www.chromium.org/Home/chromium-security/site-isolation)
makes it possible for sites (i.e., combination of scheme and eTLD+1) to run in
dedicated renderer processes. This can mitigate [speculative side channel
attacks](https://www.chromium.org/Home/chromium-security/ssca) as well as
attacks from compromised renderer processes.  Site Isolation is enabled for all
sites on desktop platforms. On Android, Site Isolation is enabled for sites
where users enter passwords, but it does not yet mitigate compromised renderers.

In scope:

 * Bugs that cause two or more cross-site documents from the web to commit in
   the same process. i.e. force pre-Site Isolation behaviour.
 * Bugs that cause cross-site data disclosure, even if the bug assumes a
   compromised renderer. Examples of data protected by Site Isolation: cookies,
   saved passwords, localStorage, IndexedDB, HTTP resources covered by
   [CORB](https://www.chromium.org/Home/chromium-security/corb-for-developers)
   or
   [CORP](https://developer.mozilla.org/en-US/docs/Web/HTTP/Cross-Origin_Resource_Policy_(CORP)).

Out of scope and known issues:

 * Site Isolation on Android is not enabled for all sites or devices. Reports
   should work when Site Isolation is enabled for the victim site (e.g., when
   the victim site is specified in `chrome://flags/#isolate-origins`).
 * Compromised renderers are currently out of scope for Site Isolation on
   Android reports.
 * Sandboxed frames and data: URLs are currently treated as the same site as
   their creator.
 * CORB is not enforced for the Flash plugin, which is disabled by default and
   will be removed. CORB is also not enforced for a small set of [allowlisted
   extensions](https://www.chromium.org/Home/chromium-security/extension-content-script-fetches),
   until these extensions have a chance to update to the new security model.
 * Compromised renderers can still spoof other sites (e.g., spoof Origin headers
   or Sec-Fetch-Site headers).
 * Timing attacks and cross-site-search attacks are out of scope and may need to
   be mitigated by robust server-side CSRF protection.
 * Problems in websites (e.g. missing CORB protection because of incorrect
   Content-Type header) or
   [extensions](https://groups.google.com/a/chromium.org/d/topic/chromium-extensions/0ei-UCHNm34/discussion)
   (e.g., privilege escalation via messages from a compromised content script)
   are out of scope of the Chrome VRP, but may be covered by a separate
   website-specific or extension-specific VRP.

Examples of in-scope Site Isolation issues:

 * Unexpected process sharing: https://crbug.com/863069
 * Cross-Origin Read Blocking (CORB) bypass: https://crbug.com/927849
 * Disclosing IndexedDB data to a cross-site renderer process:
   https://crbug.com/917668
