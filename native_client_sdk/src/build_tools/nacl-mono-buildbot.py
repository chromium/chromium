#!/usr/bin/env python
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import hashlib
import json
import os
import sys

import buildbot_common
import build_version
from build_paths import SCRIPT_DIR

GS_MANIFEST_PATH = 'gs://nativeclient-mirror/nacl/nacl_sdk/'
SDK_MANIFEST = 'naclsdk_manifest2.json'
MONO_MANIFEST = 'naclmono_manifest.json'

def build_and_upload_mono(sdk_revision, pepper_revision, sdk_url,
                          upload_path, args):
  install_dir = 'naclmono'
  buildbot_common.RemoveDir(install_dir)

  revision_opt = ['--sdk-revision', sdk_revision] if sdk_revision else []
  url_opt = ['--sdk-url', sdk_url] if sdk_url else []

  buildbot_common.Run([sys.executable, 'nacl-mono-builder.py',
                      '--arch', 'x86-32', '--install-dir', install_dir] +
                      revision_opt + url_opt + args)
  buildbot_common.Run([sys.executable, 'nacl-mono-builder.py',
                      '--arch', 'x86-64', '--install-dir', install_dir] +
                      revision_opt + url_opt + args)
  buildbot_common.Run([sys.executable, 'nacl-mono-builder.py',
                      '--arch', 'arm', '--install-dir', install_dir] +
                      revision_opt + url_opt + args)
  buildbot_common.Run([sys.executable, 'nacl-mono-archive.py',
                      '--upload-path', upload_path,
                      '--pepper-revision', pepper_revision,
                      '--install-dir', install_dir] + args)

def get_sdk_build_info():
  '''Returns a list of dictionaries for versions of NaCl Mono to build which are
     out of date compared to the SDKs available to naclsdk'''

  # Get a copy of the naclsdk manifest file
  buildbot_common.Run([buildbot_common.GetGsutil(), 'cp',
      GS_MANIFEST_PATH + SDK_MANIFEST, '.'])
  manifest_file = open(SDK_MANIFEST, 'r')
  sdk_manifest = json.loads(manifest_file.read())
  manifest_file.close()

  pepper_infos = []
  for key, value in sdk_manifest.items():
    if key == 'bundles':
      stabilities = ['stable', 'beta', 'dev', 'post_stable']
      # Pick pepper_* bundles, need pepper_19 or greater to build Mono
      bundles = filter(lambda b: (b['stability'] in stabilities
                                  and 'pepper_' in b['name'])
                                  and b['version'] >= 19, value)
      for b in bundles:
        newdict = {}
        newdict['pepper_revision'] = str(b['version'])
        linux_arch = filter(lambda u: u['host_os'] == 'linux', b['archives'])
        newdict['sdk_url'] = linux_arch[0]['url']
        newdict['sdk_revision'] = b['revision']
        newdict['stability'] = b['stability']
        newdict['naclmono_name'] = 'naclmono_' + newdict['pepper_revision']
        pepper_infos.append(newdict)

  # Get a copy of the naclmono manifest file
  buildbot_common.Run([buildbot_common.GetGsutil(), 'cp',
      GS_MANIFEST_PATH + MONO_MANIFEST, '.'])
  manifest_file = open(MONO_MANIFEST, 'r')
  mono_manifest = json.loads(manifest_file.read())
  manifest_file.close()

  ret = []
  mono_manifest_dirty = False
  # Check to see if we need to rebuild mono based on sdk revision
  for key, value in mono_manifest.items():
    if key == 'bundles':
      for info in pepper_infos:
        bundle = filter(lambda b: b['name'] == info['naclmono_name'], value)
        if len(bundle) == 0:
          info['naclmono_rev'] = '1'
          ret.append(info)
        else:
          if info['sdk_revision'] != bundle[0]['sdk_revision']:
            # This bundle exists in the mono manifest, bump the revision
            # for the new build we're about to make.
            info['naclmono_rev'] = str(bundle[0]['revision'] + 1)
            ret.append(info)
          elif info['stability'] != bundle[0]['stability']:
            # If all that happened was the SDK bundle was promoted in stability,
            # change only that and re-write the manifest
            mono_manifest_dirty = True
            bundle[0]['stability'] = info['stability']

  # re-write the manifest here because there are no bundles to build but
  # the manifest has changed
  if mono_manifest_dirty and len(ret) == 0:
    manifest_file = open(MONO_MANIFEST, 'w')
    manifest_file.write(json.dumps(mono_manifest, sort_keys=False, indent=2))
    manifest_file.close()
    buildbot_common.Run([buildbot_common.GetGsutil(), 'cp', '-a', 'public-read',
        MONO_MANIFEST, GS_MANIFEST_PATH + MONO_MANIFEST])

  return ret

