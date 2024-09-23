# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

""" Contains the global configuration object.
"""

import os
import platform
import re
from . import shell
from . import packages
from . import errors


class RoboConfiguration:
    __slots__ = ('_sushi_branch_prefix', '_gn_commit_title',
                 '_patches_commit_title', '_readme_chromium_commit_title',
                 '_origin_merge_base', '_llvm_path', '_autorename_git_file',
                 '_chrome_src', '_host_operating_system', '_host_architecture',
                 '_ffmpeg_home', '_relative_asan_directory', '_branch_name',
                 '_sushi_branch_name', '_readme_chromium_commit_title',
                 '_nasm_path', '_prompt_on_call', '_os_flavor',
                 '_force_gn_rebuild', '_skip_allowed', '_script_directory',
                 '_relative_x86_directory')

    def __init__(self, quiet=False):
        # Important: Robosushi might be running the --setup command, so some of
        # the sanity checks should only be for things that we don't plan to fix
        # as part of that.

        self.set_prompt_on_call(False)
        # Pull this from parsed args
        self._force_gn_rebuild = False
        # Allowed to skip steps, default to yes, needs --no-skip flag to disable.
        self._skip_allowed = True
        # This is the prefix that our branches start with.
        self._sushi_branch_prefix = "sushi-"
        # This is the title that we use for the commit with GN configs.
        self._gn_commit_title = "GN Configuration"
        # Title of the commit with chromium/patches/README.
        self._patches_commit_title = "Chromium patches file"
        # Title of the commit with README.chromium
        self._readme_chromium_commit_title = "README.chromium file"

        self.EnsureHostInfo()
        self.EnsureChromeSrc()
        self.EnsureScriptDirectory()

        # Origin side of the merge. Only needs to change if you're trying to
        # modify and test robosushi itself. See robosushi.py for details.
        self._origin_merge_base = "origin/master"  # nocheck

        # Directory where llvm lives.
        self._llvm_path = os.path.join(self.chrome_src(), "third_party",
                                       "llvm-build", "Release+Asserts", "bin")

        self.EnsurePathContainsLLVM()
        self.EnsureNoMakeInfo()
        self.EnsureFFmpegHome()
        self.EnsureGNConfig()
        self.ComputeBranchName()

        if not quiet:
            shell.log(f"Using chrome src: {self.chrome_src()}")
            shell.log(f"Using script dir: {self._script_directory}")
            shell.log(f"Using ffmpeg home: {self.ffmpeg_home()}")
            shell.log(f"On branch: {self.branch_name()}")
            if self.sushi_branch_name():
                shell.log(f"On sushi branch: {self.sushi_branch_name()}")

        # Filename that we'll ask generate_gn.py to write git commands to.
        # TODO: Should this use script_directory, or stay with ffmpeg?  As long as
        # there's a .gitignore entry, either should be fine.
        self._autorename_git_file = os.path.join(self.ffmpeg_home(),
                                                 "scripts",
                                                 ".git_commands.sh")

    def chrome_src(self):
        return self._chrome_src

    def chdir_to_chrome_src(self):
        os.chdir(self.chrome_src())

    def scripts_dir(self):
        return self._script_directory

    def get_script_path(self, *parts):
        return os.path.join(self.scripts_dir(), *parts)

    def ffmpeg_home(self):
        return self._ffmpeg_home

    def chdir_to_ffmpeg_home(self):
        os.chdir(self.ffmpeg_home())

    def ffmpeg_src(self):
        # ffmpeg_src is currently the same as ffmpeg_home, but this is not
        # required to be the same place if sources were to move.
        return self.ffmpeg_home()

    def chdir_to_ffmpeg_src(self):
        os.chdir(self.ffmpeg_src())

    def target_config_directory(self, arch, opsys, target):
        return os.path.join(self.ffmpeg_home(), f'build.{arch}.{opsys}', target)

    def patches_dir_location(self):
        return os.path.join(self.ffmpeg_home(), "chromium", "patches")

    def exported_configs_directory(self, arch, opsys, target):
        return os.path.join(
            self.ffmpeg_home(), "chromium", "config", target, opsys, arch)

    def autorename_git_file(self):
        return self.get_script_path('git_commands.sh')

    def host_operating_system(self):
        """Return host type, e.g. "linux" """
        return self._host_operating_system

    def host_architecture(self):
        """Return host architecture, e.g. "x64" """
        return self._host_architecture

    def relative_asan_directory(self):
        return self._relative_asan_directory

    def absolute_asan_directory(self):
        return os.path.join(self.chrome_src(), self.relative_asan_directory())

    def relative_x86_directory(self):
        return self._relative_x86_directory

    def absolute_x86_directory(self):
        return os.path.join(self.chrome_src(), self.relative_x86_directory())

    def branch_name(self):
        """Return the current workspace's branch name, or None."""
        return self._branch_name

    def sushi_branch_name(self):
        """If the workspace is currently on a branch that we created (a "sushi
    branch"), return it.  Else return None."""
        return self._sushi_branch_name

    def sushi_branch_prefix(self):
        """Return the branch name that indicates that this is a "sushi branch"."""
        return self._sushi_branch_prefix

    def gn_commit_title(self):
        return self._gn_commit_title

    def patches_commit_title(self):
        return self._patches_commit_title

    def readme_chromium_commit_title(self):
        return self._readme_chromium_commit_title

    def nasm_path(self):
        return self._nasm_path

    def origin_merge_base(self):
        return self._origin_merge_base

    def override_origin_merge_base(self, new_base):
        self._origin_merge_base = new_base

    def os_flavor(self):
        return self._os_flavor

    def force_gn_rebuild(self):
        return self._force_gn_rebuild

    def set_force_gn_rebuild(self):
        self._force_gn_rebuild = True

    def skip_allowed(self):
        return self._skip_allowed

    def set_skip_allowed(self, to):
        self._skip_allowed = to

    def EnsureHostInfo(self):
        """Ensure that the host architecture and platform are set."""

        if re.match(r"i.86", platform.machine()):
            self._host_architecture = "ia32"
        elif platform.machine() == "x86_64" or platform.machine() == "AMD64":
            self._host_architecture = "x64"
        elif platform.machine() == "aarch64":
            self._host_architecture = "arm64"
        elif platform.machine() == "mips32":
            self._host_architecture = "mipsel"
        elif platform.machine() == "mips64":
            self._host_architecture = "mips64el"
        elif platform.machine().startswith("arm"):
            self._host_architecture = "arm"
        else:
            raise ValueError(
                f"Unrecognized CPU architecture: {platform.machine()}")

        if platform.system() == "Linux":
            self._host_operating_system = "linux"

            try:
                with open("/etc/lsb-release", "r") as f:
                    result = f.read()
                    if "Ubuntu" in result or "Debian" in result:
                        self._os_flavor = packages.OsFlavor.Debian
                    elif "Arch" in result:
                        self._os_flavor = packages.OsFlavor.Arch
                    else:
                        raise Exception(
                            "Couldn't determine OS flavor from lsb-release "
                            "(needed to install packages)")
            except:
                raise Exception(
                    "Couldn't read OS flavor from /etc/lsb-release file "
                    "(needed to install packages)")
        elif platform.system() == "Darwin":
            self._host_operating_system = "mac"
        elif platform.system() == "Windows" or "CYGWIN_NT" in platform.system(
        ):
            self._host_operating_system = "win"
        else:
            raise ValueError(f"Unsupported platform: {platform.system()}")

    def EnsureChromeSrc(self):
        """Find the /absolute/path/to/my_chrome_dir/src"""
        wd = os.getcwd()
        # Walk up the tree until we find src/AUTHORS
        while wd != "/":
            if os.path.isfile(os.path.join(wd, "src", "AUTHORS")):
                self._chrome_src = os.path.join(wd, "src")
                return
            wd = os.path.dirname(wd)
        raise Exception("could not find src/AUTHORS in any parent of the wd")

    def EnsureScriptDirectory(self):
        """Make sure we know where the scripts are."""
        # Assume that __func__ is /.../scripts/robo_lib/something.py
        self._script_directory = os.path.dirname(os.path.dirname(__file__))
        # Verify that `robosushi.py` is in this directory, for sanity.
        if not os.path.isfile(self.get_script_path("robosushi.py")):
            raise Exception("Fix EnsureScriptDir -- cannot find robosushi.py")

    def EnsureFFmpegHome(self):
        """Ensure that |self| has "ffmpeg_home" set."""
        self._ffmpeg_home = os.path.join(self.chrome_src(), "third_party",
                                         "ffmpeg")

    def EnsureGNConfig(self):
        """Find the gn directories.  Note that we don't create them."""
        # These are suitable for 'autoninja -C'
        self._relative_asan_directory = os.path.join("out", "sushi_asan")
        self._relative_x86_directory = os.path.join("out", "sushi_x86")

    def EnsurePathContainsLLVM(self):
        """Make sure that we have chromium's LLVM in $PATH.

    We don't want folks to accidentally use the wrong clang.
    """

        llvm_path = os.path.join(self.chrome_src(), "third_party",
                                 "llvm-build", "Release+Asserts", "bin")
        if self.llvm_path() not in os.environ["PATH"]:
            raise errors.UserInstructions(
                "Please add:\n%s\nto the beginning of $PATH\nExample: export PATH=%s:$PATH"
                % (self.llvm_path(), self.llvm_path()))

    def EnsureNoMakeInfo(self):
        """Ensure that makeinfo is not available."""
        if os.system("makeinfo --version > /dev/null 2>&1") == 0:
            raise errors.UserInstructions(
                "makeinfo is available and we don't need it, so please remove it\nExample: sudo apt-get remove texinfo"
            )

    def llvm_path(self):
        return self._llvm_path

    def ComputeBranchName(self):
        """Get the current branch name and set it."""
        self.chdir_to_ffmpeg_src()
        branch_name = shell.output_or_error(
            ["git", "rev-parse", "--abbrev-ref", "HEAD"])
        self.SetBranchName(str(branch_name))

    def SetBranchName(self, name):
        """Set our branch name, which may be a sushi branch or not."""
        self._branch_name = name
        # If this is one of our branches, then record that too.
        if name and not name.startswith(self.sushi_branch_prefix()):
            name = None
        self._sushi_branch_name = name

    def prompt_on_call(self):
        """ Return True if and only if we're supposed to ask the user before running
    any command that might have a side-effect."""
        return self._prompt_on_call

    def set_prompt_on_call(self, value):
        self._prompt_on_call = value

    def Call(self, args, **kwargs):
        """Run the command specified by |args| (see subprocess.call), optionally
    prompting the user."""
        if self.prompt_on_call():
            print(f"[{os.getcwd()}] About to run: `{' '.join(args)}` ")
            input("Press ENTER to continue, or interrupt the script: ")
        return shell.check_run(args, **kwargs)
