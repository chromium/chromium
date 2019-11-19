# Code Reviews

Code reviews are a central part of developing high-quality code for Chromium.
All changes must be reviewed.

The bigger patch-upload-and-land process is covered in more detail the
[contributing code](https://www.chromium.org/developers/contributing-code)
page.

# Code review policies

Ideally the reviewer is someone who is familiar with the area of code you are
touching. Any committer can review code, but an owner must provide a review
for each directory you are touching. If you have doubts, look at the git blame
for the file and the `OWNERS` files (see below).

To indicate a positive review, the reviewer provides a "Code-Review +1" in
Gerrit, also known as an LGTM ("Looks Good To Me"). A score of "-1" indicates
the change should not be submitted as-is.

If you have multiple reviewers, provide a message indicating what you expect
from each reviewer. Otherwise people might assume their input is not required
or waste time with redundant reviews.

Please also read [Respectful Changes](cl_respect.md) and
[Respectful Code Reviews](cr_respect.md).

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

More detail on the owners file format is provided in the "More information"
section below.

*Tip:* The `git cl owners` command can help find owners.

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

  * Demonstrate excellent judgment, teamwork and ability to uphold Chrome
    development principles.

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

The above are guidelines more than they are hard rules, and exceptions are
okay as long as there is a consensus by the existing owners for them.
For example, seldom-updated directories may have exceptions to the
"substantiality" and "recency" requirements. Directories in `third_party`
should list those most familiar with the library, regardless of how often
the code is updated.

### OWNERS file details

Refer to the [source code](https://chromium.googlesource.com/chromium/tools/depot_tools/+/master/owners.py)
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
related files, please first reach out to chrome-eng-review@google.com.

You have to use `set noparent` together with a reference to a file that lists
the owners for the given use case. Approved use cases are listed in
`//build/OWNERS.setnoparent`. Owners listed in those files are expected to
execute special governance functions such as eng review or ipc security review.
Every set of owners should implement their own means of auditing membership. The
minimum expectation is that membership in those files is reevaluated on
project, or affiliation changes.

In this example, only the eng reviewers are owners:
```
set noparent
file://ENG_REVIEW_OWNERS
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

Note that `per-file` directives cannot directly specify subdirectories, e.g:
```
per-file foo/bar.cc=a@chromium.org
```

is not OK; instead, place a `per-file` directive in `foo/OWNERS`.

Other `OWNERS` files can be included by reference by listing the path to the
file with `file://...`. This example indicates that only the people listed in
`//ipc/SECURITY_OWNERS` can review the messages files:
```
per-file *_messages*.h=set noparent
per-file *_messages*.h=file://ipc/SECURITY_OWNERS
```

## TBR ("To Be Reviewed")

"TBR" is our mechanism for post-commit review. It should be used rarely and
only in cases where a normal review is unnecessary, as described under
"When to TBR", below.

TBR does not mean "no review." A reviewer TBR-ed on a change should still
review the change. If there are comments after landing, the author is obligated
to address them in a followup patch.

Do not use TBR just because a change is urgent or the reviewer is being slow.
Contact the reviewer directly or find somebody else to review your change.

### How to TBR

To send a change TBR, annotate the description and send email like normal.
Otherwise the reviewer won't know to review the patch.

  * Add the reviewer's email address in the code review tool's reviewer field
    like normal.

  * Add a line "TBR=<reviewer's email>" to the bottom of the change list
    description. e.g. `TBR=reviewer1@chromium.org,reviewer2@chromium.org`

  * Type a message so that the owners in the TBR list can understand who is
    responsible for reviewing what, as part of their post-commit review
    responsibility. e.g.
    ```
    TBRing reviewers:
    reviewer1: Please review changes to foo/
    reviewer2: Please review changes to bar/
    ```

### When to TBR

#### Reverts and relands

The most common use of TBR is to revert patches that broke the build. Clean
reverts of recent patches may be submitted TBR. However, TBR should not be used
if the revert required non-trivial conflict resolution, or if the patch being
reverted is older than a few days.

A developer relanding a patch can TBR the OWNERS for changes which are identical
to the original (reverted) patch.  If the reland patch contains any new changes
(such as bug fixes) on top of the original, those changes should go through the
normal review process.

When creating a reland patch, you should first upload an up-to-date patchset
with the exact content of the original (reverted) patch, and then upload the
patchset to be relanded. This is important for the reviewers to understand what
the fix for relanding was.

#### Mechanical changes

You can use TBR with certain mechanical changes that affect many callers in
different directories. For example, adding a parameter to a common function in
`//base`, with callers in `//chrome/browser/foo`, `//net/bar`, and many other
directories. If the updates to the callers is mechanical, you can:

  1. Get a normal owner of the lower-level code you're changing (in this
     example, the function in `//base`) to do a proper review of those changes.

  2. Get _somebody_ to review the downstream changes made to the callers as a
     result of the `//base` change. This is often the same person from the
     previous step but could be somebody else.

  3. TBR the owner of the lower-level code you're changing (in this example,
     `//base`), after they've LGTM'ed the API change, to bypass owners review of
     the API consumers incurring trivial side-effects.

This process ensures that all code is reviewed prior to checkin and that the
concept of the change is reviewed by a qualified person, without having to ping
many owners with little say in the trivial side-effects they incur.

**Note:** The above policy is only viable for strictly mechanical changes. For
large-scale scripted changes you should:

  1. Have an owner of the core change review the script.

  2. Use `git cl split` to shard the large change into many small CLs with a
     clear description of what each reviewer is expected to verify
     ([example](https://chromium-review.googlesource.com/1191225)).

#### Documentation updates

You can TBR documentation updates. Documentation means markdown files, text
documents, and high-level comments in code. At finer levels of detail, comments
in source files become more like code and should be reviewed normally (not
using TBR). Non-TBR-able stuff includes things like function contracts and most
comments inside functions.

  * Use good judgement. If you're changing something very important, tricky,
    or something you may not be very familiar with, ask for the code review
    up-front.

  * Don't TBR changes to policy documents like the style guide or this document.

  * Don't mix unrelated documentation updates with code changes.

  * Be sure to actually send out the email for the code review. If you get one,
    please actually read the changes.

