#!/usr/bin/env python3
#
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Set up everything for the roll.
#
# --prompt:     require user input before taking any action.  Use in conjunction
#               with (before) other options.
# --setup:      set up the host to do a roll.  Idempotent, but probably doesn't
#               need to be run more than once in a while.
# --auto-merge: do the merge.  Requires --setup to be run first.
# --test:       configure ffmpeg for the host machine, and try running media
#               unit tests and the ffmpeg regression tests.
# --build-gn:   build ffmpeg configs for all platforms, then generate gn config.
# --patches:    generate chromium/patches/README and commit it locally.

import optparse
import os
import sys
from subprocess import check_output

import robo_branch
from robo_lib import shell
from robo_lib import errors
from robo_lib import config
import robo_build
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


def BuildGnConfigsUnconditionally(robo_configuration):
    robo_build.BuildAndImportAllFFmpegConfigs(robo_configuration)
    # Run sanity checks on the merge before we commit.
    robo_branch.CheckMerge(robo_configuration)
    # Write the config changes to help the reviewer.
    robo_branch.WriteConfigChangesFile(robo_configuration)
    # TODO(liberato): Add the 'autodetect' regex too.
    # Handle autorenames last, so that we don't stage things and then fail.
    # While it's probably okay, it's nicer if we don't.
    robo_branch.HandleAutorename(robo_configuration)
    robo_branch.AddAndCommit(robo_configuration,
                             robo_configuration.gn_commit_title())


# Array of steps that this script knows how to perform.  Each step is a
# dictionary that has the following keys:
# desc:   (required) user-friendly description of this step.
# pre_fn: (optional) function that will be run first to test if all required
#         prerequisites are done.  Should throw an exception if not.
# skip_fn:  (optional) function that will be run after |pre_fn| to determine if
#           this step is already done / not required.  Should return True to
#           skip the step, False to do it.
# do_fn: (required) function that runs this step.
steps = {
    "install_prereqs": {
        "desc": "Install required software.",
        "do_fn": robo_setup.InstallPrereqs
    },
    "ensure_toolchains": {
        "desc": "Download mac / win toolchains",
        "do_fn": robo_setup.EnsureToolchains
    },
    "ensure_new_asan_dir": {
        "desc": "Create ninja ASAN output directory",
        "do_fn": robo_setup.EnsureNewASANDirWorks
    },
    "ensure_x86_dir": {
        "desc": "Create ninja x86 output directory",
        "do_fn": robo_setup.Ensurex86ChromeOutputDir
    },
    "ensure_nasm": {
        "desc": "Compile chromium's nasm if needed",
        "do_fn": robo_setup.EnsureChromiumNasm
    },
    "ensure_remote": {
        "desc": "Set git remotes if needed",
        "do_fn": robo_setup.EnsureUpstreamRemote
    },

    # Convenience roll-up for --setup
    "setup": {
        "do_fn":
        lambda cfg: RunSteps(cfg, [
            "install_prereqs", "ensure_toolchains", "ensure_new_asan_dir",
            "ensure_x86_dir", "ensure_nasm", "ensure_remote"
        ])
    },

    # TODO(liberato): consider moving the "if needed" to |req_fn|.
    "erase_build_output": {
        "desc": "Once, at the start of the merge, delete build_ffmpeg output.",
        "do_fn": robo_build.ObliterateOldBuildOutputIfNeeded
    },
    "create_sushi_branch": {
        "desc": "Create a sushi-MDY branch if we're not on one",
        "do_fn": robo_branch.CreateAndCheckoutDatedSushiBranchIfNeeded
    },
    "merge_from_upstream": {
        "desc":
        "Merge upstream/master to our local sushi-MDY branch if needed",  # nocheck
        "do_fn": robo_branch.MergeUpstreamToSushiBranchIfNeeded
    },
    "push_merge_to_origin": {
        "desc": """Push the merge commit, without review, to origin/sushi-MDY,
                   if needed.  Also sets the local sushi-MDY to track it, so
                   that 'git cl upload' won't try to upload it for review.""",
        "do_fn": robo_branch.PushToOriginWithoutReviewAndTrackIfNeeded
    },
    "build_gn_configs": {
        "desc": "Build gn configs (slow), and commit the results locally.",
        "skip_fn": AreGnConfigsDone,
        "do_fn": BuildGnConfigsUnconditionally
    },
    "update_patches_file": {
        "desc":
        "Rewrite chromium/patches/README and commit locally if needed.",
        "skip_fn": robo_branch.IsPatchesFileDone,
        "do_fn": robo_branch.UpdatePatchesFileUnconditionally
    },
    "update_chromium_readme": {
        "desc": "Rewrite README.chromium to reflect the upstream SHA-1.",
        "skip_fn": robo_branch.IsChromiumReadmeDone,
        "do_fn": robo_branch.UpdateChromiumReadmeWithUpstream
    },
    "run_tests": {
        "desc": "Compile and run ffmpeg_regression_tests and media_unittests",
        "do_fn": robo_build.RunTests
    },
    "build_x86": {
        "desc": "Compile media_unittests for x86 to make sure it builds",
        "do_fn": robo_build.BuildChromex86
    },
    "upload_for_review": {
        "desc": "Upload everything to Gerrit for review, if needed",
        "skip_fn": robo_branch.IsUploadedForReview,
        "do_fn": robo_branch.UploadForReview
    },
    "merge_back_to_origin": {
        "desc": "Once sushi has landed after review, merge/push to origin",
        "do_fn": robo_branch.MergeBackToOriginMaster
    },

    # This is a WIP, present in case you're feeling particularly brave.  :)
    "start_fake_deps_roll": {
        "desc":
        "Try a test deps roll against the sushi (not master) branch",  # nocheck
        "do_fn": robo_branch.TryFakeDepsRoll
    },

    # This is a WIP, present in case you're feeling even more brave.  :)
    "start_real_deps_roll": {
        "desc": "Try a real deps roll against the sushi branch",
        "do_fn": robo_branch.TryRealDepsRoll
    },
    "win_the_game": {
        "desc": "Print a happy message when things have completed.",
        "do_fn": robo_branch.PrintHappyMessage
    },

    # Some things you probably don't need unless you're debugging.
    "download_mac_sdk": {
        "desc": "Try to download the mac SDK, if needed.",
        "do_fn": robo_setup.FetchMacSDK
    },

    # Roll-up for --auto-merge
    "auto-merge": {
        "do_fn":
        lambda cfg: RunSteps(
            cfg,
            [
                "erase_build_output",
                "create_sushi_branch",
                "merge_from_upstream",
                "push_merge_to_origin",
                "build_gn_configs",
                "update_patches_file",
                "update_chromium_readme",
                # TODO: If the tests fail, and this is a manual roll, then the right thing
                # to do is to upload the gn config / patches for review and land it.
                "run_tests",
                "build_x86",
                "upload_for_review",
                "merge_back_to_origin",
                "start_real_deps_roll",
                "print_happy_message",
            ])
    },
}


