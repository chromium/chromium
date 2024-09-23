# Code Reviews

Code reviews are a central part of developing high-quality code for Chromium.
All change lists (CLs) must be reviewed.

The general patch, upload, and land process is covered in more detail in the
[contributing code](contributing.md) page. To learn about the code review changes
and OWNERS policy changes launched on March 24, 2021, see
[Mandatory Code Review and Native OWNERS](code_review_owners.md).

# Code review policies

Any [committer](https://www.chromium.org/getting-involved/become-a-committer/#what-is-a-committer) can review code, but
an owner must provide a review for each directory you are touching. Ideally you should choose
reviewers who are familiar with the area of code you are touching. If you have doubts, look
at the `git blame` for the file and the `OWNERS` files ([more info](#owners-files)).

To indicate a positive review, the reviewer provides a `Code-Review +1` in
Gerrit, also known as an LGTM ("Looks Good To Me"). A score of "-1" indicates
the change should not be submitted as-is.

Submissions to the chromium/src repository by a change contributor who is not a Chromium
committer require two committers to Code-Review+1 the submission. If the owner of the CL
is already a committer, then only one other committer is needed to review.

If you have multiple reviewers, provide a message indicating what you expect
from each reviewer. Otherwise people might assume their input is not required
or waste time with redundant reviews.

Please also read [Respectful Changes](cl_respect.md) and
[Respectful Code Reviews](cr_respect.md).

There are also a [collection of tips](cl_tips.md) for productive reviews, though
these are advisory and not policy.

#### Expectations for all reviewers

  * Aim to provide some kind of actionable response within 24 hours of receipt
    (not counting weekends and holidays). This doesn't mean you have to do a
    complete review, but you should be able to give some initial feedback,
    request more time, or suggest another reviewer.

  * Use the status field in Gerrit settings to indicate if you're away and when
    you'll be back.

  * Don't generally discourage people from sending you code reviews. This
    includes using a blanket "slow" in your status field.

## OWNERS files

In various directories there are files named `OWNERS` that list the email
addresses of people qualified to review changes in that directory. You must
get a positive review from an owner of each directory your change touches.

Owners files are recursive, so each file also applies to its subdirectories.
It's generally best to pick more specific owners. People listed in higher-level
directories may have less experience with the code in question. For example,
the reviewers in the `//chrome/browser/component_name/OWNERS` file will likely
be more familiar with code in `//chrome/browser/component_name/sub_component`
than reviewers in the higher-level `//chrome/OWNERS` file.

More detail on the owners file format is provided [here](#owners-file-details).

*Tip:* The `git cl owners` command can help find owners. Gerrit also provides
this functionality in the Reviewers field of CLs.

While owners must approve all patches, any committer can contribute to the
review. In some directories the owners can be overloaded or there might be
people not listed as owners who are more familiar with the low-level code in
question. In these cases it's common to request a low-level review from an
appropriate person, and then request a high-level owner review once that's
complete. As always, be clear what you expect of each reviewer to avoid
duplicated work.

Owners do not have to pick other owners for reviews. Since they should already
be familiar with the code in question, a thorough review from any appropriate
committer is sufficient.

#### Expectations of owners

The existing owners of a directory approve additions to the list. It is
preferable to have many directories, each with a smaller number of specific
owners rather than large directories with many owners. Owners should:

  * Demonstrate excellent judgment, teamwork and ability to uphold
    [Chromium development principles](contributing.md).

  * Be already acting as an owner, providing high-quality reviews and design
    feedback.

  * Be a Chromium project member with full commit access of at least three
    months tenure.

  * Have submitted a substantial number of non-trivial changes to the affected
    directory.

  * Have committed or reviewed substantial work to the affected directory
    within the last ninety days.

  * Have the bandwidth to contribute to reviews in a timely manner. If the load
    is unsustainable, work to expand the number of owners. Don't try to
    discourage people from sending reviews, including writing "slow" or
    "emeritus" after your name.

Seldom-updated directories may have exceptions to the "substantiality" and
"recency" requirements.

Directories in `//third_party` should list those most familiar with the
library, regardless of how often the code is updated.

#### Removal of owners

If a code owner is not meeting the [expectations of
owners](#expectations-of-owners) listed above for more than one quarter (and
they are not on a leave during that time), then they may be removed by any
co-owner or an owner from the parent directory after a 4-week notice, using
the following process:

  * Upload a change removing the owner and copy all owners in that directory,
    including the owner in question.
  * If the affected owner approves the change, it may be landed immediately.
  * Otherwise, the change author must wait five working days for feedback from
    the other owners.
    * After that time has elapsed, if the change has received 3 approvals
      with no objections from anyone else, the change may be landed.
    * If the directory does not have 4 owners, then the decision should
      be escalated to the owners of the parent directory (or directories)
      as necessary to provide enough votes.
    * If there are objections, then the decision should be escalated to
      the [../ATL_OWNERS](../ATL_OWNERS) for resolution.

Note: For the purpose of not slowing down code review, Chromium removes
inactive owners (e.g., those who made no contributions for multiple quarters)
on a regular basis. The script does not take into account personal situations
like a long leave. If you were inactive only for a certain period of time
while you were on a long leave and have been meeting the above owner's
expectations in other times, you can create a CL to re-add yourself and land
after getting local owner's approval (you can refer to this policy in the CL).
The removal script will cc the removed owner and one other owner to avoid spam.

### OWNERS file details

Refer to the [owners plugin](https://github.com/GerritCodeReview/plugins_code-owners/blob/master/resources/Documentation/backend-find-owners.md)
for all details on the file format.

This example indicates that two people are owners, in addition to any owners
from the parent directory. `git cl owners` will list the comment after an
owner address, so this is a good place to include restrictions or special
instructions.
```
# You can include comments like this.
a@chromium.org
b@chromium.org  # Only for the frobinator.
```

A `*` indicates that all committers are owners:
```
*
```

The text `set noparent` will stop owner propagation from parent directories.
This should be rarely used. If you want to use `set noparent` except for IPC
related files, please first reach out to chrome-atls@google.com.

You have to use `set noparent` together with a reference to a file that lists
the owners for the given use case. Approved use cases are listed in
`//build/OWNERS.setnoparent`. Owners listed in those files are expected to
execute special governance functions such as ATL reviews or ipc security review.
Every set of owners should implement their own means of auditing membership. The
minimum expectation is that membership in those files is reevaluated on
project, or affiliation changes.

In this example, only the ATLs are owners:
```
set noparent
file://ATL_OWNERS
```

The `per-file` directive allows owners to be added that apply only to files
matching a pattern. In this example, owners from the parent directory
apply, plus one person for some classes of files, and all committers are
owners for the readme:
```
per-file foo_bar.cc=a@chromium.org
per-file foo.*=a@chromium.org

per-file readme.txt=*
```

Other `OWNERS` files can be included by reference by listing the path to the
file with `file://...`. This example indicates that only the people listed in
`//ipc/SECURITY_OWNERS` can review the messages files:
```
per-file *_messages*.h=set noparent
per-file *_messages*.h=file://ipc/SECURITY_OWNERS
```

File globbing is supported using the
[simple path expression](https://github.com/GerritCodeReview/plugins_code-owners/blob/master/resources/Documentation/path-expressions.md#simple-path-expressions)
format.

### Owners-Override

Setting the `Owners-Override +1` label will bypass OWNERS enforcement. Active
[gardeners](gardener.md), Release Program Managers,
[Large Scale Changes](#large-scale-changes),
[Global Approvers](#global-approvals) reviewers,
[Chrome ATLs](../ATL_OWNERS)
have this capability. The power to use Owners-Override should be restricted
as follows:

  * Active gardeners and Release Program Managers can set Owners-Override only
    on CLs needed for gardening and releasing (e.g., revert, reland, test fix,
    cherry-pick).
  * Large Scale Change reviewers can set Owners-Override only on gardening CLs
    and CLs about the approved Large Scale Change.
  * Global approvers can set Owners-Override only on gardening CLs and
    mechanical CLs associated with their API changes. For example,
    //base/OWNERS can set Owners-Override on mechanical CLs associated with
    //base/ API changes.
  * Chrome ATLs can set Owners-Override on any changes to help with cases that
    cannot be handled by the above groups and expedite CLs when LSC is too
    heavyweight. However, please use one of the above groups before asking
    Chrome ATLs.

When you need Owners-Override on gardening CLs, please reach out to the
Active Sheriffs and Release Program Managers first. If none of them is
available, please send an email to lsc-owners-override@chromium.org for help.

Note that Owners-Override by itself is not enough on your own CLs. Where this
matters is when you are gardening. For example, if you want to revert or
disable a test, your Owners-Override on the CL is not enough. You also need
either another committer to LGTM the CL or, for clean reverts, a `Bot-Commit:
+1` from the [rubber-stamper bot](#automated-code_review).

## Mechanical changes

### Global Approvals
For one-off CLs, API owners of `base`, `build`, `content`,
`third_party/blink/public` and `url` can `Owners-Override +1` a change to their
APIs to avoid waiting for rubberstamp +1s from affected directories' owners.
This should only be used for mechanical updates to the affected directories.

If you are making one-off CLs that touch many directories and cannot be
handled by the global approvers, you can ask one of Chrome ATLs.

### Large Scale Changes
You can use the [Large Scale Changes](process/lsc/large_scale_changes.md)
process to get approval to bypass OWNERS enforcement for large changes like
refactoring, architectural changes, or other repetitive code changes across the
whole codebase. This is used for work that span many dozen CLs.

## Documentation updates

Documentation updates require code review. We may revisit this decision in the
future.

## Automated code-review

For verifiably safe changes like translation files, clean reverts, and clean
cherry-picks, we have automation that will vote +1 on the `Bot-Commit` label
allowing the CL to be submitted without human code-review. Add `Rubber Stamper`
(rubber-stamper@appspot.gserviceaccount.com) to your CL as a reviewer to
activate this automation. It will scan the CL after about 1 minute and reply
with its verdict. `Bot-Commit` votes are not sticky between patchsets and so
only add the bot once the CL is finalized.

When combined with the [`Owners-Override`](#owners_override) power, gardeners
can effectively revert and reland on their own.

Rubber Stamper never provides OWNERS approval, by design. It's intended to be
used by those who have owners in the directory modified or who are gardeners. If
it provided both code review and OWNERS approval, that would be an abuse vector:
that would allow anyone who can create a revert or cherry-pick to land it
without any other person being involved (e.g. the silent revert of security
patches).

Changes not supported by `Rubber Stamper` always need a +1 from another
committer.
