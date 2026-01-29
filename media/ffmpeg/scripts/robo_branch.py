#!/usr/bin/env python3
#
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Functions to facilitate a branching for merges.

A "sushi branch" is a branch that we've created and manage.  We do this to
prevent making changes to branches that we don't understand.  It's mostly as
a sanity check that we're being used correctly.
"""

import check_merge
from datetime import datetime
import config_flag_changes
import json
import os
import re
import requests
from robo_lib.errors import UserInstructions
from robo_lib import shell
import robo_setup


def AreGnConfigsDone(cfg):
    # Try to get everything to build if we haven't committed the configs yet.
    # Note that the only time we need to do this again is if some change makes
    # different files added / deleted to the build, or if ffmpeg configure
    # changes.  We don't need to do this if you just edit ffmpeg sources;
    # those will be built with the tests if they've changed since last time.
    #
    # So, if you're just editing ffmpeg sources to get tests to pass, then you
    # probably don't need to do this step again.
    if cfg.force_gn_rebuild():
        return False
    return robo_branch.IsCommitOnThisBranch(cfg, cfg.gn_commit_title())


def IsWorkingDirectoryClean():
    """Return true if and only if the working directory is clean."""
    return not shell.output_or_error(
        ["git", "status", "--untracked-files=no", "--porcelain"])


def RequiresCleanWorkingDirectory(fn):

    def wrapper(*args, **kwargs):
        if not IsWorkingDirectoryClean():
            raise Exception("Working directory is not clean.")
        return fn(*args, **kwargs)

    return wrapper


@RequiresCleanWorkingDirectory
def CreateAndCheckoutDatedSushiBranch(cfg):
    """Create a dated branch from origin/master and check it out."""  # nocheck
    now = datetime.now()
    branch_name = cfg.sushi_branch_prefix() + now.strftime("%Y-%m-%d-%H-%M-%S")
    shell.log("Creating dated branch %s" % branch_name)
    # Fetch the latest from origin
    if cfg.Call(["git", "fetch", "origin", "--prune-tags"]):
        raise Exception("Could not fetch from origin")

    # Create the named branch
    # Note that we definitely do not want this branch to track upstream; that
    # would eventually cause 'git cl upload' to push the merge commit, assuming
    # that the merge commit is pushed to origin/sushi-branch. One might argue
    # that we should push the merge to upstream, which would make this okay.
    # For now, we leave the branch untracked to make sure that the user doesn't
    # accidentally do the wrong thing. I think that with an automatic deps roll,
    # we'll want to stage things on origin/sushi-branch.
    #
    # We don't want to push anything to origin yet, though, just to keep from
    # making a bunch of sushi branches.  We can do it later just as easily.
    if cfg.Call(
        ["git", "branch", "--no-track", branch_name,
         cfg.origin_merge_base()]):
        raise Exception("Could not create branch")

    # NOTE: we could push the remote branch back to origin and start tracking it
    # now, and not worry about tracking later.  However, until the scripts
    # actually work, i don't want to push a bunch of branches to origin.

    # Check out the branch.  On failure, delete the new branch.
    if cfg.Call(["git", "checkout", branch_name]):
        cfg.Call(["git", "branch", "-D", branch_name])
        raise Exception("Could not checkout branch")

    cfg.SetBranchName(branch_name)


def AreThereAnyUntrackedAutorenamesUnderCwd(cfg):
    """Return true if and only if there are untracked autorename files."""
    # Note that this only sees files that are somewhere under `pwd`.
    return shell.output_or_error(
        ["git", "ls-files", "--other", "--exclude-standard",
         "*autorename*"]) != ""


def CreateAndCheckoutSushiBranchIfNeeded(cfg):
    """Create a dated branch from upstream/HEAD if we're not already on one."""
    if cfg.sushi_branch_name():
        shell.log(f"Already on sushi branch {cfg.sushi_branch_name()}")
        return

    cfg.chdir_to_ffmpeg_home()
    if AreThereAnyUntrackedAutorenamesUnderCwd(cfg):
        raise Exception(
            "Untracked autorename files found.  Probably delete them.")

    CreateAndCheckoutDatedSushiBranch(cfg)


