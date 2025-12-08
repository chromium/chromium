# Robosushi - A (Semi)automatic Tool for Rolling FFmpeg

Greetings, adventurer!

The Chrome Videostack team uses the open source FFmpeg project for various
decoding / demuxing tasks. We maintain a local copy of FFmpeg with the chromium
tree (//third_party/ffmpeg) that we keep reasonably up-to-date with the FFmpeg
project. This document describes how to do the "FFmpeg roll", which is the
process by which we import new versions of FFmpeg into Chromium.

"Robosushi" refers to the tool that we use to somewhat automatically merge new
changes from the FFmpeg maintainers into our local copy.

This document assumes that you are the current FFmpeg roller. We'll describe how
you do that with robosushi, what the scripts are doing, and what to do if things
don't work right.

Please contribute to this doc, the FFmpeg roll process, and to the scripts!

## Terminology

- **Origin** - the Chromium repo for FFmpeg. So, in this document,
  "**origin/master**" refers to Chromium's ffmpeg master branch. Note that
  ffmpeg still uses "master" while chromium has been updated to use "main".
- **Upstream** - the ffmpeg repo (i.e., the thing outside of Google)
- **ToT** - tip of the Chromium, rather than FFmpeg, repo. In other words, this
  is the thing with "media" and "base" and such.
- **Merge branch** - the branch locally, and in origin, that holds the
  intermediate results of the ffmpeg roll. If the roll is successful, then it
  will be merged back to origin/master.

## Overview

There are generally three sources of problems when updating Chromium's copy of
FFmpeg: local changes, unwanted upstream code, and test failures.

While we try to keep our local copy of FFmpeg as close as possible to the
official upstream version, sometimes we need to make changes to Chromium's copy
for security / correctness / performance trade-offs. We try to send these
changes back to the FFmpeg maintainers so we don't diverge, but inevitably there
are some changes that Chromium has that the maintainers haven't incorporated yet
/ don't want, etc.

As a side effect, upstream changes can sometimes conflict with ours, either in a
"git merge" sense or in a "semantically incorrect" sense.

Second, sometimes upstream FFmpeg adds code that we don't want in Chromium for
various reasons. Sometimes it's unused code that we'd like to compile out to
save space. Sometimes, the new code isn't licensed appropriately / has patent
restrictions / is otherwise unsuitable for inclusion in the Chromium project.

Third, sometimes new FFmpeg code breaks Chromium's tests. This can be due to
bugs in FFmpeg, our tests, or occasionally, both.

For all of these, the FFmpeg roller has to sort it out.

## Roll early and often

Historically, FFmpeg rolls were labor-intensive, and happened only once per
Chromium milestone. We'd like to reduce that because:

- Many tests (layout, fuzzer) happen only @ToT
  - Fuzzers, in particular, benefit from having more time to run
- It's easier to figure out why things break when we have fewer upstream changes
  to look through

Therefore, it's in our interest to roll ffmpeg early and often.

While the roll is still a manual process, the robosushi scripts are written with
the intention of moving us as close to regular, automatic rolls as we can.
Please consider improving them!

# Overview of the FFmpeg roll {#overview-of-the-ffmpeg-roll}

This part describes how the roll happens in some technical detail. **It's not
intended as a step-by-step guide**; there's a separate section for that and it's
mostly automated anyway. This part is just to get you familiar with what happens
during a roll, so that you can fix it when it's not working.

<!--TODO(crbug.com/466458817): consider making the bullet list correspond to
    subsections that describe it in more detail.-->

The overall process of a merge looks like this:

- Create a local branch in //third_party/ffmpeg to work on the roll that's
  up-to-date with origin/master
- Merge from upstream to the local branch
  - And take steps (see below) to make sure we don't spam ffmpeg committers
- Resolve conflicts, get FFmpeg to compile locally and get tests to pass
  locally.
- Build and commit GN files, READMEs
  - See below, but TL;DR: we have to convert from FFmpeg's 'configure'-type
    build to chromium's GN build.
- Get everything reviewed.
- Run the roll against the bots (including "\*san" bots)
  - This will run all chromium tests on all platforms
- Merge the branch back to origin/master.
  - Or discard the branch and try again if it goes badly. :)
