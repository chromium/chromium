# Contributing to Chromium

This page assumes a working Chromium [checkout and build][checkout-and-build].
Note that a full Chromium checkout includes external repositories with their
own workflows for contributing, such as [v8][v8-dev-guide] and
[Skia][skia-dev-guide]. Similarly, ChromiumOS, which includes Chromium as a
subrepository, has its own [development workflow][cros-dev-guide].

[TOC]

## Related resources

- [Life of a Chromium Developer][life-of-a-chromium-developer], which is mostly
  up-to-date.
- [Tutorial][noms-tutorial] by committer emeritus noms@chromium.org.
- [Commit Checklist][commit-checklist], a useful checklist to go through before
  submitting each CL on Gerrit.

## Communicate

When writing a new feature or fixing an existing bug, get a second opinion
before going too far. If it's a new feature idea, propose it to the appropriate
[discussion group][discussion-groups]. If it's in the existing code base, talk
to some of the folks in the "OWNERS" file (see [code review
policies][code-reviews] for more) for the code being changed.

- If a change needs further context outside the CL, it should be tracked in the
  [bug system][crbug]. Bugs are the right place for long histories, discussion
  and debate, attaching screenshots, and linking to other associated bugs. Bugs
  are unnecessary for changes isolated enough to need none of these.
- If there isn't a bug and there should be one, please [file a new
  bug][crbug-new].
- Just because there is a bug in the bug system doesn't necessarily mean that a
  patch will be accepted.

## Design Documents
Any nontrivial technical effort that will significantly impact Chromium should
have a design doc ([template][design-doc-template]). Specifically, we require
design docs in the following cases:
- When writing code that will have a large impact on Chromium as a whole, e.g.
  when you are changing code in Chromium's critical path (page loading,
  rendering).
- When beginning a large technical undertaking that should be documented for
  historical reasons (>1 person-month of work can be used as a general guideline).

Send public design docs to
[chromium-design-docs@chromium.org][chromium-design-docs]. Google internal Chrome
design docs should follow the process at
[go/chrome-dd-review-process][chrome-dd-review-process].

## Legal stuff

