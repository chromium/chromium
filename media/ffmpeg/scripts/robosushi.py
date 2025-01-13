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

import dataclasses
import optparse
import os
import sys
from subprocess import check_output
import typing

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


@dataclasses.dataclass
class Target():
    name: str
    desc: str
    func: typing.Callable[[config.RoboConfiguration], None]
    skip: typing.Callable[[config.RoboConfiguration], bool] = None

    @staticmethod
    def SerialTarget(name:str, desc:str, tasks:['Target']) -> 'Target':
        LoadTargets(*tasks)
        return Target(name, desc,
            lambda cfg: RunAllTargets(cfg, [t.name for t in tasks]))

    def can_skip(self, cfg:config.RoboConfiguration):
        return self.skip is not None and self.skip(cfg)

    def execute(self, cfg:config.RoboConfiguration):
        return self.func(cfg)




steps = {}
def LoadTargets(*targets):
    global steps
    for target in targets:
        steps[target.name] = target


def MakeErrorStr(queue:[Target]):
    chain = " > ".join([t.name for t in queue])
    return f"\033[31m{chain}\033[0m"


class StepError(Exception):
    def __init__(self, target_queue:[Target], nested_exception:Exception):
        super().__init__(MakeErrorStr(target_queue))
        self._steps = target_queue
        self._nested = nested_exception

    def RaiseFrom(self, target:Target) -> 'StepError':
        raise StepError([target] + self._steps, self._nested) from self._nested


def RunAllTargets(cfg:config.RoboConfiguration, tasks:[str]):
    global steps
    for target_name in tasks:
        if target_name not in steps:
            raise ValueError(f'Unknown step: {target_name}')
        RunTarget(steps[target_name], cfg)



def RunTarget(target:Target, cfg:config.RoboConfiguration):
    shell.log(f'loading step: `{target.name}`')
    try:
        if cfg.skip_allowed() and target.can_skip(cfg):
            shell.log(f'  skipped step: `{target.name}`')
        else:
            target.execute(cfg)
    except StepError as se:
        se.RaiseFrom(target)
    except Exception as e:
        raise StepError([target], e) from None


LoadTargets(
    Target(name="start_fake_deps_roll",
           desc="Try a test deps roll against the sushi (not master) branch",
           func=robo_branch.TryFakeDepsRoll),
    Target(name="download_mac_sdk",
           desc="Try to download the mac SDK, if needed.",
           func=robo_setup.FetchMacSDK),
    Target.SerialTarget(
        name="setup",
        desc="Convenience roll-up for --setup",
        tasks=[Target(name = "install_prereqs",
                      desc = "Install required software",
                      func = robo_setup.InstallPrereqs),
               Target(name = "ensure_toolchains",
                      desc = "Download mac / win toolchains",
                      func = robo_setup.EnsureToolchains),
               Target(name="ensure_new_asan_dir",
                      desc="Create ninja ASAN output directory",
                      func=robo_setup.EnsureNewASANDirWorks),
               Target(name="ensure_x86_dir",
                      desc="Create ninja x86 output directory",
                      func=robo_setup.Ensurex86ChromeOutputDir),
               Target(name="ensure_nasm",
                      desc="Compile chromium's nasm if needed",
                      func=robo_setup.EnsureChromiumNasm),
               Target(name="ensure_remote",
                      desc="Set git remotes if needed",
                      func=robo_setup.EnsureUpstreamRemote)]),
    Target.SerialTarget(
        name="auto-merge",
        desc="Roll-up for --auto-merge",
        tasks=[Target(name="erase_build_output",
                    desc="Once, at the start of the merge, delete build_ffmpeg"
                         " output.",
                    func=robo_build.ObliterateOldBuildOutputIfNeeded),
              Target(name="create_sushi_branch",
                     desc="Create a sushi-MDY branch if we're not on one",
                     func=robo_branch.CreateAndCheckoutDatedSushiBranchIfNeeded),
              Target(name="merge_from_upstream",
                     desc="Merge upstream/master to our local sushi-MDY branch",
                     func=robo_branch.MergeUpstreamToSushiBranchIfNeeded),
              Target(name="push_merge_to_origin",
                     desc="Push the merge commit, without review, to"
                          " origin/sushi-MDY.",
                     func=robo_branch.PushToOriginWithoutReviewAndTrack),
              Target(name="build_gn_configs",
                     desc="Build gn configs, and commit the results locally.",
                     skip=AreGnConfigsDone,
                     func=BuildGnConfigsUnconditionally),
              Target(name="update_patches_file",
                     desc="Rewrite chromium/patches/README and commit locally.",
                     skip=robo_branch.IsPatchesFileDone,
                     func=robo_branch.UpdatePatchesFileUnconditionally),
              Target(name="update_chromium_readme",
                     desc="Rewrite README.chromium to reflect upstream SHA-1.",
                     skip=robo_branch.IsChromiumReadmeDone,
                     func=robo_branch.UpdateChromiumReadmeWithUpstream),
              Target(name="run_tests",
                     desc="Compile and run ffmpeg_regression_tests and "
                          "media_unittests",
                     func=robo_build.RunTests),
              Target(name="build_x86",
                     desc="Compile media_unittests for x86 to ensure building",
                     func=robo_build.BuildChromex86),
              Target(name="upload_for_review",
                     desc="Upload everything to Gerrit for review, if needed",
                     skip=robo_branch.IsUploadedForReview,
                     func=robo_branch.UploadForReview),
              Target(name="merge_back_to_origin",
                     desc="Sushi has landed post review, merge/push to origin",
                     func=robo_branch.MergeBackToOriginMaster),
              Target(name="start_real_deps_roll",
                     desc="Try a real deps roll against the sushi branch",
                     func=robo_branch.TryRealDepsRoll),
              Target(name="print_happy_message",
                     desc="Print a happy message when things have completed.",
                     func=robo_branch.PrintHappyMessage)]),
)


def ListSteps():
    for name, step in steps.items():
        print(f'{name}: {step.desc}')


def main(argv):
    parser = optparse.OptionParser(usage='Usage: %prog [options]')
    parser.add_option('--branch',
                      action='store',
                      help='Manually set sushi branch name')
    parser.add_option('--prompt',
                      action='store_true',
                      help='Prompt for each robosushi step')
    parser.add_option('--verbose',
                      action='store_true',
                      help='Log all shell calls')
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
    if options.verbose:
        robo_configuration.set_log_shell_calls(True)
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
    RunAllTargets(robo_configuration, exec_steps)

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