- Update DEPS so that the rest of chromium uses the newly-rolled ffmpeg.

## FFmpeg git branch structure

The scripts will create a local branch in //third_party/ffmpeg named
"sushi-(some long-formatted date)". In this doc, we'll just call the branch name
**sushi-XX** for brevity. It will correspond to the middle branch in the diagram
below.

<!--TODO(crbug.com/466458817): review commits in the diagram, they may be out of
    order.-->

Overall, the final structure of the repository should look roughly like this
when it's done. The stuff on the left (red) is upstream. The stuff on the right
(green) is origin. The stuff in the middle (blue) is your local system and also,
roughly, the corresponding merge branch that will be pushed back to origin/,
likely with some commits squashed in origin.

To visualize your tree, consider using:

```bash
git log --graph --decorate --oneline --all
```

Also consider adding it as an alias called `git tree`:

```bash
git config --global alias.tree 'log --graph --decorate --oneline --all'
```

### Important Stuff about the Merge Commit

When merging from upstream, we have two goals. First, we want to 'git push'
(**not** 'git cl upload') just the merge commit itself to a merge branch in
origin -- no changes to chromium, just resolve conflicts and push to origin. The
script does all that, but it's important enough that you should be aware of it.

If it really doesn't compile / pass the tests, then we'll add additional commits
on top of the merge after we've pushed the merge itself back to origin.

We do it that way because the merge commit is special -- one **_SHOULD NEVER,
EVER_** 'git cl upload' a merge commit, else it SPAMS EVERY FFMPEG COMMITTER.
So, our goal is just to push the merge to origin as soon as we can **_and set
our local branch to track it_**, so that 'git cl upload' never sees the need to
upload the merge commit.

While the scripts try to be very careful about this, if you ever notice that
it's trying to upload a merge from upstream to gerrit, then you should stop it
immediately.

Also note that the merge from the sushi-XX branch back to origin at the end of
the roll doesn't have this problem. It's only the first one from upstream to the
working branch that one must be careful about.

## DEPS

The //third_party/ffmpeg repo holds Chromium's copy of FFmpeg. The DEPS file
points chromium to a particular commit in that repo.

origin/master in the ffmpeg repo will be, usually, what ToT/DEPS points to.
Updating that file is usually the very last step in the roll. Until it's
updated, though, other chromium developers aren't affected by any new / broken /
awful things you might do to the ffmpeg repo; "gclient sync" will continue to
check out whatever commit in the ffmpeg repo that it did before, based on the
copy of DEPS that the chromium developer has checked out.

Note that sometimes DEPS lags behind /third_party/ffpmeg origin/master for other
reasons, such as if somebody has committed things to origin/master but forgot to
do a deps roll. In that case, those changes will be picked up as part of the
merge back to master near the end of your roll automatically. Or, you can do a
deps roll before you start. TL;DR: don't worry about that. The key idea is that,
until you update DEPS, you probably haven't broken anything too badly. :)

## GN Configs

Chromium uses gn files to build everything, including FFmpeg. However, FFmpeg
uses a configure script.

To get these two to work together, we run 'configure' during the roll. We then
build FFmpeg using its own makefiles. We do this for each supported platform
(linux64, arm32, arm64, etc.) and FFmpeg configuration ("Chromium", "ChromeOS",
"Chrome"). Finally, we look at all the .o files that were generated, and use
them to construct a gn file for future chromium builds. Once the gn file is
built, the FFmpeg makefiles / build output can be discarded. It's never checked
into the tree. Once you have built gn files locally, one can use 'ninja' to
build chromium again. Between the merge from upstream and building the gn files,
though, 'ninja' would try to use gn files that were checked into origin/, which
are for the old version of ffmpeg. While this might compile, it's probably not
what you want. **TL;DR**: there's no reason to run ninja until the new gn
configs are built.

