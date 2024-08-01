#!/usr/bin/env python3
#
# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import atexit
import collections
import functools
import glob
import optparse
import os
import platform
import re
import shlex
import shutil
import signal
import subprocess
import sys
import tempfile
from robo_lib import config

ROBO_CONFIGURATION = config.RoboConfiguration()
FFMPEG_DIR = ROBO_CONFIGURATION.ffmpeg_src()
FFMPEG_HOME = ROBO_CONFIGURATION.ffmpeg_home()
CHROMIUM_ROOT_DIR = ROBO_CONFIGURATION.chrome_src()
NDK_ROOT_DIR = os.path.abspath(
    os.path.join(CHROMIUM_ROOT_DIR, 'third_party', 'android_toolchain', 'ndk'))
# Token to indicate that a build has completed successfully, so that we can
# skip it with `--fast`.
SUCCESS_TOKEN = 'THIS_BUILD_WORKED'

sys.path.append(os.path.join(CHROMIUM_ROOT_DIR, 'build'))
import gn_helpers

BRANDINGS = [
    'Chrome',
    'Chromium',
]

ARCH_MAP = {
    'android': ['ia32', 'x64', 'arm-neon', 'arm64'],
    'linux': ['ia32', 'x64', 'noasm-x64', 'arm', 'arm-neon', 'arm64'],
    'mac': ['x64', 'arm64'],
    'win': ['ia32', 'x64', 'arm64'],
}

USAGE_BEGIN = """Usage: %prog TARGET_OS TARGET_ARCH [options] -- [configure_args]"""
USAGE_END = """
Valid combinations are android     [%(android)s]
                       linux       [%(linux)s]
                       mac         [%(mac)s]
                       win         [%(win)s]

If no target architecture is specified all will be built. Usually you don't want
to run this script manually and should instead be using robosushi.py.

Platform specific build notes:
  android:
    Script can be run on a normal x64 Ubuntu box with an Android-ready Chromium
    checkout: https://chromium.googlesource.com/chromium/src/+/master/docs/android_build_instructions.md

  linux ia32/x64:
    Script can run on a normal Ubuntu box.

  linux arm/arm-neon/arm64/mipsel/mips64el:
    Script can run on a normal Ubuntu with ARM/ARM64 or MIPS32/MIPS64 ready Chromium checkout:
      build/linux/sysroot_scripts/install-sysroot.py --arch=arm
      build/linux/sysroot_scripts/install-sysroot.py --arch=arm64
      build/linux/sysroot_scripts/install-sysroot.py --arch=mips
      build/linux/sysroot_scripts/install-sysroot.py --arch=mips64el

  mac:
    Script must be run on Linux or macOS.  Additionally, ensure the Chromium
    (not Apple) version of clang is in the path; usually found under
    src/third_party/llvm-build/Release+Asserts/bin

    The arm64 version has to be built with an SDK that can build mac/arm64
    binaries -- currently Xcode 12 beta and its included 11.0 SDK. You must
    pass --enable-cross-compile to be able to build ffmpeg for mac/arm64 on an
    Intel Mac. On a Mac, run like so:
        PATH=$PWD/../../third_party/llvm-build/Release+Asserts/bin:$PATH \
        SDKROOT=/Applications/Xcode-beta.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX11.0.sdk \
        chromium/scripts/build_ffmpeg.py mac arm64 -- --enable-cross-compile

    On Linux, the normal robosushi flow will work for arm64.

  win:
    Script may be run unders Linux or Windows; if cross-compiling you will need
    to follow the Chromium instruction for Cross-compiling Chrome/win:
    https://chromium.googlesource.com/chromium/src/+/master/docs/win_cross.md

    Once you have a working Chromium build that can cross-compile, you'll also
    need to run $chrome_dir/tools/clang/scripts/update.py --package=objdump to
    pick up the llvm-ar and llvm-nm tools. You can then build as normal.

    If not cross-compiling, script must be run on Windows with VS2015 or higher
    under Cygwin (or MinGW, but as of 1.0.11, it has serious performance issues
    with make which makes building take hours).

    Additionally, ensure you have the correct toolchain environment for builds.
    The x86 toolchain environment is required for ia32 builds and the x64 one
    for x64 builds.  This can be verified by running "cl.exe" and checking if
    the version string ends with "for x64" or "for x86."

    Building on Windows also requires some additional Cygwin packages plus a
    wrapper script for converting Cygwin paths to DOS paths.
      - Add these packages at install time: diffutils, nasm, make, python.
      - Copy chromium/scripts/cygwin-wrapper to /usr/local/bin

Resulting binaries will be placed in:
  build.TARGET_ARCH.TARGET_OS/Chrome/
  build.TARGET_ARCH.TARGET_OS/Chromium/
  """


def PrintAndCheckCall(argv, *args, **kwargs):
    print('Running %s' % '\n '.join(argv))
    subprocess.check_call(argv, *args, **kwargs)


def GetDsoName(target_os, dso_name, dso_version):
    if target_os in ('linux', 'linux-noasm', 'android'):
        return 'lib%s.so.%s' % (dso_name, dso_version)
    elif target_os == 'mac':
        return 'lib%s.%s.dylib' % (dso_name, dso_version)
    elif target_os == 'win':
        return '%s-%s.dll' % (dso_name, dso_version)
    else:
        raise ValueError('Unexpected target_os %s' % target_os)


