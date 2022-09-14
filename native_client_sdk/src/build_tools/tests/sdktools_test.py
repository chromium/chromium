#!/usr/bin/env python
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re
import subprocess
import sys
import tarfile
import tempfile
import test_server
import unittest
import zipfile

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
BUILD_TOOLS_DIR = os.path.dirname(SCRIPT_DIR)
TOOLS_DIR = os.path.join(os.path.dirname(BUILD_TOOLS_DIR), 'tools')

sys.path.extend([BUILD_TOOLS_DIR, TOOLS_DIR])
import getos
import manifest_util
import oshelpers


MANIFEST_BASENAME = 'naclsdk_manifest2.json'

# Attribute '' defined outside __init__
# pylint: disable=W0201

class SdkToolsTestCase(unittest.TestCase):
  def tearDown(self):
    if self.server:
      self.server.Shutdown()
    oshelpers.Remove(['-rf', self.basedir])

  def SetupDefault(self):
    self.SetupWithBaseDirPrefix('sdktools')

  def SetupWithBaseDirPrefix(self, basedir_prefix, tmpdir=None):
    self.basedir = tempfile.mkdtemp(prefix=basedir_prefix, dir=tmpdir)
    self.nacl_sdk_base = os.path.join(self.basedir, 'nacl_sdk')
    self.cache_dir = os.path.join(self.nacl_sdk_base, 'sdk_cache')
    # We have to make sure that we build our updaters with a version that is at
    # least as large as the version in the sdk_tools bundle. If not, update
    # tests may fail because the "current" version (according to the sdk_cache)
    # is greater than the version we are attempting to update to.
    self.current_revision = self._GetSdkToolsBundleRevision()
    self._BuildUpdater(self.basedir, self.current_revision)
    self.manifest = self._ReadCacheManifest()
    self.sdk_tools_bundle = self.manifest.GetBundle('sdk_tools')
    self.server = test_server.LocalHTTPServer(self.basedir)

  def _GetSdkToolsBundleRevision(self):
    """Get the sdk_tools bundle revision.
    We get this from the checked-in path; this is the same file that
    build_updater uses to specify the current revision of sdk_tools."""

    manifest_filename = os.path.join(BUILD_TOOLS_DIR, 'json',
                                     'naclsdk_manifest0.json')
    manifest = manifest_util.SDKManifest()
    manifest.LoadDataFromString(open(manifest_filename, 'r').read())
    return manifest.GetBundle('sdk_tools').revision

  def _WriteConfig(self, config_data):
    config_filename = os.path.join(self.cache_dir, 'naclsdk_config.json')
    with open(config_filename, 'w') as stream:
      stream.write(config_data)

  def _WriteCacheManifest(self, manifest):
    """Write the manifest at nacl_sdk/sdk_cache.

    This is useful for faking having installed a bundle.
    """
    manifest_filename = os.path.join(self.cache_dir, MANIFEST_BASENAME)
    with open(manifest_filename, 'w') as stream:
      stream.write(manifest.GetDataAsString())

  def _ReadCacheManifest(self):
    """Read the manifest at nacl_sdk/sdk_cache."""
    manifest_filename = os.path.join(self.cache_dir, MANIFEST_BASENAME)
    manifest = manifest_util.SDKManifest()
    with open(manifest_filename) as stream:
      manifest.LoadDataFromString(stream.read())
    return manifest

  def _WriteManifest(self):
    with open(os.path.join(self.basedir, MANIFEST_BASENAME), 'w') as stream:
      stream.write(self.manifest.GetDataAsString())

  def _BuildUpdater(self, out_dir, revision=None):
    build_updater_py = os.path.join(BUILD_TOOLS_DIR, 'build_updater.py')
    cmd = [sys.executable, build_updater_py, '-o', out_dir]
    if revision:
      cmd.extend(['-r', str(revision)])

    process = subprocess.Popen(cmd, stdout=subprocess.PIPE)
    _, _ = process.communicate()
    self.assertEqual(process.returncode, 0)

  def _BuildUpdaterArchive(self, rel_path, revision):
    """Build a new sdk_tools bundle.

    Args:
      rel_path: The relative path to build the updater.
      revision: The revision number to give to this bundle.
    Returns:
      A manifest_util.Archive() that points to this new bundle on the local
      server.
    """
    self._BuildUpdater(os.path.join(self.basedir, rel_path), revision)

    new_sdk_tools_tgz = os.path.join(self.basedir, rel_path, 'sdk_tools.tgz')
    with open(new_sdk_tools_tgz, 'rb') as sdk_tools_stream:
      archive_sha1, archive_size = manifest_util.DownloadAndComputeHash(
          sdk_tools_stream)

    archive = manifest_util.Archive('all')
    archive.url = self.server.GetURL('%s/sdk_tools.tgz' % (rel_path,))
    archive.checksum = archive_sha1
    archive.size = archive_size
    return archive

  def _Run(self, args, expect_error=False):
    naclsdk_shell_script = os.path.join(self.nacl_sdk_base, 'naclsdk')
    if getos.GetPlatform() == 'win':
      naclsdk_shell_script += '.bat'
    cmd = [naclsdk_shell_script]
    cmd.extend(args)
    cmd.extend(['-U', self.server.GetURL(MANIFEST_BASENAME)])
    process = subprocess.Popen(cmd, stdout=subprocess.PIPE)
    stdout, _ = process.communicate()

    if ((expect_error and process.returncode == 0) or
        (not expect_error and process.returncode != 0)):
      self.fail('Error running nacl_sdk:\n"""\n%s\n"""' % stdout)

    return stdout

  def _RunAndExtractRevision(self):
    stdout = self._Run(['version'])
    match = re.search('version r(\d+)', stdout)
    self.assertTrue(match is not None)
    return int(match.group(1))


