# Security Labels And Components

[TOC]

Bug database labels are used very heavily for security bugs. We rely on the
labels being correct for a variety of reasons, including driving fixing efforts,
driving release management efforts (merges and release notes) and also
historical queries and data mining.

Because of the extent to which we rely on labels, it is an important part of the
Security Sheriff duty to ensure that all security bugs are correctly tagged and
managed. But even if you are not the Sheriff, please fix any labeling errors you
happen upon.

Any issue that relates to security should have one of the following:

* **Security** component: Features that are related to security.
* **Type-Bug-Security**: Designates a security vulnerability that impacts users.
This label should not be used for new features that relate to security, or
general remediation/refactoring ideas. (Use the **Security** component for
that.)

## Labels Relevant For Any **Type-Bug-Security**

* **Security_Severity-**{**Critical**, **High**, **Medium**, **Low**,
**None**}: Designates the severity of a vulnerability according to our
[severity guidelines](severity-guidelines.md).
* **Pri-#**: Priority should generally match Severity (but should be higher if
  there is evidence of active exploitation):
  * **Security_Severity-Critical**: **Pri-0**.
  * **High** and **Medium**: **Pri-1**.
  * **Low**: **Pri-2**.
* **FoundIn-#**: Designates which milestones of Chrome are
impacted by the bug. Multiple labels may be set, but the most important one
is the earliest affected milestone. See
[ChromiumDash](https://chromiumdash.appspot.com/releases?platform=Windows) for
current releases.
* **Security_Impact-**{**Head**, **Beta**, **Stable**, **Extended**, **None**}:
Derived from **FoundIn**, this label specifies the earliest affected release
channel. Should not normally be set by humans, except in the case of **None**
which means that the bug is in a disabled feature, or otherwise doesn't impact
Chrome: see the section below for more details.
    * Note that **Security_Severity** should still be set on
      **Security_Impact-None** issues, as if the feature were enabled or the
      code reachable.
* **Restrict-View-**{**SecurityTeam**, **SecurityNotify**, **Google**,
**SecurityEmbargo**}: Labels that restrict access to the bug. Meaning and usage
guidelines are as follows:
  * **Restrict-View-SecurityTeam**: Restricts access to members of
    *security@chromium.org*. This is the default that should be used for general
    security bugs that aren't sensitive otherwise.
  * **Restrict-View-SecurityNotify**: Restricts access to members of
    *security-notify@chromium.org*, which includes external parties who ship
    Chromium-based products and who need to know about available bug fixes.
    *security@chromium.org* is a member of that group so the former is a
    superset of the latter. **Restrict-View-SecurityNotify** is not suitable for
    sensitive bugs.
  * **Restrict-View-SecurityNotifyWebRTC**: As above, but additionally
    gives access to *security-notify@webrtc.org*, a community of downstream
    WebRTC embedders.
  * **Restrict-View-Google**: Restricts access to users that are Google
    employees (but also via their *chromium.org* accounts). This should be used
    for bugs that aren't OK for external contributors to see (even if we trust
    them with security work), for example due to:
      * legal reasons (bug affects a partner Google is under NDA and the
        information is subject to that)
      * the bug affecting more Google products than Chrome and Chrome OS
  * **Restrict-View-SecurityEmbargo**: Restricts access to
    *security@chromium.org* and and stops Sheriffbot from publishing the bug
    automatically. Use this if the bug in question is subject to disclosure
    decisions made externally, such as:
      * We receive advance notice of security bugs from an upstream open source
        project or Google partner and they organize a coordinated disclosure
        process. We'd remove the restriction label if/when the embargo gets
        lifted.
      * The reporter indicates a preference to remain anonymous an the bug
        history would give away the reporter's identity (if they file using an
        anonymous account, this doesn't apply).

  If multiple restriction labels are appropriate, set all of them. Note that all
  restriction labels must be satisfied for a user to have access to a bug.