def RewriteFile(path, search_replace):
    with open(path) as f:
        contents = f.read()
    with open(path, 'w') as f:
        for search, replace in search_replace:
            contents = re.sub(search, replace, contents)
        f.write(contents)


# Class for determining the 32-bit and 64-bit Android API levels that Chromium
# uses. Since @functools.cache is not available for easy memoization of the
# determination result, we use a lazy singleton instance constructed by calling
# Get().
class AndroidApiLevels:
    __instance = None

    # Extracts the Android API levels from the Chromium Android GN config.
    # Before Q1 2021, these were grep'able from build/config/android/config.gni.
    # With conditional logic introduced in that gni file, we seek to avoid
    # fragility going forwards, at the cost of creating a full temporary GN
    # Chromium Android build configuration just to extract the API levels here.
    # Caches the results in api32 and api64 instance variables.
    def Setup(self):
        print('Creating a temporary GN config to retrieve Android API levels:')

        # Make a temporary GN build output folder
        # No tempfile.TemporaryDirectory until python 3.2, so instead:
        tmp_dir = tempfile.mkdtemp(
            prefix='android_build_ffmpeg_for_api_level_config')
        print('Created temporary directory ' + tmp_dir)

        # Populate that GN build output folder with generated config for Android as
        # target OS.
        with open(os.path.join(tmp_dir, 'args.gn'), 'w') as args_gn_file:
            args_gn_file.write('target_os = "android"\n')
            print('Created ' + os.path.realpath(args_gn_file.name))

        # Ask GN to generate build files.
        PrintAndCheckCall(['gn', 'gen', tmp_dir], cwd=CHROMIUM_ROOT_DIR)

        # Query the API levels in the generated build config.
        print('Retrieving config vars')
        config_output = subprocess.check_output(
            ['gn', 'args', tmp_dir, '--short', '--list'],
            cwd=CHROMIUM_ROOT_DIR).decode('utf-8')

        # Remove the temporary GN build output folder
        print('removing temp dir ' + tmp_dir)
        shutil.rmtree(tmp_dir, ignore_errors=False)

        api64_match = re.search(r'android64_ndk_api_level\s*=\s*(\d{2})',
                                config_output)
        api32_match = re.search(r'android32_ndk_api_level\s*=\s*(\d{2})',
                                config_output)
        if not api32_match or not api64_match:
            raise Exception('Failed to find the android api levels')

        self.api32 = api32_match.group(1)
        self.api64 = api64_match.group(1)

    def ApiLevels(self):
        return (self.api32, self.api64)

    @classmethod
    def Get(cls):
        if cls.__instance is None:
            cls.__instance = AndroidApiLevels()
            cls.__instance.Setup()
        return cls.__instance.ApiLevels()


# Sets up cross-compilation (specific to host being linux-x64_64) for compiling
# Android.
# Returns the necessary configure flags as a list.
# See also https://developer.android.com/ndk/guides/other_build_systems
# As of M90, //third_party/android_ndk no longer includes mipsel or mips64el
# toolchains; they were not previously supported by default by this script, and
# currently are unsupported due to lack of toolchain in checkout.
def SetupAndroidToolchain(target_arch):
    api_level, api64_level = AndroidApiLevels.Get()
    print('Determined Android API levels: 32bit=' + api_level + ', 64bit=' +
          api64_level)

    # Toolchain prefix misery, for when just one pattern is not enough :/
    toolchain_level = api_level
    toolchain_bin_prefix = target_arch

    if target_arch == 'arm-neon' or target_arch == 'arm':
        toolchain_bin_prefix = 'arm-linux-androideabi'
    elif target_arch == 'arm64':
        toolchain_level = api64_level
        toolchain_bin_prefix = 'aarch64-linux-android'
    elif target_arch == 'ia32':
        toolchain_bin_prefix = 'i686-linux-android'
    elif target_arch == 'x64':
        toolchain_level = api64_level
        toolchain_bin_prefix = 'x86_64-linux-android'
    elif target_arch == 'mipsel':  # Unsupported beginning in M90
        toolchain_bin_prefix = 'mipsel-linux-android'
    elif target_arch == 'mips64el':  # Unsupported beginning in M90
        toolchain_level = api64_level
        toolchain_bin_prefix = 'mips64el-linux-android'

    clang_toolchain_dir = NDK_ROOT_DIR + '/toolchains/llvm/prebuilt/linux-x86_64/'

    # Big old nasty hack here, beware! The new android ndk has some foolery with
    # libgcc.a -- clang still uses gcc for its linker when cross compiling.
    # It can't just be that simple though - the |libgcc.a| file is actually a
    # super secret linkerscript which links libgcc_real.a, because apparently
    # someone decided that more flags are needed, including -lunwind; that's where
    # our story begins. ffmpeg doesn't use linunwind, and we dont really have a
    # good way to get a cross-compiled version anyway, but this silly linker
    # script insists that we must link with it, or face all sorts of horrible
    # consequences -- namely configure failures. Anyway, there is a way around it:
    # the "big old nasty hack" mentioned what feels like forever ago now. It's
    # simple, we uhh, kill the batman. Actually we just make a fake libunwind.a
    # linker script and drop it someplace nobody will ever find, like I dunno, say
    # /tmp/fakelinkerscripts or something. Then we add that path to the ldflags
    # flags and everything works again.
    fakedir = '/tmp/fakelinkerscripts'
    os.system('mkdir -p {fakedir} && touch {fakedir}/libunwind.a'.format(
        fakedir=fakedir))

    return [
        '--enable-pic',
        '--cc=clang',
        '--cxx=clang++',
        '--ld=clang',
        '--enable-cross-compile',
        '--sysroot=' + clang_toolchain_dir + 'sysroot',
        '--extra-cflags=-I' + clang_toolchain_dir + 'sysroot/usr/include',
        '--extra-cflags=-I' + clang_toolchain_dir + 'sysroot/usr/include/' +
        toolchain_bin_prefix,
        '--extra-cflags=--target=' + toolchain_bin_prefix + toolchain_level,
        '--extra-ldflags=--target=' + toolchain_bin_prefix + toolchain_level,
        '--extra-ldflags=-L{}'.format(fakedir),
        '--extra-ldflags=-L' + clang_toolchain_dir + toolchain_bin_prefix,
        '--extra-ldflags=--gcc-toolchain=' + clang_toolchain_dir,
        '--target-os=android',
    ]


