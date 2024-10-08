# Security Shepherd

## What is Security Shepherding?
Security Shepherding is a rotational assignment for security bug triage
(Primary Shepherd) and managing the flow of incoming inquiries and progressing
security issues (Secondary Shepherd). The Shepherding rota pool is made up of
people actively working on security in Chrome.

All Security Shepherds are Googlers; therefore, some links on this page may
not be externally accessible or even further restricted to just Chrome Security
Googlers.

There is a Primary and Secondary Shepherd scheduled each rotation, with two
rotations each week, one Tuesday - Thursday and Friday - Monday.

Security Shepherding is *not* an on-call rotation. There’s no pager duty, nor
are you expected to perform Shepherding duties outside of your usual working
hours, such as overnight or on holidays, weekends, or other off time.

Shepherds are only responsible for triage of security bugs during your shift.
You are not responsible for bug triage or updating partially triaged bugs past
your shift, unless you have specifically taken ownership of an issue, such as
due to a complicated or OS-specific reproduction, and arranged that with the
oncoming shepherd. All shepherds should use the handoff doc to communicate items
for handover; however, the oncoming primary shepherd should operate on the
premise all new or _under_-triaged issues are your responsibility. Please do not
leave any unaddressed red cells in the dashboard at the end of your shift.

## TL;DR Checklist for Primary Shepherding
(“I’m Primary Shepherd, what do I do???”)

The Primary Security Shepherd is the front line of security bug triage during
their shift. The goal is to triage incoming security issues accurately,
thoroughly, and quickly (_as quickly as realistically possible_).

Your PRIMARY DIRECTIVE as PRIMARY SHEPHERD is to tackle all the red cells on the
security bug dashboard.

