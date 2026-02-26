# Security Shepherd

## What is Security Shepherding?
Security Shepherding is a rotation for security bug triage. The Shepherding
rotation is made up of people actively working on security in Chrome. At any
given time there are two shepherds on shift. The shifts are during work hours
only, so you are not "on call" while shepherding.

[TOC]

## Guiding Principles

Your overarching goal is to get security bugs in Chrome fixed, which means
ensuring that valid, actionable reports end up assigned to engineers who can fix
them.

Remember that you are **triaging** bugs. There are usually many bug reports,
far too many for you to actually investigate all of them in detail. You are
deciding how to allocate your own time, and the time of other engineers, in the
way that produces the best security results for users. You are **not** fully
investigating every element of every report.

It makes sense to focus your attention first on:

* Reports from corporate security teams (Google TAG, MSRC, etc) - these are very
  likely to be valid and are usually being exploited in the wild, so triage
  these first of all
* Reports from known reporters who have previously reported valid bugs
* Reports which, if true, would be Critical (S0) or High (S1) Severity
  (arbitrary code execution, memory corruption)

## What Do I Do?

If you are the shepherd, your **PRIMARY DIRECTIVE** is to tackle all the red
cells on the [security bug
dashboard](https://goto.google.com/chrome-security-bugs).  You do that by
filling in missing fields on the bugs and assigning them to engineers who will
fix them.

To actually triage a report, you go through several steps. On a _new_ bug
report:

### Could It Be Valid?

Skim the bug to see if it looks plausible: is this likely to be a real bug,
in Chrome, with security consequences for Chrome users?

1. If it's not a valid report at all (empty submission, spam, etc), WontFix
2. If it's not likely to be a real bug, WontFix
3. If it's not a bug in Chrome, WontFix and direct the reporter elsewhere
4. If it doesn't have security consequences, convert it to Type=Bug and
   remove the visibility restrictions on it

Prefer to quickly WontFix bugs with a reason at this stage. Reporters can and
will open a new issue if they have further information to provide.

If it could plausibly be a security bug, is there enough evidence to convince
you?

1. If it claims code execution, is there an attached PoC?
2. If it claims an ASAN crash, is there an attached ASAN stack trace?
3. If it claims a v8 / d8 crash or sandbox violation, is there an attached
   JS file that hits those conditions?
4. If it's a UI spoof, is there an attached video?

Do not be shy about asking for _clear_ evidence. The burden of proof at this
point is primarily on the reporter - they need to convince you that the bug
exists and could plausibly be exploited. These things aren't enough evidence:

* A PoC that demonstrates "weird behavior" (JS APIs printing strange values,
  arrays containing unexpected values, scary console log messages)
* A unit test that crashes
* A patch against the browser that causes a crash or similar **except** that
  it's okay for a browser bug to be proven via a patch to the renderer
* Repro steps - these are sometimes sufficient to reproduce a crash or a spoof,
  but they don't generally give any info about exploitability

If the PoC looks sketchy, ask the reporter to fix it or minimize it - don't
spend your own time trying to minimize it or read through half a megabyte of
wasm bytecode to figure out what's going on.

At this stage, you should lean towards marking bugs as WontFix if you are in
doubt. Unfortunately, most incoming bug reports are not valid security bugs, and
time spent triaging those reports in detail is time not spent triaging bugs
which _are_ valid. As a rule:

* If the bug is **not probably valid**, WontFix and remove visibility
  restrictions
* If the bug is probably valid but doesn't have security consequences,
  change it to type Bug and remove visibility restrictions
* If the bug is probably valid but you're missing something critical
  (reporter forgot to attach a PoC, forgot to specify build args for a v8 bug,
  etc) or you need the reporter to minimize the PoC, reply asking and mark
  `Needs-Feedback`
* Otherwise, move on to handling special-case bugs

### Handling special-case bugs

* **If the bug is an in-the-wild report**:
    * Start a thread in the Shepherding chat immediately
* If the bug is a v8 bug (including wasm):
    * Assign it to [the current v8 shepherd](https://goto.google.com/current-v8-sheriff)
    * Set it to High Severity (S1)
    * Set the OS field to all platforms we use v8 on (everything except iOS)
    * Set FoundIn to the oldest [active
      branch](https://chromiumdash.appspot.com/branches)
    * Set the component to `Chromium > Blink > JavaScript`
    * You are now done triaging this bug, congratulations!
* If the bug is a privacy bug, rather than a security bug:
    * Add yourself and any other security team members who may need
      ongoing access to CC
    * Change the type from `Vulnerability` to `Privacy Issue`
    * Remove security@ from collaborators
    * Change the Issue Access Level to `Limited Visibility + Googlers`
    * Set the component to something sensible, maybe [Chromium >
      Privacy](https://issues.chromium.org/components/1457231/edit) or
      [Chromium > UI > Settings >
      Privacy](https://issues.chromium.org/components/1457044/edit).
    * You are now done triaging this bug, congratulations!

Now, move on to...

### Assessing Severity and Impact

Have a look at the [severity
guidelines](https://chromium.googlesource.com/chromium/src/+/main/docs/security/severity-guidelines.md),
which contain lots of examples of bugs of different severities and detailed
writeups of the various factors. The severity is based on **your judgment** of
the consequences of exploitation of the bug, not on the reporter's assessment in
the bug report.

If the report requires you to enable a specific feature or pass a specific
command-line argument, and that feature isn't default-enabled **for any Chrome
users**, then add the bug to the `Security_Impact-None` hotlist at this stage,
which exempts it from the usual severity-based fix SLOs. Note that features can
be enabled by Finch studies or origin trials, so don't just base your decision
on the default state of the feature. The [Finch state
dashboard](https://uma.googleplex.com/p/chrome/variations/state) may be helpful.

If you're in doubt about severity, ask for help in the Shepherd chat. This step
benefits a lot from judgment and experience!

Once you've assessed the severity and impact:

* If the bug is Low Severity (S3) or `Security_Impact-None`, move on to
  "Assigning the Bug" - you can ignore FoundIn and OS for these bugs;
* Otherwise, move on to "Assessing FoundIn and OS"

### Assessing FoundIn and OS

At this point, you need the ability to know if a specific OS + version
combination (up to the oldest [active
branch](https://chromiumdash.appspot.com/branches)) is affected by the bug, so
you need to either:

* Know what the root cause was and when it was introduced (a revision number) -
  particularly good reports may include this info, or
* Have [ClusterFuzz](clusterfuzz-for-shepherds.md) do this detection for you,
  for PoCs that work on and are safe to run on ClusterFuzz, or
* Manually reproduce it yourself across OS + version combos to check

In all cases, FoundIn should contain the _oldest_ milestone number which is
still [active](https://chromiumdash.appspot.com/branches) and has the bug. This
should be based on your investigation and the evidence in the bug, **not** on
what versions the reporter reported the bug against - those are often just what
the reporter happens to be testing on.

It's ok if the OS field is a guess. There is no need to manually test every OS +
version combination, but please do remember to set this field.

ClusterFuzz is far quicker than manual reproduction, and will automatically do
bisection and set FoundIn for you, so you should use ClusterFuzz if at all
possible. If you have to manually reproduce a bug instead:

* Assume that test cases may be malicious. You should only reproduce bugs
  on your local machine if you're completely certain that you understand
  100% of the test case. If not, use a disposable virtual machine. If you're
  inside Google, a good way to do this is using
  [Redshell](https://goto.google.com/redshell-for-chrome-shepherds) - or ask the
  reporter for an obviously-not-malicious test case instead!
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

Now, it's time to assign the bug:

### Assign the bug

* Set Component to the area of Chrome that contains the bug
* Set Assignee to someone likely to fix bugs in that area - consult OWNERS
  or `git blame` if in doubt
* Set CCs to everyone else in a relevant OWNERs file and everyone recently
  appearing in blame - people cannot see security issues if not CCed!
* Add a comment on the bug explaining that it's coming from a security shepherd,
  as well as anything else they might need to know about the bug (whether you
  reproed it locally or not, etc)

### Shift handoff

As you work through the queue each day, please manage your time and ensure you
have addressed all red rows and cells in the sheet to the best of your ability.
Do your best to ensure there are no red cells at the top of your sheet before
the end of your shift.

Please fill out the [Shepherding Handoff
Log](https://goto.google.com/chrome-security-shepherd-handoff) to communicate
issues from your shift that may be helpful to the oncoming shift.

### Ask for help

Security bug triage is hard. We receive hundreds of bug reports per week on
average. **If you are ever stuck or in doubt**, please ask for help from the
[Chrome Security Shepherds
chat](https://goto.google.com/chrome-security-shepherds-chat) or the [Chrome
Security Chat](https://goto.google.com/chrome-security-chat). During some
shifts, there are just too many incoming bugs. It’s okay to ask for help, please
do!

You may also like the classic [HOWTO: Be a Security Shepherd deck](https://docs.google.com/presentation/d/1eISJXxyv7dUCGUKk_rvUI9t9s2xb98QY4d_-dZSa7Wg/edit#slide=id.p)

## Other Helpful Info

### Links to Helpful Resources

Here are some of the important references and resources you need or may need
during your shepherding shift:

* [Current Shepherds](https://goto.google.com/whos-the-shepherd)
* [Chrome Security Bug Dashboard](https://goto.google.com/chrome-security-bugs)
* [Security Severity Guidelines](severity-guidelines.md)
* [Shepherding AI Reports](shepherding-ai-reports.md)
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
* [GPU for dummies](https://goto.google.com/gpu-for-dummies)

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
(and `Security_Impact`, derived from FoundIn). See
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
* A calendar invite will be sent for your upcoming shift. Please accept it to
  acknowledge your upcoming shepherding duty.
* If you **cannot make the shift**:
  * Declining the invite does not alert anyone or trigger any re-assignment.
  * If you are OOO or the assigned shift is during a holiday, please do your
    best to [swap shifts](https://goto.google.com/swap) with someone! You are
    not expected to shepherd on a holiday (: but we do want to maximize
    coverage where/when possible.
  * Ask around (shepherding chat is a good place!) for a volunteer and then
  update the [rotation sheet](https://docs.google.com/spreadsheets/d/10sLYZbi6QfLcXrhO-j5eSc82uc7NKnBz_o1pR9y8h7U/edit#gid=0).
  The calendar invites should be updated in ~10 minutes.
* To become a shepherd, please reach out to the Chrome Product Security team.
* To stop shepherding, remove yourself from g/chrome-security-shepherds. Please
  find suitable substitutes for shifts that have already been assigned to you.
  Automation should take care of the rest.
