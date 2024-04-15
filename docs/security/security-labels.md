# Security Fields, Hotlists, and Issue Access / Visibility

[TOC]

Bug database labels are used very heavily for security bugs. We rely on the
labels being correct for a variety of reasons, including driving fixing efforts,
driving release management efforts (merges and release notes) and also
historical queries and data mining.

Because of the extent to which we rely on labels, it is an important part of the
Security Sheriff duty to ensure that all security bugs are correctly tagged and
managed. But even if you are not the Security Shepherd, please fix any labeling
errors you happen upon.

Any issue that relates to security should have one of the following:

* **Security** Component Tag: Features that are related to security.
* **Type=Vulnerability**: Designates a security vulnerability that impacts
users. This label should not be used for new features that relate to security,
or general remediation/refactoring ideas. (Use the **Security** Component Tag
for that.)

## Fields and Hotlists Relevant For Any **Type=Vulnerability**

* **Security_Severity-**{**Critical (S0)**, **High (S1)**, **Medium (S2)**,
**Low(S3)**, **Unknown / Not Yet Assessed (S4)**}: Designates the severity
of a vulnerability according to our
[severity guidelines](severity-guidelines.md).
* **Priority: P#**: Priority should generally match Severity (but should be
  higher if there is evidence of active exploitation):
  * **Security_Severity-Critical**: **P0**.
  * **High** and **Medium**: **P1**.
  * **Low**: **P2**.