For [*every new incoming security bug*](#Every-New-Incoming-Security-Bug):
* Make sure it's [*self-contained*](#Ensure-self_contained-issue).
* Make sure the report is [*valid and actionable*](#Confirm-Valid-and-Actionable)
  * Ideally, you’ll be able to do this by [reproducing the bug](#Reproduce-the-bug),
    more ideally, [with ClusterFuzz](clusterfuzz-for-shepherds.md).
* Set [*severity*](#Set-severity).
* Set [*oldest impacted active release channel*](#Set-oldest-impacted-active-release-channel) – AKA FoundIn.
* Set [*impacted-operating-systems*](#Set-impacted-operating-systems).
* [*Assign*](#Assign) to an appropriate or suitable owner or engineering team.

All of the above should be completed as soon as possible during your shift,
and at least, by the [shift-handoff](#shift-handoff).

One or more of the above actions may be necessary to complete the triage of an
under-triaged bug, i.e. covering any of the open red cells in the dashboard that
were not completed from ClusterFuzz auto-triage or previous work on the bug.

All this is hard, so please remember to [ask for help](#Ask-for-help).
[Yell if you must](https://www.youtube.com/watch?v=5y_SbnPx_cE&t=37s)!

## TL;DR Checklist for Secondary Shepherding
(“I’m Secondary Shepherd, what do I do???”)

* [*Check in on triaged issues*](#Check-in-on-triaged-issues) to ensure progress
  is being made on medium+ (S2-S0) severity security bugs.
* [*Manage incoming security email*](#Handle-incoming-security-emails).


[TOC]

## Links to Helpful Resources

Here are some of the important references and resources you need or may need
during your shepherding shift:

* [Current Shepherds](https://script.google.com/a/macros/google.com/s/AKfycbz02xD4ghSzZu_tXyNRgjC95wFURATZeD_FHq0KRMHeqA-b0b9sow4NV1lhi0P2vy1j/exec)
* [Chrome Security Bug Dashboard](https://goto.google.com/chrome-security-bugs)
* [Security Severity Guidelines](severity-guidelines.md)
* [Security Labels](security-labels.md)
* FAQs addressing commonly-raised questions about security and what is / is not
  considered a security bug, to see if there is an existing stance:
  * [Chrome Security FAQ](faq.md)
  * [Extensions Security FAQ](https://chromium.googlesource.com/chromium/src/+/refs/heads/main/extensions/docs/security_faq.md)
  * [Service Worker Security FAQ](service-worker-security-faq.md)
* [Redshell for Security Shepherds](https://goto.google.com/redshell-for-chrome-shepherds)
* [Shepherding Guidelines Changelog](https://goto.google.com/shepherding-changelog) for highlighting
  any process or policy changes since your last shift.
* [Guidance for triage of theoretical or speculative issues](https://goto.google.com/chrome-speculative-bug-triage)
* [Reference for common questions about security bug lifecycle](life-of-a-security-issue.md)
* [Reference for questions related to security fix merge process](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/process/merge_request.md#Security-merge-triage)
  for answering questions (you do not need to approve merges).
* [Shepherding Handoff Log](https://goto.google.com/chrome-security-shepherd-handoff)

### Every New Incoming Security Bug

Monitor the [Chrome open security bugs dashboard](http://go/chrome-security-bugs).
Tackle all the empty red cells. New bugs populate at the top of the sheet and
will need full triage. Partially triaged bugs, such as those triaged by
ClusterFuzz or ones pending updates from a prior shift, may be lower in the
sheet. Please check the sheet for any red cells and do your best to get any bugs
to a fully triaged state.

We aim to have every bug triaged and assigned **within two business days,
preferably one.** This does not include weekends, but please ensure you leave a
clear queue before the weekend or a holiday.

### Ensure self-contained issue

There should be one complete, self-contained report, per root cause. To ensure
this is the case when assigning security bugs to engineering teams, you may
need to take some specific actions here:
* If the report is a bug chain with several underlying causes, **open one new
  bug per root cause** and mark the parent bug as `blocked on` each. The parent
  bug should be set to the severity of the full chain. Each child bug may have a
  lower severity.
  * If taking these actions for a VRP reported issue, update the Reporter field
    with the email address of the VRP reporter and cc: them on the parent issue
    so they have access.
* Get everything you need from a reporter before you try and reproduce - do not
  feel bad about asking for a clear or minimized POC or repeatable steps before
  attempting to reproduce.
* If complicated user gestures are required, encourage the reporter to upload a
  short video. This will alleviate a lot of back and forth for both them and us.

## Confirm Valid and Actionable

We expect engineering teams to address security bugs promptly. In order to do
that, our goal is to pass them actionable reports with little ambiguity.

*For each bug, please take the appropriate action, either:*

* **WontFix it as invalid** (Many recurring types of invalid reports are covered
  by the [Security FAQ](faq.md), such as those related physically local attacks
  or inputting JavaScript in the URL bar or running Javascript directly in
  DevTools not being an indication of an XSS vulnerability. Mark as WontFix and
  update the 'Issue access level' to `Default access` so the issue is
  publicly visible.
* **Mark as duplicate** – we want exactly one bug per root cause problem. Please
  check for duplicate issues of a given issue from that or other reporters /
  sources (such as ClusterFuzz).
  * Search for similar stack traces or sharing similar keyword traits in the bug
    tracker.
  * If there are two open reports of the same issue, please merge as a duplicate
    in the direction of the oldest report.
  * Use the `Mark as Duplicate` button at the upper right of the report pane.
    This will provide a pop-up to input the bug number of the canonical report
    that you are merging this report into as a duplicate of.
    * Using `Mark as Duplicate` is the best practice for merging issues as
      duplicates.

* **Convert functional bugs to Type=Bug** For example, many reports are for
  crashes of a functional nature, rather than an exploitable security condition,
  such as most null pointer dereferences. Convert such reports from
  Type=Vulnerability to Type=Bug. Do NOT remove security@chromium.org from
  collaborators first (as this will result in orphaning the bug), but update the
  'Issue Access Level' to the appropriate visibility. You may consider adding
  other visibility restrictions, such as `Limited visibility + Googlers` and add
  edit-bug-access@chromium.org to CC (this is similar to
  'Restrict-View-EditAccess' in the legacy issue tracker) if the immediate
  disclosure could result in potential abuse (e.g. denial of service issue).
* **Convert to a privacy bug** - privacy issues (such as issues with incognito)
  are not considered security bugs, but functional privacy issues.
  Convert to Type=Bug and add the Privacy component. Add yourself and any other
  security team members who may potentially need access to the cc: line.
  Update the 'Issue access level' to `Limited visibility + Googlers` and
  deselect / remove security@chromium.org from the 'add collaborator groups'.
* **Add the `Needs-Feedback` hotlist (hotlistID: 5433459) and set a Next Action
  date of 24-48 hours for more information** if there is no response, close the
  issue as `WontFix`.
* **Determine issue to be theoretical** - and follow the [process for such
  issues](http://go/chrome-speculative-bug-triage) – theoretical issues are
  ones that appear to be potentially real bugs, but the report is lacking
  evidence of exploitability or reachability. These cases can be shared with
  engineering teams with a very clear message conveying the speculative nature
  of the issue. These reports should generally not be prioritized as a Pri-1 as
  they do not warrant disruption to the engineering teams to investigate and
  prioritize without more or new information to demonstrate conditions of
  exploitability.

None of these apply? Great – this means the bug may be valid and actionable!
It can take multiple discussions with a reporter to understand a bug. Try really
hard to reach a conclusion by the end of your shift. If this isn’t possible,
please discuss outstanding cases with the next shepherd and don’t let bugs fall
through the cracks. You are responsible for any bug reported or in an un-triaged
state during your shift.

The best way to determine the validity of a security bug is to [*reproduce it*](#Reproduce-the-bug).
It’s helpful to remember that reporters invested time and energy in their bug
reports:

![alt text](apf-right-a-wrong.png "felt: a lot of Chrome vuln reports come from
well-meaning people who clearly went out of their way to try to right a wrong.
i like that.")

[link](https://twitter.com/__apf__/status/728776130564526080)


If you have to close it, please give an explanation as to why.

### Reproduce the bug

Reproducing the bug isn’t always required, but often it’s needed and the only
way to:

* Understand a report and validate the issue being presented.
* Provide actionable information to the engineering team responsible for fixing
  the bug.
* Setting the oldest impacted release channel correctly.

These things must be done correctly, so as Security Shepherd, you’ll spend a lot
of time reproducing bugs. Here are some tips in doing so:

* Assume that test cases may be malicious. You should only reproduce bugs
  on your local machine if you're completely certain that you understand
  100% of the test case. If not, use a disposable virtual machine. If you're
  inside Google, a good way to do this is using
  [Redshell](https://goto.google.com/redshell-for-chrome-shepherds).
* For any sort of a crash, CHECK/DCHECK or memory safety problem
  [use ClusterFuzz](clusterfuzz-for-shepherds.md). As well as reproducing bugs,
  ClusterFuzz will help you with lots of subsequent bisection and labelling
  tasks. Currently ClusterFuzz only supports untrusted inputs on Linux. If you
  use ClusterFuzz to reproduce on any other platform, you should be just as
  paranoid as if you were running a test case locally.
* [Instructions for using an Android emulator can be found
  here](/docs/android_emulator.md). If you're inside Google, we have a
  [guide for testing using Google infrastructure](https://goto.google.com/android-for-chrome-shepherds).
* When you can't just build from a specific branch locally, see
  [https://dev.chromium.org/getting-involved/dev-channel](https://dev.chromium.org/getting-involved/dev-channel)
  or
  [https://commondatastorage.googleapis.com/chromium-browser-asan/index.html](https://commondatastorage.googleapis.com/chromium-browser-asan/index.html)
  for the latest release of a specific version.
* The [get_asan_chrome.py](https://source.chromium.org/chromium/chromium/src/+/main:tools/get_asan_chrome/get_asan_chrome.py)
  helper script is a handy way to download ASAN Chrome. The --help flag
  provides usage instructions, e.g. to fetch builds for various versions and
  platforms.
* If you run into issues with a reproducible ClusterFuzz test case (like
  missing symbols, or if anything else seems off), try uploading the test case
  again using a different job type with a more mature tool (e.g. ASan on Linux).
  It may give more complete information.

### Set severity

Use the [Security Severity Guidelines](severity-guidelines.md).

If you can, [*reproduce it using ClusterFuzz*](clusterfuzz-for-shepherds.md), as
the severity is usually set automatically.

For V8 issues, you can tentatively set the issue as High (S1) severity – see
[Assign,below](#Assign).

Please adjust severity as your understanding of the bug evolves - but please
always add a comment explaining the change. Higher severity bugs involve
significant disruption for multiple teams; lower severity issues may not be
fixed and a fix released to users as quickly as may be warranted. That’s why
it’s important to get the severity as correct as possible.

### Set oldest impacted active release channel

We do not release severe security regressions, so we need to know the earliest
impacted Chrome release branch.

First, if an issue [doesn’t impact Chrome users by default (such as be being
behind a disabled feature or a command line flag), add the hotlist
**`Security_Impact-None`**](security-labels.md#when-to-use-security_impact_none-toc_security_impact_none);
otherwise, set a **Found In** milestone in the `Found In` field as follows:

Check [ChromiumDash](https://chromiumdash.appspot.com/releases?platform=Windows) for the earliest relevant milestone number
(Extended Stable or Stable – sometimes they are the same).
* If that branch is affected, set the `Found In` field to, to the appropriate
  milestone number.
* Otherwise, move forward through milestone numbers. Set the `Found In` field
  to the oldest impacted branch you find.

There is no general reason to test versions older than the current Extended
Stable milestone. If you can [*reproduce using ClusterFuzz*](clusterfuzz-for-shepherds.md)
the `Found In` field can often be set automatically if ClusterFuzz can identify
the culprit CL.

Otherwise, you may need to [reproduce the bug](#Reproduce-the-bug) manually to
determine the impacted branches.

If you have a bisection or other convincing evidence, that’s sufficient. You can
manually check which milestone has a given commit in
[ChromiumDash commits check](https://chromiumdash.appspot.com/commits).

Please *do not* base Found In- on the Chrome version number provided in the
original report. This is often based on the version number the individual is
using when discovering this issue or is automatically set in the report by the
tracker’s report wizard and is not correct in terms of coverage of all active
release channels.

For V8 bugs, you can set `Found In` as the current extended stable milestone
unless you have reproduced the issue or an accurate bisection has been provided.
(See [Assign, below](#Assign).)

### Set impacted operating systems

Set the `OS` field as best you can based on [these guidelines](security-labels.md#OS-Labels).
You do not need to reproduce the bug on each platform, but it really helps if
you set this field roughly right to ensure the bug has the attention of the
different desktop and mobile release teams.

Some issues may be specific to a particular platform, if you need to reproduce a
bug that is platform specific and you do not have access to a device with that
OS, please [ask for help](#Ask-for-help), there is likely someone on the team
that does and can help you.

ChromeOS is in the Google issue tracker. VRP reports for ChromeOS should be
[directly reported to ChromeOS](https://bughunters.google.com/report). Please
request the reporter submit reports directly to ChromeOS in the future. For
VRP and other human-submitted security bug reports specific to ChromeOS,
please move the report corresponding component (componentid:1335705) in the
Google issue tracker. Since this bug is being moved between trackers you will
need to use your google.com account to move the bug into that tracker component.

Some machine-discovered (Clusterfuzz, Crash AutoBugFiler, GWP-ASAN) may be
specific to ChromeOS. If this is determined to be the case after investigation
(please remember some GWP-ASAN or crash bug auto-filer bugs may have come from a
ChromeOS crash, but the issue may not be specific to ChromeOS), move the bug
to the appropriate ChromeOS component (componentid:1214738) in the Google
issue tracker for these reports. Again, you will need to use your google.com
account to move this bug into that component.

### Assign

Security bugs are not automatically visible, so you must add people to get them
fixed. For each bug, set:

* The **Component** – due to a limited set of auto-cc rules, this may add
  some visibility. This will "move" the bug into that component; this is the
  expected outcome. It can also be helpful to set additional **Component Tags**
  when a bug does not fall neatly into a single component.
* An **assignee/owner**. Use `git blame` or look for similar past bugs in the
  tracker.
* Lots of **cc**s. Copy everyone who could possibly be relevant. Use the owners
  file for a particular feature to help achieve this.
* Add a **comment** so that recipients know what’s expected, and why you think
  they’re the right person to take action.
  * Be sure to convey if you have reproduced this issue and your determinations
  about security relevance or diagnosis.

It’s okay if you cannot determine or  know the exact right assignee, but please
pass it along to / include someone who can direct it more precisely.

*Some types of bugs have specific assignment needs:*
* **V8 bugs**. First, [upload benign-looking test cases to
  ClusterFuzz](clusterfuzz-for-shepherds.md) if it isn't already
  there (please keep an eye out for any special flags and debug vs release).
  Hopefully, this will cause ClusterFuzz to reproduce and bisect the bug. If
  not:
    * Set a provisional severity of High (S1), assuming this causes renderer
      memory corruption.
    * Set a provisional `Found In` of the current Extended Stable.
    * Assign it to the current [V8
      Sheriff](https://goto.google.com/current-v8-sheriff) with
      a comment explaining that the severity and `Found In` are provisional.
      Note that V8 CHECK failure crashes can have security implications, so
      don't triage it yourself.
    * If for any reason you need to discuss the bug with a particular V8 contact,
      Googlers can look at
      [the V8 security bug triage instructions](https://goto.google.com/v8-security-issue-triage-how-to)
      for lists of component owners, but this shouldn't normally be necessary.
* **V8 Sandbox bypasses**. The V8 Sandbox is still under development, but V8
  has begun accepting bypass submissions as low-severity security bugs with
  specific submission rules. That being said, Chrome Shepherds are not expected
  to fully triage these reports. You do not need to submit the sandbox bypasses
  to Clusterfuzz. If the report is clearly a V8 sandbox bypass, simply:
    * Set a provisional severity of Low (S3).
    * Assign to the current [V8
      Sheriff](https://goto.google.com/current-v8-sheriff).
    * Apply the `Security_Impact-None` hotlist (hotlistID:5433277).
    * If possible, please also apply the `V8 Sandbox` hotlist
      (hotlistID:4802478).
* **Skia bugs** can be assigned to hcm@chromium.org. Be careful while triaging
  these! The place where we're crashing isn't necessarily the place where the
  bug was introduced, so blame may be misleading. Skia fuzzing bugs can be
  assigned to kjlubick@chromium.org, as Skia is heavily fuzzed on OSS-Fuzz and
  some issues reported in Chromium are already known or even fixed upstream.
* **URL spoofing issues**, especially related to RTL or IDNs? See
  [go/url-spoofs](http://go/url-spoofs) for a guide to triaging these.
* **SQLite bugs** can be assigned to an owner from //third_party/sqlite/OWNERS.
  CC drhsqlite@ for upstream issues.
* **Fullscreen bugs** the Open Screen team is taking ownership of Full Screen
  issues, including security bugs. Please assign Full Screen security issues to
  takumif@chromium.org and cc: atadres@chromium.org, muyaoxu@google.com, and
  mfoltz@chromium.org. They are also working on holistic solutions to improving
  the security of fullscreen, so please remember to look for potential
  duplicates of ongoing work.
* **BoringSSL** the BoringSSL project has moved into the Chromium tracker.
  BoringSSL is a library, so security bugs that do not impact Chrome may still
  be meaningful (e.g. server-side bugs). BoringSSL security issues should be
  fully assessed by the BoringSSL team. If you come across a BoringSSL bug in
  the triage queue:
    * Set a provisional severity based on the issue the report proports; the
      BoringSSL team may need to adjust based on their assessment.
    * Set `Component` to: Chromium > BoringSSL.
    * Assign to an appropriate owner based on `third_party/boringssl/OWNERS`;
      Add owners to cc: on the bug to ensure visibility.
    * Add `Security_Impact-None` hotlist; owner will update if this issue
      does impact Chrome.
* Report suspected malicious URLs to SafeBrowsing:
  * Public URLs:
    * [Report malware](https://safebrowsing.google.com/safebrowsing/report_badware/?hl=en)
    * [Report phishing](https://safebrowsing.google.com/safebrowsing/report_phish/?hl=en)
    * [Report incorrect phishing warning](https://safebrowsing.google.com/safebrowsing/report_error/?hl=en)
  * Googlers: see instructions at [go/safebrowsing-escalation](https://goto.google.com/safebrowsing-escalation)
  * Report suspected malicious file attachments to SafeBrowsing.
* If the report is in an upstream package that we pull into our tree via
  `//third_party` or elsewhere:
    * Ask the reporter to file a bug report upstream, if there is an active
      upstream. If they can't / don't, or the report is from a bot
      (clusterfuzz or similar), ask the `//third_party` package owner to file
      it.
    * For the downstream bug (the one on the Chromium tracker):
        * Add the downstream bug to [the Status-External_Dependency hotlist](https://issues.chromium.org/hotlists/5438152).
        * Assign that bug to an OWNER from the `//third_party` package.
        * Ask that owner to ensure that the upstream bug is fixed, the
          downstream copy in Chromium is rolled, and finally the
          downstream bug is marked Fixed.
* For vulnerabilities in services Chrome uses (e.g. Omaha, Chrome Web Store,
  SafeBrowsing), make sure the affected team is informed and has access to the
  necessary bugs.
* Chrome for iOS - bugs suspected to be in **WebKit**:
    * Reproduce using an iOS device or desktop Safari.
    * Set Severity, Found In, and set Component Tags fields.
    * If the issue is in Webkit
      * Add hotlist `Status_ExternalDependency` (hotlistID: [5438152](https://issues.chromium.org/hotlists/5438152))
      * If reported by an external VRP reporter, request they report the issue
      directly to Webkit and provide us the WebKit issue ID after they have done
      so.
      * If this is an internally discovered issue, file a security bug in the
      Security product at [bugs.webkit.org](https://bugs.webkit.org) and
      cc:chrome-ios-security-bugs@google.com. This alias is monitored by the iOS
      Chrome team so they can be notified when the WebKit bug is fixed.
        * Note the WebKit bug ID in the Chromium issue report.
    * All security issues need owners, the WebKit ones can be assigned to ajuma@.

### Shift handoff

As you work through the queue each day, please manage your time and ensure you
have addressed all red rows and cells in the sheet to the best of your ability.
Make sure there are no red cells at the top of your sheet before the end of your
shift. It’s not okay to leave a backlog for the next oncoming security shepherd.

Please fill out the [Shepherding Handoff
Log](https://goto.google.com/chrome-security-shepherd-handoff) to communicate
issues from your shift that may be helpful to the oncoming shift.

### Ask for help

Security bug triage is hard. We receive around 75 bug reports per week on
average. **If you are ever stuck or in doubt**, please ask for help from the
[Chrome Security Shepherds chat](https://goto.google.com/chrome-security-shepherds-chat)
or the [Chrome Security Chat](https://goto.google.com/chrome-security-chat).
During some shifts, there are just too many incoming bugs. It’s okay to ask for
help, please do!

You may also like the classic [HOWTO: Be a Security Shepherd deck](https://docs.google.com/presentation/d/1eISJXxyv7dUCGUKk_rvUI9t9s2xb98QY4d_-dZSa7Wg/edit#slide=id.p)

Because shepherding is fun. You like fun. Don't you? Fun is great.

## Secondary Shepherd

### Check in on triaged issues

Review open security bug reports and check that progress is occurring. This does
not apply to the new bug reports as these are handled by the primary shepherd.
The rule of thumb is *if there is any red cell on the dashboard, it needs your
attention*: that especially includes the `last updated` column. Our [severity
guidelines](severity-guidelines.md) contain the expected duration for shipping
fixes, but it’s important to remember that to get a fix to all users in 60 days
or so, this may require us to land a fix in a week or two.

*Suggestions for cultivating progress on security bugs:*
* Don’t just add a comment to the bug as these can disappear into spam (though a
  well-crafted, meaningful, actionable comment can be effective).
* Contact the owner via chat or email in addition to commenting on the bug (so
  others on the bug can see an update is needed).
* cc: more relevant people
* Think about what you can do to unblock the bug. What would _you_ do next?
 Perhaps you bring in a subject matter expert of some aspect of the bug that is
 a particular sticking point or suggest a different approach to reproduce the
 bug. Sometimes a security perspective can help shed light on a different way
 forward.
* Are there old, open `Security_Impact-None` bugs in unlaunched features, where
  the response has been that there are no plans to launch that feature? Perhaps
  inquire as to if that code can be removed rather than keeping vulnerable code
  production code base. (Removing code that is not being used is a win!)
* Consider if it is better for you to make meaningful steps forward on three
  bugs versus simple pings on many bugs.

You can’t possibly usher all bugs toward meaningful progress during your shift.
As a general rule, expect to spend a solid two hours each day  ushering bugs
toward progress during your shift. Use the `last updated` column to avoid
duplicating the work of the previous secondary.

### Handle incoming security emails

Ensure that all incoming inquiries to the [security@chromium.org](https://groups.google.com/a/chromium.org/forum/#!forum/security),
[security-dev@chromium.org](https://groups.google.com/a/chromium.org/forum/#!forum/security-dev),
and
[chrome-security@google.com](https://groups.google.com/a/google.com/forum/#!forum/chrome-security)
lists get a reply (by someone; not necessarily you). See
[go/chrome-security-emails](https://goto.google.com/chrome-security-emails)
for a dashboard.

* When triaging an email to be handled off of the list, make sure to bcc: the
list that it arrived on, so that others (including future secondary shepherds)
can see that it has been handled.
* Some of these emails are requests for the inclusion of third-party code.
By the time you do shift handoff, please ensure these are either completed or
have been acknowledged by some other owner. If not, you may need to do them
yourself.
  * Please see [How to do Chrome Third-Party Security Reviews](https://goto.google.com/how-to-do-chrome-third-party-security-reviews) for tips.

## Other Helpful Info

### What do all these bug labels mean?

[Security Labels](security-labels.md).

### An owner is asking for security input on backporting a security fix.
What do I do here?

You are not responsible for handling merges or approving a fix for backmerge.
If the issue is resolved and there is a landed CL, please ensure the bug is
closed as Fixed. Please also make sure the bug has a severity and FoundIn set.
This will allow the bot (Sheriffbot) to add the appropriately update the Merge
custom field with the appropriate request-MMM or review-MMM labels, where MMM =
the milestones for backmerge consideration (based on rules driven by severity
(and security_impact, derived from Found In). See
[security merge triage](../process/merge_request.md#Security-merge-triage)
for more information.

That issue will be visible to the security merge review queue. There are
designated members of the security team who have the hefty responsibility of
reviewing security issues for backmerge. Merge approvals will be handled by them
after at least the fix has had sufficient bake time on Canary.

### When / how does X happen to a security bug?

(e.g. how and when does a VRP bug get to the Chrome VRP Panel?)
[See Life of a Security Issue](life-of-a-security-issue.md).

### I have questions related to Chrome VRP policy and scope.

[Chrome VRP policies and rewards page](https://g.co/chrome/vrp) and [Chrome VRP
News and FAQs](vrp-faq.md). You can also reach out directly to the Chrome VRP
TL or ask questions in the
[Chrome Security Shepherds chat](http://go/chrome-security-shepherds-chat), all
VRP Panel members are also members of that chat.

### There is PII or other data in a report we do not want to publicly disclose.

For cases of PII, simply delete the attachment or comment that contains PII
within the issue tracker. If PII is contained in the text of the original
description of the report, simply choose the `Edit description` option and
remove any PII.

For cases in which we are just delaying public disclosure (such as when a
security issue impacts other products or vendors), please add the
`SecurityEmbargo` hotlist (hotlistID: 5432549) and set a date in the `Next
Action` field so that disclosure can be re-evaluated at that time.

### Protecting researcher identities

Many researchers report security issues under a pseudonym and from a specific
email address pertaining to that pseudonym. Please do not refer to the
researcher by the email username directly in any comments of the report.
When reports are publicly disclosed, that becomes visible to all and we have to
delete those comments to protect that information. To direct a comment at an
external security researcher, please use “OP”, “reporter”, or "researcher”.

### Deleted Reports / Issues Marked as Spam or Abuse

You may come across some reports in the security bug triage queue with a red
banner, "The issue has been deleted. Reason: ABUSE," this is generally due to
the overactive spam filtering in the issue tracker. Just click `Undelete` in the
right side of the banner, and triage the report as you normally would.

### Shepherding Scheduling

* [Current Shepherds](http://go/whos-the-shepherd)
* [Rotation schedule](https://docs.google.com/spreadsheets/d/10sLYZbi6QfLcXrhO-j5eSc82uc7NKnBz_o1pR9y8h7U/edit#gid=0)
* If you're a Shepherd, you should get a calendar invite.
  Please accept it to acknowledge your upcoming shepherding duty.
* If you need to swap shifts, ask around for a volunteer and then just update
  the [rotation sheet](https://docs.google.com/spreadsheets/d/10sLYZbi6QfLcXrhO-j5eSc82uc7NKnBz_o1pR9y8h7U/edit#gid=0) and wait 10 minutes for the calendar invites to be updated.

### Incident response

Sometimes you’ll need to handle a security emergency, such as a critical
severity bug or bug known or under active exploitation in the wild. In such
cases:
* As soon as possible, reach out to the Shepherds chat for a Chrome Security
  Incident Responder, so they can take on IR Commander responsibilities.
* Sometimes features can be switched off using feature flags – for example
  [in permissions](https://docs.google.com/document/d/17JeYt3c1GgghYoxy4NKJnlxrteAX8F4x-MAzTeXqP4U).  Check with the engineer if that is a possibility in the case of this issue.

That's a lot of stuff! You have this resource and your peers to lean on for
questions and expertise. Hopefully this doc helps.
You're gonna do great!