def RunSteps(cfg, step_names):
    for step_name in step_names:
        if not step_name in steps:
            raise Exception("Unknown step %s" % step_name)
        shell.log("Step %s" % step_name)
        step = steps[step_name]
        try:
            if "pre_fn" in step:
                raise Exception("pre_fn not supported yet")
            if cfg.skip_allowed() and "skip_fn" in step:
                if step["skip_fn"](cfg):
                    shell.log("Step %s not needed, skipping" % step_name)
                    continue
            step["do_fn"](cfg)
        except Exception as e:
            shell.log("Step %s failed" % step_name)
            raise e


def ListSteps():
    for name, step in steps.items():
        if "desc" in step:
            print(f"{name}: {step['desc']}\n")


def main(argv):
    parser = optparse.OptionParser(usage='Usage: %prog [options]')
    parser.add_option('--branch',
                      action='store',
                      help='Manually set sushi branch name')
    parser.add_option('--prompt',
                      action='store_true',
                      help='Prompt for each robosushi step')
    parser.add_option(
        '--setup',
        action='store_true',
        help='Run initial setup steps. Do this once before auto-merge.')
    parser.add_option('--test',
                      action='store_true',
                      help='Build and run all tests')
    parser.add_option(
        '--build-gn',
        action='store_true',
        help='Unconditionally build all the configs and import them.')
    parser.add_option('--patches',
                      action='store_true',
                      help='Update patches file only')
    parser.add_option('--auto-merge',
                      action='store_true',
                      help='Run auto-merge. (Usually what you want)')
    parser.add_option('--list', action='store_true', help='List steps')
    parser.add_option('--no-skip',
                      action='store_true',
                      help='Don\'t allow any steps to be skipped')
    parser.add_option('--force-gn-rebuild',
                      action='store_true',
                      help='Force rebuild of GN args.')
    parser.add_option('--step', action='append', help='Step to run.')

    options, args = parser.parse_args(argv)

    if options.list:
        ListSteps()
        return 0

    robo_configuration = config.RoboConfiguration()
    robo_configuration.chdir_to_ffmpeg_home()

    exec_steps = []

    if options.prompt:
        robo_configuration.set_prompt_on_call(True)
    if options.no_skip:
        robo_configuration.set_skip_allowed(False)
    if options.force_gn_rebuild:
        robo_configuration.set_force_gn_rebuild()
    if options.branch:
        robo_configuration.SetBranchName(options.branch)

    if options.setup:
        exec_steps += ["setup"]
    if options.test:
        robo_build.BuildAndImportFFmpegConfigForHost(robo_configuration)
        robo_build.RunTests(robo_configuration)
    if options.build_gn:
        # Unconditionally build all the configs and import them.
        robo_build.BuildAndImportAllFFmpegConfigs(robo_configuration)
    if options.patches:
        # To be run after committing a local change to fix the tests.
        if not robo_branch.IsWorkingDirectoryClean():
            raise errors.UserInstructions(
                "Working directory must be clean to generate patches file")
        robo_branch.UpdatePatchesFileUnconditionally(robo_configuration)
    if options.auto_merge:
        exec_steps += ["auto-merge"]
    if options.step:
        exec_steps += options.step

    if len(exec_steps) == 0:
        parser.print_help()
        return 1

    # TODO: make sure that any untracked autorename files are removed, or
    # make sure that the autorename git script doesn't try to 'git rm'
    # untracked files, else the script fails.
    RunSteps(robo_configuration, exec_steps)

    # TODO: Start a fake deps roll.  To do this, we would:
    # Create new remote branch from the current remote sushi branch.
    # Create and check out a new local branch at the current local branch.
    # Make the new local branch track the new remote branch.
    # Push to origin/new remote branch.
    # Start a fake deps roll CL that runs the *san bots.
    # Switch back to original local branch.
    # For extra points, include a pointer to the fake deps roll CL in the
    # local branch, so that when it's pushed for review, it'll point the
    # reviewer at it.
    # TODO: git cl upload for review.


if __name__ == "__main__":
    main(sys.argv[1:])
