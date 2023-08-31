# Git submodules

A Git submodule is a Git repository inside another Git repository. Chromium
project doesn't rely on Git submodules directly. Instead, gclient sync is used
to manage Git dependencies.

In 2023Q3, we started to move source of Git dependencies from DEPS files to Git
submodules. While we do our best to hide complexities of submodules, some will
be exposed.

IMPORTANT NOTE: Due to a bug in fsmonitor, we encourage you to disable it until
the underlying bug is fixed. More details in https://crbug.com/1475405.

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

The simplest approach is to always run gclient sync after updated chromium
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

NOTE: due to a bug in gclient (crbug.com/1475448), it's possible that gclient
left unmanaged git repository. You may need to manually remove those unmanaged
repositories.

```
# Inside chromium/src checkout:
# This ensures that all managed dependencies are in sync:
gclient sync -D
# This moves all unused dependencies to ../unused directory in gclient root
# (just outside of src directory). It then tells git to restore gitlink.
for f in $( git status | grep '(new commits)' | awk '{print $2}' ); do mkdir -p "../unused/`dirname $f`" && mv $f "../unused/$f" && git checkout -- $f; done
# inspect ../unused/ if you'd like, and remove it there's nothing useful there,
# e.g. no non-uploaded commits.
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

