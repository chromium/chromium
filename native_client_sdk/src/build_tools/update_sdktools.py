#!/usr/bin/env python
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script that reads omahaproxy and gsutil to determine a version of the
sdk_tools bundle to use.

Please note the differences between this script and update_nacl_manifest.py:

update_sdktools.py is run by a SDK-team developer to assist in updating to a
new sdk_tools bundle. A file on the developer's hard drive is modified, and
must be checked in for the new sdk_tools bundle to be used.

update_nacl_manifest.py is customarily run by a cron job, and does not check in
any changes. Instead it modifies the manifest file in cloud storage."""


import argparse
import collections
import difflib
import json
import re
import sys
import urllib2

from manifest_util import DownloadAndComputeHash, DictToJSON
from update_nacl_manifest import RealDelegate


SDK_TOOLS_DESCRIPTION_FORMAT = 'Native Client SDK Tools, revision %d'
BUCKET_PATH = 'nativeclient-mirror/nacl/nacl_sdk/'
GS_BUCKET_PATH = 'gs://' + BUCKET_PATH
HTTPS_BUCKET_PATH = 'https://storage.googleapis.com/' + BUCKET_PATH


def GetSdkToolsUrl(revision):
  return HTTPS_BUCKET_PATH + 'trunk.%d/sdk_tools.tgz' % revision


def GetTrunkRevisions(delegate):
  urls = delegate.GsUtil_ls(GS_BUCKET_PATH)
  revisions = []
  for url in urls:
    m = re.match(GS_BUCKET_PATH + 'trunk\.(\d+)', url)
    if m:
      revisions.append((int(m.group(1)), url))
  return sorted(revisions)


def FindMostRecentSdkTools(delegate):
  for revision, url in reversed(GetTrunkRevisions(delegate)):
    sdktools_url = url + 'sdk_tools.tgz'
    if delegate.GsUtil_ls(sdktools_url):
      return revision, sdktools_url
  return None


def JsonLoadFromString(json_string):
  if sys.version_info > (2, 7):
    return json.loads(json_string, object_pairs_hook=collections.OrderedDict)
  else:
    return json.loads(json_string)


def GetBundleByName(bundles, name):
  for bundle in bundles:
    if bundle['name'] == name:
      return bundle
  return None


def UpdateSdkToolsBundle(sdk_tools_bundle, revision, url, sha1, size):
  sdk_tools_bundle['description'] = SDK_TOOLS_DESCRIPTION_FORMAT % revision
  sdk_tools_bundle['revision'] = revision
  # Update archive for each OS
  for archive in sdk_tools_bundle['archives']:
    archive['url'] = url
    archive['checksum']['sha1'] = sha1
    archive['size'] = size


def UpdateManifest(manifest, revision):
  sdk_tools_bundle = GetBundleByName(manifest['bundles'], 'sdk_tools')
  url = GetSdkToolsUrl(revision)
  sha1, size = DownloadAndComputeHash(urllib2.urlopen(url))
  UpdateSdkToolsBundle(sdk_tools_bundle, revision, url, sha1, size)


def UpdateManifestFileToRevision(filename, revision):
  with open(filename) as stream:
    manifest_string = stream.read()

  manifest = JsonLoadFromString(manifest_string)
  UpdateManifest(manifest, revision)
  new_manifest_string = DictToJSON(manifest)

  diff_string = ''.join(difflib.unified_diff(manifest_string.splitlines(1),
                                             new_manifest_string.splitlines(1)))

  print 'diff %s' % filename
  print diff_string
  print

  with open(filename, 'w') as stream:
    stream.write(new_manifest_string)


def main(args):
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('-r', '--revision',
      help='set revision manually, rather than using the latest version')
  options = parser.parse_args(args)

  # TODO(binji): http://crbug.com/169047. Rename RealDelegate to something else.
  delegate = RealDelegate()
  if not options.revision:
    revision, _ = FindMostRecentSdkTools(delegate)
  else:
    revision = int(options.revision)

  UpdateManifestFileToRevision('json/naclsdk_manifest0.json', revision)
  UpdateManifestFileToRevision('json/naclsdk_manifest2.json', revision)
  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