def SetupWindowsCrossCompileToolchain(target_arch):
    # First retrieve various MSVC and Windows SDK paths.
    output = subprocess.check_output([
        os.path.join(CHROMIUM_ROOT_DIR, 'build', 'vs_toolchain.py'),
        'get_toolchain_dir'
    ]).decode('utf-8')

    new_args = [
        '--enable-cross-compile',
        '--cc=clang-cl',
        '--ld=lld-link',
        '--nm=llvm-nm',
        '--ar=llvm-ar',

        # Separate from optflags because configure strips it from msvc builds...
        '--extra-cflags=-O2',
    ]

    if target_arch == 'ia32':
        new_args += ['--extra-cflags=-m32']
    if target_arch == 'ia32':
        target_arch = 'x86'
    if target_arch == 'arm64':
        new_args += [
            # With ASM enabled, an ARCH must be specified.
            '--arch=aarch64',
            # When cross-compiling (from Linux), armasm64.exe is not available.
            '--as=clang-cl',
            # FFMPEG is not yet enlightened for ARM64 Windows.
            # Imitate Android workaround.
            '--extra-cflags=--target=arm64-windows'
        ]

    # Turn this into a dictionary.
    win_dirs = gn_helpers.FromGNArgs(output)

    # Use those paths with a second script which will tell us the proper lib paths
    # to specify for ldflags.
    output = subprocess.check_output([
        'python3',
        os.path.join(CHROMIUM_ROOT_DIR, 'build', 'toolchain', 'win',
                     'setup_toolchain.py'), win_dirs['vs_path'],
        win_dirs['sdk_path'], win_dirs['runtime_dirs'], 'win', target_arch,
        'none'
    ]).decode('utf-8')

    flags = gn_helpers.FromGNArgs(output)

    # Q1 2021 update to LLVM now lets us use a sysroot for cross-compilation
    # targeting Windows, instead of specificying a variety of individual include
    # folders which now include whitespace within paths within the SDK. Either
    # injection of such paths into environment variable or using the new sysroot
    # option is required, since using a /tmp symlink solution to avoid the spaces
    # broke cross-compilation for win-arm64. For at least now, we'll use the
    # sysroot approach, until and unless the environment variable injection
    # approach is determined to be better or more consistent.
    new_args += [
        '--extra-cflags=/winsysroot' + win_dirs['vs_path'],
        '--extra-ldflags=/winsysroot:' + win_dirs['vs_path'],
    ]

    # FFmpeg configure doesn't like arguments with spaces in them even if quoted
    # or double-quoted or escape-quoted (whole argument and/or the internal
    # spaces). To automate this for now, every path that has a space in it is
    # replaced with a symbolic link created in the OS' temp folder to the real
    # path.
    def do_remove_temp_link(temp_name):
        assert os.path.exists(temp_name)
        assert os.path.islink(temp_name)
        print('Removing temporary link ' + temp_name)
        os.remove(temp_name)

    def do_make_temp_link(real_target):
        temp_file = tempfile.NamedTemporaryFile(prefix='windows_build_ffmpeg')
        temp_name = temp_file.name
        # Destroy |temp_file|, but reuse its name for the symbolic link which
        # survives this helper method.
        temp_file.close()
        os.symlink(real_target, temp_name)
        assert os.path.exists(temp_name)
        assert os.path.islink(temp_name)
        atexit.register(do_remove_temp_link, temp_name)
        return temp_name

    return new_args