def DeleteExtraTags(cfg):
    """Deletes local tags that are not present on origin.

    This is important because, depending on your git configuration, git may
    attempt to upload any tags it deems are "missing" from origin as part of
    any `git push` or `git cl upload` commands, which will cause a permission
    denied error and reject your attempt to upload."""
    shell.log("Cleaning up extra tags...")

    # 1. Get local tags
    local_tags_output = shell.output_or_error(["git", "tag", "--list"])
    local_tags = set(filter(None, local_tags_output.splitlines()))

    # 2. Get remote tags from origin
    # Use ls-remote to check remote tags
    origin_tags_output = shell.output_or_error(
        ["git", "ls-remote", "--tags", "--refs", "origin"])
    origin_tags = set()
    for line in origin_tags_output.splitlines():
        parts = line.split()
        if len(parts) == 2:
            ref = parts[1]
            if ref.startswith("refs/tags/"):
                origin_tags.add(ref.replace("refs/tags/", ""))

    # 3. Identify tags to remove
    tags_to_remove = local_tags - origin_tags

    if not tags_to_remove:
        shell.log("No extra tags to delete.")
        return

    shell.log(f"Deleting {len(tags_to_remove)} extra tags...")

    # Delete in batches
    batch_size = 100
    sorted_tags = sorted(list(tags_to_remove))
    for i in range(0, len(sorted_tags), batch_size):
        batch = sorted_tags[i:i + batch_size]
        cfg.Call(["git", "tag", "-d"] + batch)


@RequiresCleanWorkingDirectory
def MergeUpstreamToSushiBranch(cfg):
    shell.log("Merging upstream/master to local branch")  # nocheck
    if not cfg.sushi_branch_name():
        raise Exception("Refusing to do a merge on a branch I didn't create")

    # Ensure upstream remote exists before fetching.
    robo_setup.EnsureUpstreamRemote(cfg)

    if cfg.Call(["git", "fetch", "upstream"]):
        raise Exception("Could not fetch from upstream")

    # Upstream fetch might have brought in thousands of tags. Clean them up.
    DeleteExtraTags(cfg)

    if cfg.Call(["git", "merge", "upstream/master"]):
        raise UserInstructions("Merge failed -- resolve conflicts manually.")
    shell.log("Merge has completed successfully")


def GetMergeParentsIfAny(cfg):
    """Return the set of commit sha-1s of the merge commit, if one exists,
       between HEAD and where it joins up with upstream. Otherwise,
       return [].
    """
    sha1s = shell.output_or_error([
        "git", "log", "--format=%H",
        f'{cfg.origin_merge_base()}..{cfg.branch_name()}'
    ]).split()
    for sha1 in sha1s:
        # Does |sha1| have more than one parent commit?
        parents = shell.output_or_error(
            ["git", "show", "--no-patch", "--format=%P", sha1]).split()
        if len(parents) > 1:
            return parents
    return []


def IsMergeCommitOnThisBranch(cfg):
    """Return true if there's a merge commit on this branch."""
    return GetMergeParentsIfAny(cfg) != []


def FindUpstreamMergeParent(cfg):
    """Return the sha-1 of the upstream side of the merge, if there is a merge
  commit on this branch.  Otherwise, fail."""
    sha1s = GetMergeParentsIfAny(cfg)
    for sha1 in sha1s:
        # 'not' is correct -- it returns zero if it is an ancestor => upstream.
        cmd = ["git", "merge-base", "--is-ancestor", sha1,
               "upstream/master"]  # nocheck
        if not shell.run(cmd).returncode:
            return sha1
    raise Exception("No upstream merge parent found.  Is the merge committed?")


def MergeUpstreamToSushiBranchIfNeeded(cfg):
    """Start a merge if we've not started one before, or do nothing successfully
  if the merge is complete.  If it's half done, then get mad and exit."""
    if IsMergeCommitOnThisBranch(cfg):
        shell.log("Merge commit already marked as complete")
        return
    # See if a merge is in progress.  "git merge HEAD" will do nothing if it
    # succeeds, but will fail if a merge is in progress.
    if cfg.Call(["git", "merge", "HEAD"]):
        raise UserInstructions(
            "Merge is in progress -- please resolve conflicts and complete it."
        )
    # There is no merge on this branch, and none is in progress.  Start a merge.
    MergeUpstreamToSushiBranch(cfg)


def CheckMerge(cfg):
    """Verify that the merge config looks good."""
    # If we haven't built all configs, then we might not be checking everything.
    # The check might look at config for each platform, etc.
    shell.log("Checking merge for common failures")
    cfg.chdir_to_ffmpeg_src()
    check_merge.main([])


