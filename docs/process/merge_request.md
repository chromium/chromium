# Merge Request Process

[TOC]

## tl;dr

* Release managers (and delegates like the security team) must review all
  merges made to release branches
* Merge criteria become more strict as the stable release date approaches; use
  Chromium Dash's [Branches page](https://chromiumdash.appspot.com/branches) to
  understand which branches are active and what merges are acceptable for each
  branch
* Ensure your change is [safe to merge](#verifying-eligibility-and-safety)
  before initiating the merge review process unless it's time-sensitive
* Use Monorail's [project queries](#monitoring-merge-requests) to track your
  approved merges as well as your pending requests
* Use Gerrit or git to land your merge only after it's been approved

## Introduction

Chromium is a main-first development team; generally, all code should land on
main then roll out to stable users only after the milestone containing the code
is branched, stabilized and shipped to the stable channel (to learn more about
the release cycle, click
[here](https://chromium.googlesource.com/chromium/src.git/+/main/docs/process/release_cycle.md)).
This is because merging (also known as cherry-picking) code to an older release
branch introduces risk and costs time across the team. However, there are times
when the benefits outweigh the costs and a merge might be appropriate, e.g. to
fix a web platform regression, address a crash or patch a security vulnerability.

To ensure we make the right decisions, release managers leverage a merge review
process to evaluate each request. They'll ask questions about the reason you
would like to merge a change and the risk of the merge itself, and you'll work
together to make a judgement call on whether or not the merge should be
approved or rejected.

Generally, merges follow these high-level steps:

* Developers update bug with relevant details and request a merge using the
  *Merge-Request-##* label, then wait for review
* Release managers and automation review and approve, reject, or ask
  questions about the merge within two business days
* Developers wait for review and, if approved, land the merge ASAP

For details on each step, see below.

**NOTE:** Because security issues (identified with *Type=Bug-Security*) follow
a more complex flow, you may simply mark security issues as *Fixed* in Monorail
and [automation](#security-merge-triage) will handle the remainder of the merge
request process flow for you; simply process the merge if it is requested and
approved.

## Requesting a merge

### Verifying eligibility and safety

Before requesting a merge, first ensure your change is a good merge candidate:

* Ensure it meets the merge criteria (via
  [Chromium Dash](https://chromiumdash.appspot.com/branches)) of the branch(es)
  you'd like to merge to; merge criteria become more strict the older the
  branch is, more details on criteria [below](#merge-criteria-phases)
* Verify merging the change to an older branch would be safe, e.g. unlikely to
  introduce new regressions, no major merge conflicts, automated test coverage
  present, etc; chat with your TL for input if you're not sure
* Confirm your change fixes the issue at hand, preferably by testing on and
  monitoring the canary channel for 24 hours post-release (see
  [Chromium Dash](https://chromiumdash.appspot.com/commits) to determine if
  your change has shipped)

  * You may skip this step if a release manager or security team member has
    told you that the merge is urgent, e.g. is actively blocking a release

### Updating crbug/

Next, ensure you have a crbug/ (generally the bug being fixed by the merge)
with the following information present and accurate:

* Title and description clearly describing the bug being fixed
* Priority (*Pri-#*), OS (*OS-OS*) and target milestone(s) (*Target-##*)
* Owner, generally the person requesting / performing the merge
* [Release block label](./release_blockers.md) if applicable
  (*ReleaseBlock=Channel*)
* Issue status:

  * Fixed: You're confident the issue is fixed on main, e.g. you've
    locally built and tested the issue, no additional crash reports are
    generated after the fix was released, etc (most issues)
  * Assigned / Started: Diagnostic merges only, e.g. to merge code to track
    down the root cause of an issue that only exists on branch

### Applying merge request label

Once you've verified all the above, you're ready to request a merge! Simply add
the label *Merge-Request-##* to the issue (where ## indicates the milestone
you'd like to merge to), and use multiple labels for multiple milestones, e.g.
*Merge-Request-91 Merge-Request-92* for M91 and M92. Please also copy the
following questions and answer them in a comment on the issue:

1. Why does your merge fit within the merge criteria for these milestones
  ([Chrome Browser](https://chromiumdash.appspot.com/branches),
  [Chrome OS](https://goto.google.com/cros-release-branch-merge-guidelines))?
2. What changes specifically would you like to merge? Please link to Gerrit.
3. Have the changes been released and tested on canary?
4. Is this a new feature? If yes, is it behind a Finch flag and are
  experiments active in any release channels?
5. [Chrome OS only]: Was the change reviewed and approved by the
  [Eng Prod Representative](https://goto.google.com/cros-engprodcomponents)?
6. If this merge addresses a major issue in the stable channel, does it require
   manual verification by the test team? If so, please describe required
   testing.

## Monitoring merge requests

After you've applied the *Merge-Request-##* label, automation will evaluate
your request and may either approve it, reject it, or pass it along to a
release manager for manual evaluation; see [here](#merge-request-triage) to
learn more about this automation. If manual review is required, release
managers strive to answer all merge requests within two business days, but
extenuating circumstances may cause delays.

At this point, following along via bug comments sent by email will always keep
you in the loop, but you can also use the following saved project queries in
Monorail (dropdown to the left of the search bar) to track your merges:

* [Approved and TBD merges](https://bugs.chromium.org/p/chromium/issues/list?q=owner%3Ame&can=41025836):
  Merges that require your follow-up, either by landing the relevant
  merge (if approved) or determining whether or not a merge is actually
  required and if so, requesting it (if TBD)
* [Requested merges](https://bugs.chromium.org/p/chromium/issues/list?q=owner%3Ame&can=41025837):
  Merges that are waiting for input from release managers or automation; feel
  free to ping bugs that sit in this queue for two business days (assuming you
  verified that the change was already deployed to canary ahead of requesting a
  merge)
* [Rejected and NA merges](https://bugs.chromium.org/p/chromium/issues/list?q=owner%3Ame&can=41025838):
  Merges that were either rejected by release managers, or not applicable to be
  merged; generally, no action is needed for these items unless you disagree
  with a merge's rejection and wish to escalate
* [All merges](https://bugs.chromium.org/p/chromium/issues/list?q=owner%3Ame&can=41025839):
  Includes every possible merge state, useful when wanting to find an item you
  considered for merging but can't recall the state it was last in.

For a description of each label used to track the merge process, see the
appendix [below](#merge-states-and-labels).

## Landing an approved merge

Once your merge has been approved for a given milestone (via the release
manager or automation applying the *Merge-Approved-##* label), you have two
options to land the merge:

* Gerrit UI, easiest for clean cherry-picks or those requiring only minor
  changes
* git, for more complex cherry-picks and / or when local verification may be
  beneficial

Regardless of which method you choose, please ensure you land your cherry-pick
ASAP so that it can be included in the next release built from the branch; if
you don't merge your cherry-pick soon after approval, it will eventually be
rejected for merge.

Once the cherry-pick has landed a bot will apply the *merge-merged-##* label if
the commit references the issue. If for some reason the commit did not
reference the issue, add the *Merge-Merged-##* label to the issue. After the
merge is completed the *Merge-Approved-##* label should be manually removed.

### Using Gerrit UI

Select the "..." button in the Gerrit UI, then choose "Cherry Pick". When
prompted for a branch, enter *refs/branch-heads/####*, where #### corresponds
to the release branch you are merging to (available on
[Chromium Dash](https://chromiumdash.appspot.com/branches) in the "Chromium"
column).

Once the cherry-pick CL is prepared, you can bypass code review (but not
OWNERS approval) within 14 days of the original change by adding the Rubber
Stamper bot (rubber-stamper@appspot.gserviceaccount.com) as a reviewer. If the
CL meets the
[Rubber Stamper criteria](https://chromium.googlesource.com/infra/infra/+/refs/heads/main/go/src/infra/appengine/rubber-stamper/README.md),
the bot will vote *Bot-Commit+1* to bypass code review. If the CL is marked
*Auto-Submit+1*, the bot will also submit the CL to the CQ on your behalf.

### Using git

The commands below should set up your environment to be able to successfully
upload a cherry-pick to a release branch, where *####* corresponds to the
release branch you are merging to (available on
[Chromium Dash](https://chromiumdash.appspot.com/branches) in the "Chromium"
column):

```
$ gclient sync --with_branch_heads
$ git fetch
$ git checkout -b BRANCH_NAME refs/remotes/branch-heads/####
$ git cl upstream branch-heads/####
$ git cherry-pick -x COMMIT_HASH_MAIN
$ gclient sync
```

From here, your environment should be ready to adjust the change as required;
use ninja to build and test your changes, and when ready upload for review:

```
$ git cl upload
```

**Adjust the change description** to omit the "Change-Id: ..." line from
original patch, otherwise you may experience issues when uploading the change
to Gerrit. Once complete, use Gerrit to initiate review and approval of the
merge as TBR has been discontinued.

Other tips & tricks when merging with git via release branches:
* Consider using multiple working directories when creating the release branch
* Editing the change description to denote this is a merge (e.g. "Merge to
  release branch" at the top) will help reviewers distinguish between the
  cherry-pick and the original change

## Merge automation

The release team has built automation via
[Sheriffbot](https://www.chromium.org/issue-tracking/autotriage) to assist in
several merge flows: security merge triage, general merge request triage, and
preventing missed merges.

### Security merge triage

Given the additional complexity inherent in security merges, the security team
has built custom automation to handle this flow end to end; simply mark any
security issue as *Fixed* and Sheriffbot will evaluate applicable milestones,
determine if merges are required and automatically request them if need be.

### Merge request triage

To reduce release manager toil, Sheriffbot performs the first pass review of
all merge requests; it may auto-approve the issue if it can detect the issue
meets the right criteria for the current merge phase (e.g. a ReleaseBlock-Dev
issue requesting a merge before beta promotion), and it may auto-reject the
issue similarly (e.g. a Pri-3 issue requesting a merge post-stable). If it
cannot decide, it will pass the issue to a release manager for manual review.

Generally, Sheriffbot takes action on merge requests only after one of the two
conditions below are met:

* One or more changelists (via Gitwatcher) are present on the merge request
  issue, and all changes have been landed for >= 24 hours
* No changelists are present on the merge request issue, and the merge request
  label has been applied for >= 24 hours

These conditions help ensure any relevant changelists have had sufficient
runtime in our canary channel and thus are low risk for introducing a new
regression onto our release branch.

### Preventing missed merges

To avoid the situation where a critical issue is present on a release branch
but the fix isn't merged, Sheriffbot evaluates all release-blocking issues
targeting a milestone that has already branched and adds a *Merge-TBD-##* label
if the issue was marked as fixed after branch day but hasn't been merged.
When this occurs, developers should evaluate the issue and either request a
merge if required (e.g. the fix did miss the release branch point) by adding
the *Merge-Request-##* label, or add the *Merge-NA-##* label if not (e.g. the
fix is present in the release branch already or the merge is unnecessary for
other reasons).

## Appendix

### Merge criteria phases

The table below describes the different phases that each milestone progresses
through during its release cycle; this data is available via the
Chromium Dash [front-end](https://chromiumdash.appspot.com/branches) and
[API](https://chromiumdash.appspot.com/fetch_milestones).

| Branch Phase             | Period Begins   | Period Ends     | Acceptable Merges Include Fixes For:                                                                                                                                                     |
|--------------------------|-----------------|-----------------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| branch                   | M(X) Branch     | M(X) Beta       | Polish issues for Finch-gated features (no workflow changes), any new regressions, any release blockers, any security issues, any string issues (.GRD changes)                           |
| beta                     | M(X) Beta       | M(X) Stable Cut | Non-functional issues for Finch-gated features (e.g. add metrics, fix crash), noticeable new regressions, any release blockers, any security issues, urgent string issues (.GRD changes) |
| stable_cut               | M(X) Stable Cut | M(X) Stable     | Urgent new regressions, all release blockers, important security issues (medium severity or higher), emergency string issues (.GRD changes)                                              |
| stable                   | M(X) Stable     | M(X+1) Stable   | Urgent new regressions (especially user reports), urgent release blockers, important security issues (medium severity or higher) requested by the security team                          |
| extended (if applicable) | M(X+1) Stable   | M(X+2) Stable   | Important security issues (medium severity or higher) applicable to any platform supported by Chrome Browser requested by the security team                                              |

### Merge states and labels

The table below describes the different merge states and labels used to track
them. All labels follow the form *Merge-[State]-##*, where ## corresponds to
the applicable milestone. If multiple merges are required, these labels may
appear multiple times on the same bug in different states (e.g. a merge request
could have both *Merge-Approved-92* and *Merge-Rejected-91* at the same time).

| Label / State | Step Owner       | Next Steps                                                                                                                                                                    |
|---------------|------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Request       | Release manager  | Automation will review and either approve / reject directly, or pass the review to a release manager for manual evaluation                                                    |
| Review        | Release manager  | Release manager will evaluate and either approve, reject, or request additional information within two business days                                                          |
| Approved      | Issue owner      | Issue owner should cherry-pick the fix to the appropriate release branch ASAP                                                                                                 |
| Merged        | None             | N/A; merge has already been landed, no further work required for given milestone                                                                                              |
| Rejected      | Issue owner      | Issue owner should re-add *Merge-Request-##* to escalate if they feel the merge was erroneously rejected and should be re-evaluated                                           |
| TBD           | Issue owner      | Issue owner should evaluate if a merge is required, then remove *Merge-TBD-##* and replace it with *Merge-NA-##* (if no merge needed) or *Merge-Request-##* (if merge needed) |
| NA            | None             | N/A; merge is not required to the relevant milestone, no further work required for given milestone                                                                            |