All contributors must have valid Gerrit/Google accounts (which means you must
be [old enough to manage your own
account](https://support.google.com/accounts/answer/1350409)) and complete the
contributor license agreement.

For individual contributors, please complete the [Individual Contributor License
Agreement][individual-cla] online. Corporate contributors must fill out the
[Corporate Contributor License Agreement][corporate-cla] and send it to us as
described on that page.

### First-time contributors

Add your (or your organization's) name and contact info to the AUTHORS file for
[Chromium][cr-authors] or [Chromium OS][cros-authors]. Please include this as
part of your first patch and not as a separate standalone patch.

### External contributor checklist for reviewers

Before LGTMing a change from a non-chromium.org address, ensure that the
contribution can be accepted:

- Definition: The "author" is the email address that owns the code review
  request on <https://chromium-review.googlesource.com>
- Ensure the author is already listed in [AUTHORS][cr-authors]. In some cases, the
  author's company might have a wildcard rule (e.g. \*@google.com).
- If the author or their company is not listed, the CL should include a new
  AUTHORS entry.
  - Ensure the new entry is reviewed by a reviewer who works for Google.
  - Contributor License Agreement can be verified by Googlers at http://go/cla.
  - If there is a corporate CLA for the author's company, it must list the
    person explicitly (or the list of authorized contributors must say
    something like "All employees"). If the author is not on their company's
    roster, do not accept the change.

## Initial git setup

1. Set up [Gerrit access](https://www.chromium.org/developers/gerrit-guide/).
2. Tell git about your name, email and some other settings.
   ```
   git config --global user.name "My Name"
   git config --global user.email "myemail@chromium.org"
   git config --global core.autocrlf false
   git config --global core.filemode false
   git config --local gerrit.host true
   # Uncomment this if you want your pull commands to always rebase.
   # git config --global branch.autosetuprebase always
   # Uncomment if you want new branches to track the current branch.
   # git config --global branch.autosetupmerge always
   ```
3. Visit <https://chromium-review.googlesource.com/settings/> to ensure that
   your preferred email is set to the same one you use in your git
   configuration.

## Creating a change

First, create a new branch for your change in git. Here, we create a branch
called `mychange` (use whatever name you want here), with `origin/main` as
the upstream branch.

```
git checkout -b mychange -t origin/main
```

Write and test your change.

- Conform to the [style guide][cr-styleguide].
- Include tests.
- Patches should be a reasonable size to review. Review time often increases
  exponentially with patch size.

Commit your change locally in git:

```
git commit -a
```

If you are not familiar with `git`, GitHub's [resources to learn
git][github-tutorial] is useful for the basics. However, keep in mind that the
Chromium workflow is not the same as the GitHub pull request workflow.

## Uploading a change for review

Note: If your change is to a dependent project, see the documentation on
[changing dependencies](dependencies.md#changing-dependencies). Otherwise, go
through the [commit checklist][commit-checklist] for Chromium before uploading a
change for review.

Chromium uses a Gerrit instance hosted at
<https://chromium-review.googlesource.com> for code reviews. In order to upload
your local change to Gerrit, use `git-cl` from
[depot\_tools][depot-tools-setup] to create a new Gerrit change, based on the
diff between the current branch and its upstream branch:

```
git cl upload
```

This will open a text editor to create a description for the new change. This
description will be used as the commit message when the change is landed in the
Chromium tree. Descriptions should be formatted as follows:

```
Summary of change (one line)

Longer description of change addressing as appropriate: why the change
is made, context if it is part of many changes, description of previous
behavior and newly introduced differences, etc.

Long lines should be wrapped to 72 columns for easier log message
viewing in terminals.

Bug: 123456
```

A short subject and a blank line after the subject are crucial: `git` uses this
as a heuristic for tools like `git log --oneline`. Use the bug number from the
[issue tracker][crbug] (see more on [CL footer syntax](#cl-footer-reference)).
Also see [How to Write a Git Commit Message][good-git-commit-message], which
has more in-depth tips for writing a good commit description.

### Chromium-specific description tips

- Links to previous CLs should be formatted as `https://crrev.com/c/NUMBER`,
  which is slightly shorter than <https://chromium-review.googlesource.com>.

- If there are instructions for testers to verify the change is correct,
  include them with the `Test:` tag:

  ```
  Test: Load example.com/page.html and click the foo-button; see
  crbug.com/123456 for more details.
  ```

After saving the change description, `git-cl` runs some presubmit scripts to
check for common errors. If everything passes, `git-cl` will print something
like this:

```
remote: SUCCESS
remote:
remote: New Changes:
remote:   https://chromium-review.googlesource.com/c/chromium/src/+/1485699 Use base::TimeDelta::FromTimeSpec helper in more places. [WIP]
```

Additional flags can be used to specify reviewers, bugs fixed by the change, et
cetera:

```
git cl upload -r foo@example.com,bar@example.com -b 123456
```

See `git cl help upload` for a full list of flags.

### Uploading dependent changes

If you wish to work on multiple related changes without waiting for
them to land, you can do so in Gerrit using dependent changes.

To put this into an example, let‘s say you have a commit for feature A
and this is in the process of being reviewed on Gerrit.  Now let’s say
you want to start more work based on it before it lands on main.

```
git checkout featureA
git checkout -b featureB
git branch --set-upstream-to featureA
# ... edit some files
# ... git add ...
git commit
git cl upload
```

In Gerrit, there would then be a “relation chain” shown where the
feature A change is the parent of the feature B change.  If A
introduces a new file which B changes, the review for B will only show
the diff from A.

## Code review

Code reviews are covered in more detail on the [code review
policies][code-reviews] page.

### Finding a reviewer

Please note here that a "reviewer" in this context is someone that not
only provides comment on the CL but also someone who can approve the
submission by providing a "Code-Review +1".

Reviewers must be [committers](https://www.chromium.org/getting-involved/become-a-committer/).
Ideally they should be committers who are familiar with the area of code
in question. If you're not sure who these should be, check with anyone in
the nearest ancestor OWNERS file.

- There must be at least one owner for each affected directory.
- If there are multiple reviewers, make it clear what each reviewer is
expected to review.
- `git cl owners` automatically suggests reviewers based on the OWNERS
files.

_Note:_ By default, please only select one reviewer for each file (that is, a
single reviewer may review multiple files, but typically each file only needs
to be reviewed by one person). It can be tempting to add multiple reviewers so
that "whoever gets to it first" can review, but this has two common failure
modes:
- Reviewer Alpha and Beta both review the CL, resulting in duplicate effort.
- Out of fear of the above failure case, neither reviewer Alpha nor Beta review
  the CL.

There are times when requesting multiple reviewers for the same file may be
desirable - such as when the code is particularly complicated, or when the file
uses multiple systems and a perspective from each is valuable. In this case,
please make it explicit that you would like both reviewers to review.

Submissions to the chromium/src repository by a change contributor who is
not a Chromium committer will require two committers to "Code-Review +1" the
submissions. If the owner of the CL is already a committer, then only one
other committer is needed to "Code-Review +1".

### Requesting review

Open the change on [the web][crrev]. If you can't find the link, running `git
cl issue` will display the review URL for the current branch. Alternatively,
visit <https://chromium-review.googlesource.com> and look in the "Outgoing
Reviews" section.

Reviewers expect to review code that compiles and passes tests. If you have
access, now is a good time to run your change through the [automated
tests](#running-automated-tests).

Click **Add Reviewers** in the left column (if you don't see this link, make
sure you are logged in). In the **Reviewers** field, enter a comma-separated
list of the reviewers you picked.

In the same dialog, you can include an optional message to your reviewers. This
space can be used for specific questions or instructions. Once you're done,
make sure to click **Start Review**, which notifies the requested reviewers that
they should review your change.

**IMPORTANT: UNTIL YOU SEND THE REVIEW REQUEST, NO ONE WILL LOOK AT THE REVIEW**

### Review process

All changes must be reviewed (see [code review policies][code-reviews]).

You should get a response within **one** business day; re-ping your reviewers
if you do not.

To upload new patch sets that address comments from the reviewers, simply
commit more changes to your local branch and run `git cl upload` again.

### Approval

When the reviewer is happy with the change, they will set the "Code-Review +1"
label. Owners of all affected files must approve before a change can be
committed. See: [code review policies: owners][code-reviews-owners].

All code review comments must be marked resolved before a CL can be committed.
In some cases a reviewer may give "Code-Review +1" with some additional
comments. These should be addressed and responded to, or at least acknowledged
with the ACK button to resolve them. If you cannot resolve all comments an
override is provided through an "Unresolved-Comment-Reason:" stanza in your
commit message.

## Running automated tests

Before being submitted, a change must pass the commit queue (CQ). The commit
queue is an automated system which sends a patch to multiple try bots running
different platforms: each try bot compiles Chromium with the patch and ensures
the tests still pass on that platform.

To trigger this process, click **CQ Dry Run** in the upper right corner of the
code review tool. Note that this is equivalent to setting the "Commit-Queue +1"
label. Anyone can set this label; however, the CQ will not process the patch
unless the person setting the label has [try job access][try-job-access].

If you don't have try job access and:

- you have an @chromium.org email address, request access for yourself.
- you have contributed a few patches, ask a reviewer to nominate you for access.
- neither of the above is true, request that a reviewer run try jobs for you in
  the code review request message.

The status of the latest try job for a given patchset is visible just below the
list of changed files. Each bot has its own bubble, using one of the following
colors to indicate its status:

- Gray: the bot has not started processing the patch yet.
- Yellow: the run is in progress. Check back later!
- Purple: the trybot encountered an exception while processing the patch.
  Usually, this is not the fault of the patch. Try clicking **CQ Dry Run**
  again.
- Red: tests failed. Click on the failed bot to see what tests failed and why.
- Green: the run passed!

## Committing

Changes are committed via the [commit queue][commit-queue].
This is done by clicking **Submit to CQ** in the upper right corner, or setting
the "Commit-Queue +2" label on the change. The commit queue will then
send the patch to the try bots. If all try bots return green, the change will
automatically be committed. Yay!

Sometimes a test might be flaky. If you have an isolated failure that appears
unrelated to your change, try sending the change to the commit queue again.

In emergencies, a developer with commit access can [directly
commit][direct-commit] a change, bypassing the commit queue and all safety nets.

## Relanding a change

Occasionally changes that pass the [commit queue][commit-queue] and get
submitted into Chromium will later be reverted. If this happens to your change,
don't be discouraged! This can be a common part of the Chromium development
cycle and happens for a variety of reasons, including a conflict with an
unanticipated change or tests not covered on the commit queue.

If this happens to your change, you're encouraged to pursue a reland. When doing
so, following these basic steps can streamline the re-review process:
- **Create the reland**: Click the `CREATE RELAND` button on the original change
  in Gerrit. This will create a new change whose diff is identical to the
  original, but has a small paper-trail in the commit message that leads back to
  the original. This can be useful for sheriffs when debugging regressions.
- **Append the fix**: If the reland requires file modifications not present in
  the original change, simply upload these fixes in a subsequent patchset to the
  reland change. By comparing the first patchset with the latest, this gives
  reviewers the ability to see the diff of _just_ the reland fix.
- **Describe the fix**: In the commit message of the reland change, briefly
  summarize what's changed that makes relanding again safe. Explanations can
  include: "included needed fix", "disabled failing tests", "crash was fixed
  elsewhere". Specifically for that last case: if the reland change is identical
  to the original and the reland fix was handled separately in a preceding
  change, make sure to link to that change in the commit message of the reland.

## Code guidelines

In addition to the adhering to the [styleguide][cr-styleguide], the following
general rules of thumb can be helpful in navigating how to structure changes:

- **Code in the Chromium project should be in service of other code in the
  Chromium project.** This is important so developers can understand the
  constraints informing a design decision. Those constraints should be apparent
  from the scope of code within the boundary of the project and its various
  repositories. In general, for each line of code, you should be able to find a
  product in the Chromium repositories that depends on that line of code or else
  the line of code should be removed.

  When you are adding support for a new OS, a new architecture, a new port or
  a new top-level directory, please send an email to
  chrome-atls@google.com and get approval. For long-term maintenance
  reasons, we will accept only things that are used by the Chromium project
  (including Chromium-supported projects like V8 and Skia) and things whose
  benefit to Chromium outweighs any cost increase in maintaining Chromium's
  supported architectures / platforms (e.g. adding one ifdef branch for an
  unsupported architecture / platform has negligible cost and is likely fine,
  but introducing new abstractions or changes to higher level directories has
  a high cost and would need to provide Chromium with corresponding benefit).
  Note that an unsupported architecture / platform will not have bots on
  Google-managed waterfalls (even FYI bots) or maintained by Chromium
  developers. Please use existing ifdef branches as much as possible.

- **Code should only be moved to a central location (e.g., //base) when
  multiple consumers would benefit.** We should resist the temptation to
  build overly generic common libraries as that can lead to code bloat and
  unnecessary complexity in common code.

- **The code likely wasn't designed for everything we are trying to do with it
  now.** Take time to refactor existing code to make sure the new feature or
  subcomponent you are developing fits properly within the system. Technical
  debt is easy to accumulate and is everyone's responsibility to avoid.

- **Common code is everyone's responsibility.** Large files that are at the
  cross-roads of many subsystems, where integration happens, can be some of the
  most fragile in the system. As a companion to the previous point, be
  cognizant of how you may be adding more complexity to the commons as you
  venture to complete your task.

- **Changes should include corresponding tests.** Automated testing is at the
  heart of how we move forward as a project. All changes should include
  corresponding tests so we can ensure that there is good coverage for code and
  that future changes will be less likely to regress functionality. Protect
  your code with tests!

- **Stick to the current set of supported languages as described in the
  [styleguide][cr-styleguide].** While there is likely always a slightly better
  tool for any particular job, maintainability of the codebase is paramount.
  Reducing the number of languages eases toolchain and infrastructure
  requirements, and minimizes the learning hurdles for developers to be
  successful contributing across the codebase. Additions of new languages must
  be approved by [//ATL_OWNERS](../ATL_OWNERS).

- **When your team is making API changes or migrating between services, the
  team mandating the change needs to do at least 80% of the work.** The
  rationale is to reduce externalities by having the team that requires a
  change spend the vast majority of the time required to make it happen.
  This naturally encourages designing to minimize the cost of change, be it
  through automation, tooling, or pooled centralized expertise. You can find
  more detailed rationale in [this doc](https://docs.google.com/document/d/1elJisUpOb3h4-7WA4Wn754nzfgeCJ4v2kAFvMOzNfek/edit#)
  (Google internal). If you need an exception or help, please contact
  chromium-code-health-rotation@google.com.

## Tips

### Review etiquette

During the lifetime of a review, you may want to rebase your change onto a newer
source revision to minimize merge conflicts. The reviewer-friendly way to do
this is to first address any unresolved comments and upload those changes as a
patchset. Then, rebase to the newer revision and upload that as its own
patchset (with no other changes). This makes it easy for reviewers to see the
changes made in response to their comments, and then quickly verify the diffs
from the rebase.

Code authors and reviewers should keep in mind that Chromium is a global
project: contributors and reviewers are often in time zones far apart. Please
read these guidelines on [minimizing review lag][review-lag] and take them in
consideration both when writing reviews and responding to review feedback.

### Watchlists

If you would like to be notified about changes to a set of files covering a
topic or an area of Chromium, you may use the [watchlists][watchlist-doc]
feature in order to receive email notifications.

## Appendix: CL footer reference {#cl-footer-reference}

Chromium stores a lot of information in footers at the bottom of commit
messages. With the exception of `R=`, these footers are only valid in the
last paragraph of a commit message; any footers separated from the last
line of the message by whitespace or non-footer lines will be ignored.
This includes everything from the unique `Change-Id` which identifies a
Gerrit change, to more useful metadata like bugs the change helps fix,
trybots which should be run to test the change, and more. This section
includes a listing of well-known footers, their meanings, and their
formats.

* **Bug:**
  * A comma-separated list of bug references.
  * A bug reference
    * can be a bare number, e.g. `Bug: 123456`, or
    * can specify a project and a number, e.g. `Bug: skia:1234`.
  * On chromium-review, the default project is assumed to be `chromium`,
    so all bugs in non-chromium projects on bugs.chromium.org should be
    qualified by their project name.
  * The Google-internal issue tracker is accessible by using the `b:`
    project prefix.
* **Fixed:** The same as `Bug:`, but will automatically close the
  bug(s) as fixed when the CL lands.
* **R=**
  * This footer is _deprecated_ in the Chromium project; it was
    deprecated when code review migrated to Gerrit. Instead, use
    `-r foo@example.com` when running `git cl upload`.
  * A comma-separated list of reviewer email addresses (e.g.
    foo@example.com, bar@example.com).
* **Cq-Include-Trybots:**
  * A comma-separated list of trybots which should be triggered and
    checked by the CQ in addition to the normal set.
  * Trybots are indicated in `bucket:builder` format (e.g.
    `luci.chromium.try:android-asan`).
  * The "Choose Tryjobs" UI in the "Checks" tab in Gerrit shows (and has
    a button to copy) the Cq-Include-Trybots syntax for the currently
    selected tryjobs.
* **No-Presubmit:**
  * If present, the value should always be the string `true`.
  * Indicates to the CQ that it should not run presubmit checks on the CL.
  * Used primarily on automated reverts.
* **No-Try:**
  * If present, the value should always be the string `true`.
  * Indicates to the CQ that it should not start or check the results of
    any tryjobs.
  * Used primarily on automated reverts.
* **No-Tree-Checks:**
  * If present, the value should always be the string `true`.
  * Indicates to the CQ that it should ignore the tree status and submit
    the change even to a closed tree.
  * Used primarily on automated reverts.
* **Test:**
  * A freeform description of manual testing performed on the change.
  * Not necessary if all testing is covered by trybots.
* **Reviewed-by:**
  * Automatically added by Gerrit when a change is submitted.
  * Lists the names and email addresses of the people who approved
    (set the `Code-Review` label on) the change prior to submission.
* **Reviewed-on:**
  * Automatically added by Gerrit when a change is submitted.
  * Links back to the code review page for easy access to comment and
    patch set history.
* **Change-Id:**
  * Automatically added by `git cl upload`.
  * A unique ID that helps Gerrit keep track of commits that are part of
    the same code review.
* **Cr-Commit-Position:**
  * Automatically added by the git-numberer Gerrit plugin when a change
    is submitted.
  * This is of the format `fully/qualified/ref@{#123456}` and gives both
    the branch name and "sequence number" along that branch.
  * This approximates an SVN-style monotonically increasing revision
    number.
* **Cr-Branched-From:**
  * Automatically added by the git-numberer Gerrit plugin on changes
    which are submitted to non-main branches.
  * Aids those reading a non-main branch history in finding when a
    given commit diverged from main.

[//]: # (the reference link section should be alphabetically sorted)
[checkout-and-build]: https://chromium.googlesource.com/chromium/src/+/main/docs/#checking-out-and-building
[chrome-dd-review-process]: http://go/chrome-dd-review-process
[chromium-design-docs]: https://groups.google.com/a/chromium.org/forum/#!forum/chromium-design-docs
[code-reviews-owners]: code_reviews.md#OWNERS-files
[code-reviews]: code_reviews.md
[commit-checklist]: commit_checklist.md
[commit-queue]: infra/cq.md
[core-principles]: https://www.chromium.org/developers/core-principles
[corporate-cla]: https://cla.developers.google.com/about/google-corporate?csw=1
[cr-authors]: https://chromium.googlesource.com/chromium/src/+/HEAD/AUTHORS
[cr-styleguide]: https://chromium.googlesource.com/chromium/src/+/main/styleguide/styleguide.md
[crbug-new]: https://bugs.chromium.org/p/chromium/issues/entry
[crbug]: https://bugs.chromium.org/p/chromium/issues/list
[cros-authors]: https://chromium.googlesource.com/chromium/src/+/main/AUTHORS
[cros-dev-guide]: https://chromium.googlesource.com/chromiumos/docs/+/main/developer_guide.md
[crrev]: https://chromium-review.googlesource.com
[depot-tools-setup]: https://commondatastorage.googleapis.com/chrome-infra-docs/flat/depot_tools/docs/html/depot_tools_tutorial.html#_setting_up
[design-doc-template]: https://docs.google.com/document/d/14YBYKgk-uSfjfwpKFlp_omgUq5hwMVazy_M965s_1KA
[direct-commit]: https://dev.chromium.org/developers/contributing-code/direct-commit
[discussion-groups]: https://www.chromium.org/developers/discussion-groups
[github-tutorial]: https://try.github.io
[good-git-commit-message]: https://chris.beams.io/posts/git-commit/
[individual-cla]: https://cla.developers.google.com/about/google-individual?csw=1
[life-of-a-chromium-developer]: https://docs.google.com/presentation/d/1abnqM9j6zFodPHA38JG1061rG2iGj_GABxEDgZsdbJg/edit
[noms-tutorial]: https://meowni.ca/posts/chromium-101
[review-lag]: https://dev.chromium.org/developers/contributing-code/minimizing-review-lag-across-time-zones
[skia-dev-guide]: https://skia.org/docs/dev/contrib/
[try-job-access]: https://www.chromium.org/getting-involved/become-a-committer#TOC-Try-job-access
[v8-dev-guide]: https://v8.dev/docs
[watchlist-doc]: infra/watchlists.md
