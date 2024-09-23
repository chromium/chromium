# Building old revisions

Occasionally you may want to check out and build old versions of Chromium, such
as when bisecting a regression or simply building an older release tag. Though
this is not officially supported, these tips address some common complications.

This process may be easier if you copy your checkout (starting from the
directory containing `.gclient`) to a new location, so you can just delete the
checkout when finished instead of having to undo changes to your primary working
directory.

## Get compatible depot_tools

Check out a version of depot_tools from around the same time as the target
revision. Since `gclient` auto-updates depot_tools, be sure to
[disable depot_tools auto-update](https://dev.chromium.org/developers/how-tos/depottools#TOC-Disabling-auto-update)
before continuing by setting the environment variable `DEPOT_TOOLS_UPDATE=0`.

```shell
# Get date of current revision:
~/chrome/src $ COMMIT_DATE=$(git log -n 1 --pretty=format:%ci)

# Check out depot_tools revision from the same time:
~/depot_tools $ git checkout $(git rev-list -n 1 --before="$COMMIT_DATE" main)
~/depot_tools $ export DEPOT_TOOLS_UPDATE=0
```

## Clean your working directory

To avoid unexpected gclient behavior and conflicts between revisions, remove any
directories that aren't part of the revision you've checked out. By default, Git
will preserve directories with their own Git repositories; bypass this by
passing the `--force` option twice to `git clean`.

```shell
$ git clean -ffd
```

Repeat this command until it doesn't find anything to remove.

## Sync dependencies

When running `gclient sync`, also remove any dependencies that are no longer
required:

```shell
$ gclient sync -D --force --reset
```

Note that if you are attempting to build an old revision that is on a branch,
you will need to use:

```shell
$ gclient sync -D --force --reset --with_branch_heads
```

instead.

**Warning: `gclient sync` may overwrite the URL of your `origin` remote** if it
encounters problems. You'll notice this when Git starts thinking everything is
"untracked" or "deleted". If this happens, fix and fetch the remote before
continuing:

```shell
$ git remote get-url origin
https://chromium.googlesource.com/chromium/deps/opus.git
$ git remote set-url origin https://chromium.googlesource.com/chromium/src.git
$ git fetch origin
```

It may also be necessary to run the revision's version of
[build/install-build-deps.sh](/build/install-build-deps.sh).

## Build

Since build tools change over time, you may need to build using older versions
of tools like Visual Studio.

You may also need to disable reclient (if enabled).

## Get back to trunk

When returning to a normal checkout, you may need to undo some of the changes
above:

*   Restore `depot_tools` to the `main` branch.
*   Clean up any `_bad_scm/` directories in the directory containing `.gclient`.
*   Revert your `.gclient` file if `gclient` changed it:

    ```
    WARNING: gclient detected an obsolete setting in your .gclient file.  The
    file has been automagically updated.  The previous version is available at
    .gclient.old.
    ```
