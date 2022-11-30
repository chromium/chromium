#!/usr/bin/env python
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import copy
import datetime
import hashlib
import logging
import os
import posixpath
import subprocess
import sys
import tempfile
import unittest
import urlparse

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
BUILD_TOOLS_DIR = os.path.dirname(SCRIPT_DIR)

sys.path.append(BUILD_TOOLS_DIR)
import manifest_util
import update_nacl_manifest
from update_nacl_manifest import CANARY_BUNDLE_NAME


HTTPS_BASE_URL = 'https://storage.googleapis.com' \
    '/nativeclient_mirror/nacl/nacl_sdk/'

OS_CR = ('cros',)
OS_L = ('linux',)
OS_M = ('mac',)
OS_ML = ('mac', 'linux')
OS_MW = ('mac', 'win')
OS_LW = ('linux', 'win')
OS_MLW = ('mac', 'linux', 'win')
OS_ALL = ('all',)
POST_STABLE = 'post_stable'
STABLE = 'stable'
BETA = 'beta'
DEV = 'dev'
CANARY = 'canary'


def GetArchiveURL(basename, version):
  return urlparse.urljoin(HTTPS_BASE_URL, posixpath.join(version, basename))


def GetPlatformArchiveUrl(host_os, version):
  basename = 'naclsdk_%s.tar.bz2' % (host_os,)
  return GetArchiveURL(basename, version)


def MakeGsUrl(rel_path):
  return update_nacl_manifest.GS_BUCKET_PATH + rel_path


def GetPathFromGsUrl(url):
  assert url.startswith(update_nacl_manifest.GS_BUCKET_PATH)
  return url[len(update_nacl_manifest.GS_BUCKET_PATH):]


def GetPathFromHttpsUrl(url):
  assert url.startswith(HTTPS_BASE_URL)
  return url[len(HTTPS_BASE_URL):]


def MakeArchive(url, host_os):
  archive = manifest_util.Archive(host_os)
  archive.url = url
  # dummy values that won't succeed if we ever use them, but will pass
  # validation. :)
  archive.checksum = {'sha1': 'foobar'}
  archive.size = 1
  return archive


def MakePlatformArchive(host_os, version):
  return MakeArchive(GetPlatformArchiveUrl(host_os, version), host_os)


def MakeNonPlatformArchive(basename, version):
  return MakeArchive(GetArchiveURL(basename, version), 'all')


def MakeNonPepperBundle(name, with_archives=False):
  bundle = manifest_util.Bundle(name)
  bundle.version = 1
  bundle.revision = 1
  bundle.description = 'Dummy bundle'
  bundle.recommended = 'yes'
  bundle.stability = 'stable'

  if with_archives:
    for host_os in OS_MLW:
      archive = manifest_util.Archive(host_os)
      archive.url = 'http://example.com'
      archive.checksum = {'sha1': 'blah'}
      archive.size = 2
      bundle.AddArchive(archive)
  return bundle


def MakePepperBundle(major_version, revision=0, version=None, stability='dev',
                     bundle_name=None):
  assert (version is None or
          version.split('.')[0] == 'trunk' or
          version.split('.')[0] == str(major_version))
  if not bundle_name:
    bundle_name = 'pepper_' + str(major_version)

  bundle = manifest_util.Bundle(bundle_name)
  bundle.version = major_version
  bundle.revision = revision
  bundle.description = 'Chrome %s bundle, revision %s' % (major_version,
                                                          revision)
  bundle.repath = 'pepper_' + str(major_version)
  bundle.recommended = 'no'
  bundle.stability = stability

  return bundle


def MakePlatformBundle(major_version, revision=0, version=None, host_oses=None,
                       stability='dev'):
  bundle = MakePepperBundle(major_version, revision, version, stability)

  if host_oses:
    for host_os in host_oses:
      bundle.AddArchive(MakePlatformArchive(host_os, version))

  return bundle


