# Git submodules

A Git submodule is a Git repository inside another Git repository. Chromium
project doesn't rely on Git submodules directly. Instead, gclient sync is used
to manage Git dependencies.

In 2023Q3, we started to move source of Git dependencies from DEPS files to Git
submodules. While we do our best to hide complexities of submodules, some will
be exposed.

[TOC]

## A quick introduction to Git submoduldes

[Git submodules](https://git-scm.com/docs/gitsubmodules) are managed via the
combination of `.gitmodules` files and gitlinks. `.gitmodules` is a text file
that configures submodules, and each submodule entry contains the path to the
submodule's worktree and the URL of the submodule. Gitlink is a special type of
file in the Git database that tracks a submodule commit.

You can find an example of Git dependency below. Note that gclient-condition is
a custom property used by gclient and not git. It's identical to `condition` in
`DEPS` and the allowed variables are defined in `vars = {` section of `DEPS`.

`.gitmodules`:

```
[submodule "third_party/catapult"]
	path = third_party/catapult
	url = https://chromium.googlesource.com/catapult.git
	gclient-condition = checkout_linux
```

gitlink entry, retrieved using `git ls-files -s -- third_party/catapult`:

```
160000 0b39a694c0b61392d1180520ed1c13e390029c41 0       third_party/catapult
```

Corresponding DEPS entry would look like:

```
  'third_party/catapult': {
    'url': 'https://chromium.googlesource.com/catapult.git@0b39a694c0b61392d1180520ed1c13e390029c41',
    'condition': 'checkout_linux',
}
```

## How to avoid accidental Git submodule updates?

The simplest approach is to always run gclient sync after updating chromium
checkout (e.g. after `git pull`, or `git checkout`). You can automate that by
adding post-checkout hook (example below). To confirm there are no changes, run
`git status`. If you use `git commit -a`, check the "Changes to be committed"
section that shows up in the edit commit message.

### Automatically run gclient sync after git pull / git checkout

We need to have Git two hooks: post-checkout and post-merge. In chromium/src
directory, edit `.git/hooks/post-checkout`:

```
#!/bin/sh

set -u
gclient sync
```

and set it to be executable: `chmod +x .git/hooks/post-checkout`. Repeat the
same for `.git/hooks/post-merge`.

More information about githooks can be found
[here](https://git-scm.com/docs/githooks).

### Git status shows modified dependencies. What does that mean?

If a submodule is checked out at a different commit than one tracked by its
parent, `git status` in the parent repo will show unstaged changes with "new
commits" in parenthesis, such as:

```
modified: <git deps name> (new commits)
```

Commands like `git commit -a` or `git add *|.|-A|u` WILL include this in your
commit and your CL (which is likely NOT what you want).

Instead you can:

```
# Run gclient sync to sync dependencies
gclient sync
# check git status again

# OR
git add <file> # for each file you want to stage
# Then commit your staged files
git commit -v -m "Fix foo/bar"
```

If a submodule has uncommitted changes (i.e. you made some manual changes to the
affected submodule), running `git status` in its parent repo will show them as
unstaged changes:

```
  modified: <git deps name> (modified content)

# or

  modified: <git deps name> (untracked content)
```

It's not possible to add those changes to the parent repository. You can ignore
such status, or you can cd into submodule and address it. E.g. you may delete
untracked files (content) or reset modified content to match HEAD.

## I accidentally staged Git submodule (not yet committed)

If you accidentally stage a Git submodule, you can unstage it by running `git
restore --staged <path to submodule>`.

## I accidentally committed Git submodule

We will need to create either a commit that sets it back to old value, or amend
the commit that added it. You can try to run `gclient sync` to bring the commit
back to what is expected. If that doesn't work, you can use `gclient setdep -r
<path>@<old hash>`, run `gclient gitmodules` to sync all submodules commits back
to what is in DEPS, or check detailed instructions in [Managing
dependencies](dependencies.md).

NOTE: setdep for chromium/src is always prefixed with src/. For example, if you
are updating v8, the command would be `gclient setdep -r src/v8@<hash>.

## Workflows with submodules

### Submodules during 'git status', 'git commit', and 'git add'

For `git status`, submodules that show up under `Changes not staged for commit`
can be hidden with `git -c diff.ignoreSubmodules=all status`

For `git commit -a` you can exclude all submodules with
`git -c diff.ignoreSubmodules=all commit -a`.

`git add` does NOT support `diff.ignoreSubmodules`. Submodules that were
hidden from you with `git -c diff.ignoreSubmodules=all status` would still
be staged with `git add .|--all|-A` and therefore committed with
`git -c diff.ignoreSubmodules=all commit`.

Instead you can run `git add ':(exclude,attr:builtin_objectmode=160000)'` which
will stage all changes except for submodules.

(git assigns `160000` as the objectmode submodules. You can read more about
[`builtin_objectmode`](https://kernel.googlesource.com/pub/scm/git/git/+/refs/heads/next/Documentation/gitattributes.txt#110)
and magic [pathspecs](https://git-scm.com/docs/gitglossary#Documentation/gitglossary.txt-aiddefpathspecapathspec))

To make these commands shorter, you can create git aliases for them by adding
the following to your $HOME/.gitconfig (globally) or src/.git/config file (just
chromium/src):
```
[alias]
        # 's', 'c', or whatever alias you want for each command
        s = -c diff.ignoreSubmodules=all status
        c = -c diff.ignoreSubmodules=all commit -a
        d = -c diff.ignoreSubmodules=all difftool --dir-diff
        a = add ':(exclude,attr:builtin_objectmode=160000)'
```
With the above, you can execute these commands by running `git s`, `git c`, etc.
Or you may also use the pre-commit git hook detailed below.

### Understanding diff.ignoreSubmodules

`git config diff.ignoreSubmodules` sets a default behavior for `diff`, `status`,
and several other git subcommands, using one of the [supported values of
`--ignore-submodules`](https://www.git-scm.com/docs/git-diff/#Documentation/git-diff.txt---ignore-submodulesltwhengt).

By default, `gclient sync` sets this to `dirty` as a local config in the
chromium checkout. This elides submodule output for `git status` in a clean
checkout, but will show submodules as modified when developers locally touch
them.

Manually setting this to `all` elides such output in all cases. This also omits
submodule changes from `git commit -a`, which can decrease the likelihood of
accidental submodule commits. However, it does not omit such changes from
`git add -A`, meaning developers who use this flow are actually _more_ likely to
commit accidental changes, since they'll be invisible beforehand unless
developers manually set `--ignore-submodules=dirty` or use a lower-level command
such as `git diff-tree`.

Because `all` can result in misleading output and doesn't fully prevent
accidental submodule commits, typical developers are likely better-served by
leaving this configured to `dirty` and installing the
[commit hook described below](#install-hook) to prevent such commits.
Accordingly, `gclient sync` will warn if it detects a different setting locally;
developers who understand the consequences can silence the warning via the
`GCLIENT_SUPPRESS_SUBMODULE_WARNING` environment variable.

### Submodules during a 'git rebase-update'
While resolving merge conflicts during a `git rebase-update` you may see
submodules show up in unexpected places.

#### Submodules under "Changes not staged for commit"
Submodules under this section can be safely ignored. This simply shows that the
local commits of these submodules do not match the latest pinned commits fetched
from remote. In other words, these submodules have been rolled since your last
`git rebase-update`.

If you use a diff tool like meld you can run:
`git -c diff.ignoreSubmodules=all difftool --dir-diff`
to prevent these submodules from showing up in your diff tool.

#### Submodules under "Unmerged paths"
If Submodules show up under this section it means that new revisions were
committed for those submodules (either intentional or unintentionally) and these
submodules were also rolled at remote. So now there is a conflict.

If you DID NOT intentionally make any submdoules changes, you should run:
`gclient gitmodules`. This will update the submdoules for you, to match whatever
commits are listed in DEPS (which you have just pulled from remote).

If you DID intentionally roll submodules, you can resolve this conflict just by
resetting it:
`gclient setdep -r {path}@{hash}`

## Install a hook to help detect unintentional submodule commits {#install-hook}

depot_tools provides an opt-in pre-commit hook to detect unintentional submodule
 changes during `git commit` and remove them from the commit.

To install the hook: `gclient installhooks`

If there is an existing pre-commit hook, gclient will instruct you how to update
it. If you have already installed this hook, gclient will do nothing.

To uninstall the hook, in `chromium/src` `rm .git/hooks/pre-commit` if you have
no other hooks. Otherwise update `.git/hooks/pre-commit` to remove the gclient
provided hook.

To bypass this hook run `git commit --no-verify` (which bypasses all hooks you
 may have) OR set the following environment variable: `SKIP_GITLINK_PRECOMMIT=1`
(which bypasses this specific hook).

Note that this is currently and best effort solution and does not guarantee
that unintentional commits will always be detected. The team will iterate
quickly on this hook to fill in other gaps and behavior is subject to change.
Please file an [issue](https://bugs.chromium.org/p/chromium/issues/entry?components=Infra%3ESDK&labels=submodules-feedback&cc=sokcevic@chromium.org,jojwang@chromium.org&description=Please%20steps%20to%20reproduce%20the%20problem:%0A%0ADo%20you%20have%20any%20custom%20environment%20setups%20like%20git%20hooks%20or%20git%20configs%20that%20you%20have%20set%20yourself%0A%0APlease%20attach%20output%20of:%0Agit%20config%20-l%0Agit%20map-branches%20-vv%0A%0AIf%20this%20is%20an%20issue%20with%20git%20cl%20upload%20please%20include%20the%20git%20trace%20file%20for%20the%20problematic%20run%20found%20in:%0A%3Cdepot_tools_path%3E/traces/%3Clatest%20trace%3E) for any feedback.

## FAQ

### Why do we have Git dependencies in both DEPS and Git submodules?

Lots of Chromium infrastructure already parse DEPS file directly. Instead of a
massive switch, it's easier to transition to Git submodules this way. Moreover,
unwanted Git submodule updates can be detected and developers can be warned.

### How do I manually roll Git submodule?

See the [dependencies](dependencies.md) page.

### I got a conflict on a submodule, how do I resolve it?

First, you will need to determine what is the right commit hash. If you
accidentally committed a gitlink, which got in the meantime updated, you most
likely want to restore the original updated gitlink. You can run `gclient
gitmodules`, which will take care of all unmerged submodule paths, and set it to
match DEPS file.

If you prefer to manually resolve it, under git status, you will see "Unmerged
paths". If those are submodules, you want to restore them by running the
following command:

```
git restore --staging <affected path>
```

### How do I see what revision is pinned?

`gclient getdep` will return whatever commit is pinned for the deps in `DEPS`
(unstaged, staged, or committed). If the repo is using git submodules only
(and has no git deps in `DEPS`) it will return the whatever pinned commit is
staged or committed.

```
gclient getdep -r <path>
```


If you want to keep your gitlink, then run `git add <affected path>`.

### How can I provide feedback?

Please file [a bug under Infra>SDK
component](https://bugs.chromium.org/p/chromium/issues/entry?components=Infra%3ESDK).