def WriteConfigChangesFile(cfg):
    """Write a file that summarizes the config changes, for easier reviewing."""
    cfg.chdir_to_ffmpeg_home()
    deltas = config_flag_changes.get_config_flag_changes(cfg)
    flags_file = os.path.join(cfg.patches_dir_location(),
                              "config_flag_changes.txt")
    with open(flags_file, "w") as f:
        for delta in deltas:
            f.write(f'{delta}\n')


def AddAndCommit(cfg, commit_title=None):
    """Add everything, and commit locally with |commit_title|"""
    commit_title = commit_title or cfg.gn_commit_title()
    shell.log("Creating local commit %s" % commit_title)
    cfg.chdir_to_ffmpeg_src()
    if IsWorkingDirectoryClean():
        shell.log("No files to commit to %s" % commit_title)
        return
    if cfg.Call(["git", "add", "-u"]):
        raise Exception("Could not add files")
    if cfg.Call(["git", "commit", "-m", commit_title]):
        raise Exception("Could create commit")


def IsTrackingBranchSet(cfg):
    """Check if the local branch is tracking upstream."""
    output = shell.output_or_error(
        ["git", "branch", "-vv", "--list",
         cfg.sushi_branch_name()])
    # Note that it might have ": behind" or other things.
    return "[origin/%s" % cfg.sushi_branch_name() in output


def PushToOriginWithoutReviewAndTrack(cfg):
    """Push the local branch to origin/ if we haven't yet."""
    cfg.chdir_to_ffmpeg_src()
    # If the tracking branch is unset, then assume that we haven't done this yet
    if IsTrackingBranchSet(cfg):
        shell.log("Already have local tracking branch")
        return
    shell.log("Pushing merge to origin without review")
    cfg.Call([
        "git", "push", "origin",
        cfg.sushi_branch_name(), "-o", "push-justification=b/1234", "-o",
        "banned-words~skip"
    ])
    shell.log("Setting tracking branch")
    cfg.Call([
        "git", "branch",
        "--set-upstream-to=origin/%s" % cfg.sushi_branch_name()
    ])
    # Sanity check. We don't want to start pushing other commits without review.
    if not IsTrackingBranchSet(cfg):
        raise Exception("Tracking branch is not set, but I just set it!")


def HandleAutorename(cfg):
    # We assume that there is a script written by generate_gn.py that adds /
    # removes files needed for autorenames.  Run it.
    shell.log("Updating git for any autorename changes")
    cfg.chdir_to_ffmpeg_src()
    if cfg.Call(["chmod", "+x", cfg.autorename_git_file()]):
        raise Exception("Unable to chmod %s" % cfg.autorename_git_file())
    shell.log(f'executing {cfg.autorename_git_file()} at {os.getcwd()}')
    if cfg.Call([cfg.autorename_git_file()]):
        raise Exception("Unable to run %s" % cfg.autorename_git_file())


def IsCommitOnThisBranch(robo_configuration, commit_title):
    """Detect if we've already committed the |commit_title| locally."""
    # Get all commit titles between us and origin/mastеr
    robo_configuration.chdir_to_ffmpeg_home()
    titles = shell.output_or_error([
        "git",
        "log",
        "--format=%s",
        "origin/master..%s" % robo_configuration.branch_name()  # nocheck
    ])
    return commit_title in titles


def IsChromiumReadmeDone(robo_configuration):
    """Return False if and only if README.chromium isn't checked in."""
    if IsCommitOnThisBranch(robo_configuration,
                            robo_configuration.readme_chromium_commit_title()):
        shell.log("Skipping README.chromium file since already committed")
        return True
    return False


@RequiresCleanWorkingDirectory
def UpdateChromiumReadmeWithUpstream(robo_configuration):
    """Update the upstream info in README.chromium and commit the result."""
    shell.log("Updating merge info in README.chromium")
    merge_sha1 = FindUpstreamMergeParent(robo_configuration)
    robo_configuration.chdir_to_ffmpeg_home()
    with open("README.chromium", "r+") as f:
        readme = f.read()
    last_upstream_merge = "Last Upstream Merge:"
    merge_date = shell.output_or_error([
        "git", "log", "-1", "--date=format:%b %d %Y", "--format=%cd",
        merge_sha1
    ])
    readme = re.sub(r"(Last Upstream Merge:).*\n",
                    r"\1 %s, %s\n" % (merge_sha1, merge_date), readme)
    with open("README.chromium", "w") as f:
        f.write(readme)
    AddAndCommit(robo_configuration,
                 robo_configuration.readme_chromium_commit_title())


