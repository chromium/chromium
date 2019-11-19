# Merge Request Process

[TOC]

## tl;dr

* After branch point, all merges will be closely reviewed (either manually or
  automatically) by TPMs and release owners.
* Requirements for approving merges are becoming more
  stringent and scrutiny of merges gets higher as we get closer to the
  stable launch date. Any necessary merges should be requested as early as
  possible.
* All merge candidates should have full automated unit test coverage, have been deployed in Canary
  for at least 24 hours, and full developer confidence that change will be
  safe.
* When requesting merge review, please provide clear explanation for why a merge
  is required, the criticality and impact of the issue, and ensure that
  bug is correctly labeled for all impacted OS's.

## Introduction

Chrome ships across multiple channels which are built from different release
branches. In general, changes should first land on trunk, be shipped and
verified in a canary release, and then promoted to our Dev, Beta, and Stable
channels overtime. However, due to many reasons and scenarios, it’s
possible that changes may miss branch date and require a merge post branch.

**Merge**: any change that is cherry picked from trunk to a release branch.

Please read overview of [Chrome Release
Cycle](https://chromium.googlesource.com/chromium/src.git/+/master/docs/process/release_cycle.md)
to understand in detail how the Chrome release cycle works and understand key
release concepts and terminology. Please read [Defining Release
Blockers](https://chromium.googlesource.com/chromium/src.git/+/master/docs/process/release_blockers.md)
to understand how issues/bugs are categorized as release blocking.
List of schedule and release owners can be found at [Chrome
Calendar](https://chromepmo.appspot.com/calendar) (Googlers only, opening to all in the near future).

## When to Request a Merge?

This section will discuss when and what type of bugs can be merged based on
criticality of the issue. Please note that the scrutiny of merges
gets higher as we get closer to the stable launch date. Merges post
stable-rollout have a higher bar than merges prior to Stable.

![Chrome Merge Schedule](images/chrome_merge_schedule.png)

**Phase 1: First Two Weeks After Branch (two weeks before beta promotion)**

There are bugs and polish fixes that may not necessarily be considered
critical but help with the overall quality of the product. There are also
scenarios where dependent CL’s are missed by hours or days. To accommodate
these scenarios, merges will be considered for all polish bugs, regressions,
and release blocking bugs for the first two weeks after branch point.
Please note that merges will not be accepted for
implementing or enabling brand new features or functionality. Features
should already be complete. Merges will be reviewed manually and
automatically, depending on the type of change.

GRD file changes are allowed only during this phase. If you have a critical
string change needed after this phase, please reach out to release owner or
Chrome TPMs.

**Phase 2: First Four Weeks of Beta Rollout**

During the first four weeks of Beta, merges should only be requested if:

* The bug is considered either release blocking or
  considered a high-impact regression
* The merge is related to a feature which (1) is entirely gated behind
  a flag and (2) does not change user functionality in a substantial way
  (e.g. minor tweaks and metrics code are OK, workflow changes are not)

Security bugs should be consulted with
[chrome-security@google.com](chrome-security@google.com) to
determine criticality. If your issue does not meet the above criteria
but you would still like to consider this merge, please reach out to
release owner or TPMs with a detailed justification.

**Phase 3: Last Two Weeks of Beta and Post Stable**

During the last 2 weeks of Beta and after stable promotion, merges
should only be requested for critical, release blocking issues where the
fix is low complexity.

Security bugs should be consulted with [chrome-security@](chrome-security@google.com)
to determine criticality.

If it is unclear whether the severity of the issue meets the bar for merging
consult with the [TPM](https://chromiumdash.appspot.com/schedule) and your
manager.

This table below provides key dates and phases as an example, for M61 release.

Key Event  | Date
------- | --------
Feature Freeze | June 23rd, 2017
Branch Date | July 20th, 2017
Branch Stabilization Period | July 20th to August 3rd, 2017
Merge Reviews Phase 1 | July 20th to August 3rd, 2017
Beta Rollout | August 3rd to September 12th 2017
Merge Reviews Phase 2 | August 3rd to August 31st 2017
Merge Reviews Phase 3 | August 31st 2017 and post Stable release
Stable Release | September 6th, 2017 + rollout schedule

## Merge Requirements

Before requesting a merge, please ensure following conditions are met:
*   **Full automated unit test coverage:** please add unit tests or
    functional tests before requesting a merge.
*   **Deployed in Canary for at least 24 hours:** change has to
    be tested and verified in Canary or Dev, before requesting a
    merge. Fix should be tested by either test engineering or the
    original reporter of the bug.
*   **Safe Merge:** Need full developer confidence that the
    change will be a safe merge. Safe merge means that your
    change will not introduce new regressions, or break
    anything existing. If you are not confident that your
    merge is fully safe, then reach out to TL or TPMs for
    guidance.

## Merge Request
If the merge review requirements are met (listed in
section above) and your change fits one of the timelines
above, please go ahead and apply the
Merge-Request-[Milestone Number] label on the bug and
please provide clear justification in the bug.

Please provide clear explanation for why a merge is required, what is the
criticality and impact of the issue, and ensure that bug is correctly
labeled for all impacted OS's.

Approved merge requests in Phase 2 and Phase 3 will require a post mortem.

Once Merge is approved, the bug will be marked with
Merge-Approved-[Milestone-Number] label. Please merge
**immediately after**. Please note that if change is not
merged in time after approval, it can be rejected.

If merge is rejected, “Merge-Rejected” label will be
applied. If you think it’s important to consider the
change for merge and does not meet the criteria above,
please reach out to the release owners, TPMs or TLs for
guidance.

## Merge Reviews

The release team has an automated process that helps
with the merge evaluation process. It will enforce many
of the rules listed in sections above. If the rules
above don’t pass, it will either auto-reject or flag
for manual review. Please allow up to 24 hours for the
automated process to take effect.

Manual merge reviews will be performed by release
owners and TPMs.
