# Git Tips

When using Git, there are a few tips that are particularly useful when working
on the Chromium codebase, especially due to its size.

[TOC]

## Remember the basic git convention:

    git COMMAND [FLAGS] [ARGUMENTS]

Various git commands have underlying executable with a hyphenated name, such as
`git-grep`, but these can also be called via the `git` wrapper script as
`git grep` (and `man` should work either way too).

## Git references

The following resources can provide background on how Git works:

*   [Git-SVN Crash Course](http://git-scm.com/course/svn.html) -- this crash
    course is useful for Subversion users switching to Git.
*   [Think Like (a) Git](http://think-like-a-git.net/) -- does a great job of
    explaining the main purpose of Git operations.
*   [Git User's Manual](http://schacon.github.com/git/user-manual.html) -- a
    great resource to learn more about ho to use Git properly.
*   [A Visual Git Reference](https://marklodato.github.io/visual-git-guide/index-en.html)
    -- a resource that explains various Git operations for visual learners.
*   [Git Cheat Sheet](http://cheat.errtheblog.com/s/git) -- now that you
    understand Git, here's a cheat sheet to quickly remind you of all the
    commands you need.

## Optimizing (Speeding up) Git for a Large Repository

Git has numerous options, among which some are intended to optimize for large
repositories.
[feature.manyFiles](https://git-scm.com/docs/git-config#Documentation/git-config.txt-featuremanyFiles)
is a convenient option that turns on the group of options that optimize for
large repositories. Run the following inside the Chromium git repository:

    git config feature.manyFiles true

## Configuring the output of "git log"

By default, the date that "git log" displays is the "author date." In Chromium,
this generally corresponds to the date that the committed patch was last
uploaded. In most cases, however, the date that is of interest is the date that
the patch was committed in the tree. To configure "git log" to instead display
the latter date for your Chromium checkout, execute the following command:

```shell
git config format.pretty 'format:%C(auto,yellow)commit %H%C(auto)%d%nAuthor:    %an <%ae>%nCommitted: %cd%n%n%w(0,4,4)%B%-%n'
```

If you want to change *all* your repos (e.g., because you have multiple Chromium
checkouts and don't care about having the default for other repos), add
"--global" after "config" in the above command.

## Committing changes

For a simple workflow (always commit all changed files, don't keep local
revisions), the following script handles check; you may wish to call it `gci`
(git commit) or similar.

Amending a single revision is generally easier for various reasons, notably for
rebasing and for checking that CLs have been committed. However, if you don't
use local revisions (a local branch with multiple revisions), you should make
sure to upload revisions periodically to code review if you ever need to go to
an old version of a CL.

```bash
#!/bin/bash
# Commit all, amending if not initial commit.
if git status | grep -q "Your branch is ahead of 'origin/main' by 1 commit."
then
  git commit --all --amend
else
  git commit --all  # initial, not amendment
fi
```

## Listing and changing branches

```shell
git branch  # list branches
git checkout -  # change to last branch
```

To quickly list the 5 most recent branches, add the following to `.gitconfig`
in the `[alias]` section:

```shell
last5 = "!git for-each-ref --sort=committerdate refs/heads/ \
    --format='%(committerdate:short) %(refname:short)' | tail -5 | cut -c 12-"
```

A nicely color-coded list, sorted in descending order by date, can be made by
the following bash function:

```bash
git-list-branches-by-date() {
  local current_branch=$(git rev-parse --symbolic-full-name --abbrev-ref HEAD)
  local normal_text=$(echo -ne '\E[0m')
  local yellow_text=$(echo -ne '\E[0;33m')
  local yellow_bg=$(echo -ne '\E[7;33m')
  git for-each-ref --sort=-committerdate \
      --format=$'  %(refname:short)  \
          \t%(committerdate:short)\t%(authorname)\t%(objectname:short)' \
          refs/heads \
      | column -t -s $'\t' -n \
      | sed -E "s:^  (${current_branch}) :* ${yellow_bg}\1${normal_text} :" \
      | sed -E "s:^  ([^ ]+):  ${yellow_text}\1${normal_text}:"
}
```

## Searching

Use `git-grep` instead of `grep` and `git-ls-files` instead of `find`, as these
search only files in the index or _tracked_ files in the work tree, rather than
all files in the work tree.

Note that `git-ls-files` is rather simpler than `find`, so you'll often need to
use `xargs` instead of `-exec` if you want to process matching files.

## Global changes

To make global changes across the source tree, it's often easiest to use `sed`
with `git-ls-files`, using `-i` for in-place changing (this is generally safe,
as we don't use symlinks much, but there are few places that do). Remember that
you don't need to use `xargs`, since sed can take multiple input files. E.g., to
strip trailing whitespace from C++ and header files:

    sed -i -E 's/\s+$//' $(git ls-files '*.cpp' '*.h')


You may also find `git-grep` useful for limiting the scope of your changes,
using `-l` for listing files.

    sed -i -E '...' $(git grep -lw Foo '*.cpp' '*.h')

Remember that you can restrict sed actions to matching (or non-matching) lines.
For example, to skip lines with a line comment, use the following:

    '\,//, ! s/foo/bar/g'

## Diffs

    git diff --shortstat

Displays summary statistics, such as:

    2104 files changed, 9309 insertions(+), 9309 deletions(-)