def HasGerritIssueNumber(robo_configuration):
    """Return True if and only if this branch has been pushed for review."""
    robo_configuration.chdir_to_ffmpeg_home()
    return os.system(
        "git cl issue 2>/dev/null |grep Issue |grep None >/dev/null") != 0


def IsUploadedForReview(robo_configuration):
    """Check if the local branch is already uploaded."""
    robo_configuration.chdir_to_ffmpeg_home()
    if not HasGerritIssueNumber(robo_configuration):
        shell.log("No Gerrit issue number exists.")
        return False

    if not IsWorkingDirectoryClean():
        shell.log(
            "Working directory is not clean -- commit changes and update CL")
        return False

    # Has been uploaded for review.  Might or might not have been landed yet.
    return True


def IsUploadedForReviewAndLanded(robo_configuration):
    """
    Check if the local sushi branch has been uploaded for review, and has also
    been landed."""
    robo_configuration.chdir_to_ffmpeg_home()
    if not IsUploadedForReview(robo_configuration):
        shell.log("Is not uploaded for review")
        return False
    # Make sure we're up-to-date with origin, to fetch the (hopefully) landed
    # sushi branch.
    if robo_configuration.Call(["git", "fetch", "origin", "--prune-tags"]):
        raise Exception("Could not fetch from origin")

    branch_name = robo_configuration.sushi_branch_name()
    # See if origin/sushi and local/sushi are the same.  This check by itself
    # isn't sufficient, since it would return true any time the two are in sync.
    upstream_branch = shell.output_or_error(
        ["git", "rev-parse", "--abbrev-ref", "@{u}"])
    if 'origin' not in upstream_branch:
        print(f"WARNING - Your branch upstream is set to `{upstream_branch}`" +
              " but it must be set to an upstream branch in order to continue")
        return False
    diff = shell.output_or_error(["git", "diff", upstream_branch, branch_name])
    if diff:
        print(
            "WARNING: Local and origin branches differ. Run `git diff origin/"
            + branch_name + " " + branch_name + "` to see how.")

    return not diff


@RequiresCleanWorkingDirectory
def UploadForReview(robo_configuration):
    """Assuming that tests pass (we can't check), upload to review."""
    robo_configuration.chdir_to_ffmpeg_home()
    if IsUploadedForReview(robo_configuration):
        raise Exception(
            "Sushi branch is already uploaded for review!  (try git cl web)")
    shell.log("Uploading sushi branch for review.")
    robo_configuration.Call(
        ["git", "cl", "upload", "-T", "-o", "banned-words~skip"])


@RequiresCleanWorkingDirectory
def IsSushiMergedBackToOriginMasterAndPushed(robo_configuration):
    """Return true if and only if the most recent common ancestor of the sushi
  branch and upstream is the remote sushi branch itself.  Note that this
  sneakily also checks that it's been pushed back to origin, since we do the
  merge on the local sushi branch, then push to upstream, then push to
  origin/sushi."""
    robo_configuration.chdir_to_ffmpeg_home()

    if robo_configuration.Call(["git", "fetch", "origin", "--prune-tags"]):
        raise Exception("Could not fetch from origin")

    # Get the most recent common ancestor of upstream and local sushi.  If
    # we did not successfully push it back to upstream, even if we merged
    # locally, then this will report that the merge is not complete, which is
    # probably the right thing to do.
    local_sushi = robo_configuration.sushi_branch_name()
    mca_sha1 = shell.output_or_error(
        ["git", "merge-base", local_sushi, "origin/master"])  # nocheck
    if mca_sha1 == "":
        raise Exception("Cannot get sha1 of most recent common ancestor")

    # See if it's the same as remote/sushi.
    sushi_sha1 = shell.output_or_error(
        ["git", "log", "-1", "--format=%H", local_sushi])
    return mca_sha1 == sushi_sha1


