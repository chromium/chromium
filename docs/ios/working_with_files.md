# Working With Files

Adding, removing, and renaming files in iOS Chromium needs to follow a specific
procedure that will be unfamiliar to engineers coming from other iOS projects.
Conceptually, every file is recorded in _four_ locations: the local filesystem,
git, the `BUILD.gn` files, and the XCode projects. Of these, the XCode project
is wholly generated from the others.

[TOC]

## Overview

**Do not use XCode to manipulate files.** The XCode project used for iOS
Chromium is _generated_; it's functionally a build artifact. The various
`BUILD.gn` files in Chromium define structure of the XCode project file. Running
`gclient runhooks` causes the project files to be regenerated.

Individual files can have their contents edited within XCode, and all of the
regular testing and debugging activities can be done within XCode. It just can't
be used to create files, rename files, delete files, or manipulate the project
group structure in any way. To do these things, follow the procedures below.

## Adding files

To add any files (new headers, `.mm` or `.cc` implementation files, asset files
of any kind), the following general steps need to happen:

1.  The file needs to exist in the correct directory of the file system for your
    Chromium checkout. New files need to be created, or assets need to be
    copied to the right location.

1.  The new files need to be added to git.

1.  The new files need to be added to a target in a `BUILD.gn` file (usually in
    the same directory as the newly added file).

1.  The XCode project needs to be regenerated.

For adding new header or implementation files, the following procedure is
recommended:

1.  Generate the new files using `tools/boilerplate.py`. This will generate
    header guard macros and include the copyright boilerplate. Make sure to run
    this from root (`src/`) so the header guards include the correct, full
    path.

2.  Add the newly created files using `git add`.

3.  Edit the `BUILD.gn` file for the directory where the files were added. For
    each file, add it to the `sources` list for the correct `source_set` in the
    `BUILD.gn` file. Note that `gn format` (which is run as part of `git cl
    format`) will take care of alphabetizing the lists along with other
    formatting, so there's no need to manually do these things. (`BUILD.gn`
    files cand be edited directly in XCode, or in another editor such as `vi`).

4.  Once all of the files have been created, `add`ed and all `BUILD.gn` files
    have been updated, run `gclient runhooks` to regenerate all XCode projects.

5.  If XCode is open, it may prompt to "Autocreate Schemes". If so, click on the
    highlighted "Automatically Create Schemes" button.

In the shell, this procedure would look like this:
```bash
// Step 1 -- generate new files.
$ tools/boilerplate.py ios/chrome/browser/some_feature/feature_class.h
$ tools/boilerplate.py ios/chrome/browser/some_feature/feature_class.mm
$ tools/boilerplate.py ios/chrome/browser/some_feature/feature_class_unittest.mm
// Step 2 -- add the new files.
$ git add ios/chrome/browser/some_feature/feature_class*
// Step 3 -- edit the BUILD.gn file in the editor of your choice
$ vi ios/chrome/browser/some_feature/BUILD.gn
// Step 4 -- regenerate the XCode Projects
$ gclient runhooks
```

To add asset files, follow this procedure:

1.  Copy the asset files to the correct directory, with the correct names
    (including `@2x` and `@3x` suffixes) Note that images are stored in
    `.imageset` directories, conventionally inside `resources` directories for a
    given UI feature. New directories, if needed, are created in the usual way
    (`mkdir`). Note that there is no equivalent of `boilerplate.py` for images
    or other asset files.

1.  Create or copy `Contents.json` files for each new `.imageset` directory,
    with the appropriate contents.

1.  Add all image and `Contents.json` files using `git add`.

1.  Edit the `BUILD.gn` file in the containing `resources` directory, adding
    `imageset` entries for each added `.imageset` directory, and then grouping
    all assets into a new or existing `group()` declaration with a `public_deps`
    list containing all of the `imageset` targets.

1.  Regenerate the XCode project with `gclient runhooks`.

To add Markdown documentation files, the procedure is much simpler. These files
are automatically added to the XCode project without `BUILD.gn` entries, and
they have no required boilerplate. So adding new docs is as simple as:

1.  Create a new `.md` file in the appropriate directory (`docs/ios`, for
    example).

1. `git add` the file.

The newly added file will be visible in XCode after the next `gclient runhooks`.

## Moving and renaming files.

