# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''This file describes the nested steps necessary for robosushi to run.

This file specifically _does not_ contain implementations for any of these
steps in order to avoid mixing the roll logic with a bunch of wrappers for a
command line or git script calls.

Task objects are self-registering. Simply importing this file is sufficient for
loading all objects.
'''

from robo_lib.task import Task

import robo_branch
import robo_build
import robo_setup


# I'm not actually sure when / why this is needed.
Task(name="download_mac_sdk",
       desc="Try to download the mac SDK, if needed.",
       func=robo_setup.FetchMacSDK),


# Sets up the user's environment for running the rest of robosushi
Task.Serial(
    name="setup",
    desc="Convenience roll-up for --setup",
    tasks=[Task(name = "install_prereqs",
                desc = "Install required software",
                func = robo_setup.InstallPrereqs),
           Task(name = "ensure_toolchains",
                desc = "Download mac / win toolchains",
                func = robo_setup.EnsureToolchains),
           Task(name="ensure_new_asan_dir",
                desc="Create ninja ASAN output directory",
                func=robo_setup.EnsureNewASANDirWorks),
           Task(name="ensure_x86_dir",
                desc="Create ninja x86 output directory",
                func=robo_setup.Ensurex86ChromeOutputDir),
           Task(name="ensure_nasm",
                desc="Compile chromium's nasm if needed",
                func=robo_setup.EnsureChromiumNasm),
           Task(name="ensure_remote",
                desc="Set git remotes if needed",
                func=robo_setup.EnsureUpstreamRemote)]),


# Does A thing???
Task(name="start_fake_deps_roll",
       desc="Try a test deps roll against the sushi (not master) branch",
       func=robo_branch.TryFakeDepsRoll),


'''
The general steps for the merge are as follows:
  - clean up any existing builds
  - create a branch in third_party/ffmpeg called 'sushi-{date}'
  - create a branch in chromium/src called 'sashimi-{date}'
  - merge ffmpeg@(upstream/master) into 'sushi-{date}'
  - push 'sushi-{date}' to 'origin/sushi-{date}'
  - do
    - build gn configs
    - build unit tests and full browser
    - run unit tests locally (this only gives partial coverage!)
  - while {ffmpeg_requires_changes}
  - git cl upload 'sushi-{date}' to get config change reviews
  - commit an ffmpeg dep roll to 'sashimi-{date}'
  - upload this dep roll, run against bots for full coverage
  - get approval from ffmpeg owners
  - merge 'sushi-{date}' into 'origin/master'
  - if the deps roll required no changes to chromium, autoroll will work, else:
    - update deps roll to 'origin/master' commit
    - submit deps roll CL.
'''
Task.Serial(
    name="auto-merge",
    desc="Roll-up for --auto-merge",
    tasks=[
        Task(name="erase_build_output",
             desc="Once, at the start of the merge, delete build_ffmpeg"
                  " output.",
             func=robo_build.ObliterateOldBuildOutputIfNeeded),
        Task(name="create_sushi_branch",
               desc="Create a sushi-MDY branch if we're not on one",
               func=robo_branch.CreateAndCheckoutSushiBranchIfNeeded),
        Task(name="merge_from_upstream",
               desc="Merge upstream/master to our local sushi-MDY branch",
               func=robo_branch.MergeUpstreamToSushiBranchIfNeeded),
        Task(name="push_merge_to_origin",
               desc="Push the merge commit, without review, to"
                    " origin/sushi-MDY.",
               func=robo_branch.PushToOriginWithoutReviewAndTrack),
        Task.Serial(
             name="build_gn_configs",
             desc="Build gn configs, and commit the results locally.",
             skip=robo_branch.AreGnConfigsDone,
             tasks=[
                 Task(
                    name="build_ffmpeg_configs",
                    desc="Build and import all the ffmpeg configs",
                    func=robo_build.BuildAndImportAllFFmpegConfigs),
                 Task(
                    name="check_merge",
                    desc="Run sanity checks on the merge before we commit",
                    func=robo_branch.CheckMerge),
                 Task(
                    name="write config_changes",
                    desc="Write config changes to help the reviewer",
                    func=robo_branch.WriteConfigChangesFile),
                 Task(
                    name="handle_autorename",
                    desc="Handle autorenames last so that we dont stage things"
                         " and then fail. It's nicer this way trust me.",
                    func=robo_branch.HandleAutorename),
                 Task(
                    name="commit_configs",
                    desc="Commit the configs",
                    func=robo_branch.AddAndCommit),
             ]),
        Task(name="update_chromium_readme",
               desc="Rewrite README.chromium to reflect upstream SHA-1.",
               skip=robo_branch.IsChromiumReadmeDone,
               func=robo_branch.UpdateChromiumReadmeWithUpstream),
        Task(name="run_tests",
               desc="Compile and run ffmpeg_regression_tests and "
                    "media_unittests",
               func=robo_build.RunTests),
        Task(name="build_x86",
               desc="Compile media_unittests for x86 to ensure building",
               func=robo_build.BuildChromex86),
        Task(name="upload_for_review",
               desc="Upload everything to Gerrit for review, if needed",
               skip=robo_branch.IsUploadedForReview,
               func=robo_branch.UploadForReview),
        Task(name="start_fake_deps_roll",
             desc="Try a test deps roll against the sushi (not master) branch",
             func=robo_branch.TryFakeDepsRoll,
             skip=robo_branch.DoesFakeDepsRollExist),
        Task(name="merge_back_to_origin",
               desc="Sushi has landed post review, merge/push to origin",
               func=robo_branch.MergeBackToOriginMaster),
        Task(name="start_real_deps_roll",
               desc="Try a real deps roll against the sushi branch",
               func=robo_branch.TryRealDepsRoll),
        Task(name="print_happy_message",
               desc="Print a happy message when things have completed.",
               func=robo_branch.PrintHappyMessage)
        ])