@RequiresCleanWorkingDirectory
def MergeBackToOriginMaster(robo_configuration):
    if not DoesFakeDepsRollExist(robo_configuration):
        raise UserInstructions(
            "You must upload the fake merge to chromium CQ to run *san bots")

    if not IsFakeDepsRollGreen(robo_configuration):
        raise UserInstructions("You must wait for the CQ to be pass all tests")

    """Once the sushi branch has landed in origin after review, merge it back
     to upstream locally and push it."""
    shell.log("Considering merge back of sushi to origin/master")  # nocheck
    robo_configuration.chdir_to_ffmpeg_home()

    # Make sure that the CL has landed on the remote sushi branch, so that we
    # can print a more detailed error.  Note that the check for merge-back next
    # would fail if this were not the case, but this way we can say something a
    # little more helpful about what should be done next.
    shell.log("Checking if CL is landed yet...")
    if not IsUploadedForReviewAndLanded(robo_configuration):
        raise Exception(
            "The CL must be reviewed and landed before proceeding.")

    # See if the merge is already complete, and do nothing (successfully) if so.
    shell.log("Checking if sushi branch is already merged back locally")
    if IsSushiMergedBackToOriginMasterAndPushed(robo_configuration):
        shell.log("Sushi branch has already been merged and pushed, skipping")
        return

    # Merge from origin/mastеr into the local sushi branch. Remember that merges
    # are more or less symmetric.  The instructions describe this in the other
    # direction, but there's no real reason why it has to be that way.
    shell.log(
        "Performing local merge of origin/mastеr into local sushi branch")
    if robo_configuration.Call(["git", "merge", "origin/master"]):  # nocheck
        raise Exception("Could not merge from origin/master")  # nocheck

    # Push the result of the merge (local sushi branch) back to origin/mastеr.
    shell.log("Pushing local merge back to origin/master")  # nocheck
    refspec = "%s:master" % robo_configuration.sushi_branch_name()  # nocheck
    if robo_configuration.Call(
        ["git", "push", "origin", refspec, "-o", "push-justification=b/0"]):
        raise Exception("Could not push to 'origin %s'" % refspec)

    # TODO(crbug.com/450394703): should we move the local mastеr branch to
    # origin/mastеr too?


@RequiresCleanWorkingDirectory
def TryRealDepsRoll(robo_configuration):
    """Once the roll is merged back to upstream, we can start a deps roll"""
    shell.log("Trying to start DEPS roll")
    # TODO(crbug.com/450394703): check if there's already a DEPS roll in
    # progress, somehow.

    if not IsSushiMergedBackToOriginMasterAndPushed(robo_configuration):
        raise Exception(
            "Must merge sushi branch back to origin/master")  # nocheck

    robo_configuration.chdir_to_ffmpeg_home()
    sha1 = shell.output_or_error(["git", "log", "-1", "--format=%H"])
    if not sha1:
        raise Exception("Cannot get sha1 of HEAD for DEPS roll")

    robo_configuration.chdir_to_chrome_src()
    # TODO(crbug.com/450394703): do we need to check if there's already
    # a 'git cl issue'?
    # TODO(crbug.com/450394703): --bug would be nice.
    shell.output_or_error([
        "roll_dep.py", "--roll-to", sha1, "--log-limit", "10",
        "src/third_party/ffmpeg"
    ])
    shell.output_or_error(["git", "cl", "upload"])
    shell.output_or_error(["git", "cl", "try"])
    shell.log("Started DEPS roll!")
    shell.log("Please add all the *san bots manually!")


def PrintHappyMessage(robo_configuration):
    # Success!
    shell.log("==================", style=shell.Style.GREEN)
    shell.log(
        "If you have not already done so, add *san bots to the DEPS roll.",
        style=shell.Style.GREEN)
    shell.log("Wait until the DEPS roll completes, land it, and then you will",
              style=shell.Style.GREEN)
    shell.log("have completed your quest to roll the most recent FFmpeg into",
              style=shell.Style.GREEN)
    shell.log("chromium.  Congratulations, Adventurer!",
              style=shell.Style.GREEN)
    shell.log("==================", style=shell.Style.GREEN)


