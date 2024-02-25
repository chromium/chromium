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
interrupts to your work, especially if they are **High Severity (S1)** or
**Critical Severity (S0)**. However, the expectation is that you handle security
issues within your normal working hours, not after-hours, weeknights, or on
vacation. Everyone shares the responsibility of keeping our users safe!

## 1. Understand why you were Assigned

All incoming security bugs are analyzed and triaged by the current [security
shepherd](shepherd.md). If you have been assigned a security bug, it is because
the shepherd thinks you are the responsible owner for the code in question. The
shepherd assigned you the bug because either:

1. They have verified the bug is valid and the shepherd expects you to fix it
2. There is a technical question that needs to be answered before the bug can be
   fully triaged

In either case, if you are not the correct owner, please suggest a more
appropriate person and re-assign it to that person. Or, if you do not know the
correct owner, remove yourself from the Assignee field so that the bug
re-enters the shepherd’s queue. Setting a component alone will not grant view
access or alert the component owners, so the shepherd's queue is the best
way to ensure the bug is properly triaged.

In the case where the shepherd is asking you technical questions, they will
further triage the bug after considering your responses.

Security bugs are also view-restricted until after the fix is released to users.
It is okay to CC additional people (including yourself if you re-assign the
issue) that can help diagnose and fix the bug.

## 2. Participate in the discussion

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

## 3. Fix the bug

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

## 4. Merge the fix

After the bug has been marked **Fixed**, automation (or a member of the security
team) will request merge to the applicable release branches. Once the merge
questionnaire is posted to the bug, please respond to the questions.

If the merge is approved, it is your responsibility to merge the CL to the
approved branches. The merge approval will show up as the 'Merge' custom field as
"Approved-&lt;Milestone&gt;".

## 5. Think about patterns

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
