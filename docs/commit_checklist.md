# Commit Checklist for Chromium Workflow

Here is a helpful checklist to go through before uploading change lists (CLs) on
Gerrit and during the code review process. Gerrit is the code review platform
for the Chromium project. This checklist is designed to be streamlined. See
[contributing to Chromium][contributing] for a more thorough reference. The
intended audience is software engineers who are unfamiliar with contributing to
the Chromium project. Feel free to skip steps that are not applicable to the
patchset you're currently uploading.

According to the Checklist Manifesto by Atul Gawande, checklists are a marvelous
tool for ensuring consistent quality in the work you produce. Checklists also
help you work more efficiently by ensuring you never skip a step or waste brain
power figuring out the next step to take.

[TOC]

## 1. Create a new branch or switch to the correct branch

You should create a new branch before starting any development work. It's
helpful to branch early and to branch often in Git. Use the command
`git new-branch <branch_name>`. This is equivalent to
`git checkout -b <branch_name> --track origin/master`.

You may also want to set another local branch as the upstream branch. You can do
that with `git checkout -b <branch_name> --track <upstream_branch>`. Do this if
you want to split your work across multiple CLs, but some CLs have dependencies
on others.

Mark the associated crbug as "started" so that other people know that you have
started working on the bug. Taking this step can avoid duplicated work.