def SetupMacCrossCompileToolchain(target_arch):
    # First compute the various SDK paths.
    mac_min_ver = '10.10'
    developer_dir = os.path.join(CHROMIUM_ROOT_DIR, 'build', 'mac_files',
                                 'xcode_binaries', 'Contents', 'Developer')
    sdk_dir = os.path.join(developer_dir, 'Platforms', 'MacOSX.platform',
                           'Developer', 'SDKs', 'MacOSX.sdk')

    if target_arch == 'x64':
        target_triple = 'x86_64-apple-macosx'
    elif target_arch == 'arm64':
        target_triple = 'arm64-apple-macosx'
    else:
        raise Exception("unknown arch " + target_arch)

    # We're guessing about the right sdk path, so warn if we don't find it.
    if not os.path.exists(sdk_dir):
        print(sdk_dir)
        raise Exception("Can't find the mac sdk.  Please see crbug.com/841826")

    frameworks_dir = os.path.join(sdk_dir, "System", "Library", "Frameworks")
    libs_dir = os.path.join(sdk_dir, "usr", "lib")

    new_args = [
        '--enable-cross-compile',
        '--cc=clang',
        # This is replaced with fake_linker.py further down. We need a real linker
        # at configure time for a few configure checks. These checks only link
        # very basic programs, so it's ok to use ld64.lld, even though it's not
        # generally production quality.
        '--ld=ld64.lld',
        '--nm=llvm-nm',
        '--ar=llvm-ar',
        '--target-os=darwin',
        '--extra-cflags=--target=' + target_triple,
        '--extra-cflags=-F' + frameworks_dir,
        '--extra-cflags=-mmacosx-version-min=' + mac_min_ver
    ]

    # We need to pass -nostdinc so that clang does not pick up linux headers,
    # but then it also can't find its own headers like stddef.h. So tell it
    # where to look for those headers.
    clang_dir = glob.glob(
        os.path.join(CHROMIUM_ROOT_DIR, 'third_party', 'llvm-build',
                     'Release+Asserts', 'lib', 'clang', '*', 'include'))[0]

    new_args += [
        '--extra-cflags=-fblocks',
        '--extra-cflags=-nostdinc',
        '--extra-cflags=-isystem%s/usr/include' % sdk_dir,
        '--extra-cflags=-isystem' + clang_dir,
        '--extra-ldflags=-syslibroot',
        '--extra-ldflags=' + sdk_dir,
        '--extra-ldflags=' + '-L' + libs_dir,
        '--extra-ldflags=-lSystem',
        '--extra-ldflags=-macosx_version_min',
        '--extra-ldflags=' + mac_min_ver,
        '--extra-ldflags=-sdk_version',
        '--extra-ldflags=' + mac_min_ver,
        # ld64.lld requires -platform_version <platform> <min_version>
        # <sdk_version>
        '--extra-ldflags=-platform_version',
        '--extra-ldflags=macos',
        '--extra-ldflags=' + mac_min_ver,
        '--extra-ldflags=' + mac_min_ver
    ]

    return new_args