The two scripts that do this are "build*ffmpeg.py" and "generate_gn.py". The
former builds FFmpeg locally on one or more platforms using configure / make.
The output is stored locally in a build.* dir in //third*party/ffmpeg . The
latter looks at the contents of the build.* dirs, and builds the gn files based
on what's in them.

A side-effect of this is that, if one builds only one platform, then the gn file
will likely be quite wrong, since the build._ dirs will be missing / incomplete.
They'll be right only for the build._ that were up to date. If you see really
weird gn files for ffmpeg, then that's likely caused by running generate_gn.py
without first rebuilding everything. A diff against origin/master for ffmpeg
gn/gni files will show huge differences.

Once the gn files are built, one may use 'ninja' from the chromium root
normally.

## License restrictions

<!--TODO(crbug.com/466458817): describe licensing checks, etc.-->

# How to do the FFmpeg roll

## Initial Host Setup

We assume that you are using an x64 linux machine. If you're not, stop here and
go get one.

You should only need to do this initial setup once when you take over a roll,
but you can do it again if you think there's a reason.

First, check out chromium if needed. Be sure to include the following line in
your .gclient file:

```python
target_os = [ 'android', 'win' ]
```

Then, run this command:

```bash
cd /path/to/chromium/src ./media/ffmpeg/scripts/robosushi.py --setup
```

This will install the right packages, set up gclient targets, check out the
ffmpeg internal repo, and (possibly) give you instructions on what to do. A
common request is to modify your path -- the script will tell you if you do.
Re-run --setup after making the changes, until it completes successfully.

## Doing an auto-merge

```bash
./media/ffmpeg/scripts/robosushi.py --auto-merge [--prompt]
```