Renaming a file involves updating the filename in all of the places where it
exists: the file names in the filesystem, the file names in git, header guards
in files, import declarations in files, listings in BUILD.gn files, and
internally in the XCode project. As with adding a file, different tools are used
for each of these. Unlike creating a file, which starts with actually adding a
file to the filesystem, a rename starts with updating git (via `git mv`), then
using the `mass-rename` tool to update file contents.

`tools/git/mass-rename.py` works by looking at _uncommitted_ file moves in git,
and then updating all includes, header guards, and BUILD.gn entries to use the
new name. It doesn't update some other files, such as `Contents.json` files for
image assets. It also doesn't change any symbols in code, so class and variable
names won't be changed.

For many file moves, it will be simpler to use another tool,
`tools/git/move_source_file.py`, which combines `git mv` and `mass-rename` in a
single action. For example, renaming `feature_class` to `renamed_class` would be
done like this:
```bash
    $ tools/git/move_source_file.py ios/chrome/browser/some_feature/feature_class.h \
        ios/chrome/browser/some_feature/renamed_class.h
    $ tools/git/move_source_file.py ios/chrome/browser/some_feature/feature_class.mm \
        ios/chrome/browser/some_feature/renamed_class.mm
```

The step-by-step procedure for a rename is:

1.  If there are other uncommitted changes before the move, it's usually
    cleanest to commit before starting the move.

1.  `move_source_file` each file that needs to be renamed. This renames the file
    in both the file system and in git, and in most places where it's used in
    code.

1.  Run `gclient runhooks` to update the XCode project. Check that all of the
    needed name changes have been made (for example, by building all targets).
    Make any other needed fixes.

1.  If any classes or other symbols need to be renamed (remember that the name
    of the primary interface in each file must match the file name), make those
    changes. Find-and-replace tools like `tools/git/mffr.py` or XCode's
    Find/Replace can help here, but there are no compiler-aware tools that can
    do a "smart" rename.

1.  Commit all changes (`git commit -a -m <your comment>`).

A move—where a file is moved to a different directory—is in most respects
performed using the same steps as a rename. However, while `mass-rename.py` (and
thus `move_source_file.py`) will update existing file names in `BUILD.gn` files,
it won't move entries from one `BUILD.gn` file to another. To move files to a
different directory, the preceding procedure is used, but between steps 2 and 3
(after moving the files, but before regenerating the XCode project), the old
filenames will need to be removed from the `BUILD.gn` files in the old
directories and added to the `BUILD.gn` files in the new directories.

Also note that while `move_source_file` must be used separately for each file
being renamed within a directory, it (just like `git mv`) can move multiple
files without renaming to a new directory in a single command:

```bash
$ tools/git/mass-rename.py ios/chrome/browser/some_feature/feature_class.* \
    ios/chrome/browser/some_feature/feature_class_unittest.mm \
    ios/chrome/browser/other_feature/
```

## Deleting files.

Deleting files follows the same patterns as adding and moving files. As with a
file move, it's best to begin with deleting the files from git.

Typically, before actually removing a file, first all usage of the interface(s)
in the file(s) will be removed, and the file will no longer be `#imported`
anywhere.

Step-by step:

1.  `git rm` the files you want to remove. This will also remove the files
    from the filesystem.

1.  Manually remove the `BUILD.gn` entries for the files.

1.  Regenerate the XCode project (with `gclient runhooks`) to remove the files
    from XCode.

## Finally.

It's easy to miss some uses of a file that was renamed or deleted, and fixing
compilation errors discovered in the commit queue means another
commit-upload-dry run cycle (at least). To minimize this, after any change that
adds, renames, moves, or deletes files, be sure to take the following steps:

1.  `git cl format` to update the formatting of all files.

1.  `gn check` to make sure that any new or moved files follow the dependency
    rules (for example: `gn check -C out/Debug-iphonesimulator/`).

1.  Build all targets, to make sure that everything has been added, changed, or
    removed correctly. This can be done by selecting the "All" target in XCode
    and building (`⌘-B`), or from the command line (for example, `autoninja -C
    out/Debug-iphonesimulator/`).

Changes that involve adding or deleting more than a few files, and most renames
of any size, should be in a single CL with no other changes, for ease of
reviewing and (if necessary) reverting or cherry-picking.

## Recovering from accidental XCode Project usage.

If files are accidentally added, renamed, or moved through XCode, other settings
in the XCode project may be changed that will introduce strange local build
failures. In this case, take the following steps to recover.

1. Quit XCode.

1. Delete all generated XCode projects and associated files: `rm -rf out/build`.

1. Regenerate all XCode projects: `gclient runhooks`.