* **Found In: MMM#**: Designates which milestones of Chrome are
impacted by the bug. Multiple milestones may be set in the Found In field,
but the most important one is the earliest affected milestone. See
[ChromiumDash](https://chromiumdash.appspot.com/releases?platform=Windows) for
current releases.
* **Security_Impact-**{**Head**, **Beta**, **Stable**, **Extended**, **None**}
  hotlists: Derived from milestones set in the **Found In** field,, this
  hotlist specifies the earliest affected release channel. Should not normally
  be set by humans, except in the case of **Security_Impact-None**
  (hotlistID: 5433277) which means that the bug is in a disabled feature, or
  otherwise doesn't impact Chrome: see the section below for more details.
    * Note that **Severity** should still be set to the appropriate Severity
    (S0-S3) for **Security_Impact-None** issues, as if the feature were enabled
    or the code reachable.
* **Issue access level & collaborator groups**{**collaborators=
  security@chromium.org**, **collaborators=security-notify@chromium.org**,
  **Limited Visibility + Google**, **SecurityEmbargo hotlist**}: settings
  that restrict access to the bug. Meaning and usage guidelines are as follows:
  * **Issue Access level: Limited Visibility + collaborator group =
    security@chromium.org**: Restricts access to members of
    *security@chromium.org*. This is the default that should be used for general
    security bugs that aren't sensitive otherwise.
  * **Issue Access level: Limited Visibility + collaborator group =
    security-notify@chromium.org**: Restricts access to members of
    *security-notify@chromium.org*, which includes external parties who ship
    Chromium-based products and who need to know about available bug fixes.
    *security@chromium.org* is a member of that group so the former is a
    superset of the latter.
    **Collaborator group = security-notify@chromium.org** is not suitable for
    sensitive bugs.
  * **Issue Access level: Limited Visibility + collaborator group =
    security-notify@webrtc.org**: As above, but additionally give access to
    *security-notify@webrtc.org*, a community of downstream WebRTC embedders.
  * **Issue Access level: Limited Visibility + Googlers**: Restricts access to
    users that are Google employees (but also via their *chromium.org*
    accounts). This should be used for bugs that aren't OK for external
    contributors to see (even if we trust them with security work), for
    example due to:
      * legal reasons (bug affects a partner Google is under NDA and the
        information is subject to that)
      * the bug affecting more Google products than Chrome and Chrome OS
  * **SecurityEmbargo hotlist** (hotlistID: 5432549): Keeps issues already
    set with Issue Access level: Limited Visibility + collaborator group =
    *security@chromium.org* from being automatically de-restricted by Blintz
    and keeping the bug from being opened for public disclosure. Use this if
    the bug in question is subject to disclosure decisions made externally,
    such as:
      * We receive advance notice of security bugs from an upstream open source
        project or Google partner and they organize a coordinated disclosure
        process. We'd remove the restriction hotlist if/when the embargo gets
        lifted.
      * The reporter indicates a preference to remain anonymous and the bug
        history would give away the reporter's identity (if they file using an
        anonymous account, this doesn't apply).

* **reward-**{**topanel**, **unpaid**, **na**, **inprocess**, _#_} hotlists:
  Hotlists used for tracking bugs nominated for our [Vulnerability Reward
Program](https://www.chromium.org/Home/chromium-security/vulnerability-rewards-program).
* **reward_to-external** (hotlistID: 5432589): If a bug is filed by a Google or
Chromium user on behalf of an external party, use **reward_to-external** to
ensure the report is still properly credited to the external reporter in the
release notes. IF, however, _the reporter is an individual with an email
address_ you should set the **Reporter** field to reflect the email address
of the external reporter. If the reporter was an organization or entity with a
specific email address, then do not alter the **Reporter** field and use the
**reward-to_external** hotlist. Despite its name, you should add
this label whether or not the reporter is in scope for the vulnerability rewards
program, because external reports are credited in the release notes irrespective.
* **M-#** field: Target milestone for the fix.
* Chromium 'Component Tags': For bugs filed as **Type=Vulnerability**, we also
want to track which Chromium component(s) the bug is in.
* **ReleaseBlock field = Stable**: When we find a security bug regression that
has not yet shipped to stable, we use this label to try and prevent the security
regression from ever affecting users of the Stable channel.
* **OS-**{**Chrome**, **Linux**, **Windows**, ...}: Denotes which operating
systems are affected.
* **Merge: field**{**Request-?**, **Approved-?**, **Merged-?**}: Security fixes
are frequently merged to earlier release branches.
* **Security Release: #-M###**: Denotes which exact patch a security fix made
it into. This is more fine-grained than the **M-#** label. **Release-0-M105**
denotes the initial release of an M105 release to Stable channel.
* **CVE field: -####-####**: For security bugs that get assigned a CVE, we
update the CVE field for appropriate bug(s) with the CVE number  for easy
searching.
**Type=Vulnerability** bugs should always have **Severity of S0-S3**,
**Found In - ### set**, **Security_Impact** hotlist, **OS**, **Priority**,
**M**, **Component Tags**, and an **Assigner** set.

### When to use the  Security_Impact-None hotlist {#TOC-Security-Impact-None}

**Security_Impact-None** says that the bug can't affect any users running the
default configuration of Chrome. It's most commonly used for cases where
code is entirely disabled or absent in the production build.

Other cases where it's OK to set **Security_Impact-None**:

* The impacted code runs behind a feature flag which is *disabled by default*,
  and the field trial configuration has not been switched on.
* The impacted code only runs behind a command-line flag or `chrome://flags`
  entry. (In particular, if a bug can only affect those who have
  set `#enable-experimental-web-platform-features`, it is **Security_Impact-None**.
* It's a V8 feature behind flags such as `--future`, `--es-staging` or
  `--wasm-staging` or other experimental flags that are disabled by default.

Cases where it's *not* OK to set **Security_Impact-None**:

* Features enabled via normal UI or settings which users might happen across
  in normal usage. For instance, accessibility features and the Chrome Labs
  experimental features accessible from the toolbar.
* Origin trials. Origin trials are only active on some websites, but the
  affected code does run for Chrome users with the default Chrome configuration.
* The impacted code runs behind a feature flag which is *enabled by default*,
  even if that field trial configuration has been switched off. That's because
  the code may be active for devices which can't access the field trial
  configuration service.
* The feature is turned on only for a small percent of users, e.g. 1%.
* Feature or flag checks are done somewhere that the attacker could influence.
  For example a privilege escalation from a lower-privileged process
  (e.g. renderer) to a higher-privileged process (e.g. browser)
  assumes that the lower-privileged process is already compromised. The
  attacker could overwrite memory for any feature checks performed within
  that lower-privileged process; the bug only qualifies as impact **None**
  if checks are performed in the higher-privileged process.
* If a bug involves a patch to a renderer or use of a flag to turn on
  [MojoJS](../../mojo/public/js/README.md)
  this may mean it's a simulation of a compromised renderer and the
  bug may still be a valid [sandbox escape
  bug](severity-guidelines.md#TOC-High-severity).

It's important to get this right, because this label influences how rapidly
we merge and release the fix. Ask for help if you're not sure.

Some **Security_Impact-None** bugs may still be subject to VRP rewards, if
those bugs are found in code that we're likely to enable in the future.

### OS Field

It can be hard to know which OS(s) a bug applies to. Here are some guidelines:

* Blink is used on all platforms except iOS. A (say) UAF in Blink is probably
not particular to whatever platform it was found on; it's probably applicable
to all.
* The same is true of Skia, and the net/ code.
* If the bug is in a file named `foo_{win,linux,mac,...}.cc`, it's specific to
the named platform.
* Java code is particular to Android.
* Objective-C++ (`foo.mm`) is particular to macOS and iOS. (But note that most
of our Objective-C++ is particular to macOS *or* iOS. You can usually tell by
the pathname.)
* Views code (e.g. `ui/message_center/views`) is used on Windows, Linux, Chrome
OS, and perhaps Fuchsia (?). Views for macOS is increasingly a thing, but Cocoa
code (e.g. `ui/message_center/cocoa`) is particular to macOS.

## After the bug is fixed: Merge field labels {#TOC-Merge-labels}

Once you've landed a complete fix for a security bug, please immediately
mark the bug as Fixed. Do not request merges: Sheriffbot will request
appropriate merges to beta or stable according to our guidelines.
However, it is really helpful if you comment upon any unusual stability or
compatibility risks of merging.

(Some Chromium teams traditionally deal with merges _before_ marking bugs as
Fixed. Please don't do that for security bugs.)

Please take the opportunity to consider whether there are any variants
or related problems. It's very common for attackers to tweak working attack code
to exploit a similar situation elsewhere. If you've even the remotest thought
that there _might_ be equivalent patterns or variants elsewhere, file a bug
with type=Bug-Security. It can be nearly blank. The important thing is to record
the fact that something may need doing.

## Sheriffbot automation

Security labels guide the actions taken by
[Blintz](https://www.chromium.org/issue-tracking/autotriage). The source of
truth for the actual rule set is
[go/chrome-blintz-source](https://goto.google.com/chrome-blintz-source) (sorry,
Google employees only). The motivation behind these rules is to help automate
the security bug life cycle so security shepherds and security engineers in
general spend less time updating bugs and can do more useful work instead.

The following sections describe the current set of rules relevant to security
bugs. The list below only describes rules that change the labels described
above. There are additional rules for sending nag messages and janitorial tasks;
check the [Chrome blintz source](https://goto.google.com/chrome-blintz-source)
for details.

### Remove Invalid **Release-#** Field Entries

Only bugs that affect stable should carry a release designator, this rule
removes release designators that are set on bugs not affecting stable.

### Remove Invalid **Security_Impact-X** Hotlists

Each bug should be on exactly one **Security_Impact-X** and it should be one of
the 5 valid Security_Impact hotlists (None, Extended, Stable, Beta, Head). This
rule a bug from any invalid and excess Security_Impact hotlists.

### Adjust **Security_Impact-X** Hotlists to match Found In

Based on **Found In #** milestone set in the field this rule assigns
corresponding **Security_Impact-X** hotlists if they are incorrect or absent.
**Security_Impact-None** is never changed.

### Update **M-#** Field

Bugs that are set with milestones earlier than the current milestone will
be updated to set the field to the current milestone and
**Security_Impact-Extended**.

Bugs that carry a **Security_Impact-X** hotlist but are missing a milestone
field being set will be updated so that the **M-#** field reflects the
corresponding to the respective milestone.

### Set **ReleaseBlock field** For Regressions

If there's a high or medium severity security regression in beta or ToT, update
the **ReleaseBlock** field to Stable to prevent that regression from being
shipped to users.

Similarly, critical security regressions are marked **ReleaseBlock: Beta**.

### Adjust **Priority P#** To Match Severity

Adjust **Priority P#** according to the priority rules for severity labels described
above. If there is evidence of active exploitation then a higher priority should
be used.

### Drop **Visibility Group Restrictions** From Old, Fixed Bugs for Disclosure

Remove **security@chromium.org, **security-notify@chromium.org** and
**security-notify@webrtc.org** from **Collaborator Groups** and Update **Issue
Access level to Default Visibility** for security bugs that have been closed
(Fixed, Verified, Duplicate, WontFix,Invalid) more than 14 weeks ago, making
them publicly accessible. The idea here is that by 14 weeks, important security
fixes will have shipped in a Stable channel update and allowing users time to
update.

### Set **security-notify@chromium.org as Collaborator** On Fixed Bugs

While **Issue Access level** remains **Limited Visibility** removes
**security@chromium.org** as Collaborator field / Add Collaborator Groups
and replaces updates with **security-notify@chromium.org** for fixed security
bugs. Rationale is that while fixed bugs are generally not intended to become
public immediately, we'd like to give access to external parties depending on
Chromium via *security-notify@chromium.org*.
(Collaborator for WebRTC bugs is instead updated to
**security-notify@webrtc.org**).

### Update **Merge Field with Request-X** For Fixed Bugs

Fixed security bugs that affect stable or beta and are critical or high severity
will automatically trigger a merge request for the current beta branch, and
perhaps stable if also impacted.

### Drop **X from ReleaseBlock field** For **Security_Impact-None** Bugs

No need to stop a release if the bug doesn't have any consequences.

## An Example

Given the importance and volume of field, hotlists, and visiblity settings,
an example might be useful.

1. An external researcher files a security bug, with a repro that demonstrates
memory corruption against the latest (e.g.) M123 dev channel. The will present
as **Type=Vulnerability** and **Visibility / Issue Access level** will be set
to **Limited Visibility** with **security@chromium.org** set as
**Collaborator**.
2. The Security Shepherd triages the issue and uses ClusterFuzz to confirm
that the bug is a novel and dangerous-looking buffer overflow in the renderer
process. ClusterFuzz also confirms that all current releases are affected. Since
M121 is the current Stable release, M120 is Extended Stable, and M122 is in
Beta, we update the **Found In field** to **120** to reflect Extended Stable as
the earliest / oldest affected release channel. The severity of a buffer
overflow in a renderer implies **High (S1) Severity** and **P1 Priority**. Any
external report for a confirmed vulnerability needs **reward-topanel**. Blintz
will usually add it automatically once the bug is fixed. The stack trace
provided by ClusterFuzz suggests that the bug is in the component **Blink>DOM**,
and such bugs should be labeled as applying to all OSs except iOS (where Blink
is not used): **OS-**{**Linux**, **Windows**, **Android**, **Chrome**,
**Fuchsia**}. Blintz will check whether 120 is the current extended stable,
stable, beta or head milestone; and will add the **Security_Impact-Extended
hotlist**.
3. Within a day or two, the Security Shepherd was able to get the bug assigned
and — oh joy! — fixed very quickly. When the bug's status changes to **Fixed**,
Blintz will update the **Merge field with the appropriate request-MMM** labels,
and will change update Visibility by changing **Collaborators from
security@chromium.org to security-notify@chromium.org**.
4. Later that week, the Chrome Security VRP TL does a sweep of all
**reward-topanel** bugs. This one gets rewarded, so that one reward label is
replaced with two: **reward-7000** and **reward-unpaid**. Later,
**reward-unpaid** becomes **reward-inprocess** and is later still removed when
all is done. Of course, **reward-7000** remains forever. (We use it to track
total payout!)
5. A Chrome Security TPM with the responsibility of doing merge review regularly
checks the security merge review queue based on Type=Vulnerability issues with
**Merge fields** updated with **request-MMM* or **review-MMM** entries. They
review this issue and fix, having had enough bake time in Canary and being safe
to merge, and approve it for merge, updating the Merge fields to
**Approved-MMM** for each milestone the fix is being approved for backmerge to.
There are weekly Stable channel updates, and this fix was approved for merge to
M121, so it will be shipped in the following week's update of M121.
6. Just before the release, a Chrome Security TPM runs a series of scripts
and verifies the security fixes being shipped in that Stable channel update and
applies the appropriate **#-M121** tag in the **Release** field. Since the bug
was externally reported, it will also be issued a CVE ID, and the **CVE** field
is updated with the appropriate CVE for that issue: **2024-####**.
7. 14 weeks after the bug is marked **Fixed**, Blintz updates the **Visibility**
from **Issue access level -- Limited Visibility** to **Default Visibility** and
removes security-notify@chromium.org from Collaborators, making the issue
publicly visible. There is one crucial exception -- Blintz will not update the
Visibility or remove security@chromium.org from Collaborators if the issue is
on the **SecurityEmbargo** hotlist.