def MakeBionicBundle(major_version, revision=0, version=None, host_oses=None):
  bundle = MakePepperBundle(major_version, revision, version, 'dev')

  if host_oses:
    for host_os in host_oses:
      bundle.AddArchive(MakeBionicArchive(host_os, version))

  return bundle


class MakeManifest(manifest_util.SDKManifest):
  def __init__(self, *args):
    manifest_util.SDKManifest.__init__(self)

    for bundle in args:
      self.AddBundle(bundle)

  def AddBundle(self, bundle):
    self.MergeBundle(bundle, allow_existing=False)


class MakeHistory(object):
  def __init__(self):
    # used for a dummy timestamp
    self.datetime = datetime.datetime.utcnow()
    self.history = []

  def Add(self, host_oses, channel, version):
    for host_os in host_oses:
      timestamp = self.datetime.strftime('%Y-%m-%d %H:%M:%S.%f')
      self.history.append((host_os, channel, version, timestamp))
      self.datetime += datetime.timedelta(0, -3600) # one hour earlier
    self.datetime += datetime.timedelta(-1) # one day earlier


class MakeFiles(dict):
  def AddOnlineManifest(self, manifest_string):
    self['naclsdk_manifest2.json'] = manifest_string

  def Add(self, bundle, add_archive_for_os=OS_MLW, add_json_for_os=OS_MLW):
    for archive in bundle.GetArchives():
      if not archive.host_os in add_archive_for_os:
        continue

      self.AddArchive(bundle, archive, archive.host_os in add_json_for_os)

  def AddArchive(self, bundle, archive, add_json=True):
    path = GetPathFromHttpsUrl(archive.url)
    self[path] = 'My Dummy archive'

    if add_json:
      # add .json manifest snippet, it should look like a normal Bundle, but
      # only has one archive.
      new_bundle = manifest_util.Bundle('')
      new_bundle.CopyFrom(bundle)
      del new_bundle.archives[:]
      new_bundle.AddArchive(archive)
      self[path + '.json'] = new_bundle.GetDataAsString()


class TestDelegate(update_nacl_manifest.Delegate):
  def __init__(self, manifest, history, files):
    self.manifest = manifest
    self.history = history
    self.files = files
    self.dryrun = 0
    self.called_gsutil_cp = False
    self.called_sendmail = False

  def GetRepoManifest(self):
    return self.manifest

  def GetHistory(self):
    return self.history

  def GsUtil_ls(self, url):
    path = GetPathFromGsUrl(url)
    result = []
    for filename in self.files.iterkeys():
      if not filename.startswith(path):
        continue

      # Find the first slash after the prefix (path).
      # +1, because if the slash is directly after path, then we want to find
      # the following slash anyway.
      slash = filename.find('/', len(path) + 1)

      if slash != -1:
        filename = filename[:slash]

      result.append(MakeGsUrl(filename))

    # Remove dupes.
    return list(set(result))

  def GsUtil_cat(self, url):
    path = GetPathFromGsUrl(url)
    if path not in self.files:
      raise subprocess.CalledProcessError(1, 'gsutil cat %s' % (url,))
    return self.files[path]

  def GsUtil_cp(self, src, dest, stdin=None):
    self.called_gsutil_cp = True
    dest_path = GetPathFromGsUrl(dest)
    if src == '-':
      self.files[dest_path] = stdin
    else:
      src_path = GetPathFromGsUrl(src)
      if src_path not in self.files:
        raise subprocess.CalledProcessError(1, 'gsutil cp %s %s' % (src, dest))
      self.files[dest_path] = self.files[src_path]

  def SendMail(self, subject, text):
    self.called_sendmail = True