* **reward-**{**topanel**, **unpaid**, **na**, **inprocess**, _#_}: Labels used
in tracking bugs nominated for our [Vulnerability Reward
Program](https://www.chromium.org/Home/chromium-security/vulnerability-rewards-program).
* **reward_to-**. If a bug is filed by a Google or Chromium user on behalf of
an external party, use **reward_to** to ensure the report is still properly credited
to the external reporter in the release notes. Normally, the latter half of this
label would be an e-mail address with '@' replaced with '_at_'. But if the
reporter was a whole organization or some other entity without a specific e-mail
address, then **reward_to-external** is sufficient to ensure it is credited.
Despite its name, you should add this label whether or not the reporter is
in scope for the vulnerability rewards program, because external reports are
credited in the release notes irrespective.
* **M-#**: Target milestone for the fix.
* Component: For bugs filed as **Type-Bug-Security**, we also want to track
which component(s) the bug is in.
* **ReleaseBlock-Stable**: When we find a security bug regression that has not
yet shipped to stable, we use this label to try and prevent the security
regression from ever affecting users of the Stable channel.
* **OS-**{**Chrome**, **Linux**, **Windows**, ...}: Denotes which operating
systems are affected.
* **Merge-**{**Request-?**, **Approved-?**, **Merged-?**}: Security fixes
are frequently merged to earlier release branches.
* **Release-#-M##**: Denotes which exact patch a security fix made it into.
This is more fine-grained than the **M-#** label. **Release-0-M50** denotes the
initial release of a M50 to Stable.
* **CVE-####-####**: For security bugs that get assigned a CVE, we tag the
appropriate bug(s) with the label for easy searching.
**Type-Bug-Security** bugs should always have **Security_Severity**,
**Security_Impact**, **OS**, **Pri**, **M**, **Component**, and an
**owner** set.

### When to use Security_Impact-None {#TOC-Security-Impact-None}

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
those bugs are found in found in code that we're likely to enable in the future.

### OS Labels

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

## After the bug is fixed: Merge labels {#TOC-Merge-labels}

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
[SheriffBot](https://www.chromium.org/issue-tracking/autotriage). The source of
truth for the actual rule set is
[go/sheriffbot-source](https://goto.google.com/sheriffbot-source) (sorry, Google
employees only). The motivation behind these rules is to help automate the
security bug life cycle so security sheriffs and security engineers in general
spend less time updating bugs and can do more useful work instead.

The following sections describe the current set of rules relevant to security
bugs. The list below only describes rules that change the labels described
above. There are additional rules for sending nag messages and janitorial tasks;
check the [sheriffbot source](https://goto.google.com/sheriffbot-source) for
details.

### Remove Invalid **Release-#** Labels

Only bugs that affect stable should carry a release label, this rule removes
release labels that are set on bugs not affecting stable.

### Remove Invalid **Security_Impact-X** Labels

There should be exactly one **Security_Impact-X** label and it should be one of
the 5 valid impact labels (None, Extended, Stable, Beta, Head). This rule
removes any invalid and excess impact labels.

### Adjust **Security_Impact-X** To Match FoundIn Labels

Based on **FoundIn-#** milestone labels this rule assigns corresponding
**Security_Impact-X** labels if they are incorrect or absent.
**Security_Impact-None** is never changed.

### Update **M-#** Labels

Bugs that are labelled with milestones earlier than the current milestone will
be relabeled to set the label for the current milestone and
**Security_Impact-Extended**.

Bugs that carry a **Security_Impact-X** label but are missing a milestone label
will be assigned the **M-#** label corresponding to the respective milestone.

### Set **ReleaseBlock-X** For Regressions

If there's a high or medium severity security regression in beta or ToT, add a
**ReleaseBlock-Stable** label to prevent that regression to be shipped to users.

Similarly, critical security regressions are marked **ReleaseBlock-Beta**.

### Adjust **Pri-#** To Match Severity

Adjust **Pri-#** according to the priority rules for severity labels described
above. If there is evidence of active exploitation then a higher priority should
be used.

### Drop **Restrict-View-{SecurityTeam,SecurityNotify}** From Old And Fixed Bugs

Remove **Restrict-View-SecurityTeam**, **Restrict-View-SecurityNotify** and
**Restrict-View-SecurityNotifyWebRTC** from
security bugs that have been closed (Fixed, Verified, Duplicate, WontFix,
Invalid) more than 14 weeks ago and add the **allpublic** label to make the bugs
accessible publicly. The idea here is that security bug fixes will generally
require 12 weeks to ship (2 release cycles for ToT changes to hit stable). This
catches cases where the bug owner forgets to mark the bug public after the fix
is released.

### Set **Restrict-View-SecurityNotify** On Fixed Bugs

Replace **Restrict-View-SecurityTeam** with **Restrict-View-SecurityNotify** for
fixed security bugs. Rationale is that while fixed bugs are generally not
intended to become public immediately, we'd like to give access to external
parties depending on Chromium via *security-notify@chromium.org*.
(WebRTC bugs instead get set to **Restrict-View-SecurityNotifyWebRTC**).

### Set **Merge-Request-X** For Fixed Bugs

Fixed security bugs that affect stable or beta and are critical or high severity
will automatically trigger a merge request for the current beta branch, and
perhaps stable if also impacted.

### Drop **ReleaseBlock-X** Labels From **Security_Impact-None** Bugs

No need to stop a release if the bug doesn't have any consequences.

### Set **Status:Fixed** For Open Security Bugs With Merge Labels

Security bugs that have a merge label (but excluding bugs with **component:OS**)
are marked as fixed automatically. The rationale is that if something gets
merged to a release branch, there's a high likelihood that the bug is actually
fixed.

## An Example

Given the importance and volume of labels, an example might be useful.

1. An external researcher files a security bug, with a repro that demonstrates
memory corruption against the latest (e.g.) M29 dev channel. The labels
**Restrict-View-SecurityTeam** and **Type-Bug-Security** will be applied.
1. The sheriff jumps right on it and uses ClusterFuzz to confirm that the bug is
a novel and nasty-looking buffer overflow in the renderer process. ClusterFuzz
also confirms that all current releases are affected. Since M27 is the current
Stable release, and M28 is in Beta, we add the labels of the earliest affected
release: **FoundIn-27**. The severity of a buffer overflow
in a renderer implies **Security_Severity-High** and **Pri-1**. Any external
report for a confirmed vulnerability needs **reward-topanel**. Sheriffbot will
usually add it automatically. The stack trace provided by ClusterFuzz suggests
that the bug is in the component **Blink>DOM**, and such bugs should be labeled
as applying to all OSs except iOS (where Blink is not used): **OS-**{**Linux**,
**Windows**, **Android**, **Chrome**, **Fuchsia**}. Sheriffbot will check
whether 27 is the current extended stable, stable, beta or head milestone; let's
assume **Security_Impact-Stable** is applied by Sheriffbot this time.
1. Within a day or two, the sheriff was able to get the bug assigned and — oh
joy! — fixed very quickly. When the bug's status changes to **Fixed**,
Sheriffbot will add the **Merge-Requested** label, and will change
**Restrict-View-SecurityTeam** to **Restrict-View-SecurityNotify**.
1. Later that week, the Chrome Security manager does a sweep of all
**reward-topanel** bugs. This one gets rewarded, so that one reward label is
replaced with two: **reward-1000** and **reward-unpaid**. Later,
**reward-unpaid** becomes **reward-inprocess** and is later still removed when
all is done. Of course, **reward-1000** remains forever. (We use it to track
total payout!)
1. The next week, a Chrome TPM states that the first Chrome M27 stable patch is
going on and asks if we want to include security fixes. We do, of course.
Having had this particular fix "bake" in another M29 Dev channel, the Chrome
Security release manager decides to merge it. **Merge-Approved-M##** is
replaced with **Merge-Merged-M##**. (IMPORTANT: This transition only occurs
after the fix is merged to ALL applicable branches, i.e. M28 as well as M27 in
this case.) We now know that users of Chrome Stable will get the fix in the
first M27 Stable patch, so the following labels are changed/applied: **M-27**,
**Release-1**. Since the bug was externally reported, it definitely gets its
own CVE, and a label is eventually added: **CVE-2013-31337**.
1. 14 weeks after the bug is marked **Fixed**, Sheriffbot removes the
**Restrict-View-SecurityNotify** label and other **Restrict-View-?** labels,
making the bug public. There is one crucial exception: Sheriffbot will not
remove **Restrict-View-SecurityEmbargo**.