class TestSdkTools(SdkToolsTestCase):
  def testPathHasSpaces(self):
    """Test that running naclsdk from a path with spaces works."""
    self.SetupWithBaseDirPrefix('sdk tools')
    self._WriteManifest()
    self._RunAndExtractRevision()


class TestBuildUpdater(SdkToolsTestCase):
  def setUp(self):
    self.SetupDefault()

  def testUpdaterPathsAreSane(self):
    """Test that the paths to files in nacl_sdk.zip and sdktools.tgz are
    relative to the output directory."""
    nacl_sdk_zip_path = os.path.join(self.basedir, 'nacl_sdk.zip')
    zip_stream = zipfile.ZipFile(nacl_sdk_zip_path, 'r')
    try:
      self.assertTrue(all(name.startswith('nacl_sdk')
                          for name in zip_stream.namelist()))
    finally:
      zip_stream.close()

    # sdktools.tgz has no built-in directories to look for. Instead, just look
    # for some files that must be there.
    sdktools_tgz_path = os.path.join(self.basedir, 'sdk_tools.tgz')
    tar_stream = tarfile.open(sdktools_tgz_path, 'r:gz')
    try:
      names = [m.name for m in tar_stream.getmembers()]
      self.assertTrue('LICENSE' in names)
      self.assertTrue('sdk_update.py' in names)
    finally:
      tar_stream.close()


class TestAutoUpdateSdkTools(SdkToolsTestCase):
  def setUp(self):
    self.SetupDefault()

  def testNoUpdate(self):
    """Test that running naclsdk with current revision does nothing."""
    self._WriteManifest()
    revision = self._RunAndExtractRevision()
    self.assertEqual(revision, self.current_revision)

  def testUpdate(self):
    """Test that running naclsdk with a new revision will auto-update."""
    new_revision = self.current_revision + 1
    archive = self._BuildUpdaterArchive('new', new_revision)
    self.sdk_tools_bundle.RemoveAllArchivesForHostOS(archive.host_os)
    self.sdk_tools_bundle.AddArchive(archive)
    self.sdk_tools_bundle.revision = new_revision
    self._WriteManifest()

    revision = self._RunAndExtractRevision()
    self.assertEqual(revision, new_revision)

  def testManualUpdateIsIgnored(self):
    """Test that attempting to manually update sdk_tools is ignored.

    If the sdk_tools bundle was updated normally (i.e. the old way), it would
    leave a sdk_tools_update folder that would then be copied over on a
    subsequent run. This test ensures that there is no folder made.
    """
    new_revision = self.current_revision + 1
    archive = self._BuildUpdaterArchive('new', new_revision)
    self.sdk_tools_bundle.RemoveAllArchivesForHostOS(archive.host_os)
    self.sdk_tools_bundle.AddArchive(archive)
    self.sdk_tools_bundle.revision = new_revision
    self._WriteManifest()

    sdk_tools_update_dir = os.path.join(self.nacl_sdk_base, 'sdk_tools_update')
    self.assertFalse(os.path.exists(sdk_tools_update_dir))
    stdout = self._Run(['update', 'sdk_tools'])
    self.assertTrue(stdout.find('Ignoring manual update request.') != -1)
    self.assertFalse(os.path.exists(sdk_tools_update_dir))

  def testHelpCommand(self):
    """Running naclsdk with -h should work.

    This is a regression test for a bug where the auto-updater would remove the
    sdk_tools directory when running "naclsdk -h".
    """
    self._WriteManifest()
    self._Run(['-h'])


class TestAutoUpdateSdkToolsDifferentFilesystem(TestAutoUpdateSdkTools):
  def setUp(self):
    # On Linux (on my machine at least), /tmp is a different filesystem than
    # the current directory. os.rename fails when the source and destination
    # are on different filesystems. Test that case here.
    self.SetupWithBaseDirPrefix('sdktools', tmpdir='.')


if __name__ == '__main__':
  sys.exit(unittest.main())