# Shorthand for premade bundles/versions
V18_0_1025_163 = '18.0.1025.163'
V18_0_1025_175 = '18.0.1025.175'
V18_0_1025_184 = '18.0.1025.184'
V19_0_1084_41 = '19.0.1084.41'
V19_0_1084_67 = '19.0.1084.67'
V21_0_1145_0 = '21.0.1145.0'
V21_0_1166_0 = '21.0.1166.0'
V26_0_1386_0 = '26.0.1386.0'
V26_0_1386_1 = '26.0.1386.1'
V37_0_2054_0 = '37.0.2054.0'
VTRUNK_140819 = 'trunk.140819'
VTRUNK_277776 = 'trunk.277776'
B18_0_1025_163_MLW = MakePlatformBundle(18, 132135, V18_0_1025_163, OS_MLW)
B18_0_1025_184_MLW = MakePlatformBundle(18, 134900, V18_0_1025_184, OS_MLW)
B18_NONE = MakePlatformBundle(18)
B19_0_1084_41_MLW = MakePlatformBundle(19, 134854, V19_0_1084_41, OS_MLW)
B19_0_1084_67_MLW = MakePlatformBundle(19, 142000, V19_0_1084_67, OS_MLW)
B19_NONE = MakePlatformBundle(19)
BCANARY_NONE = MakePepperBundle(0, stability=CANARY,
                                bundle_name=CANARY_BUNDLE_NAME)
B21_0_1145_0_MLW = MakePlatformBundle(21, 138079, V21_0_1145_0, OS_MLW)
B21_0_1166_0_MW = MakePlatformBundle(21, 140819, V21_0_1166_0, OS_MW)
B21_NONE = MakePlatformBundle(21)
B26_NONE = MakePlatformBundle(26)
B26_0_1386_0_MLW = MakePlatformBundle(26, 177362, V26_0_1386_0, OS_MLW)
B26_0_1386_1_MLW = MakePlatformBundle(26, 177439, V26_0_1386_1, OS_MLW)
BTRUNK_140819_MLW = MakePlatformBundle(21, 140819, VTRUNK_140819, OS_MLW)
NON_PEPPER_BUNDLE_NOARCHIVES = MakeNonPepperBundle('foo')
NON_PEPPER_BUNDLE_ARCHIVES = MakeNonPepperBundle('bar', with_archives=True)