def BuildFFmpeg(target_os, target_arch, host_os, host_arch, parallel_jobs,
                config_only, config, configure_flags, options):
    config_dir = ROBO_CONFIGURATION.target_config_directory(
        target_arch, target_os, config)

    # See if the token file exists, and skip building if '--fast' is given.
    token_file = os.path.join(config_dir, SUCCESS_TOKEN)
    if os.path.exists(token_file) and options.fast:
        print('Success token exists, skipping build of %s' % config_dir)
        return

    shutil.rmtree(config_dir, ignore_errors=True)
    os.makedirs(config_dir)

    PrintAndCheckCall([os.path.join(FFMPEG_DIR, 'configure')] +
                      configure_flags,
                      cwd=config_dir)

    # These rewrites force disable various features and should be applied before
    # attempting the standalone ffmpeg build to make sure compilation succeeds.
    pre_make_rewrites = [
        (r'(#define HAVE_VALGRIND_VALGRIND_H [01])',
         r'#define HAVE_VALGRIND_VALGRIND_H 0 /* \1 -- forced to 0. See '
         r'https://crbug.com/590440 */')
    ]
    pre_make_asm_rewrites = [
        (r'(%define HAVE_VALGRIND_VALGRIND_H [01])',
         r'%define HAVE_VALGRIND_VALGRIND_H 0 ; \1 -- forced to 0. See '
         r'https://crbug.com/590440')
    ]

    if target_os == 'android':
        pre_make_rewrites += [
            (r'(#define HAVE_POSIX_MEMALIGN [01])',
             r'#define HAVE_POSIX_MEMALIGN 0 /* \1 -- forced to 0. See '
             r'https://crbug.com/604451 */')
        ]

    # Linux configs is also used on Fuchsia. They are mostly compatible with
    # Fuchsia except that Fuchsia doesn't support sysctl(). On Linux sysctl()
    # isn't actually used, so it's safe to set HAVE_SYSCTL to 0. Linux is also
    # removing <sys/sysctl.h> soon, so this is needed to silence a deprecation
    # #warning which will be converted to an error via -Werror.
    # There is also no prctl.h
    if target_os in ['linux', 'linux-noasm']:
        pre_make_rewrites += [
            (r'(#define HAVE_SYSCTL [01])',
             r'#define HAVE_SYSCTL 0 /* \1 -- forced to 0 for Fuchsia */'),
            (r'(#define HAVE_PRCTL [01])',
             r'#define HAVE_PRCTL 0 /* \1 -- forced to 0 for Fuchsia */')
        ]

    # Turn off bcrypt, since we don't have it on Windows builders, but it does
    # get detected when cross-compiling.
    if target_os == 'win':
        pre_make_rewrites += [(r'(#define HAVE_BCRYPT [01])',
                               r'#define HAVE_BCRYPT 0')]

    # Sanitizers can't compile the h264 code when EBP is used.
    # Pre-make as ffmpeg fails to compile otherwise.
    if target_arch == 'ia32':
        pre_make_rewrites += [
            (r'(#define HAVE_EBP_AVAILABLE [01])',
             r'/* \1 -- ebp selection is done by the chrome build */')
        ]

    RewriteFile(os.path.join(config_dir, 'config.h'), pre_make_rewrites)
    asm_path = os.path.join(config_dir, 'config.asm')
    if os.path.exists(asm_path):
        RewriteFile(asm_path, pre_make_asm_rewrites)

    # Windows linking resolves external symbols. Since generate_gn.py does not
    # need a functioning set of libraries, ignore unresolved symbols here.
    # This is especially useful here to avoid having to build a local libopus for
    # windows. We munge the output of configure here to avoid this LDFLAGS setting
    # triggering mis-detection during configure execution.
    if target_os == 'win':
        RewriteFile(os.path.join(config_dir, 'ffbuild/config.mak'),
                    [(r'(LDFLAGS=.*)', r'\1 -FORCE:UNRESOLVED')])

    # TODO(crbug.com/41387829): Linking when targeting mac on linux is
    # currently broken.
    # Replace the linker step with something that just creates the target.
    if target_os == 'mac' and host_os == 'linux':
        RewriteFile(os.path.join(config_dir, 'ffbuild/config.mak'),
                    [(r'LD=ld64.lld', r'LD=' +
                      ROBO_CONFIGURATION.get_script_path('fake_linker.py'))])

    # The FFMPEG roll build hits a bug in lld-link that does not impact the
    # overall Chromium build.
    # Replace the linker step with something that just creates the target.
    if target_os == 'win' and target_arch == 'arm64' and host_os == 'linux':
        RewriteFile(os.path.join(config_dir, 'ffbuild/config.mak'),
                    [(r'LD=lld-link', r'LD=' +
                      ROBO_CONFIGURATION.get_script_path('fake_linker.py'))])

    if target_os in (host_os, host_os + '-noasm', 'android', 'win',
                     'mac') and not config_only:
        PrintAndCheckCall(['make', '-j%d' % parallel_jobs], cwd=config_dir)
    elif config_only:
        print('Skipping build step as requested.')
    else:
        print(
            'Skipping compile as host configuration differs from target.\n'
            'Please compare the generated config.h with the previous version.\n'
            'You may also patch the script to properly cross-compile.\n'
            'Host OS : %s\n'
            'Target OS : %s\n'
            'Host arch : %s\n'
            'Target arch : %s\n' %
            (host_os, target_os, host_arch, target_arch))

    # These rewrites are necessary to facilitate various Chrome build options.
    post_make_rewrites = [
        (r'(#define FFMPEG_CONFIGURATION .*)',
         r'/* \1 -- elide long configuration string from binary */')
    ]

    if target_arch in ('arm', 'arm-neon', 'arm64'):
        post_make_rewrites += [
            (r'(#define HAVE_VFP_ARGS [01])',
             r'/* \1 -- softfp/hardfp selection is done by the chrome build */'
             ),
            (r'(#define HAVE_VFP_INLINE [01])', r'#define HAVE_VFP_INLINE 1'),
            (r'(#define HAVE_VFP_EXTERNAL [01])',
             r'#define HAVE_VFP_EXTERNAL 1'),
            (r'(#define HAVE_VFP [01])', r'#define HAVE_VFP 1')
        ]

    RewriteFile(os.path.join(config_dir, 'config.h'), post_make_rewrites)

    # Yay!  create the token file so that we can skip this in the future.
    with open(token_file, 'w'):
        pass


def main(argv):
    clean_arch_map = {k: '|'.join(v) for k, v in ARCH_MAP.items()}
    formatted_usage_end = USAGE_END % clean_arch_map
    parser = optparse.OptionParser(usage=USAGE_BEGIN + formatted_usage_end)
    parser.add_option(
        '--branding',
        action='append',
        dest='brandings',
        choices=BRANDINGS,
        help='Branding to build; determines e.g. supported codecs')
    parser.add_option('--config-only',
                      action='store_true',
                      help='Skip the build step. Useful when a given platform '
                      'is not necessary for generate_gn.py')
    parser.add_option(
        '--fast',
        action='store_true',
        help='Skip building (successfully) if the success token file exists')
    options, args = parser.parse_args(argv)

    if len(args) < 1:
        parser.print_help()
        return 1

    target_os = args[0]
    target_arch = ''
    if len(args) >= 2:
        target_arch = args[1]
    configure_args = args[2:]

    if target_os not in ('android', 'linux', 'linux-noasm', 'mac', 'win',
                         'all'):
        parser.print_help()
        return 1

    host_os = ROBO_CONFIGURATION.host_operating_system()
    host_arch = ROBO_CONFIGURATION.host_architecture()
    parallel_jobs = 8

    if target_os.split('-', 1)[0] != host_os and (host_os != 'linux'
                                                  or host_arch != 'x64'):
        print('Cross compilation can only be done from a linux x64 host.')
        return 1

    for os in ARCH_MAP.keys():
        if os != target_os and target_os != 'all':
            continue
        for arch in ARCH_MAP[os]:
            if target_arch and arch != target_arch:
                continue

            print('System information:\n'
                  'Host OS       : %s\n'
                  'Target OS     : %s\n'
                  'Host arch     : %s\n'
                  'Target arch   : %s\n' % (host_os, os, host_arch, arch))
            ConfigureAndBuild(arch,
                              os,
                              host_os,
                              host_arch,
                              parallel_jobs,
                              configure_args,
                              options=options)