def update_mono_sdk_json(infos):
  '''Update the naclmono manifest with the newly built packages'''
  if len(infos) == 0:
    return

  manifest_file = open(MONO_MANIFEST, 'r')
  mono_manifest = json.loads(manifest_file.read())
  manifest_file.close()

  for info in infos:
    bundle = {}
    bundle['name'] = info['naclmono_name']
    bundle['description'] = 'Mono for Native Client'
    bundle['stability'] = info['stability']
    bundle['recommended'] = 'no'
    bundle['version'] = 'experimental'
    archive = {}
    sha1_hash = hashlib.sha1()
    f = open(info['naclmono_name'] + '.bz2', 'rb')
    sha1_hash.update(f.read())
    archive['size'] = f.tell()
    f.close()
    archive['checksum'] = { 'sha1': sha1_hash.hexdigest() }
    archive['host_os'] = 'all'
    archive['url'] = ('https://storage.googleapis.com/'
                      'nativeclient-mirror/nacl/nacl_sdk/%s/%s/%s.bz2'
                      % (info['naclmono_name'], info['naclmono_rev'],
                        info['naclmono_name']))
    bundle['archives'] = [archive]
    bundle['revision'] = int(info['naclmono_rev'])
    bundle['sdk_revision'] = int(info['sdk_revision'])

    # Insert this new bundle into the manifest,
    # probably overwriting an existing bundle.
    for key, value in mono_manifest.items():
      if key == 'bundles':
        existing = filter(lambda b: b['name'] == info['naclmono_name'], value)
        if len(existing) > 0:
          loc = value.index(existing[0])
          value[loc] = bundle
        else:
          value.append(bundle)

  # Write out the file locally, then upload to its known location.
  manifest_file = open(MONO_MANIFEST, 'w')
  manifest_file.write(json.dumps(mono_manifest, sort_keys=False, indent=2))
  manifest_file.close()
  buildbot_common.Run([buildbot_common.GetGsutil(), 'cp', '-a', 'public-read',
      MONO_MANIFEST, GS_MANIFEST_PATH + MONO_MANIFEST])


def main(args):
  args = args[1:]

  # Delete global configs that would override the mono builders' configuration.
  if 'AWS_CREDENTIAL_FILE' in os.environ:
    del os.environ['AWS_CREDENTIAL_FILE']
  if 'BOTO_CONFIG' in os.environ:
    del os.environ['BOTO_CONFIG']

  buildbot_revision = os.environ.get('BUILDBOT_REVISION', '')
  buildername = os.environ.get('BUILDBOT_BUILDERNAME', '')

  os.chdir(SCRIPT_DIR)

  if buildername == 'linux-sdk-mono32':
    assert buildbot_revision
    sdk_revision = buildbot_revision.split(':')[0]
    pepper_revision = build_version.ChromeMajorVersion()
    build_and_upload_mono(sdk_revision, pepper_revision, None,
                          'trunk.' + sdk_revision, args)
  elif buildername == 'linux-sdk-mono64':
    infos = get_sdk_build_info()
    for info in infos:
      # This will put the file in naclmono_19/1/naclmono_19.bz2 for example.
      upload_path = info['naclmono_name'] + '/' + info['naclmono_rev']
      build_and_upload_mono(None, info['pepper_revision'], info['sdk_url'],
          upload_path, args)
    update_mono_sdk_json(infos)



if __name__ == '__main__':
  sys.exit(main(sys.argv))