If you have already created a branch, don't forget to `git checkout
<branch_name>` to the correct branch before resuming development work. There's
few things more frustrating than to finish implementing your ideas or feedback,
and to spend hours debugging some mysterious bug, only to discover that the bug
was caused by working on the wrong branch this whole time.

## 2. If there's a local upstream branch, rebase the upstream changes

Suppose you have a downstream branch chained to an upstream branch. If you
commit changes to the upstream branch, and you want the changes to appear in
your downstream branch, you need to:

*   `git checkout <branch_name>` to the downstream branch.
*   Run `git rebase -i @{u}` to pull the upstream changes into the current
    branch.
*   Run `git rebase -i @{u}` again to rebase the downstream changes onto the
    upstream branch.

Expect to fix numerous merge conflicts. Use `git rebase --continue` once you're
done.

## 3. Make your changes

Do your thing. There's no further advice here about how to write or fix code.

## 4. Make sure the code builds correctly

After making your changes, check that common targets build correctly:

*   chrome (for Linux, ChromeOS, etc.)
*   unit_tests
*   browser_tests

You can find [instructions here][build-instructions] for building various
targets.

It's easy to inadvertently break one of the other builds you're not currently
working on without realizing it. Even though the Commit Queue should catch any
build errors, checking locally first can save you some time since the CQ Dry Run
can take a while to run, on the order of a few hours sometimes.

## 5. Test your changes

Test your changes manually by running the Chrome binary or deploying your
changes to a test device. If you're testing Chrome for ChromeOS, follow the
[Simple Chrome][simple-chrome] instructions to deploy your changes to a test
device. Make sure you hit every code path you changed.

## 6. Write unit or browser tests for any new code

Consider automating any manual testing you did in the previous step.

## 7. Ensure the code is formatted nicely

Run `git cl format --js`. The `--js` option also formats JavaScript changes.

## 8. Review your changes

Use `git diff` to review all of the changes you've made from the previous
commit. Use `git upstream-diff` to review all of the changes you've made
from the upstream branch. The output from `git upstream-diff` is what will
be uploaded to Gerrit.

## 9. Stage relevant files for commit

Run `git add <path_to_file>` for all of the files you've modified that you want
to include in the CL. Unlike other version-control systems such as svn, you have
to specifically `git add` the files you want to commit before calling
`git commit`.

## 10. Commit your changes

Run `git commit`. Be sure to write a useful commit message. Here are some
[tips for writing good commit messages][uploading-a-change-for-review]. A
shortcut for combining steps the previous step and this one is `git commit -a -m
<commit_message>`.

## 11. Squash your commits

If you have many commits on your current branch, and you want to avoid some
nasty commit-by-commit merge conflicts in the next step, consider collecting all
your changes into one commit. Run `git rebase -i @{u}`. The `@{u}` is a
short-hand pointer for the upstream branch, which is usually origin/master, but
can also be one of your local branches. After running the `git rebase` command,
you should see a list of commits, with each commit starting with the word
"pick". Make sure the first commit says "pick" and change the rest from "pick"
to "squash". This will squash each commit into the previous commit, which will
continue until each commit is squashed into the first commit.

An alternative way to squash your commits into a single commit is to do `git
commit --amend` in the previous step.

## 12. Rebase your local repository

Rebasing is a neat way to sync changes from the remote repository and resolve
any merge conflict errors on your CL. Run `git rebase-update`. This command
updates all of your local branches with remote changes that have landed since
you started development work, which could've been a while ago. It also deletes
any branches that match the remote repository, such as after the CL associated
with that branch has been merged. In summary, `git rebase-update` cleans up your
local branches.

You may run into rebase conflicts. Fix them manually before proceeding with
`git rebase --continue`. Note that rebasing has the potential to break your
build, so you might want to try re-building afterwards.

## 13. Upload the CL to Gerrit

Run `git cl upload`. Some useful options include:

*   `--cq-dry-run` (or `-d`) will set the patchset to do a CQ Dry Run. It is a
    good idea to run try jobs for each new patchset with significant changes.
*   `-r <chromium_username>` will add reviewers.
*   `-b <bug_number>` automatically populates the bug reference line of the
    commit message. Use `-b None` is there is no relevant crbug.
*   `--edit-description` will let you update the commit message. Using square
    brackets in the commit message title, like [hashtag], will add a hashtag to
    your CL. This feature is useful for grouping related CLs together.

To help guide your reviewers, it is also recommended to provide a title for each
patchset summarizing the changes and indicating whose comments the patchset
addresses. Running `git cl upload` will upload a new patchset and prompt you for
a brief patchset title. The title defaults to your most recent commit summary,
so if you tend to squash all your commits into one, try to enter a new summary
each time you upload. You can also modify the patchset title directly in Gerrit.

## 14. Check the CL again in Gerrit

Run `git cl web` to go to the Gerrit URL associated with the current branch.
Open the latest patchset and verify that all of the uploaded files are correct.
Click `Expand All` to check over all of the individual line-by-line changes
again. Basically do a self-review before asking your reviewers for a review.

## 15. Make sure all auto-regression tests pass

Click `CQ Dry Run`. Fix any errors because otherwise the CL won't pass the
commit queue (CQ) checks. Consider waiting for the CQ Dry Run to pass before
notifying your reviewers, in case the results require major changes in your CL.

## 16. Add reviewers to review your code

Click `Find Owners` or run `git cl owners` to find file owners to review your
code and instruct them about which parts you want them to focus on. Add anyone
else you think should review your code. The blame functionality in Code Search
is a good way to identify reviewers who may be familiar with the parts of code
your CL touches. For your CL to land, you need an approval from an owner for
each file you've changed, unless you are an owner of some files, in which case
you don't need separate owner approval for those files.

## 17. Implement feedback from your reviewers

Then go through this commit checklist again. Reply to all comments from the
reviewers on Gerrit and mark all resolved issues as resolved (clicking `Done` or
`Ack` will do this automatically). Click `Reply` to ensure that your reviewers
receive a notification. Doing this signals that your CL is ready for review
again, since the assumption is that your CL is not ready for review until you
hit reply.

If your change is simple and you feel confident that your reviewer will approve
your CL on the next iteration, you can set Auto-Submit +1. The CL will proceed
to the next step automatically after approval. This feature is useful if your
reviewer is in a different time zone and you want to land the CL sooner. Setting
this flag also puts the onus on your reviewer to land the CL.

## 18. Land your CL

Once you have obtained a Looks Good To Me (LGTM), which is reflected by a
Code-Review+1 in Gerrit, from at least one owner for each file, then you have
the minimum prerequisite to land your changes. It may be helpful to wait for all
of your reviewers to approve your changes as well, even if they're not owners.
Don't use `chrome/OWNERS` as a blanket stamp if your CL makes significant
changes to subsystems. Click `Submit to CQ` to try your change in the commit
queue (CQ), which will land it if successful.

## 19. Cleanup

After your CL is landed, you can use `git rebase-update` or `git cl archive` to
clean up your local branches. These commands will automatically delete merged
branches. Mark the associated crbug as "fixed".

[//]: # (the reference link section should be alphabetically sorted)
[build-instructions]: https://chromium.googlesource.com/chromium/src.git/+/master/docs/#Checking-Out-and-Building
[contributing]: contributing.md
[simple-chrome]: https://chromium.googlesource.com/chromiumos/docs/+/master/simple_chrome_workflow.md
[uploading-a-change-for-review]: contributing.md#Uploading-a-change-for-review