The above command tries to do most of the FFmpeg roll. Please see [Overview of
the FFmpeg Roll](#overview-of-the-ffmpeg-roll) section if you want to know what
it's doing. If that's not enough detail, then see [The merge in more
detail](#the-merge-in-more-detail). You should be able to run it, after running
--setup as instructed earlier, and it'll do **almost** everything.

In particular:

- It will do the merge and runs the tests.
- It will put a CL out for review to update the sushi branch.
- Once that lands, you can re-run it to merge back to origin and start the DEPS
  roll
- You must still add the \*san bots manually to the DEPS roll, and land it in
  gerrit.

The `--prompt` option is optional, and will cause robosushi to ask you before
running any command with side-effects. This will give you the option to see what
it's doing.

<!--TODO(crbug.com/466458817): include sample output here.-->

When you run --auto-merge, it will create a local branch, fetch and merge from
upstream, build gn configs, update the Chromium patches file, and try to run the
tests. If everything completes successfully, then you're almost done!

However, if it fails, you'll have to fix the problem it tells you about, and
commit the results. Then, just **re-run it and it should pick up roughly where
it left off**. It knows where it left off mostly by looking at the commits in
the repository. For example, it checks for a merge commit with upstream on the
current branch to see if it needs to try doing a merge. Or, it looks for some
"magically named" commits to see if the GN configs have been built.

Note: auto-merge is WIP -- there may be bugs, but it generally works. Please
feel free to use, fix (or report bugs) and improve it!

Should auto-merge be two or three steps? --generate-gn and --run-tests? Then
it's easy to know which step failed.

#### If it fails before compiling the tests

"The Tests", in this case, means ffmpeg_regression_tests and media_unittests. It
tries to run these as the last step of the auto-merge. If it can't get far
enough to compile them, then you probably want to address that by reading this
section.

1. If the merge failed due to a conflict, then you'll need to fix up the issues
   manually.
   - If you need to edit an existing ffmpeg file to delete something (or
     otherwise modify it), please comment out the original and add a comment
     indicating that it was done for chromium and why. It will make it easier
     for the next person to see what was done, so that they can do the same
     thing if there is a conflict.
   - After making all the changes, when running 'git commit', uncomment the list
     of files that had conflicts so that they're listed in the commit. This also
     helps future rollers see what happened.
   - It's okay if things don't quite compile or run at this point. Just get it
     as close as possible and commit the result. It's okay if you don't even try
     to compile it via build_ffmpeg, if the conflicts look simple.
     - The rationale is that, if there were no merge conflicts, it would try to
       build automatically as part of the auto-merge
     - However, upstream ffmpeg can be broken, in the "tests fail / chromium
       doesn't work" sense, without merge conflicts. It's really somewhat
       arbitrary that the auto-merge was forced to stop here.
     - The merge process is robust against the latter, and gives you a chance to
       find and fix those issues.
     - Fixing any merge conflicts can be done at that point too.
   - Once it's committed, re-run the script and it will try to compile
     everything, to build the gn configs. You can continue to iterate (see the
     next step) if this fails.
2. If robosushi fails to upload to the chromium gerrit, make sure you are a
   member of
   [https://chromium-review.googlesource.com/admin/groups/5595,members](https://chromium-review.googlesource.com/admin/groups/5595,members)
   (it will 404 if you are not). If you aren't a member, you should ask the
   previous roller to add you to the group.
3. If robosushi fails to build configs, perhaps because the merge wasn't quite
   right, then one should modify and commit those changes locally after the
   merge commit. Then, just re-run "robosushi.py --auto-merge" until it works.
   - You can also use 'chromium/scripts/build_ffmpeg.py' to build individual
     targets, rather than wait for auto-merge to do them all. Still run
     auto-merge at the end once you're somewhat confident that it will work, and
     it will pick up where it left off.
   - If it fails again, that's okay. Just make more changes and commit them
     locally.
   - You might want to land them separately, before the gn config, if you think
     that you'll want to upstream them. Otherwise, feel free to keep them
     locally, and they'll get squashed on the remote side into the 'gn configs'
     commit by 'git cl upload'. You can always cherry-pick your local commit
     later if you decide to upstream it; just be sure to put a branch there so
     you can find it easily.
4. If the tests don't compile, then use your judgment about how big of a problem
   it is. If it seems like small fixes to ffmpeg that you might want to
   upstream, then it's best to pretend you didn't notice and proceed to "Once
   the tests compile". That way, the fixes to get the tests to compile are
   separate from the merge, and easily upstreamed.
   - In general, it's better to do things after the merge than before it. Merge
     commits are weird.

Note that you might need to modify chromium code, in addition to ffmpeg, to get
the tests to compile; the tests are chromium code. If it looks like the changes
are in chromium, then definitely proceed to "Once the tests compile".

The only time you want to handle test compilation here, is if it looks like the
problem is that the ffmpeg merge went really badly, and something's entirely
broken. In that case, fix it first.

#### Once the tests compile (or almost compile)…

Once the tests compile (and, if you're really lucky, finish successfully),
you'll have the gn files and patches committed locally. This is a great time to
'git cl upload' them, and get them reviewed. It's okay to do this even if the
tests compile but fail, or almost-but-not-quite compile, depending on whether
you think it's worth upstreaming the patch to fix them.

When the tests pass, robosushi will upload the CL for you. Send it to the
previous roller for review.

After uploading the merge CL for review, include the output from the following
commands in the PTAL message:

```bash
git diff origin/master configure | grep '^.*\[autodetect\]'
```

<!--TODO(crbug.com/466458817): consider making robosushi write this into a file,
    like it does with the gn config changes currently.-->

#### If the tests fail, or almost-but-not-quite compile

We'll assume that you already have the gn configs reviewed / landed. If not, do
that. It's okay to land them in the remote sushi branch even if the tests fail.
You can always abandon the branch if needed. Remember, we're not doing a DEPS
roll yet, so chromium developers won't know you're doing any of this.

5. If a test fails (media_unittests or ffmpeg_regression_tests), then you may
   have to update the chromium test code to get it to pass. After that is done
   (and committed locally), run './chromium/scripts/robosushi.py --auto-merge'
   to rerun the tests.
   - This is new code. It should detect that there is no need to re-build all of
     the ffmpeg configs, and jump straight to building chromium tests (via
     ninja). If it doesn't, please let us know. It'll work either way, but
     building configs is slow.
   <!--TODO(crbug.com/466458817): document how to make changes to ffmpeg and end
       up with one commit per change.-->
   - If you do want to re-build the gn configs for some reason, then you'll need
     to use 'git rebase -i some_sha1' to remove the 'GN Configuration' commit.
     Then re-run auto-merge, and it'll re-do that step.
   <!--TODO(crbug.com/466458817): it would be nice if there were robosushi
       options to remove this automatically.  Maybe, instead, commit / review
       the gn files and patches file first, then add patches to get the tests to
       pass.  Alternatively, the new --step= could do it, if there were a way to
       ignore the preconditions.-->
   - Note that the ffmpeg regression tests and media_unittests are part of the
     chromium repo, not //third_party/ffmpeg . You'll want to land that
     separately from the ffmpeg roll, either before or after. This means that it
     must (a) pass both with and without the roll, or (b) be turned off
     temporarily until the roll lands, or (c) do a chromium commit with the test
     changes \+ DEPS changes.
     - Option c is a manual deps roll that includes other changes.
1. You might also need to make small patches to ffmpeg to get them to compile.
   Do these locally, upload them for review, and land them.
   - If you want to, run `robosushi.py --patches` to re-generate the patches
     file and commit it locally. However, feel free to do this once at the end
     instead after all the patches have landed. That way, the patch file will
     have correct sha-1's for each patch.
   - There's a lint script that will yell at you for not updating patches. It's
     okay to ignore it, since patches can be re-generated at any time.
     Forgetting patches is no big deal. It used to be, since it was done
     manually, and forgetting to update pretty much guaranteed that the file
     would be out of date forever.

#### Once the tests pass…

Great! You've landed the gn configs, and any of the patches needed to get the
tests running. Now, you just have to finish the roll.

## Merge to master and finish the roll

Once the CL has landed, re-run `robosushi –auto-merge` to merge back to origin,
push the merge to origin, and start a DEPS roll.

Be sure to add \*san bots to the DEPS roll.

Alternatively, since this is somewhat experimental, you should:

```bash
git checkout sushi-branch-name-here
git fetch origin
git merge origin/master
git push origin sushi-branch-name-here:
git push origin sushi-branch-name-here
```

To verify what's in the branch, you can look at the branches
[here](https://chromium.googlesource.com/chromium/third_party/ffmpeg/+refs) (or
use gitk)

<!--TODO(crbug.com/466458817): Move these docs here.-->

#### Tips

There are a couple of caveats:

1. If something fails, make sure that the working directory is clean before
   trying to commit anything locally. In particular, if it fails while building
   configs, then you'd commit half-built gn files that you probably don't want
   to.
   1. Is this actually a problem? If robosushi hasn't built them yet, then it
      will do it anyway. If it has, then it's unlikely anything you do will
      cause files to go away. I guess if one fiddles with configure options when
      media_unittests fails, or something.
2. The script does not put things out for review, try the \*san bots, do a merge
   back to origin, or start a deps roll.

## The merge in more detail {#the-merge-in-more-detail}

<!--TODO(crbug.com/466458817): this is a little out of date.-->

The rough outline, if you are doing it manually, is as follows:

1. Git fetch upstream; git fetch origin.
2. See if we're on a branch named 'sushi-XX'. If so, skip to the next step.
   Otherwise:
   1. Make a local sushi-XX branch that is up-to-date with origin/master but
      does not track anything.
      1. This is for safety -- we don't want to be able to 'git cl upload' the
         merge commit. Without a tracking branch, 'git cl' won't let us.
      2. git branch --no-track sushi-XX origin/master
   2. Check out the local sushi-XX branch.
3. See if a merge is in progress. If so, then stop and tell you to resolve
   conflicts.
4. See if a merge commit is already committed. If so, then skip to the next
   step. Otherwise:
   1. Merge from upstream (git merge --no-ff upstream/master)
   2. If there are conflicts, stop and wait for you, gentle reader, to resolve
      conflicts and commit the merge locally. Then, you'll re-run the auto-merge
      script.
5. Sanity check (e.g., 'gitk') the merge.
6. See if our local branch tracks origin. If so, then the merge has been pushed
   to origin, and we should skip to the next step. Otherwise:
   1. `git push origin sushi-XX:sushi-XX`
      1. This gets the merge to the origin merge branch (**not** origin/master),
         without review.
      2. Remember that we cannot upload it for review without sending a review
         notification to every ffmpeg committer in the merge. They do not want
         this.
   2. `git branch --set-upstream-to=origin/sushi-XX`
      1. This makes your local sushi-XX branch track the remote one. This
         enables 'git cl upload'.
      2. This also makes 'git cl upload' not want to upload the merge commit,
         since it's already pushed to origin on the remote tracking branch.
7. See if there's a magically named commit "GN Configuration" locally. If so,
   then gn configs have been built already, and we should skip to the next step.
   Otherwise:
   1. Try to build for all supported platforms locally.
   2. If it doesn't stop and wait for you to fix it.
      1. As an aside, `robosushi.py --build` will build all platforms and
         generate gn configs. <!--TODO(crbug.com/466458817) Do we still need
         this any more? It's unlikely -->
      2. One can use build_ffmpeg.py to build individual platforms while fixing
         things; it's much faster. Be sure to re-run "robosushi.py --auto-merge"
         to regenerate everything once you're done, though.
      3. If you do need to make changes, you might want to verify that your
         working directory is clean first. Otherwise, you might include
         half-built gn files that would clutter up your CL.
   3. Commit the gn configs, once they're all built.
      1. This commit is magic, in the sense that the auto-roller will look for
         it. If it's there, then it won't try to rebuild all configs for all
         hosts.
      2. If you want it to do rebuild them, then use 'git rebase -i' to delete
         that commit and everything after it. At least, everything after it that
         was committed automatically by the script. Might work if you just
         remove that commit, not sure.
8. Make sure that the gn configs, and the merge in general, don't have any stuff
   we don't want (e.g., check licenses, make sure we're not including unchecked
   bitstream parsers, etc.)
   1. Runs check_merge.py to catch the cases we know about.
   2. The eventual review of these commits should also look at the build files,
      to make sure there aren't new problems.
   3. If there are new problems discovered during review, then please add checks
      for them to check_merge.py if possible.
   4. If it detects problems, then it'll stop and wait for you.
9. Try media_unittests and ffmpeg_regression_tests locally.
   1. If they fail, then it'll wait for you to fix it, and add local commits to
      get it to pass.
   2. You may run `robosushi.py --test` to build the local ffmpeg config (if
      needed) and run the tests. (Do we need this? Probably not. Just use ninja
      or --auto-merge).
   3. You may also use ninja to build whatever you want. The scripts create an
      out/asan directory that you may use also. 'Ninja' is much faster if you're
      trying to iterate to a fix. Once you've generated the gn files a few steps
      ago, ninja works.
10. Write out the CHROMIUM.patches file by running find_patches.py
11. Add / remove any autorename changes found by generate_gn.py .

This is where the script currently stops. The following steps are taken by you:

12. You to put all of the commits out for review.
    1. It's okay to let 'git cl upload' squash them if you like.
    2. None of this should include the merge commit from upstream; we pushed it
       to origin/sushi-XX, and your local sushi-XX branch tracks it.
    3. If the review finds questionable things, then please consider updating
       check_merge.py to catch cases like that in the future automatically.
13. Land all of it in origin/sushi-XX.
14. Do a test deps roll of origin/sushi-XX branch. Don' t land it, just see if
    it passes.
    1. Be sure to add the \*san bots manually!
15. Merge origin/merge-branch back to origin-master
16. Do a real deps roll that references the origin/master commit.

## Future Work

There are some steps it should do:

1. Handle any autorename files after building configs. It just needs to git add
   / git rm the appropriate files.
   1. Also, check if we still need autorename at all. It may just be due to
      weirdness in the mac linker with duplicate object names in different
      directories added to the same library. Maybe it just works now.
   2. As of Jan 2019, renames are still needed by gn. However, they should be
      handled automatically by auto-merge now.
2. Do some branch-magic to start a deps roll.
   1. After all the local commits are done, it would push to
      origin/fake-merge-branch, and start a deps roll of that sha1, including
      the \*san bots.
   2. It would provide the URL to that CL somewhere in the local merge-branch,
      for easy review.
   3. origin/fake-merge-branch would never be merged back. It could be deleted,
      actually, since we just need the sha1 for the fake deps roll.
3. Do the 'git cl upload' once all the local commits are ready.
   1. This would be after starting the fake deps roll, so that it could include
      the fake deps roll CL URL.
   2. This is really easy -- just ask the user and run 'git cl upload'. Probably
      should check 'git cl issue' to make sure it hasn't been done before.
4. Should --auto-merge refuse to start if the working directory isn't clean?
   1. Probably not, since it runs tests. You shouldn't have to commit changes
      just to see if the tests pass.
5. Autoroller for master. It would be nice if we automatically started a deps
   roll anytime something is committed to origin/master .
   1. The new workflow would be something like this:
      1. Before uploading everything for review, do a local merge back to
         origin/master.
      2. Upload the merge commit via 'git cl'. (note: this is not the merge
         commit from upstream! That's already pushed to origin/sushi-XX).
      3. Reviewer approves => lands merge commit in origin/master
      4. Starts deps roll (this is just a origin/master autoroller)
      5. Somewhere in there, we need to run all the san bots on the merge. The
         reviewer shouldn't land the merge until that passes.
   2. Landing the merge commit will, i think, also update the origin/sushi-XX
      branch, so the branch structure ends up the same.
   3. However, i'm not sure if this will end up pushing the upstream merge also,
      which would be bad.
      1. Gerrit wouldn't upload the upstream merge (we pushed it earlier), so if
         gerrit only reviews stuff that's new to the origin repository, then
         this will not spam everybody
      2. However, even though the upstream merge is in the repo, it's not
         reachable from origin/master. Since this second merge commit is for
         origin/master, gerrit might review all the commits that become
         reachable, regardless of whether it had to push them or not. In this
         case, it'll spam everybody.
      3. This behavior is easily testable without doing a real upstream merge,
         so we can see who gets emailed.
      4. Note also that we have to be sure that gerrit's behavior won't change
         in this regard.
   4. The alternative is to review everything on origin/sushi-XX, and push the
      merge back to origin/master without review. It's an extra step, though.
   5. I'm also not sure that this won't make many small reviews for the middle
      commits in the diagram, plus a boring one for the origin merge itself.
6. Rename the primary development branch to `main` and update these
   instructions.

## CHROMIUM.patches and other files

In the current ffmpeg roll instructions, we keep a "CHROMIUM.patches" file that
describes changes that exist in origin but not in upstream. This file is no
longer managed by hand.

Instead, [find_patches.py](find_patches.py) generates it automatically for any
given branch.

There is also a file that tracks the SHA1 of the most recent merge. We recommend
ignoring that; `git log` will tell us that should we ever need it. Find_patches,
for example, can find the most recent upstream commit programmatically via:

```bash
git merge-base upstream/master origin/merge-m68
```
