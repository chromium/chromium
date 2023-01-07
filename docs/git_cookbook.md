# Git Cookbook

A collection of git recipes to do common git tasks.

See also [Git Tips](git_tips.md).

[TOC]

## Introduction

This is designed to be a cookbook for common command sequences/tasks relating to
git, git-cl, and how they work with Chromium development. It might be a little
light on explanations.

If you are new to git, or do not have much experience with a distributed version
control system, you should also check out
[The Git Community Book](http://book.git-scm.com/) for an overview of basic git
concepts and general git usage. Knowing what git means by branches, commits,
reverts, and resets (as opposed to what SVN means by them) will help make the
following much more understandable.

## Chromium-specific Git Extensions

Chromium ships a large number of git extensions in depot_tools. Some (like
`git cl`) are required for the Chromium development workflow, while others
(like `git map-branches`) are simple utilities to make your life easier.
Please take a look at the full
[depot_tools tutorial](https://commondatastorage.googleapis.com/chrome-infra-docs/flat/depot_tools/docs/html/depot_tools_tutorial.html),
and at the extensive
[man pages](https://commondatastorage.googleapis.com/chrome-infra-docs/flat/depot_tools/docs/html/depot_tools.html)
for all the extensions.

## Excluding file(s) from git-cl, while preserving them for later use

Since git-cl assumes that the diff between your current branch and its tracking
branch is what should be used for the CL, the goal is to remove the unwanted
files from the current branch, and preserve them in another branch.

### Method #1: Reset your current branch, and selectively commit files

1.  `git log`  See the list of your commits. Find the hash of the last commit
     before your changes.
1.  `git reset --soft abcdef` where abcdef is the hash found in the step above.
1.  `git commit <files_for_this_cl> -m "files to upload"` commit the files you
     want included in the CL here.
1.  `git new-branch new_branch_name` Create a new branch for the
    files that you want to exclude.
1.  `git commit -a -m "preserved files"` Commit the rest of the files.

### Method #2: Create a new branch, reset, then commit files to preserve

This method creates a new branch from your current one to preserve your changes.
The commits on the new branch are undone, and then only the files you want to
preserve are recommitted.

1.  `git checkout -b new_branch_name` This preserves your old files.
1.  `git log` See the list of your commits. Find the hash of the last commit
    before your changes.
1.  `git reset --soft abcdef` Where abcdef is the hash found in the step above.
1.  `git commit <files_to_preserve> -m "preserved files"` Commit the found files
    into the `new_branch_name`.

Then revert your files however you'd like in your old branch. The files listed
in step 4 will be saved in `new_branch_name`

### Method #3: Cherry-pick changes into review branches

If you are systematic in creating separate local commits for independent
changes, you can make a number of different changes in the same client and then
cherry-pick each one into a separate review branch.

1.  Make and commit a set of independent changes.
1.  `git log`  # see the hashes for each of your commits.
1.  repeat checkout, cherry-pick, upload steps for each change 1..n
    1.  `git new-branch review-changeN` Create a new review branch
        tracking origin
    1.  `git cherry-pick <hash of change N>`
    1.  `git cl upload`

If a change needs updating due to review comments, you can go back to your main
working branch, update the commit, and re-cherry-pick it into the review branch.

1.  `git checkout <working branch>`
1.  Make changes.
1.  If the commit you want to update is the most recent one:
    1.  `git commit --amend <files>`
1.  If not:
    1.  `git commit <files>`
    1.  `git rebase -i origin`  # use interactive rebase to squash the new
        commit into the old one.
1.  `git log`  # observe new hash for the change
1.  `git checkout review-changeN`
1.  `git reset --hard`  # remove the previous version of the change
1.  `git cherry-pick <new hash of change N>`
1.  `git cl upload`

## Sharing code between multiple machines

Assume Windows computer named vista, and a Linux one named penguin.
Prerequisite: both machines have git clones of the main git tree.

```shell
vista$ git remote add linux ssh://penguin/path/to/git/repo
vista$ git fetch linux
vista$ git branch -a   # should show "linux/branchname"
vista$ git checkout -b foobar linux/foobar
vista$ hack hack hack; git commit -a
vista$ git push linux  # push branch back to linux
penguin$ git reset --hard  # update with new stuff in branch
```

Note that, by default, `gclient sync` will update all remotes. If your other
machine (i.e., `penguin` in the above example) is not always available,
`gclient sync` will timeout and fail trying to reach it. To fix this, you may
exclude your machine from being fetched by default:

    vista$ git config --bool remote.linux.skipDefaultUpdate true

## Reverting commits

The command `git revert X` patches in the inverse of a particular commit.
Using this command is one way of making a revert:

```shell
git checkout origin   # start with trunk
git revert abcdef
git cl upload
```

## Retrieving, or diffing against an old file revision

Git works in terms of commits, not files. Thus, working with the history of a
single file requires modified version of the show and diff commands.

```shell
# Find the commit you want in the file's commit log.
git log path/to/file
# This prints out the file contents at commit 123abc.
git show 123abc:path/to/file
# Diff the current version against path/to/file against the version at
# path/to/file
git diff 123abc -- path/to/file
```

When invoking `git show` or `git diff`, the `path/to/file` is **not relative the
the current directory**. It must be the full path from the directory where the
.git directory lives. This is different from invoking `git log` which
understands relative paths.

## Reusing a Git mirror

If you have a nearby copy of a Git repo, you can quickly bootstrap your copy
from that one then adjust it to point it at the real upstream one.

1.  Clone a nearby copy of the code you want: `git clone coworker-machine:/path/to/repo`
1.  Change the URL your copy fetches from to point at the real git repo:
    `git remote set-url origin https://chromium.googlesource.com/chromium/src.git`
1.  Update your copy: `git fetch`
1.  Delete any extra branches that you picked up in the initial clone:
    `git prune origin`