class TestUpdateManifest(unittest.TestCase):
  def setUp(self):
    self.history = MakeHistory()
    self.files = MakeFiles()
    self.version_mapping = {}
    self.delegate = None
    self.uploaded_manifest = None
    self.manifest = None

    logging.basicConfig(level=logging.CRITICAL)
    # Uncomment the following line to enable more debugging info.
    # logging.getLogger('update_nacl_manifest').setLevel(logging.INFO)

  def _MakeDelegate(self):
    self.delegate = TestDelegate(self.manifest, self.history.history,
        self.files)

  def _Run(self, host_oses, extra_archives=None, fixed_bundle_versions=None):
    update_nacl_manifest.Run(self.delegate, host_oses, extra_archives,
                             fixed_bundle_versions)

  def _HasUploadedManifest(self):
    return 'naclsdk_manifest2.json' in self.files

  def _ReadUploadedManifest(self):
    self.uploaded_manifest = manifest_util.SDKManifest()
    self.uploaded_manifest.LoadDataFromString(
        self.files['naclsdk_manifest2.json'])

  def _AssertUploadedManifestHasBundle(self, bundle, stability,
                                       bundle_name=None):
    if not bundle_name:
      bundle_name = bundle.name

    uploaded_manifest_bundle = self.uploaded_manifest.GetBundle(bundle_name)
    # Bundles that we create in the test (and in the manifest snippets) have
    # their stability set to "dev". update_nacl_manifest correctly updates it.
    # So we have to force the stability of |bundle| so they compare equal.
    test_bundle = copy.copy(bundle)
    test_bundle.stability = stability
    if bundle_name:
      test_bundle.name = bundle_name
    self.assertEqual(uploaded_manifest_bundle, test_bundle)

  def _AddCsvHistory(self, history):
    import csv
    import cStringIO
    history_stream = cStringIO.StringIO(history)
    self.history.history = [(platform, channel, version, date)
        for platform, channel, version, date in csv.reader(history_stream)]

  def testNoUpdateNeeded(self):
    self.manifest = MakeManifest(B18_0_1025_163_MLW)
    self._MakeDelegate()
    self._Run(OS_MLW)
    self.assertFalse(self._HasUploadedManifest())

    # Add another bundle, make sure it still doesn't update
    self.manifest.AddBundle(B19_0_1084_41_MLW)
    self._Run(OS_MLW)
    self.assertFalse(self._HasUploadedManifest())

  def testSimpleUpdate(self):
    self.manifest = MakeManifest(B18_NONE)
    self.history.Add(OS_MLW, BETA, V18_0_1025_163)
    self.files.Add(B18_0_1025_163_MLW)
    self._MakeDelegate()
    self._Run(OS_MLW)
    self._ReadUploadedManifest()
    self._AssertUploadedManifestHasBundle(B18_0_1025_163_MLW, BETA)
    self.assertEqual(len(self.uploaded_manifest.GetBundles()), 1)

  def testOnePlatformHasNewerRelease(self):
    self.manifest = MakeManifest(B18_NONE)
    self.history.Add(OS_M, BETA, V18_0_1025_175)  # Mac has newer version
    self.history.Add(OS_MLW, BETA, V18_0_1025_163)
    self.files.Add(B18_0_1025_163_MLW)
    self._MakeDelegate()
    self._Run(OS_MLW)
    self._ReadUploadedManifest()
    self._AssertUploadedManifestHasBundle(B18_0_1025_163_MLW, BETA)
    self.assertEqual(len(self.uploaded_manifest.GetBundles()), 1)

  def testMultipleMissingPlatformsInHistory(self):
    self.manifest = MakeManifest(B18_NONE)
    self.history.Add(OS_ML, BETA, V18_0_1025_184)
    self.history.Add(OS_M, BETA, V18_0_1025_175)
    self.history.Add(OS_MLW, BETA, V18_0_1025_163)
    self.files.Add(B18_0_1025_163_MLW)
    self._MakeDelegate()
    self._Run(OS_MLW)
    self._ReadUploadedManifest()
    self._AssertUploadedManifestHasBundle(B18_0_1025_163_MLW, BETA)
    self.assertEqual(len(self.uploaded_manifest.GetBundles()), 1)

  def testUpdateOnlyOneBundle(self):
    self.manifest = MakeManifest(B18_NONE, B19_0_1084_41_MLW)
    self.history.Add(OS_MLW, BETA, V18_0_1025_163)
    self.files.Add(B18_0_1025_163_MLW)
    self._MakeDelegate()
    self._Run(OS_MLW)
    self._ReadUploadedManifest()
    self._AssertUploadedManifestHasBundle(B18_0_1025_163_MLW, BETA)
    self._AssertUploadedManifestHasBundle(B19_0_1084_41_MLW, DEV)
    self.assertEqual(len(self.uploaded_manifest.GetBundles()), 2)

  def testUpdateTwoBundles(self):
    self.manifest = MakeManifest(B18_NONE, B19_NONE)
    self.history.Add(OS_MLW, DEV, V19_0_1084_41)
    self.history.Add(OS_MLW, BETA, V18_0_1025_163)
    self.files.Add(B18_0_1025_163_MLW)
    self.files.Add(B19_0_1084_41_MLW)
    self._MakeDelegate()
    self._Run(OS_MLW)
    self._ReadUploadedManifest()
    self._AssertUploadedManifestHasBundle(B18_0_1025_163_MLW, BETA)
    self._AssertUploadedManifestHasBundle(B19_0_1084_41_MLW, DEV)
    self.assertEqual(len(self.uploaded_manifest.GetBundles()), 2)

  def testUpdateWithMissingPlatformsInArchives(self):
    self.manifest = MakeManifest(B18_NONE)
    self.history.Add(OS_MLW, BETA, V18_0_1025_184)
    self.history.Add(OS_MLW, BETA, V18_0_1025_163)
    self.files.Add(B18_0_1025_184_MLW, add_archive_for_os=OS_M)
    self.files.Add(B18_0_1025_163_MLW)
    self._MakeDelegate()
    self._Run(OS_MLW)
    self._ReadUploadedManifest()
    self._AssertUploadedManifestHasBundle(B18_0_1025_163_MLW, BETA)
    self.assertEqual(len(self.uploaded_manifest.GetBundles()), 1)

  def testUpdateWithMissingManifestSnippets(self):
    self.manifest = MakeManifest(B18_NONE)
    self.history.Add(OS_MLW, BETA, V18_0_1025_184)
    self.history.Add(OS_MLW, BETA, V18_0_1025_163)
    self.files.Add(B18_0_1025_184_MLW, add_json_for_os=OS_ML)
    self.files.Add(B18_0_1025_163_MLW)
    self._MakeDelegate()
    self._Run(OS_MLW)
    self._ReadUploadedManifest()
    self._AssertUploadedManifestHasBundle(B18_0_1025_163_MLW, BETA)
    self.assertEqual(len(self.uploaded_manifest.GetBundles()), 1)

  def testRecommendedIsStable(self):
    for channel in STABLE, BETA, DEV, CANARY:
      self.setUp()
      bundle = copy.deepcopy(B18_NONE)
      self.manifest = MakeManifest(bundle)
      self.history.Add(OS_MLW, channel, V18_0_1025_163)
      self.files.Add(B18_0_1025_163_MLW)
      self._MakeDelegate()
      self._Run(OS_MLW)
      self._ReadUploadedManifest()
      self.assertEqual(len(self.uploaded_manifest.GetBundles()), 1)
      uploaded_bundle = self.uploaded_manifest.GetBundle('pepper_18')
      if channel == STABLE:
        self.assertEqual(uploaded_bundle.recommended, 'yes')
      else:
        self.assertEqual(uploaded_bundle.recommended, 'no')

  def testNoUpdateWithNonPepperBundle(self):
    self.manifest = MakeManifest(NON_PEPPER_BUNDLE_NOARCHIVES,
        B18_0_1025_163_MLW)
    self._MakeDelegate()
    self._Run(OS_MLW)
    self.assertFalse(self._HasUploadedManifest())

  def testUpdateWithHistoryWithExtraneousPlatforms(self):
    self.manifest = MakeManifest(B18_NONE)
    self.history.Add(OS_ML, BETA, V18_0_1025_184)
    self.history.Add(OS_CR, BETA, V18_0_1025_184)
    self.history.Add(OS_CR, BETA, V18_0_1025_175)
    self.history.Add(OS_MLW, BETA, V18_0_1025_163)
    self.files.Add(B18_0_1025_163_MLW)
    self._MakeDelegate()
    self._Run(OS_MLW)
    self._ReadUploadedManifest()
    self._AssertUploadedManifestHasBundle(B18_0_1025_163_MLW, BETA)
    self.assertEqual(len(self.uploaded_manifest.GetBundles()), 1)

  def testSnippetWithStringRevisionAndVersion(self):
    # This test exists because some manifest snippets were uploaded with
    # strings for their revisions and versions. I want to make sure the
    # resulting manifest is still consistent with the old format.
    self.manifest = MakeManifest(B18_NONE)
    self.history.Add(OS_MLW, BETA, V18_0_1025_163)
    bundle_string_revision = MakePlatformBundle('18', '1234', V18_0_1025_163,
                                                OS_MLW)
    self.files.Add(bundle_string_revision)
    self._MakeDelegate()
    self._Run(OS_MLW)
    self._ReadUploadedManifest()
    uploaded_bundle = self.uploaded_manifest.GetBundle(
        bundle_string_revision.name)
    self.assertEqual(uploaded_bundle.revision, 1234)
    self.assertEqual(uploaded_bundle.version, 18)

  def testUpdateCanary(self):
    self.manifest = MakeManifest(copy.deepcopy(BCANARY_NONE))
    self.files.Add(BTRUNK_140819_MLW)
    self._MakeDelegate()
    self._Run(OS_MLW)
    self._ReadUploadedManifest()
    self._AssertUploadedManifestHasBundle(BTRUNK_140819_MLW, CANARY,
                                          bundle_name=CANARY_BUNDLE_NAME)

  def testCanaryShouldOnlyUseCanaryVersions(self):
    canary_bundle = copy.deepcopy(BCANARY_NONE)
    self.manifest = MakeManifest(canary_bundle)
    self.history.Add(OS_MW, CANARY, V21_0_1166_0)
    self.history.Add(OS_MW, BETA, V19_0_1084_41)
    self.files.Add(B19_0_1084_41_MLW)
    self.version_mapping[V21_0_1166_0] = VTRUNK_140819
    self._MakeDelegate()
    self.assertRaises(Exception, self._Run, OS_MLW)

  def testExtensionWorksAsBz2(self):
    # Allow old bundles with just .bz2 extension to work
    self.manifest = MakeManifest(B18_NONE)
    self.history.Add(OS_MLW, BETA, V18_0_1025_163)
    bundle = copy.deepcopy(B18_0_1025_163_MLW)
    archive_url = bundle.GetArchive('mac').url
    bundle.GetArchive('mac').url = archive_url.replace('.tar', '')
    self.files.Add(bundle)
    self._MakeDelegate()
    self._Run(OS_MLW)
    self._ReadUploadedManifest()
    self._AssertUploadedManifestHasBundle(bundle, BETA)
    self.assertEqual(len(self.uploaded_manifest.GetBundles()), 1)

  def testOnlyOneStableBundle(self):
    # Make sure that any bundle that has an older version than STABLE is marked
    # as POST_STABLE, even if the last version we found was BETA, DEV, etc.
    for channel in STABLE, BETA, DEV, CANARY:
      self.setUp()
      self.manifest = MakeManifest(B18_NONE, B19_NONE)
      self.history.Add(OS_MLW, channel, V18_0_1025_163)
      self.history.Add(OS_MLW, STABLE, V19_0_1084_41)
      self.files.Add(B18_0_1025_163_MLW)
      self.files.Add(B19_0_1084_41_MLW)
      self._MakeDelegate()
      self._Run(OS_MLW)
      self._ReadUploadedManifest()
      p18_bundle = self.uploaded_manifest.GetBundle(B18_NONE.name)
      self.assertEqual(p18_bundle.stability, POST_STABLE)
      self.assertEqual(p18_bundle.recommended, 'no')
      p19_bundle = self.uploaded_manifest.GetBundle(B19_NONE.name)
      self.assertEqual(p19_bundle.stability, STABLE)
      self.assertEqual(p19_bundle.recommended, 'yes')

  def testDontPushIfNoChange(self):
    # Make an online manifest that already has this bundle.
    online_manifest = MakeManifest(B18_0_1025_163_MLW)
    self.files.AddOnlineManifest(online_manifest.GetDataAsString())

    self.manifest = MakeManifest(B18_NONE)
    self.history.Add(OS_MLW, DEV, V18_0_1025_163)
    self.files.Add(B18_0_1025_163_MLW)

    self._MakeDelegate()
    self._Run(OS_MLW)
    self.assertFalse(self.delegate.called_gsutil_cp)

  def testDontPushIfRollback(self):
    # Make an online manifest that has a newer bundle
    online_manifest = MakeManifest(B18_0_1025_184_MLW)
    self.files.AddOnlineManifest(online_manifest.GetDataAsString())

    self.manifest = MakeManifest(B18_NONE)
    self.history.Add(OS_MLW, DEV, V18_0_1025_163)
    self.files.Add(B18_0_1025_163_MLW)

    self._MakeDelegate()
    self._Run(OS_MLW)
    self.assertFalse(self.delegate.called_gsutil_cp)

  def testRunWithFixedBundleVersions(self):
    self.manifest = MakeManifest(B18_NONE)
    self.history.Add(OS_MLW, BETA, V18_0_1025_163)
    self.files.Add(B18_0_1025_163_MLW)
    self.files.Add(B18_0_1025_184_MLW)

    self._MakeDelegate()
    self._Run(OS_MLW, fixed_bundle_versions=[('pepper_18', '18.0.1025.184')])
    self._ReadUploadedManifest()
    self._AssertUploadedManifestHasBundle(B18_0_1025_184_MLW, BETA)
    self.assertEqual(len(self.uploaded_manifest.GetBundles()), 1)

  def testRunWithMissingFixedBundleVersions(self):
    self.manifest = MakeManifest(B18_NONE)
    self.history.Add(OS_MLW, BETA, V18_0_1025_163)
    self.files.Add(B18_0_1025_163_MLW)

    self._MakeDelegate()
    self._Run(OS_MLW, fixed_bundle_versions=[('pepper_18', '18.0.1025.184')])
    # Nothing should be uploaded if the user gives a missing fixed version.
    self.assertFalse(self.delegate.called_gsutil_cp)

  def testDontIncludeRandomBundles(self):
    self.manifest = MakeManifest(B26_NONE)
    self.history.Add(OS_MLW, BETA, V26_0_1386_0)
    self.files.Add(B26_0_1386_0_MLW)

    some_other_bundle = MakePepperBundle(26, 1, V26_0_1386_0, BETA)
    some_other_archive = MakeNonPlatformArchive('some_other.tar.bz2',
                                                V26_0_1386_0)
    some_other_bundle.AddArchive(some_other_archive)
    self.files.AddArchive(some_other_bundle, some_other_archive)

    self._MakeDelegate()
    self._Run(OS_MLW)
    self._ReadUploadedManifest()
    uploaded_bundle = self.uploaded_manifest.GetBundle('pepper_26')
    self.assertEqual(1, len(uploaded_bundle.GetHostOSArchives()))

  def _AddNaclportBundles(self):
    # Add NaclPorts "bundle" for 18, 21 and 26.
    self.manifest = MakeManifest(B18_NONE, B21_NONE, B26_NONE)
    self.history.Add(OS_MLW, BETA, V26_0_1386_0)
    self.files.Add(B26_0_1386_0_MLW)
    self.history.Add(OS_MLW, BETA, V21_0_1145_0)
    self.files.Add(B21_0_1145_0_MLW)
    self.history.Add(OS_MLW, BETA, V18_0_1025_163)
    self.files.Add(B18_0_1025_163_MLW)

    naclports_bundle = MakePepperBundle(26, 1, V26_0_1386_0, BETA)
    naclports_archive = MakeNonPlatformArchive('naclports.tar.bz2',
                                               V26_0_1386_0)
    naclports_bundle.AddArchive(naclports_archive)
    self.files.AddArchive(naclports_bundle, naclports_archive)

    naclports_bundle = MakePepperBundle(21, 1, V21_0_1145_0, BETA)
    naclports_archive = MakeNonPlatformArchive('naclports.tar.bz2',
                                               V21_0_1145_0)
    naclports_bundle.AddArchive(naclports_archive)
    self.files.AddArchive(naclports_bundle, naclports_archive)

    naclports_bundle = MakePepperBundle(18, 1, V18_0_1025_163, BETA)
    naclports_archive = MakeNonPlatformArchive('naclports.tar.bz2',
                                               V18_0_1025_163)
    naclports_bundle.AddArchive(naclports_archive)
    self.files.AddArchive(naclports_bundle, naclports_archive)

  def testNaclportsBundle(self):
    self._AddNaclportBundles()
    self._MakeDelegate()
    extra_archives = [('naclports.tar.bz2', '19.0.0.0', '22.0.0.0')]
    self._Run(OS_MLW, extra_archives=extra_archives)
    self._ReadUploadedManifest()

    uploaded_bundle = self.uploaded_manifest.GetBundle('pepper_26')
    self.assertEqual(1, len(uploaded_bundle.GetHostOSArchives()))

    uploaded_bundle = self.uploaded_manifest.GetBundle('pepper_21')
    self.assertEqual(2, len(uploaded_bundle.GetHostOSArchives()))

    uploaded_bundle = self.uploaded_manifest.GetBundle('pepper_18')
    self.assertEqual(1, len(uploaded_bundle.GetHostOSArchives()))

  def testKeepBundleOrder(self):
    # This is a regression test: when a bundle is skipped (because it isn't
    # newer than the online bundle), it was added to the end of the list.

    # Make an online manifest that already has B18.
    online_manifest = MakeManifest(B18_0_1025_163_MLW)
    self.files.AddOnlineManifest(online_manifest.GetDataAsString())

    self.manifest = MakeManifest(B18_NONE, B19_NONE)
    self.history.Add(OS_MLW, STABLE, V18_0_1025_163)
    self.history.Add(OS_MLW, STABLE, V19_0_1084_41)
    self.files.Add(B18_0_1025_163_MLW)
    self.files.Add(B19_0_1084_41_MLW)

    self._MakeDelegate()
    self._Run(OS_MLW)
    self._ReadUploadedManifest()

    # Bundle 18 should be before bundle 19.
    bundles = self.uploaded_manifest.GetBundles()
    self.assertEqual(2, len(bundles))
    self.assertEqual('pepper_18', bundles[0].name)
    self.assertEqual('pepper_19', bundles[1].name)

  def testBundleWithoutHistoryUsesOnline(self):
    online_manifest = MakeManifest(B18_0_1025_163_MLW)
    self.files.AddOnlineManifest(online_manifest.GetDataAsString())

    self.manifest = MakeManifest(B18_NONE)

    self._MakeDelegate()
    # This should not raise.
    self._Run(OS_MLW)
    self._ReadUploadedManifest()

    # But it should have sent an email nagging the users to lock this bundle
    # manually.
    self.assertTrue(self.delegate.called_sendmail)

    uploaded_bundle = self.uploaded_manifest.GetBundle('pepper_18')
    self.assertEqual(uploaded_bundle, B18_0_1025_163_MLW)

  def testBundleWithoutHistoryOrOnlineRaises(self):
    self.manifest = MakeManifest(B18_NONE)
    self._MakeDelegate()
    self.assertRaises(update_nacl_manifest.UnknownLockedBundleException,
                      self._Run, OS_MLW)


