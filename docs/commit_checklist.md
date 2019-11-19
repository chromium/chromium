# Commit Checklist for Chromium Workflow

Here is a helpful checklist to go through before uploading change lists (CLs) on
Gerrit, which is the code review platform for the Chromium project. This
checklist is designed to be streamlined. See
[contributing to Chromium][contributing] for a more thorough reference. The
intended audience is software engineers who are unfamiliar with contributing to
the Chromium project.

[TOC]

## 1. Create a new branch

You should create a new branch before starting any development work. It's
helpful to branch early and to branch often in Git.

    git new-branch <branch_name>

which is equivalent to

    git checkout -b <branch_name> --track origin/master

Mark the associated crbug as "started" so that other people know that you have
started work on the bug. Doing this can avoid duplicated work.

## 2. Make your changes

Do your thing. There's no further advice here about how to write or fix code.

## 3. Make sure the code builds correctly

After making your changes, check that common targets build correctly:

*   chrome (for Linux, ChromeOS, etc.)
*   unit_tests
*   browser_tests

It's easy to inadvertently break one of the other builds you're not currently
working on without realizing it. Even though the Commit Queue should catch any
build errors, checking locally first can save you some time since the CQ Dry Run
can take a while.

## 4. Test your changes

Make sure you hit every code path you changed.

## 5. Write unit or browser tests for any new code

Consider automating any manual testing you did in the previous step.

## 6. Ensure the code is formatted nicely

Run `git cl format --js`. The `--js` option also formats JavaScript changes.

## 7. Check over your changes

Run `git upstream-diff` to check over all of the changes you've made from the
most recent checkpoint on the remote repository.

## 8. Stage relevant files for commit

Run `git add <path_to_file>` for all of the relevant files you've modified.
Unlike other version-control systems such as svn, you have to specifically `git
add` the files you want to commit before calling `git commit`.

## 9. Commit your changes

Run `git commit`. Here are some
[tips for writing good commit messages][uploading-a-change-for-review].

## 10. Squash your commits

If you have many commits on your current branch, and you want to avoid some
nasty commit-by-commit merge conflicts in the next step, it is recommended to
squash your commits into a single commit. This is done by running `git rebase -i
<upstream-branch>`. The upstream branch is usually origin/master, but check `git
branch -vv` to make sure. After running the git rebase command, you should see a
list of commits, each commit starting with the word "pick". Make sure the first
commit says "pick" and change the rest from "pick" to "squash". This will squash
each commit into the previous commit, which will continue until each commit is
squashed into the first commit.

## 11. Rebase your local repository

Run `git rebase-update`. This command updates all of your local branches with
remote changes that have landed since you started development work, which
could've been a while ago. It also deletes any branches that match the remote
repository, such as after the CL associated with that branch had merged. You may
run into rebase conflicts which should be manually fixed before proceeding with
`git rebase --continue`. Rebasing prevents unintended changes from creeping into
your CL.

Note that rebasing has the potential to break your build, so you might want to
try re-building afterwards.

## 12. Upload the CL to Gerrit

Run `git cl upload`. Some useful options include:

*   `--cq-dry-run` (or `-d`) will set the patchset to do a CQ Dry Run.
*   `-r <chromium_username>` will add reviewers.
*   `-b <bug_number>` automatically populates the bug reference line of the
    commit message.

## 13. Check the CL again in Gerrit

Run `git cl web` to go to the Gerrit URL associated with the current branch.
Open the latest patch set and verify that all of the uploaded files are correct.
Click `Expand All` to check over all of the individual line-by-line changes
again.

## 14. Make sure all auto-regression tests pass

Click `CQ Dry Run`. Fix any errors because otherwise the CL won't pass the
commit queue (CQ) checks. Consider waiting for the CQ Dry Run to pass before
notifying your reviewers, in case the results require major changes in your CL.

## 15. Add reviewers to review your code

Click `Find Owners` or run `git cl owners` to find file owners to review your
code and instruct them about which parts you want them to focus on. Add anyone
else you think should review your code. The blame functionality in Code Search
is a good way to identify reviewers who may be familiar with the parts of code
your CL touches. For your CL to land, you need an approval from an owner for
each file you've changed, unless you are an owner of some files, in which case
you don't need separate owner approval for those files.

## 16. Implement feedback from your reviewers

Then go through this commit checklist again. Reply to all comments from the
reviewers on Gerrit and mark all resolved issues as resolved (clicking `Done` or
`Ack` will do this automatically). Click `Reply` to ensure that your reviewers
receive a notification. Doing this signals that your CL is ready for review
again, since the assumption is that your CL is not ready for review until you
hit reply.

## 17. Land your CL

Once you have obtained a Looks Good To Me (LGTM), which is reflected by a
Code-Review+1 in Gerrit, from at least one owner for each file, then you have
the minimum prerequisite to land your changes. It may be helpful to wait for all
of your reviewers to approve your changes as well, even if they're not owners.
Click `Submit to CQ` to try your change in the commit queue (CQ), which will
land it if successful.

## 18. Cleanup

After your CL is landed, you can use `git rebase-update` or `git cl archive` to
clean up your local branches. These commands will automatically delete merged
branches. Mark the associated crbug as "fixed".

[//]: # (the reference link section should be alphabetically sorted)
[contributing]: contributing.md
[uploading-a-change-for-review]: contributing.md#Uploading-a-change-for-review