def ConfigureAndBuild(target_arch, target_os, host_os, host_arch,
                      parallel_jobs, configure_args, options):
    if target_os == 'linux' and target_arch == 'noasm-x64':
        target_os = 'linux-noasm'
        target_arch = 'x64'

    configure_flags = collections.defaultdict(list)

    # Common configuration.  Note: --disable-everything does not in fact disable
    # everything, just non-library components such as decoders and demuxers.
    configure_flags['Common'].extend([
        '--disable-everything',
        '--disable-all',
        '--disable-doc',
        '--disable-htmlpages',
        '--disable-manpages',
        '--disable-podpages',
        '--disable-txtpages',
        '--disable-static',
        '--enable-avcodec',
        '--enable-avformat',
        '--enable-avutil',
        '--enable-static',
        '--enable-libopus',

        # Disable features.
        '--disable-debug',
        '--disable-bzlib',
        '--disable-error-resilience',
        '--disable-iconv',
        '--disable-network',
        '--disable-schannel',
        '--disable-sdl2',
        '--disable-symver',
        '--disable-xlib',
        '--disable-zlib',
        '--disable-securetransport',
        '--disable-faan',
        '--disable-alsa',
        '--disable-iamf',

        # Disable automatically detected external libraries. This prevents
        # automatic inclusion of things like hardware decoders. Each roll should
        # audit new [autodetect] configure options and add any desired options to
        # this file.
        '--disable-autodetect',

        # Common codecs.
        '--enable-decoder=vorbis,libopus,flac',
        '--enable-decoder=pcm_u8,pcm_s16le,pcm_s24le,pcm_s32le,pcm_f32le,mp3',
        '--enable-decoder=pcm_s16be,pcm_s24be,pcm_mulaw,pcm_alaw',
        '--enable-demuxer=ogg,matroska,wav,flac,mp3,mov',
        '--enable-parser=opus,vorbis,flac,mpegaudio,vp9',

        # Setup include path so Chromium's libopus can be used.
        '--extra-cflags=-I' +
        os.path.join(CHROMIUM_ROOT_DIR, 'third_party/opus/src/include'),

        # Disable usage of Linux Performance API. Not used in production code, but
        # missing system headers break some Android builds.
        '--disable-linux-perf',

        # Force usage of nasm.
        '--x86asmexe=nasm',
    ])

    if target_os == 'android':
        configure_flags['Common'].extend([
            # This replaces --optflags="-Os" since it implies it and since if it is
            # also specified, configure ends up dropping all optflags :/
            '--enable-small',
        ])

        configure_flags['Common'].extend(SetupAndroidToolchain(target_arch))
    else:
        configure_flags['Common'].extend([
            # --optflags doesn't append multiple entries, so set all at once.
            '--optflags="-O2"',
        ])

    if target_os in ('linux', 'linux-noasm', 'android'):
        if target_arch == 'x64':
            if target_os == 'android':
                configure_flags['Common'].extend([
                    '--arch=x86_64',
                ])
            else:
                configure_flags['Common'].extend([
                    '--enable-lto',
                    '--arch=x86_64',
                    '--target-os=linux',
                ])

                if host_arch != 'x64':
                    configure_flags['Common'].extend([
                        '--enable-cross-compile',
                        '--cross-prefix=/usr/bin/x86_64-linux-gnu-',
                        '--extra-cflags=--target=x86_64-linux-gnu',
                        '--extra-ldflags=--target=x86_64-linux-gnu',
                    ])
        elif target_arch == 'ia32':
            configure_flags['Common'].extend([
                '--arch=i686',
                '--extra-cflags="-m32"',
                '--extra-ldflags="-m32"',
            ])
            # Android ia32 can't handle textrels and ffmpeg can't compile without
            # them.  http://crbug.com/559379
            if target_os == 'android':
                configure_flags['Common'].extend([
                    '--disable-x86asm',
                ])
        elif target_arch == 'arm' or target_arch == 'arm-neon':
            # TODO(ihf): ARM compile flags are tricky. The final options
            # overriding everything live in chroot /build/*/etc/make.conf
            # (some of them coming from src/overlays/overlay-<BOARD>/make.conf).
            # We try to follow these here closely. In particular we need to
            # set ffmpeg internal #defines to conform to make.conf.
            # TODO(ihf): For now it is not clear if thumb or arm settings would be
            # faster. I ran experiments in other contexts and performance seemed
            # to be close and compiler version dependent. In practice thumb builds are
            # much smaller than optimized arm builds, hence we go with the global
            # CrOS settings.
            configure_flags['Common'].extend([
                '--arch=arm',
                '--enable-armv6',
                '--enable-armv6t2',
                '--enable-vfp',
                '--enable-thumb',
                '--extra-cflags=-march=armv7-a',
            ])

            if target_os == 'android':
                configure_flags['Common'].extend([
                    # Runtime neon detection requires /proc/cpuinfo access, so ensure
                    # av_get_cpu_flags() is run outside of the sandbox when enabled.
                    '--enable-neon',
                    '--extra-cflags=-mtune=generic-armv7-a',
                    # Enabling softfp lets us choose either softfp or hardfp when doing
                    # the chrome build.
                    '--extra-cflags=-mfloat-abi=softfp',
                ])
                if target_arch == 'arm':
                    print(
                        'arm-neon is the only supported arm arch for Android.\n'
                    )
                    return 1

                if target_arch == 'arm-neon':
                    configure_flags['Common'].extend([
                        '--extra-cflags=-mfpu=neon',
                    ])
                else:
                    configure_flags['Common'].extend([
                        '--extra-cflags=-mfpu=vfpv3-d16',
                    ])
            else:
                if host_arch != 'arm':
                    configure_flags['Common'].extend([
                        '--enable-cross-compile',
                        '--target-os=linux',
                        '--extra-cflags=--target=arm-linux-gnueabihf',
                        '--extra-ldflags=--target=arm-linux-gnueabihf',
                        '--sysroot=' + os.path.join(
                            CHROMIUM_ROOT_DIR,
                            'build/linux/debian_bullseye_armhf-sysroot'),
                        '--extra-cflags=-mtune=cortex-a8',
                        # NOTE: we don't need softfp for this hardware.
                        '--extra-cflags=-mfloat-abi=hard',
                        # For some reason configure drops this...
                        '--extra-cflags=-O2',
                    ])

                if target_arch == 'arm-neon':
                    configure_flags['Common'].extend([
                        '--enable-neon',
                        '--extra-cflags=-mfpu=neon',
                    ])
                else:
                    configure_flags['Common'].extend([
                        '--disable-neon',
                        '--extra-cflags=-mfpu=vfpv3-d16',
                    ])
        elif target_arch == 'arm64':
            if target_os != 'android':
                if host_arch != 'arm64':
                    configure_flags['Common'].extend([
                        '--enable-cross-compile',
                        '--cross-prefix=/usr/bin/aarch64-linux-gnu-',
                        '--extra-cflags=--target=aarch64-linux-gnu',
                        '--extra-ldflags=--target=aarch64-linux-gnu',
                    ])

                configure_flags['Common'].extend([
                    '--target-os=linux',
                    '--sysroot=' +
                    os.path.join(CHROMIUM_ROOT_DIR,
                                 'build/linux/debian_bullseye_arm64-sysroot'),
                    # See crbug.com/1467681. These could be removed eventually
                    '--disable-dotprod',
                    '--disable-i8mm',
                ])
            configure_flags['Common'].extend([
                '--arch=aarch64',
                '--enable-armv8',
                '--extra-cflags=-march=armv8-a',
            ])
        elif target_arch == 'mipsel':
            # These flags taken from android chrome build with target_cpu='mipsel'
            configure_flags['Common'].extend([
                '--arch=mipsel',
                '--disable-mips32r6',
                '--disable-mips32r5',
                '--disable-mips32r2',
                '--disable-mipsdsp',
                '--disable-mipsdspr2',
                '--disable-msa',
                '--enable-mipsfpu',
                '--extra-cflags=-march=mipsel',
                '--extra-cflags=-mcpu=mips32',
                # Required to avoid errors about dynamic relocation w/o -fPIC.
                '--extra-ldflags=-z notext',
            ])
            if target_os == 'linux':
                configure_flags['Common'].extend([
                    '--enable-cross-compile',
                    '--target-os=linux',
                    '--sysroot=' +
                    os.path.join(CHROMIUM_ROOT_DIR,
                                 'build/linux/debian_bullseye_mips-sysroot'),
                    '--extra-cflags=--target=mipsel-linux-gnu',
                    '--extra-ldflags=--target=mipsel-linux-gnu',
                ])
        elif target_arch == 'mips64el':
            # These flags taken from android chrome build with target_cpu='mips64el'
            configure_flags['Common'].extend([
                '--arch=mips64el',
                '--enable-mipsfpu',
                '--disable-mipsdsp',
                '--disable-mipsdspr2',
                '--extra-cflags=-march=mips64el',
                # Required to avoid errors about dynamic relocation w/o -fPIC.
                '--extra-ldflags=-z notext',
            ])
            if target_os == 'android':
                configure_flags['Common'].extend([
                    '--enable-mips64r6',
                    '--extra-cflags=-mcpu=mips64r6',
                    '--disable-mips64r2',
                    '--enable-msa',
                ])
            if target_os == 'linux':
                configure_flags['Common'].extend([
                    '--enable-cross-compile',
                    '--target-os=linux',
                    '--sysroot=' + os.path.join(
                        CHROMIUM_ROOT_DIR,
                        'build/linux/debian_bullseye_mips64el-sysroot'),
                    '--enable-mips64r2',
                    '--disable-mips64r6',
                    '--disable-msa',
                    '--extra-cflags=-mcpu=mips64r2',
                    '--extra-cflags=--target=mips64el-linux-gnuabi64',
                    '--extra-ldflags=--target=mips64el-linux-gnuabi64',
                ])
        else:
            print('Error: Unknown target arch %r for target OS %r!' %
                  (target_arch, target_os),
                  file=sys.stderr)
            return 1

    if target_os == 'linux-noasm':
        configure_flags['Common'].extend([
            '--disable-asm',
            '--disable-inline-asm',
        ])

    if 'win' not in target_os and 'android' not in target_os:
        configure_flags['Common'].extend([
            '--enable-pic',
            '--cc=clang',
            '--cxx=clang++',
            '--ld=clang',
        ])

        # Clang Linux will use the first 'ld' it finds on the path, which will
        # typically be the system one, so explicitly configure use of Clang's
        # ld.lld, to ensure that things like cross-compilation and LTO work.
        # This does not work for ia32 and is always used on mac.
        if target_arch != 'ia32' and target_os != 'mac':
            configure_flags['Common'].append('--extra-ldflags=-fuse-ld=lld')

    # Should be run on Mac, unless we're cross-compiling on Linux.
    if target_os == 'mac':
        if host_os != 'mac' and host_os != 'linux':
            print('Script should be run on a Mac or Linux host.\n',
                  file=sys.stderr)
            return 1

        if host_os != 'mac':
            configure_flags['Common'].extend(
                SetupMacCrossCompileToolchain(target_arch))
        else:
            # ffmpeg links against Chromium's libopus, which isn't built when this
            # script runs. Suppress all undefined symbols (which matches the default
            # on Linux), to get things to build. This also requires opting in to
            # flat namespaces.
            configure_flags['Common'].extend([
                '--extra-ldflags=-Wl,-flat_namespace -Wl,-undefined,warning',
            ])

        if target_arch == 'x64':
            configure_flags['Common'].extend([
                '--arch=x86_64',
                '--extra-cflags=-m64',
                '--extra-ldflags=-arch x86_64',
            ])
        elif target_arch == 'arm64':
            configure_flags['Common'].extend([
                '--arch=arm64',
                '--extra-cflags=-arch arm64',
                '--extra-ldflags=-arch arm64',
            ])
        else:
            print('Error: Unknown target arch %r for target OS %r!' %
                  (target_arch, target_os),
                  file=sys.stderr)

    # Should be run on Windows.
    if target_os == 'win':
        configure_flags['Common'].extend([
            '--toolchain=msvc',
            '--extra-cflags=-I' +
            os.path.join(FFMPEG_DIR, 'chromium/include/win'),
        ])

        if target_arch == 'x64':
            configure_flags['Common'].extend(['--target-os=win64'])
        elif target_arch == 'x86':
            configure_flags['Common'].extend(['--target-os=win32'])

        if host_os != 'win':
            configure_flags['Common'].extend(
                SetupWindowsCrossCompileToolchain(target_arch))

        if 'CYGWIN_NT' in platform.system():
            configure_flags['Common'].extend([
                '--cc=cygwin-wrapper cl',
                '--ld=cygwin-wrapper link',
                '--nm=cygwin-wrapper dumpbin -symbols',
                '--ar=cygwin-wrapper lib',
            ])

    # Google Chrome specific configuration.
    configure_flags['Chrome'].extend([
        '--enable-decoder=aac,h264',
        '--enable-demuxer=aac',
        '--enable-parser=aac,h264',
    ])

    configure_flags['ChromeAndroid'].extend([
        '--enable-demuxer=aac',
        '--enable-parser=aac',
        '--enable-decoder=aac',
    ])

    def do_build_ffmpeg(branding, configure_flags):
        if options.brandings and branding not in options.brandings:
            print('%s skipped' % branding)
            return

        print('%s configure/build:' % branding)
        BuildFFmpeg(target_os, target_arch, host_os, host_arch, parallel_jobs,
                    options.config_only, branding, configure_flags, options)

    # Don't build video decoders for 32-bit Android ARM due to binary size
    # concerns.
    if target_os != 'android' or not target_arch in ("arm", "arm-neon"):
        do_build_ffmpeg(
            'Chromium', configure_flags['Common'] +
            configure_flags['Chromium'] + configure_args)
        do_build_ffmpeg(
            'Chrome', configure_flags['Common'] + configure_flags['Chrome'] +
            configure_args)
    else:
        do_build_ffmpeg('Chromium', configure_flags['Common'] + configure_args)
        do_build_ffmpeg(
            'Chrome', configure_flags['Common'] +
            configure_flags['ChromeAndroid'] + configure_args)

    print('Done. If desired you may copy config.h/config.asm into the '
          'source/config tree using copy_config.sh.')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
