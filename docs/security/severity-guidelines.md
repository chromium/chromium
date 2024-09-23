# Severity Guidelines for Security Issues

[TOC]

Vendors shipping products based on Chromium might wish to rate the severity of
security issues in the products they release. This document contains guidelines
for how to rate these issues. Check out our
[security release management page](https://www.chromium.org/Home/chromium-security/security-release-management)
for guidance on how to release fixes based on severity.

Any significant mitigating factors will generally reduce an issue's severity by one or
more levels:
* Not web accessible, reliant solely on direct UI interaction to trigger.
* Unusual or unlikely user interaction will normally reduce severity by one
  level. This means interaction which may sometimes occur, but would not be
  typical of an average user engaging with Chrome or a particular feature in
  Chrome, nor could a user be easily convinced to perform by a persuasive web page.
* Requiring profile destruction or browser shutdown will normally reduce
  severity by one level.
* [MiraclePtr protection](#TOC-MiraclePtr)

Bugs that require implausible interaction, interactions a user would not
realistically be convinced to perform, will generally be downgraded to a
functional bug and not considered a security bug.

Conversely, we do not consider it a mitigating factor if a vulnerability applies
only to a particular group of users. For instance, a Critical vulnerability is
still considered Critical even if it applies only to Linux or to those users
running with accessibility features enabled.

Also note that most crashes do not indicate vulnerabilities. Chromium is designed
to crash in a controlled manner (e.g., with a ```__debugBreak```) when memory is
exhausted or in other exceptional circumstances.


## Critical severity (S0) {#TOC-Critical-severity}

Critical severity (S0) issues allow an attacker to read or write arbitrary
resources (including but not limited to the file system, registry, network,
etc.) on the underlying platform, with the user's full privileges.

They are normally assigned Priority **P0** and assigned to the current stable
milestone (or earliest milestone affected). For critical severity bugs,
[SheriffBot](https://www.chromium.org/issue-tracking/autotriage) will
automatically assign the milestone.

**For critical severity (S0) vulnerabilities, we aim to deploy the patch to all
Chrome users in under 30 days.**

Critical vulnerability details may be made public in 60 days,
in accordance with Google's general [vulnerability disclosure recommendations](https://security.googleblog.com/2010/07/rebooting-responsible-disclosure-focus.html),
or [faster (7 days)](https://security.googleblog.com/2013/05/disclosure-timeline-for-vulnerabilities.html)
if there is evidence of active exploitation.

Example bugs:

* Memory corruption in the browser process ([319125](https://crbug.com/319125#c10)).
* Memory corruption in an unsandboxed GPU process when it is reachable directly from web
  content without compromising the renderer.
  ([1420130](https://crbug.com/1420130), [1427865](https://crbug.com/1427865)).
  ([on some platforms we consider the GPU process 'sandboxed'](../../docs/security/process-sandboxes-by-platform.md)).
* Exploit chains made up of multiple bugs that can lead to code execution
  outside of the sandbox ([416449](https://crbug.com/416449)).
* A bug that enables web content to read local files
  ([962500](https://crbug.com/962500)).

Note that the individual bugs that make up the chain will have lower severity
ratings.

## High severity (S1) {#TOC-High-severity}

High severity (S1) vulnerabilities allow an attacker to execute code in the context
of, or otherwise impersonate other origins or read cross-origin data.
Bugs which would normally be
critical severity with unusual mitigating factors may be rated as high severity.
For example, renderer sandbox escapes fall into this category as their impact is
that of a critical severity bug, but they require the precondition of a
compromised renderer. (Bugs which involve using [MojoJS](../../mojo/public/js/README.md)
to trigger an exploitable browser process crash usually fall into this category).
Another example are bugs that result in memory corruption in the browser
process, which would normally be critical severity, but require browser shutdown
or profile destruction, which would lower these issues to high severity. A
bug with the precondition of browser shutdown or profile destruction should be
considered to have a maximum severity of high and could potentially be
reduced by other mitigating factors.

They are normally assigned Priority **P1** and assigned to the current stable
milestone (or earliest milestone affected). For high severity bugs,
[SheriffBot](https://www.chromium.org/issue-tracking/autotriage) will
automatically assign the milestone.

**For high severity (S1) vulnerabilities, we aim to deploy the patch to all
Chrome users in under 60 days.**

Example bugs:

* A bug that allows full circumvention of the same origin policy. Universal XSS
bugs fall into this category, as they allow script execution in the context of
an arbitrary origin ([534923](https://crbug.com/534923)).
* A bug that allows arbitrary code execution within the confines of the sandbox,
such as memory corruption in the renderer process
([570427](https://crbug.com/570427), [468936](https://crbug.com/468936)).
* Complete control over the apparent origin in the omnibox
([76666](https://crbug.com/76666)).
* Memory corruption in the browser or another high privileged process (e.g. a
  GPU or network process on a [platform where they're not sandboxed](../../docs/security/process-sandboxes-by-platform.md)),
  that can only be triggered from a compromised renderer,
  leading to a sandbox escape ([1393177](https://crbug.com/1393177),
  [1421268](crbug.com/1421268)).
* Kernel memory corruption that could be used as a sandbox escape from a
compromised renderer ([377392](https://crbug.com/377392)).
* Memory corruption in the browser or another high privileged process (e.g.
  GPU or network process on a [platform where they're not sandboxed](../../docs/security/process-sandboxes-by-platform.md))
  that requires specific user interaction, such as granting a permission ([455735](https://crbug.com/455735)).
* Site Isolation bypasses:
    - Cross-site execution contexts unexpectedly sharing a renderer process
      ([863069](https://crbug.com/863069), [886976](https://crbug.com/886976)).
    - Cross-site data disclosure
      ([917668](https://crbug.com/917668), [927849](https://crbug.com/927849)).


## Medium severity (S2) {#TOC-Medium-severity}

Medium severity (S2) bugs allow attackers to read or modify limited amounts of
information, or are not harmful on their own but potentially harmful when
combined with other bugs. This includes information leaks that could be useful
in potential memory corruption exploits, or exposure of sensitive user
information that an attacker can exfiltrate. Bugs that would normally be rated
at a higher severity level with unusual mitigating factors may be rated as
medium severity.

They are normally assigned Priority **P1** and assigned to the current stable
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
* Memory corruption in the browser process, triggered by a browser shutdown that
  is not reliably triggered and/or is difficult to trigger ([1230513](https://crbug.com/1230513)).
* Memory corruption in the browser process, requiring a non-standard flag and
  user interaction ([1255332](https://crbug.com/1255332)).
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


## Low severity (S3) {#TOC-Low-severity}

Low severity (S3) vulnerabilities are usually bugs that would normally be a
higher severity, but which have extreme mitigating factors or highly limited
scope.

They are normally assigned Priority **P2**. Milestones can be assigned to low
severity bugs on a case-by-case basis, but they are not normally merged to
stable or beta branches.

Example bugs:

* Bypass requirement for a user gesture ([256057](https://crbug.com/256057)).
* Partial CSP bypass ([534570](https://crbug.com/534570)).
* A limited extension permission bypass ([169632](https://crbug.com/169632)).
* An uncontrolled single-byte out-of-bounds read
([128163](https://crbug.com/128163)).

## Priority for in the wild vulnerabilities {#TOC-itw-pri}

If there is evidence of a weaponized exploit or active exploitation in the wild,
the vulnerability is considered a P0 priority - regardless of the severity
rating -with a SLO of 7 days or faster. Our goal is to release a fix in a
Stable channel update of Chrome as soon as possible.

## Can't impact Chrome users by default {#TOC-No-impact}

If the bug can't impact Chrome users by default, this is denoted instead by
the **Security-Impact_None** hotlist (hotlistID: 5433277). See
[the security labels document](security-labels.md#TOC-Security_Impact-None)
for more information. The bug should still have a severity set according
to these guidelines.


## Not a security bug {#TOC-Not-a-security-bug}

The [security FAQ](faq.md) covers many of the cases that we do not consider to
be security bugs, such as [denial of service](faq.md#TOC-Are-denial-of-service-issues-considered-security-bugs-)
and, in particular, null pointer dereferences with consistent fixed offsets.


## "MiraclePtr" protection against use-after-free {#TOC-MiraclePtr}

["MiraclePtr"](../../base/memory/raw_ptr.md) is a technology designed to
deterministically prevent exploitation of use-after-free bugs. Address
sanitizer is aware of MiraclePtr and will report on whether a given
use-after-free bug is protected or not:

```
MiraclePtr Status: NOT PROTECTED
No raw_ptr<T> access to this region was detected prior to the crash.
```

or

```
MiraclePtr Status: PROTECTED
The crash occurred while a raw_ptr<T> object containing a dangling pointer was being dereferenced.
MiraclePtr should make this crash non-exploitable in regular builds.
```

MiraclePtr is now active on all Chrome platforms in non-renderer processes as
of 118 and on Fuchsia as of 128. Severity assessments are made with
consideration of all active release channels (Dev, Beta, Stable, and Extended Stable);
BRP is now enabled in all active release channels.

As of 128, if a bug is marked `MiraclePtr Status:PROTECTED`, it is not
considered a security issue. It should be converted to type:Bug and assigned to
the appropriate engineering team as functional issue.
