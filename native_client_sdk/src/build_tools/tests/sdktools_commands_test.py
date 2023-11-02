#!/usr/bin/env python
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import re
import tarfile
import tempfile
import unittest
from sdktools_test import SdkToolsTestCase

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
BUILD_TOOLS_DIR = os.path.dirname(SCRIPT_DIR)
TOOLS_DIR = os.path.join(os.path.dirname(BUILD_TOOLS_DIR), 'tools')

sys.path.extend([BUILD_TOOLS_DIR, TOOLS_DIR])
import manifest_util
import oshelpers


class TestCommands(SdkToolsTestCase):
  def setUp(self):
    self.SetupDefault()

  def _AddDummyBundle(self, manifest, bundle_name):
    bundle = manifest_util.Bundle(bundle_name)
    bundle.revision = 1337
    bundle.version = 23
    bundle.description = bundle_name
    bundle.stability = 'beta'
    bundle.recommended = 'no'
    bundle.repath = bundle_name
    archive = self._MakeDummyArchive(bundle_name)
    bundle.AddArchive(archive)
    manifest.SetBundle(bundle)

    # Need to get the bundle from the manifest -- it doesn't use the one we
    # gave it.
    return manifest.GetBundle(bundle_name)

  def _MakeDummyArchive(self, bundle_name, tarname=None, filename='dummy.txt'):
    tarname = (tarname or bundle_name) + '.tar.bz2'
    temp_dir = tempfile.mkdtemp(prefix='archive')
    try:
      dummy_path = os.path.join(temp_dir, filename)
      with open(dummy_path, 'w') as stream:
        stream.write('Dummy stuff for %s' % bundle_name)

      # Build the tarfile directly into the server's directory.
      tar_path = os.path.join(self.basedir, tarname)
      tarstream = tarfile.open(tar_path, 'w:bz2')
      try:
        tarstream.add(dummy_path, os.path.join(bundle_name, filename))
      finally:
        tarstream.close()

      with open(tar_path, 'rb') as archive_stream:
        sha1, size = manifest_util.DownloadAndComputeHash(archive_stream)

      archive = manifest_util.Archive(manifest_util.GetHostOS())
      archive.url = self.server.GetURL(os.path.basename(tar_path))
      archive.size = size
      archive.checksum = sha1
      return archive
    finally:
      oshelpers.Remove(['-rf', temp_dir])

  def testInfoBasic(self):
    """The info command should display information about the given bundle."""
    self._WriteManifest()
    output = self._Run(['info', 'sdk_tools'])
    # Make sure basic information is there
    bundle = self.manifest.GetBundle('sdk_tools')
    archive = bundle.GetHostOSArchive();
    self.assertTrue(bundle.name in output)
    self.assertTrue(bundle.description in output)
    self.assertTrue(str(bundle.revision) in output)
    self.assertTrue(str(archive.size) in output)
    self.assertTrue(archive.checksum in output)
    self.assertTrue(bundle.stability in output)

  def testInfoUnknownBundle(self):
    """The info command should notify the user of unknown bundles."""
    self._WriteManifest()
    bogus_bundle = 'foobar'
    output = self._Run(['info', bogus_bundle])
    self.assertTrue(re.search(r'[uU]nknown', output))
    self.assertTrue(bogus_bundle in output)

  def testInfoMultipleBundles(self):
    """The info command should support listing multiple bundles."""
    self._AddDummyBundle(self.manifest, 'pepper_23')
    self._AddDummyBundle(self.manifest, 'pepper_24')
    self._WriteManifest()
    output = self._Run(['info', 'pepper_23', 'pepper_24'])
    self.assertTrue('pepper_23' in output)
    self.assertTrue('pepper_24' in output)
    self.assertFalse(re.search(r'[uU]nknown', output))

  def testInfoMultipleArchives(self):
    """The info command should display multiple archives."""
    bundle = self._AddDummyBundle(self.manifest, 'pepper_26')
    archive2 = self._MakeDummyArchive('pepper_26', tarname='pepper_26_more',
                                      filename='dummy2.txt')
    archive2.host_os = 'all'
    bundle.AddArchive(archive2)
    self._WriteManifest()
    output = self._Run(['info', 'pepper_26'])
    self.assertTrue('pepper_26' in output)
    self.assertTrue('pepper_26_more' in output)

  def testListBasic(self):
    """The list command should display basic information about remote
    bundles."""
    self._WriteManifest()
    output = self._Run(['list'])
    self.assertTrue(re.search('I.*?sdk_tools.*?stable', output, re.MULTILINE))
    # This line is important (it's used by the updater to determine if the
    # sdk_tools bundle needs to be updated), so let's be explicit.
    self.assertTrue('All installed bundles are up to date.')

  def testListMultiple(self):
    """The list command should display multiple bundles."""
    self._AddDummyBundle(self.manifest, 'pepper_23')
    self._WriteManifest()
    output = self._Run(['list'])
    # Added pepper_23 to the remote manifest not the local manifest, so it
    # shouldn't be installed.
    self.assertTrue(re.search('^[^I]*pepper_23', output, re.MULTILINE))
    self.assertTrue('sdk_tools' in output)

  def testListWithRevision(self):
    """The list command should display the revision, if desired."""
    self._AddDummyBundle(self.manifest, 'pepper_23')
    self._WriteManifest()
    output = self._Run(['list', '-r'])
    self.assertTrue(re.search('pepper_23.*?r1337', output))

  def testListWithUpdatedRevision(self):
    """The list command should display when there is an update available."""
    p23bundle = self._AddDummyBundle(self.manifest, 'pepper_23')
    self._WriteCacheManifest(self.manifest)
    # Modify the remote manifest to have a newer revision.
    p23bundle.revision += 1
    self._WriteManifest()
    output = self._Run(['list', '-r'])
    # We should see a display like this:  I* pepper_23 (r1337 -> r1338)
    # The star indicates the bundle has an update.
    self.assertTrue(re.search('I\*\s+pepper_23.*?r1337.*?r1338', output))

  def testListLocalVersionNotOnRemote(self):
    """The list command should tell the user if they have a bundle installed
    that doesn't exist in the remote manifest."""
    self._WriteManifest()
    p23bundle = self._AddDummyBundle(self.manifest, 'pepper_23')
    self._WriteCacheManifest(self.manifest)
    # Create pepper_23 directory so that manifest entry doesn't get purged
    os.mkdir(os.path.join(self.nacl_sdk_base, 'pepper_23'))
    output = self._Run(['list', '-r'])
    message = 'Bundles installed locally that are not available remotely:'
    self.assertIn(message, output)
    # Make sure pepper_23 is listed after the message above.
    self.assertTrue('pepper_23' in output[output.find(message):])

  def testSources(self):
    """The sources command should allow adding/listing/removing of sources.
    When a source is added, it will provide an additional set of bundles."""
    other_manifest = manifest_util.SDKManifest()
    self._AddDummyBundle(other_manifest, 'naclmono_23')
    with open(os.path.join(self.basedir, 'source.json'), 'w') as stream:
      stream.write(other_manifest.GetDataAsString())

    source_json_url = self.server.GetURL('source.json')
    self._WriteManifest()
    output = self._Run(['sources', '--list'])
    self.assertTrue('No external sources installed.' in output)
    output = self._Run(['sources', '--add', source_json_url])
    output = self._Run(['sources', '--list'])
    self.assertTrue(source_json_url in output)

    # Should be able to get info about that bundle.
    output = self._Run(['info', 'naclmono_23'])
    self.assertTrue('Unknown bundle' not in output)

    self._Run(['sources', '--remove', source_json_url])
    output = self._Run(['sources', '--list'])
    self.assertTrue('No external sources installed.' in output)

  def testUpdateBasic(self):
    """The update command should install the contents of a bundle to the SDK."""
    self._AddDummyBundle(self.manifest, 'pepper_23')
    self._WriteManifest()
    self._Run(['update', 'pepper_23'])
    self.assertTrue(os.path.exists(
        os.path.join(self.basedir, 'nacl_sdk', 'pepper_23', 'dummy.txt')))

  def testUpdateInCacheButDirectoryRemoved(self):
    """The update command should update if the bundle directory does not exist,
    even if the bundle is already in the cache manifest."""
    self._AddDummyBundle(self.manifest, 'pepper_23')
    self._WriteCacheManifest(self.manifest)
    self._WriteManifest()
    self._Run(['update', 'pepper_23'])
    self.assertTrue(os.path.exists(
        os.path.join(self.basedir, 'nacl_sdk', 'pepper_23', 'dummy.txt')))

  def testUpdateNoNewVersion(self):
    """The update command should do nothing if the bundle is already up to date.
    """
    self._AddDummyBundle(self.manifest, 'pepper_23')
    self._WriteManifest()
    self._Run(['update', 'pepper_23'])
    output = self._Run(['update', 'pepper_23'])
    self.assertTrue('is already up to date.' in output)

  def testUpdateWithNewVersion(self):
    """The update command should update to a new version if it exists."""
    bundle = self._AddDummyBundle(self.manifest, 'pepper_23')
    self._WriteManifest()
    self._Run(['update', 'pepper_23'])

    bundle.revision += 1
    self._WriteManifest()
    output = self._Run(['update', 'pepper_23'])
    self.assertTrue('already exists, but has an update available' in output)

    # Now update using --force.
    output = self._Run(['update', 'pepper_23', '--force'])
    self.assertTrue('Updating bundle' in output)

    cache_manifest = self._ReadCacheManifest()
    num_archives = len(cache_manifest.GetBundle('pepper_23').GetArchives())
    self.assertEqual(num_archives, 1)

  def testUpdateUnknownBundles(self):
    """The update command should ignore unknown bundles and notify the user."""
    self._WriteManifest()
    output = self._Run(['update', 'foobar'])
    self.assertTrue('unknown bundle' in output)

  def testUpdateRecommended(self):
    """The update command should update only recommended bundles when run
    without args.
    """
    bundle_25 = self._AddDummyBundle(self.manifest, 'pepper_25')
    bundle_25.recommended = 'no'
    bundle_26 = self._AddDummyBundle(self.manifest, 'pepper_26')
    bundle_26.recommended = 'yes'

    self._WriteManifest()
    output = self._Run(['update'])

    # Should not try to update sdk_tools (even though it is recommended)
    self.assertTrue('Ignoring manual update request.' not in output)
    self.assertFalse(os.path.exists(
        os.path.join(self.basedir, 'nacl_sdk', 'pepper_25')))
    self.assertTrue(os.path.exists(
        os.path.join(self.basedir, 'nacl_sdk', 'pepper_26', 'dummy.txt')))

  def testUpdateCanary(self):
    """The update command should create the correct directory name for repath'd
    bundles.
    """
    bundle = self._AddDummyBundle(self.manifest, 'pepper_26')
    bundle.name = 'pepper_canary'
    self._WriteManifest()
    output = self._Run(['update', 'pepper_canary'])
    self.assertTrue(os.path.exists(
        os.path.join(self.basedir, 'nacl_sdk', 'pepper_canary', 'dummy.txt')))

  def testUpdateMultiArchive(self):
    """The update command should include download/untar multiple archives
    specified in the bundle.
    """
    bundle = self._AddDummyBundle(self.manifest, 'pepper_26')
    archive2 = self._MakeDummyArchive('pepper_26', tarname='pepper_26_more',
                                      filename='dummy2.txt')
    archive2.host_os = 'all'
    bundle.AddArchive(archive2)
    self._WriteManifest()
    output = self._Run(['update', 'pepper_26'])
    self.assertTrue(os.path.exists(
        os.path.join(self.basedir, 'nacl_sdk', 'pepper_26', 'dummy.txt')))
    self.assertTrue(os.path.exists(
        os.path.join(self.basedir, 'nacl_sdk', 'pepper_26', 'dummy2.txt')))

  def testUpdateBadSize(self):
    """If an archive has a bad size, print an error.
    """
    bundle = self._AddDummyBundle(self.manifest, 'pepper_26')
    archive = bundle.GetHostOSArchive();
    archive.size = -1
    self._WriteManifest()
    stdout = self._Run(['update', 'pepper_26'], expect_error=True)
    self.assertTrue('Size mismatch' in stdout)

  def testUpdateBadSHA(self):
    """If an archive has a bad SHA, print an error.
    """
    bundle = self._AddDummyBundle(self.manifest, 'pepper_26')
    archive = bundle.GetHostOSArchive();
    archive.checksum = 0
    self._WriteManifest()
    stdout = self._Run(['update', 'pepper_26'], expect_error=True)
    self.assertTrue('SHA1 checksum mismatch' in stdout)

  def testUninstall(self):
    """The uninstall command should remove the installed bundle, if it
    exists.
    """
    # First install the bundle.
    self._AddDummyBundle(self.manifest, 'pepper_23')
    self._WriteManifest()
    output = self._Run(['update', 'pepper_23'])
    self.assertTrue(os.path.exists(
        os.path.join(self.basedir, 'nacl_sdk', 'pepper_23', 'dummy.txt')))

    # Now remove it.
    self._Run(['uninstall', 'pepper_23'])
    self.assertFalse(os.path.exists(
        os.path.join(self.basedir, 'nacl_sdk', 'pepper_23')))

    # The bundle should not be marked as installed.
    output = self._Run(['list'])
    self.assertTrue(re.search('^[^I]*pepper_23', output, re.MULTILINE))

  def testReinstall(self):
    """The reinstall command should remove, then install, the specified
    bundles.
    """
    # First install the bundle.
    self._AddDummyBundle(self.manifest, 'pepper_23')
    self._WriteManifest()
    output = self._Run(['update', 'pepper_23'])
    dummy_txt = os.path.join(self.basedir, 'nacl_sdk', 'pepper_23', 'dummy.txt')
    self.assertTrue(os.path.exists(dummy_txt))
    with open(dummy_txt) as f:
      self.assertEqual(f.read(), 'Dummy stuff for pepper_23')

    # Change some files.
    foo_txt = os.path.join(self.basedir, 'nacl_sdk', 'pepper_23', 'foo.txt')
    with open(foo_txt, 'w') as f:
      f.write('Another dummy file. This one is not part of the bundle.')
    with open(dummy_txt, 'w') as f:
      f.write('changed dummy.txt')

    # Reinstall the bundle.
    self._Run(['reinstall', 'pepper_23'])

    self.assertFalse(os.path.exists(foo_txt))
    self.assertTrue(os.path.exists(dummy_txt))
    with open(dummy_txt) as f:
      self.assertEqual(f.read(), 'Dummy stuff for pepper_23')

    cache_manifest = self._ReadCacheManifest()
    num_archives = len(cache_manifest.GetBundle('pepper_23').GetArchives())
    self.assertEqual(num_archives, 1)

  def testReinstallWithDuplicatedArchives(self):
    """The reinstall command should only use the most recent archive if there
    are duplicated archives.

    NOTE: There was a bug where the sdk_cache/naclsdk_manifest2.json file was
    duplicating archives from different revisions. Make sure that reinstall
    ignores old archives in the bundle.
    """
    # First install the bundle.
    self._AddDummyBundle(self.manifest, 'pepper_23')
    self._WriteManifest()
    self._Run(['update', 'pepper_23'])

    manifest = self._ReadCacheManifest()
    bundle = manifest.GetBundle('pepper_23')
    self.assertEqual(len(bundle.GetArchives()), 1)

    # Now add a bogus duplicate archive
    archive2 = self._MakeDummyArchive('pepper_23', tarname='pepper_23',
                                      filename='dummy2.txt')
    bundle.AddArchive(archive2)
    self._WriteCacheManifest(manifest)

    output = self._Run(['reinstall', 'pepper_23'])
    # When updating just one file, there is no (file 1/2 - "...") output.
    self.assertFalse('file 1/' in output)
    # Should be using the last archive.
    self.assertFalse(os.path.exists(
        os.path.join(self.basedir, 'nacl_sdk', 'pepper_23', 'dummy.txt')))
    self.assertTrue(os.path.exists(
        os.path.join(self.basedir, 'nacl_sdk', 'pepper_23', 'dummy2.txt')))

  def testReinstallDoesntUpdate(self):
    """The reinstall command should not update a bundle that has an update."""
    # First install the bundle.
    bundle = self._AddDummyBundle(self.manifest, 'pepper_23')
    self._WriteManifest()
    self._Run(['update', 'pepper_23'])
    dummy_txt = os.path.join(self.basedir, 'nacl_sdk', 'pepper_23', 'dummy.txt')
    self.assertTrue(os.path.exists(dummy_txt))
    with open(dummy_txt) as f:
      self.assertEqual(f.read(), 'Dummy stuff for pepper_23')

    # Update the revision.
    bundle.revision += 1
    self._WriteManifest()

    # Change the file.
    foo_txt = os.path.join(self.basedir, 'nacl_sdk', 'pepper_23', 'foo.txt')
    with open(dummy_txt, 'w') as f:
      f.write('changed dummy.txt')

    # Reinstall.
    self._Run(['reinstall', 'pepper_23'])

    # The data has been reinstalled.
    self.assertTrue(os.path.exists(dummy_txt))
    with open(dummy_txt) as f:
      self.assertEqual(f.read(), 'Dummy stuff for pepper_23')

    # ... but the version hasn't been updated.
    output = self._Run(['list', '-r'])
    self.assertTrue(re.search('I\*\s+pepper_23.*?r1337.*?r1338', output))

  def testArchiveCacheBasic(self):
    """Downloaded archives should be stored in the cache by default."""
    self._AddDummyBundle(self.manifest, 'pepper_23')
    self._WriteManifest()
    self._Run(['update', 'pepper_23'])
    archive_cache = os.path.join(self.cache_dir, 'archives')
    cache_contents = os.listdir(archive_cache)
    self.assertEqual(cache_contents, ['pepper_23'])
    cache_contents = os.listdir(os.path.join(archive_cache, 'pepper_23'))
    self.assertEqual(cache_contents, ['pepper_23.tar.bz2'])

  def testArchiveCacheEviction(self):
    archive_cache = os.path.join(self.cache_dir, 'archives')
    self._AddDummyBundle(self.manifest, 'pepper_23')
    self._AddDummyBundle(self.manifest, 'pepper_22')
    self._WriteManifest()

    # First install pepper_23
    self._Run(['update', 'pepper_23'])
    archive = os.path.join(archive_cache, 'pepper_23', 'pepper_23.tar.bz2')
    archive_size = os.path.getsize(archive)

    # Set the mtime on the pepper_23 bundle to be a few seconds in the past.
    # This is needed so that the two bundles don't end up with the same
    # timestamp which can happen on systems that don't report sub-second
    # timestamps.
    atime = os.path.getatime(archive)
    mtime = os.path.getmtime(archive)
    os.utime(archive, (atime, mtime-10))

    # Set cache limit to size of pepper archive * 1.5
    self._WriteConfig('{ "cache_max": %d }' % int(archive_size * 1.5))

    # Now install pepper_22, which should cause pepper_23 to be evicted
    self._Run(['update', 'pepper_22'])
    cache_contents = os.listdir(archive_cache)
    self.assertEqual(cache_contents, ['pepper_22'])

  def testArchiveCacheZero(self):
    """Archives should not be cached when cache_max is zero."""
    self._AddDummyBundle(self.manifest, 'pepper_23')
    self._WriteConfig('{ "cache_max": 0 }')
    self._AddDummyBundle(self.manifest, 'pepper_23')
    self._WriteManifest()
    self._Run(['update', 'pepper_23'])
    archive_cache = os.path.join(self.cache_dir, 'archives')
    # Archive folder should be completely remove by cache cleanup
    self.assertFalse(os.path.exists(archive_cache))

if __name__ == '__main__':
  unittest.main()
