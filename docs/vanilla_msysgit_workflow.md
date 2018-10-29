# Vanilla msysgit workflow

This describes how you can use msysgit on Windows to work on the Chromium git
repository, without setting up Cygwin or hacking the `git cl`, `git try` and
other scripts to work under a regular Windows shell.

The basic setup is to set up a regular git checkout on a Linux (or Mac) box, and
use this exclusively to create your branches and run tools such as `git cl`, and
have your Windows box treat this git repository as its upstream.

The advantage is, you get a pretty clean setup on your Windows box that is
unlikely to break when the various custom git tools like `git cl` change. The
setup is also advantageous if you regularly build code on Windows and then want
to test it on Linux, since all you need to test on your Linux box is a `git
push` from Windows followed by building and testing under Linux.

The disadvantage is that it adds an extra layer between the Chromium git repo
and your Windows checkout.  In my experience (joi@chromium.org) this does not
actually slow you down much, if at all.

The most frequently used alternative to this workflow on Windows seems to be
using Cygwin and creating a checkout directly according to the instructions at
UsingGit. The advantage of that approach is you lose the extra overhead, the
disadvantage seems to be mostly speed and having to run a Cygwin shell rather
than just a normal Windows cmd.

Please note that the instructions below are mostly from memory so they may be
slightly incorrect and steps may be missing.  Please feel free to update the
page with corrections and additions based on your experience.

## Details

Create your checkouts:

1.  Create a git checkout on your Linux box, with read/write abilities, as per
    UsingGit. The rest of these instructions assume it is located at
    /home/username/chrome
1.  Install msysgit on your Windows box.

Starting a new topic branch:

1.   Linux: `git branch mytopic`
     (or you may want to use e.g. the LKGR script from UsingGit).
1.   Windows: `git fetch` then `git checkout mytopic`

Normal workflow on Windows:

1.  ...edit/add some files...
1.  `git commit -a -m "my awesome change"`
1.  ...edit more...
1.  `git commit -a -m "follow-up awesomeness"`
1.  `git push`

Normal workflow on Linux:

*   (after `git push` from windows): `git cl upload && git try`
*   (after LGTM and successful try): `git cl commit`
    (but note the `tot-mytopic` trick in the pipelining section below)

Avoiding excessive file changes (to limit amount of Visual Studio rebuilds when
switching between branches):

*   Base all your different topic branches off of the same base branch; I
    generally create a new LKGR branch once every 2-3 working days and then `git
    merge` it to all of my topic branches.
*   To track which base branch topic branches are based off, you can use a
    naming convention; I use e.g. lk0426 for an LKGR branch created April 26th,
    then use e.g. lk0426-topic1, lk0426-topic2 for the topic branches that have
    all changes merged from lk0426. I (joi@chromium.org) also have a script to
    update the base branch for topic branches and rename them - let me know if
    interested.
*   Now that all your branch names are prefixed with the base revision (whether
    you use my naming convention or not), you can know before hand when you
    switch between branches on Windows whether you should expect a major
    rebuild, or a minor rebuild.  If you are able to remember which of your
    topic branches have gn changes and which don't (or I guess you could use
    `git diff` to figure this out), then you will also have a good idea whether
    you need to run `gclient runhooks` or not when you switch branches.  Another
    nice thing is that you should never have to run `gclient sync` when you
    switch between branches with the same base revision, unless some of your
    branches have changes to DEPS files.

Pipelining:

1.  Linux:
    1.  `git checkout lk0426-mytopic`
    1.  `git checkout -b lk0426-mytopic-nextstep`
1.  Windows:
    1.  `git fetch && git checkout lk0426-mytopic-nextstep`
    1.  ...work as usual...
    1.  `git push`
1.  Later, on Linux:
    1.  `make_new_lkgr_branch lk0428`
    1.  `git merge lk0428 lk0426-mytopic`
    1.  `git branch -m lk0426-mytopic lk0428-mytopic` (to rename)
    1.  `git merge lk0428-mytopic lk0426-mytopic-nextstep`
    1.  `git branch -m lk0428-mytopic-nextstep lk0428-mytopic-nextstep`
        (to rename)
1.  Later, when you want to commit one of the earlier changes in the pipeline;
    all on Linux.  The reason you may want to create the separate tip-of-tree
    branch is in case the try bots show your change failing on tip-of-tree and
    you need to do significant additional work, this avoids having to roll back
    the tip-of-tree merge:

Janitorial work on Windows:

*   When you rename branches on the Linux side, the Windows repo will not know
    automatically; so if you already had a branch `lk0426-mytopic` open on
    Windows and then `git fetch`, you will still have `lk0426-mytopic` even if
    that was renamed on the Linux side to `lk0428-mytopic`.
*   Dealing with this is straight-forward; you just
    `git checkout lk0428-mytopic` to switch to the renamed (and likely updated)
    branch. Then `git branch -d lk0426-mytopic` to get rid of the tracking
    branch for the older name.  Then, occasionally, `git remotes prune origin`
    to prune remote tracking branches (you don't normally see these listed
    unless you do `git branch -a`).

Gotchas:

*   You should normally create your branches on Linux only, so that the Windows
    repo gets tracking branches for them.  Any branches you create in the
    Windows repo would be local to that repository, and so will be non-trivial
    to push to Linux.
*   `git push` from Windows will fail if your Linux repo is checked out to the
    same branch. It is easy to switch back manually, but I also have a script I
    call `safepush` that switches the Linux-side branch for you before pushing;
    let me (joi@chromium.org) know if interested.