class TestUpdateVitals(unittest.TestCase):
  def setUp(self):
    f = tempfile.NamedTemporaryFile('w', prefix="test_update_nacl_manifest")
    self.test_file = f.name
    f.close()
    test_data = "Some test data"
    self.sha1 = hashlib.sha1(test_data).hexdigest()
    self.data_len = len(test_data)
    with open(self.test_file, 'w') as f:
      f.write(test_data)

  def tearDown(self):
    os.remove(self.test_file)

  def testUpdateVitals(self):
    archive = manifest_util.Archive(manifest_util.GetHostOS())
    path = os.path.abspath(self.test_file)
    if sys.platform == 'win32':
      # On Windows, the path must start with three slashes, i.e.
      # (file:///C:\whatever)
      path = '/' + path
    archive.url = 'file://' + path

    bundle = MakePlatformBundle(18)
    bundle.AddArchive(archive)
    manifest = MakeManifest(bundle)
    archive = manifest.GetBundles()[0]['archives'][0]

    self.assertTrue('size' not in archive)
    self.assertTrue('checksum' not in archive)
    self.assertRaises(manifest_util.Error, manifest.Validate)

    manifest.Validate(add_missing_info=True)

    self.assertEqual(archive['size'], self.data_len)
    self.assertEqual(archive['checksum']['sha1'], self.sha1)


if __name__ == '__main__':
  unittest.main()
