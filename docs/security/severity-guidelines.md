# Severity Guidelines for Security Issues

[TOC]

Vendors shipping products based on Chromium might wish to rate the severity of
security issues in the products they release. This document contains guidelines
for how to rate these issues. Check out our
[security release management page](https://www.chromium.org/Home/chromium-security/security-release-management)
for guidance on how to release fixes based on severity.

Any significant mitigating factors, such as unusual or additional user
interaction, or running Chrome with a specific command line flag or non-default
feature enabled, may reduce an issue’s severity by one or more levels. Also note
that most crashes do not indicate vulnerabilities. Chromium is designed to crash
in a controlled manner (e.g., with a ```__debugBreak```) when memory is
exhausted or in other exceptional circumstances.


## Critical severity {#TOC-Critical-severity}

Critical severity issues allow an attacker to read or write arbitrary resources
(including but not limited to the file system, registry, network, et c.) on the
underlying platform, with the user's full privileges.

They are normally assigned priority **Pri-0** and assigned to the current stable
milestone (or earliest milestone affected). For critical severity bugs,
[SheriffBot](https://www.chromium.org/issue-tracking/autotriage) will
automatically assign the milestone.

**For critical severity vulnerabilities, we aim to deploy the patch to all
Chrome users in under 30 days.**

Critical vulnerability details may be made public in 60 days,
in accordance with Google's general [vulnerability disclosure recommendations](https://security.googleblog.com/2010/07/rebooting-responsible-disclosure-focus.html),
or [faster (7 days)](https://security.googleblog.com/2013/05/disclosure-timeline-for-vulnerabilities.html)
if there is evidence of active exploitation.

Example bugs:

* Memory corruption in the browser process ([319125](https://crbug.com/319125#c10)).
* Exploit chains made up of multiple bugs that can lead to code execution
  outside of the sandbox ([416449](https://crbug.com/416449)).
* A bug that enables web content to read local files
  ([962500](https://crbug.com/962500)).

Note that the individual bugs that make up the chain will have lower severity
ratings.


## High severity {#TOC-High-severity}

High severity vulnerabilities allow an attacker to execute code in the context
of, or otherwise impersonate other origins or read cross-origin data.
Bugs which would normally be
critical severity with unusual mitigating factors may be rated as high severity.
For example, renderer sandbox escapes fall into this category as their impact is
that of a critical severity bug, but they require the precondition of a
compromised renderer.

They are normally assigned priority **Pri-1** and assigned to the current stable
milestone (or earliest milestone affected). For high severity bugs,
[SheriffBot](https://www.chromium.org/issue-tracking/autotriage) will
automatically assign the milestone.

**For high severity vulnerabilities, we aim to deploy the patch to all Chrome
users in under 60 days.**

Example bugs:

* A bug that allows full circumvention of the same origin policy. Universal XSS
bugs fall into this category, as they allow script execution in the context of
an arbitrary origin ([534923](https://crbug.com/534923)).
* A bug that allows arbitrary code execution within the confines of the sandbox,
such as renderer or GPU process memory corruption
([570427](https://crbug.com/570427), [468936](https://crbug.com/468936)).
* Complete control over the apparent origin in the omnibox
([76666](https://crbug.com/76666)).
* Memory corruption in the browser process that can only be triggered from a
compromised renderer, leading to a sandbox escape
([469152](https://crbug.com/469152)).
* Kernel memory corruption that could be used as a sandbox escape from a
compromised renderer ([377392](https://crbug.com/377392)).
* Memory corruption in the browser process that requires specific user
interaction, such as granting a permission ([455735](https://crbug.com/455735)).
* Site Isolation bypasses:
    - Cross-site execution contexts unexpectedly sharing a renderer process
      ([863069](https://crbug.com/863069), [886976](https://crbug.com/886976)).
    - Cross-site data disclosure
      ([917668](https://crbug.com/917668), [927849](https://crbug.com/927849)).


## Medium severity {#TOC-Medium-severity}

Medium severity bugs allow attackers to read or modify limited amounts of
information, or are not harmful on their own but potentially harmful when
combined with other bugs. This includes information leaks that could be useful
in potential memory corruption exploits, or exposure of sensitive user
information that an attacker can exfiltrate. Bugs that would normally be rated
at a higher severity level with unusual mitigating factors may be rated as
medium severity.

They are normally assigned priority **Pri-1** and assigned to the current stable
milestone (or earliest milestone affected). If the fix seems too complicated to
merge to the current stable milestone, they may be assigned to the next stable
milestone.

Example bugs:

* An out-of-bounds read in a renderer process
([281480](https://crbug.com/281480)).
* An uninitialized memory read in the browser process where the values are
passed to a compromised renderer via IPC ([469151](https://crbug.com/469151)).
* Memory corruption that requires a specific extension to be installed
([313743](https://crbug.com/313743)).
* An HSTS bypass ([461481](https://crbug.com/461481)).
* A bypass of the same origin policy for pages that meet several preconditions
([419383](https://crbug.com/419383)).
* A bug that allows web content to tamper with trusted browser UI
([550047](https://crbug.com/550047)).
* A bug that reduces the effectiveness of the sandbox
([338538](https://crbug.com/338538)).
* A bug that allows arbitrary pages to bypass security interstitials
([540949](https://crbug.com/540949)).
* A bug that allows an attacker to reliably read or infer browsing history
([381808](https://crbug.com/381808)).
* An address bar spoof where only certain URLs can be displayed, or with other
mitigating factors ([265221](https://crbug.com/265221)).
* Memory corruption in a renderer process that requires specific user
interaction, such as dragging an object ([303772](https://crbug.com/303772)).


## Low severity {#TOC-Low-severity}

Low severity vulnerabilities are usually bugs that would normally be a higher
severity, but which have extreme mitigating factors or highly limited scope.

They are normally assigned priority **Pri-2**. Milestones can be assigned to low
severity bugs on a case-by-case basis, but they are not normally merged to
stable or beta branches.

Example bugs:

* Bypass requirement for a user gesture ([256057](https://crbug.com/256057)).
* Partial CSP bypass ([534570](https://crbug.com/534570)).
* A limited extension permission bypass ([169632](https://crbug.com/169632)).
* An uncontrolled single-byte out-of-bounds read
([128163](https://crbug.com/128163)).

The [security FAQ](faq.md) covers many of the cases that we do not consider to
be security bugs, such as [denial of service](faq.md#TOC-Are-denial-of-service-issues-considered-security-bugs-).
