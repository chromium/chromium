# You’ve just been assigned a security bug…

If you have just been assigned a security bug, **don’t panic!** Security bugs
are a fact of life in Chromium, and the project has a team of people and robust
processes to help analyze and get security issues fixed. This document is meant
to help Chromium developers handle their first (few) security bug(s). You may
also want to review the [Life of a Security Issue](life-of-a-security-issue.md)
to understand how you as a developer fit into the larger security bug life
cycle.

Chromium has [public commitments](severity-guidelines.md) to fix security issues
within certain timeframes. Please treat security issues as high-priority
interrupts to your work, especially if they are **High Severity (S1)** (P1) or
**Critical Severity (S0)** (P0). However, the expectation is that you handle
security issues within your normal working hours, not after-hours, weeknights,
or on vacation. Everyone shares the responsibility of keeping our users safe!

## Validate then fix the issue

### Validate the issue before working on a fix

Before working on a security issue you can require that it meets our [reporting
guidelines](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/security/vrp-faq.md#Best-Practices-for-Security-Bug-Reporting).

The report must:

* Include a self-contained reproduction case (or video*) attached to the report
* Attach a complete, fully symbolized, ASAN or MTE stack trace
* Be short and concise
* Attach patches that simulate a compomised renderer as a `.diff` file

The report must not:

* Only provide patched browser or unittests
* Run Chrome in a python or `.sh` harness, or drive Chrome with CDP
* Patch non-renderer code
* Host reproduction cases on external urls

If the report does not meet these basic criteria you are *encouraged* to close
it as `Not Reproducible`.

### Close the issue if it is not actionable

If the issue does not meet Chrome security reporting guidelines you should close
it and request the reporter resubmit it after making improvements. You do not
have to dig through a poorly written report, or search for commandline arguments
in a shell script. The information you need must be present and easy to find,
if it is not, you should close the issue as WontFix - Not Reproducible. Use the
following template if it is helpful:

```markdown
This report does not include the information we require to reproduce and
investigate a security bug and will be closed.

If you can provide:
  * [.. what is missing ..]

please open a new report that follows our reporting guidelines.

https://chromium.googlesource.com/chromium/src/+/HEAD/docs/security/vrp-faq.md#Best-Practices-for-Security-Bug-Reporting.
```

### Reproduce the issue

If the report, on the surface, is valid, move on to reproduce the issue, or read
the traces from the reproduction robots. If it is a UI issue, try the steps the
reporter suggested. Upload your own traces and commandlines to the issue to help
future developers test and validate your work on the issue. This will help you
test that the issue has been fixed by your CLs later.

### Participate in the discussion on the issue

Some bugs involve discussion with the reporter and/or members of the security
team. For example, the issue may be in a feature or system that the shepherd is
not well-equipped to reproduce, and they may ask you for help in determining if
the bug is valid. The shepherd may also try to determine if the bug is mitigated,
meaning that the security impact is smaller or greater than described by the
reporter. As the developer, you may have questions about certain preconditions
assumed by the reporter. We encourage you to interact with the reporter and the
shepherd, directly in the issue tracker, as much as you need in order to identify
and fix the issue.

Please do _not_ adjust any of the [security metadata](security-labels.md) on the
bug (namely the **Severity** field and **Security\_Impact** hotlists). If you think a
bug is not a security issue or its severity should be downgraded, discuss it with
the security team and let them adjust the metadata. However, you can adjust the
**Found In** field if you know the versions a bug affects.

### Fix the bug

This is the normal part of the job! Write a fix and a regression test, upload
the CL, and get it reviewed by the appropriate code owner. The shepherd who
assigned you the bug does not need to be included on the CL. Once the CL has
landed, please [_immediately_ mark the bug as
**Fixed**](https://groups.google.com/a/chromium.org/g/chromium-dev/c/JNJdU-dnjTk/m/4jXI96pdAgAJ).
That status change will kick off the security team’s automation to ensure the
fix is released to users in a timely fashion.

A word on CL descriptions: Do not hide or obscure the fact that the CL is fixing
a security bug; it is okay to mention that the CL fixes a use-after-free.
However, the best CL description isn’t “[component] Fix uaf” – it is better to
describe _what_ lifetimes are being corrected, as well as the faulty underlying
assumption that led to the bug. As an example, [this
CL](https://chromium-review.googlesource.com/c/chromium/src/+/2167426) fixes a
use-after-free and describes the lifetime issue and change.

### Merge the fix

After the bug has been marked **Fixed**, automation (or a member of the security
team) will request merge to the applicable release branches using child bugs of
the initial issue. Please respond to these quickly and merge to release branches
to ensure that fixes reach people using Chrome as fast as possible.

### Think about patterns

After the reported bug has been fixed and possibly merged, consider if the same
bug may exist in other places. For example:

* If you fixed one instance of using `base::Unretained` in an unsafe manner,
  check the surrounding code for other usages that may be unsafe.
* If you converted an incorrect `DCHECK` to an early return or `CHECK`, look for
  similar incorrect `DCHECKs`.
* If there was an integer overflow, look at other arithmetic operations and
  consider using base/numerics/.

## Summary

**Do:**

* CC additional subject-matter-experts to the bug
* Have a productive discussion in the bug issue comments
* Fix the bug as quickly as you can in your normal working hours
* Set the bug’s status to **Fixed** as soon as the CL lands
* Merge the CLs to the appropriate branches after receiving merge approval
* See also our [top security things checklist](checklist.md)

**Don’t:**

* Panic
* Communicate with the reporter about the issue outside of the bug tracker
* Adjust the [security labels](security-labels.md) like the **Severity** field
  or **Security\_Impact** hotlists.

## FAQs

### I need some help from the Security team?

If you need help from the security team please ask your question on the issue
and add the [Chrome Security Klaxon](https://issues.chromium.org/hotlists/8699168)
hotlistid:8699168 for our oncall to see and respond to. Please do not assign to
security@ or individual security team members.

If you'd prefer to ask on chat: Googlers can join go/chrome-security-questions
and all project members can join #security on the Chromium Slack.

### Why was I assigned? Can I send to it someone else?

Incoming security bugs are analyzed and triaged by a robot (the Chrome Security
Sheepdog) based on OWNERS, recent commits and OOO status when available. Your
first responsibility is to ensure you are the right assignee, or if the issue
should properly be handled by someone else. Please quickly take a look at the
issue and make this determination. If you are the wrong person, please CC folks
that should be involved, and assign to one of them if the code and stacks on the
bug point towards a better owner.

Try not to simply unassign yourself as this will produce work for another human,
but if you have no idea (or are leaving the project) you can unassign yourself
and the bug will enter the shepherd’s queue. Remember that people cannot see
security issues unless they are CC'd in. (Feel free to CC people in!)

Critical, High and Medium severity security issues must be assigned to someone,
this is Chromium policy, so if you feel you are the wrong assignee try not to
simply remove yourself, instead work to find an alternative assignee.

### My team has a triage rotation or oncall can we assign NEW security issues ourselves?

In general: no. Chromium is too complex to have different security processes for
every component or directory. Managing security SLOs requires issues have owners
and we cannot relax that for every team.

The robots can use an oncall or rotation tool to preferentially assign new
security issues in a component to the current oncaller. Googlers can contact the
product security team (go/sheepdog-feedback) to set this up for your component.

Large teams with a proven record of quickly fixing security issues may implement
a [delegated triage](delegated-triage.md) process with the agreement of the
Chrome security team.

### We cannot work on the issue immediately, can we mark it as NEW to mark it as available?

Security issues must be owned by a person who is responsible for fixing or
tracking updates to the issue, and Chrome's processes for enforcing SLOs are
based on issues being assigned to people. For example, the issue reporter might
provide new information and this could be missed if the issue is not assigned.
Security issues with Low (S3) severity do not have SLOs and may be unassigned.
