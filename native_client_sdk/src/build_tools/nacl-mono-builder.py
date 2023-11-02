#!/usr/bin/env python
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import sys
import tarfile

import buildbot_common
from build_paths import SCRIPT_DIR

SDK_BUILD_DIR = SCRIPT_DIR
MONO_BUILD_DIR = os.path.join(SDK_BUILD_DIR, 'mono_build')
MONO_DIR = os.path.join(MONO_BUILD_DIR, 'nacl-mono')


def main(args):
  parser = argparse.ArgumentParser()
  parser.add_argument('--arch',
                    help='Target architecture',
                    dest='arch',
                    default='x86-32')
  parser.add_argument('--sdk-revision',
                    help='SDK Revision'
                         ' (default=buildbot revision)',
                    dest='sdk_revision',
                    default=None)
  parser.add_argument('--sdk-url',
                    help='SDK Download URL',
                    dest='sdk_url',
                    default=None)
  parser.add_argument('--install-dir',
                    help='Install Directory',
                    dest='install_dir',
                    default='naclmono')
  options = parser.parse_args(args)

  assert sys.platform.find('linux') != -1

  buildbot_revision = os.environ.get('BUILDBOT_REVISION', '')

  build_prefix = options.arch + ' '

  buildbot_common.BuildStep(build_prefix + 'Clean Old SDK')
  buildbot_common.MakeDir(MONO_BUILD_DIR)
  buildbot_common.RemoveDir(os.path.join(MONO_BUILD_DIR, 'pepper_*'))

  buildbot_common.BuildStep(build_prefix + 'Setup New SDK')
  sdk_dir = None
  sdk_revision = options.sdk_revision
  sdk_url = options.sdk_url
  if not sdk_url:
    if not sdk_revision:
      assert buildbot_revision
      sdk_revision = buildbot_revision.split(':')[0]
    sdk_url = 'gs://nativeclient-mirror/nacl/nacl_sdk/'\
              'trunk.%s/naclsdk_linux.tar.bz2' % sdk_revision

  sdk_url = sdk_url.replace('https://storage.googleapis.com/', 'gs://')

  sdk_file = sdk_url.split('/')[-1]

  buildbot_common.Run([buildbot_common.GetGsutil(), 'cp', sdk_url, sdk_file],
                      cwd=MONO_BUILD_DIR)
  tar_file = None
  try:
    tar_file = tarfile.open(os.path.join(MONO_BUILD_DIR, sdk_file))
    pepper_dir = os.path.commonprefix(tar_file.getnames())
    tar_file.extractall(path=MONO_BUILD_DIR)
    sdk_dir = os.path.join(MONO_BUILD_DIR, pepper_dir)
  finally:
    if tar_file:
      tar_file.close()

  assert sdk_dir

  buildbot_common.BuildStep(build_prefix + 'Checkout Mono')
  # TODO(elijahtaylor): Get git URL from master/trigger to make this
  # more flexible for building from upstream and release branches.
  if options.arch == 'arm':
    git_url = 'git://github.com/igotti-google/mono.git'
    git_rev = 'arm_nacl'
  else:
    git_url = 'git://github.com/elijahtaylor/mono.git'
    git_rev = 'HEAD'
  if buildbot_revision:
    # Unfortunately, we use different git branches/revisions
    # for ARM and x86 now, so ignore buildbot_revision variable for ARM.
    # Need to rethink this approach, if we'll plan to support
    # more flexible repo selection mechanism.
    if options.arch != 'arm':
      git_rev = buildbot_revision.split(':')[1]
  # ARM and x86 is built out of different git trees, so distinguish
  # them by appending the arch. It also makes 32 and 64 bit x86 separated,
  # which is good.
  # TODO(olonho): maybe we need to avoid modifications of global.
  global MONO_DIR
  tag = options.arch
  MONO_DIR = "%s-%s" % (MONO_DIR, tag)
  if not os.path.exists(MONO_DIR):
    buildbot_common.MakeDir(MONO_DIR)
    buildbot_common.Run(['git', 'clone', git_url, MONO_DIR])
  else:
    buildbot_common.Run(['git', 'fetch'], cwd=MONO_DIR)
  if git_rev:
    buildbot_common.Run(['git', 'checkout', git_rev], cwd=MONO_DIR)

  arch_to_bitsize = {'x86-32': '32',
                     'x86-64': '64',
                     'arm':    'arm'}
  arch_to_output_folder = {'x86-32': 'runtime-x86-32-build',
                           'x86-64': 'runtime-x86-64-build',
                           'arm':    'runtime-arm-build'}

  buildbot_common.BuildStep(build_prefix + 'Configure Mono')
  os.environ['NACL_SDK_ROOT'] = sdk_dir
  os.environ['TARGET_ARCH'] = options.arch
  os.environ['TARGET_BITSIZE'] = arch_to_bitsize[options.arch]
  buildbot_common.Run(['./autogen.sh'], cwd=MONO_DIR)
  buildbot_common.Run(['make', 'distclean'], cwd=MONO_DIR)

  buildbot_common.BuildStep(build_prefix + 'Build and Install Mono')
  nacl_interp_script = os.path.join(SDK_BUILD_DIR, 'nacl_interp_loader_mono.sh')
  os.environ['NACL_INTERP_LOADER'] = nacl_interp_script
  buildbot_common.Run(['./nacl-mono-runtime.sh',
                      MONO_DIR, # Mono directory with 'configure'
                      arch_to_output_folder[options.arch], # Build dir
                      options.install_dir],
                      cwd=SDK_BUILD_DIR)

  # TODO(elijahtaylor,olonho): Re-enable tests on arm when they compile/run.
  if options.arch != 'arm':
    buildbot_common.BuildStep(build_prefix + 'Test Mono')
    buildbot_common.Run(['make', 'check', '-j8'],
        cwd=os.path.join(SDK_BUILD_DIR, arch_to_output_folder[options.arch]))

  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