@RequiresCleanWorkingDirectory
def TryFakeDepsRoll(robo_configuration):
    """Start a deps roll against the sushi branch, and -1 it."""
    shell.log("Considering starting a fake deps roll")

    if DoesFakeDepsRollExist(robo_configuration):
        shell.log("Sorry, you can't re-run this step without a new branch")
        return

    # Make sure that we've landed the sushi commits. Note that this can happen
    # if somebody re-runs robosushi after we upload the commits to Gerrit, but
    # before they've been reviewed and landed. This way, we provide a meaningful
    # error.
    if not IsUploadedForReviewAndLanded(robo_configuration):
        raise Exception(
            "Cannot start a fake deps roll until gerrit review lands!")

    robo_configuration.chdir_to_ffmpeg_home()
    sha1 = shell.output_or_error(["git", "log", "-1", "--format=%H"])
    if not sha1:
        raise Exception("Cannot get sha1 of HEAD for fakes dep roll")

    robo_configuration.chdir_to_chrome_src()
    # TODO(crbug.com/450394703): make sure that there's not a deps roll in
    # progress, else we'll keep doing this every time we're run.
    # TODO(crbug.com/450394703): get mad otherwise.
    shell.output_or_error(["roll_dep.py", "third_party/ffmpeg", sha1])
    # TODO(crbug.com/450394703): -1 it.


def DoesFakeDepsRollExist(robo_configuration):
    robo_configuration.chdir_to_chrome_src()
    current_branch = shell.output_or_error(["git", "branch", "--show-current"])
    issue = f"branch.{current_branch}.gerritissue"
    try:
        return shell.output_or_error(["git", "config", "--get", issue]).strip()
    except:
        return False



_DETAIL = "https://chromium-review.googlesource.com/changes/{}/detail?O=16314"
_CQINFO = "https://cr-buildbucket.appspot.com/prpc/buildbucket.v2.Builds/Batch"
_CQLINK = "https://chromium-review.googlesource.com/c/chromium/src/+/{}"


def _FetchGerritUrlToJson(url:str):
    query = requests.get(url=url)
    if query.status_code // 100 != 2:
        raise Exception(f"Status code [{url}] = {query.status_code}")
    if query.text[0:4] == ")]}'":
        return json.loads(query.text[5:])
    return json.loads(query.text)


def _GetCQInfo(issue, revision):
    # BEHOLD THE MAGICAL INCANTATION
    payload = {"requests": [{
        "searchBuilds": {
            "pageSize": 1000,
            "mask": {
                "fields": "id,builder,status,critical",
                "output_properties": [
                    {"path": ["cq_fault_attributions"]}
                ]
            },
            "predicate": {
                "includeExperimental": False,
                "gerritChanges": [{
                    "change": issue,
                    "host": "chromium-review.googlesource.com",
                    "patchset": revision,
                    "project": "chromium/src",
                }]
            }
        }
    }]}
    headers = {
        "accept": "application/json",
    }
    query = requests.post(_CQINFO, json=payload, headers=headers)
    if query.status_code // 100 != 2:
        raise Exception(f"Status code [{url}] = {query.status_code}")
    if query.text[0:4] == ")]}'":
        return json.loads(query.text[5:])
    return json.loads(query.text)


def IsFakeDepsRollGreen(robo_configuration):
    if issue := DoesFakeDepsRollExist(robo_configuration):
        info = _FetchGerritUrlToJson(_DETAIL.format(issue))
        active_revision = info["revisions"][info["current_revision"]]
        rev = active_revision["_number"]
        cq_info = _GetCQInfo(issue, rev)
        try:
            link4msg = _CQLINK.format(issue)
            botlist = cq_info["responses"][0]["searchBuilds"].get("builds", [])
            required_bots = set([
                "linux-tsan-rel", "linux-ubsan-rel", "win-asan-rel",
                "linux-asan-rel", "mac-asan-rel", "mac-arm64-asan-rel"])
            bots_pending = False
            if len(botlist) < 30:
                shell.log(f"Kick off trybots on {link4msg}")
                return False

            has_rendered_botlist_url = False
            for bot in botlist:
                botname = bot["builder"]["builder"]
                if bot["status"] != "SUCCESS":
                    if not has_rendered_botlist_url:
                        shell.log(f"Follow trybot status at: {link4msg}")
                        has_rendered_botlist_url = True
                    shell.log(f"  Bot status [{botname}] is `{bot['status']}`")
                else:
                    bots_pending = True
                if botname in required_bots:
                    required_bots.remove(botname)

            if required_bots:
                shell.log(f"CQ {link4msg} is missing bots: {required_bots}")
                return False

            if bots_pending:
                shell.log(f"CQ {link4msg} bots are still pending")
                return False
        except Exception as e:
            shell.log(f"DEBUG - Why are the trybots failing?")
            print("request params", issue, rev)
            print("response", cq_info)
            raise e

        return True
    else:
        shell.log("This branch has not been uploaded to gerrit for CQ")
        return False
