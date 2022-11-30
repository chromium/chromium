#!/usr/bin/env python
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script that reads omahaproxy and gsutil to determine version of SDK to put
in manifest.
"""

# pylint is convinced the email module is missing attributes
# pylint: disable=E1101

import argparse
import buildbot_common
import csv
import cStringIO
import difflib
import email
import logging
import logging.handlers
import manifest_util
import os
import posixpath
import re
import smtplib
import subprocess
import sys
import time
import traceback
import urllib2

MANIFEST_BASENAME = 'naclsdk_manifest2.json'
SCRIPT_DIR = os.path.dirname(__file__)
REPO_MANIFEST = os.path.join(SCRIPT_DIR, 'json', MANIFEST_BASENAME)
GS_BUCKET_PATH = 'gs://nativeclient-mirror/nacl/nacl_sdk/'
GS_SDK_MANIFEST = GS_BUCKET_PATH + MANIFEST_BASENAME
GS_SDK_MANIFEST_LOG = GS_BUCKET_PATH + MANIFEST_BASENAME + '.log'
GS_MANIFEST_BACKUP_DIR = GS_BUCKET_PATH + 'manifest_backups/'

CANARY_BUNDLE_NAME = 'pepper_canary'
CANARY = 'canary'
NACLPORTS_ARCHIVE_NAME = 'naclports.tar.bz2'


logger = logging.getLogger(__name__)


def SplitVersion(version_string):
  """Split a version string (e.g. "18.0.1025.163") into its components.

  e.g.
  SplitVersion("trunk.123456") => ("trunk", "123456")
  SplitVersion("18.0.1025.163") => (18, 0, 1025, 163)
  """
  parts = version_string.split('.')
  if parts[0] == 'trunk':
    return (parts[0], int(parts[1]))
  return tuple([int(p) for p in parts])


def GetMajorVersion(version_string):
  """Get the major version number from a version string (e.g. "18.0.1025.163").

  e.g.
  GetMajorVersion("trunk.123456") => "trunk"
  GetMajorVersion("18.0.1025.163") => 18
  """
  return SplitVersion(version_string)[0]


def CompareVersions(version1, version2):
  """Compare two version strings and return -1, 0, 1 (similar to cmp).

  Versions can only be compared if they are both trunk versions, or neither is.

  e.g.
  CompareVersions("trunk.123", "trunk.456") => -1
  CompareVersions("18.0.1025.163", "37.0.2054.3") => -1
  CompareVersions("trunk.123", "18.0.1025.163") => Error

  """
  split1 = SplitVersion(version1)
  split2 = SplitVersion(version2)
  if split1[0] == split2[0]:
    return cmp(split1[1:], split2[1:])

  if split1 == 'trunk' or split2 == 'trunk':
    raise Exception("Unable to compare versions %s and %s" % (
      version1, version2))

  return cmp(split1, split2)


def JoinVersion(version_tuple):
  """Create a string from a version tuple.

  The tuple should be of the form (18, 0, 1025, 163).
  """
  assert len(version_tuple) == 4
  assert version_tuple[0] != 'trunk'
  return '.'.join(map(str, version_tuple))


def GetTimestampManifestName():
  """Create a manifest name with a timestamp.

  Returns:
    A manifest name with an embedded date. This should make it easier to roll
    back if necessary.
  """
  return time.strftime('naclsdk_manifest2.%Y_%m_%d_%H_%M_%S.json',
      time.gmtime())


def GetPlatformArchiveName(platform):
  """Get the basename of an archive given a platform string.

  Args:
    platform: One of ('win', 'mac', 'linux').

  Returns:
    The basename of the sdk archive for that platform.
  """
  return 'naclsdk_%s.tar.bz2' % platform


def GetCanonicalArchiveName(url):
  """Get the canonical name of an archive given its URL.

  This will convert "naclsdk_linux.bz2" -> "naclsdk_linux.tar.bz2", and also
  remove everything but the filename of the URL.

  This is used below to determine if an expected bundle is found in an version
  directory; the archives all have the same name, but may not exist for a given
  version.

  Args:
    url: The url to parse.

  Returns:
    The canonical name as described above.
  """
  name = posixpath.basename(url)
  match = re.match(r'naclsdk_(.*?)(?:\.tar)?\.bz2', name)
  if match:
    return 'naclsdk_%s.tar.bz2' % match.group(1)

  return name


class Delegate(object):
  """Delegate all external access; reading/writing to filesystem, gsutil etc."""

  def GetRepoManifest(self):
    """Read the manifest file from the NaCl SDK repository.

    This manifest is used as a template for the auto updater; only pepper
    bundles with no archives are considered for auto updating.

    Returns:
      A manifest_util.SDKManifest object read from the NaCl SDK repo."""
    raise NotImplementedError()

  def GetHistory(self):
    """Read Chrome release history from omahaproxy.appspot.com

    Here is an example of data from this URL:
      cros,stable,18.0.1025.168,2012-05-01 17:04:05.962578\n
      win,canary,20.0.1123.0,2012-05-01 13:59:31.703020\n
      mac,canary,20.0.1123.0,2012-05-01 11:54:13.041875\n
      win,stable,18.0.1025.168,2012-04-30 20:34:56.078490\n
      mac,stable,18.0.1025.168,2012-04-30 20:34:55.231141\n
      ...
    Where each line has comma separated values in the following format:
    platform, channel, version, date/time\n

    Returns:
      A list where each element is a line from the document, represented as a
      tuple."""
    raise NotImplementedError()

  def GsUtil_ls(self, url):
    """Runs gsutil ls |url|

    Args:
      url: The cloud storage url to list.
    Returns:
      A list of URLs, all with the gs:// schema."""
    raise NotImplementedError()

  def GsUtil_cat(self, url):
    """Runs gsutil cat |url|

    Args:
      url: The cloud storage url to read from.
    Returns:
      A string with the contents of the file at |url|."""
    raise NotImplementedError()

  def GsUtil_cp(self, src, dest, stdin=None):
    """Runs gsutil cp |src| |dest|

    Args:
      src: The file path or url to copy from.
      dest: The file path or url to copy to.
      stdin: If src is '-', this is used as the stdin to give to gsutil. The
          effect is that text in stdin is copied to |dest|."""
    raise NotImplementedError()

  def SendMail(self, subject, text):
    """Send an email.

    Args:
      subject: The subject of the email.
      text: The text of the email.
    """
    raise NotImplementedError()


class RealDelegate(Delegate):
  def __init__(self, dryrun=False, gsutil=None, mailfrom=None, mailto=None):
    super(RealDelegate, self).__init__()
    self.dryrun = dryrun
    self.mailfrom = mailfrom
    self.mailto = mailto
    if gsutil:
      self.gsutil = gsutil
    else:
      self.gsutil = buildbot_common.GetGsutil()

  def GetRepoManifest(self):
    """See Delegate.GetRepoManifest"""
    with open(REPO_MANIFEST, 'r') as sdk_stream:
      sdk_json_string = sdk_stream.read()

    manifest = manifest_util.SDKManifest()
    manifest.LoadDataFromString(sdk_json_string, add_missing_info=True)
    return manifest

  def GetHistory(self):
    """See Delegate.GetHistory"""
    url_stream = urllib2.urlopen('https://omahaproxy.appspot.com/history')
    history = [(platform, channel, version, date)
        for platform, channel, version, date in csv.reader(url_stream)]

    # The first line of this URL is the header:
    #   os,channel,version,timestamp
    return history[1:]

  def GsUtil_ls(self, url):
    """See Delegate.GsUtil_ls"""
    try:
      stdout = self._RunGsUtil(None, False, 'ls', url)
    except subprocess.CalledProcessError:
      return []

    # filter out empty lines
    return filter(None, stdout.split('\n'))

  def GsUtil_cat(self, url):
    """See Delegate.GsUtil_cat"""
    return self._RunGsUtil(None, True, 'cat', url)

  def GsUtil_cp(self, src, dest, stdin=None):
    """See Delegate.GsUtil_cp"""
    if self.dryrun:
      logger.info("Skipping upload: %s -> %s" % (src, dest))
      if src == '-':
        logger.info('  contents = """%s"""' % stdin)
      return

    return self._RunGsUtil(stdin, True, 'cp', '-a', 'public-read', src, dest)

  def SendMail(self, subject, text):
    """See Delegate.SendMail"""
    if self.mailfrom and self.mailto:
      msg = email.MIMEMultipart.MIMEMultipart()
      msg['From'] = self.mailfrom
      msg['To'] = ', '.join(self.mailto)
      msg['Date'] = email.Utils.formatdate(localtime=True)
      msg['Subject'] = subject
      msg.attach(email.MIMEText.MIMEText(text))
      smtp_obj = smtplib.SMTP('localhost')
      smtp_obj.sendmail(self.mailfrom, self.mailto, msg.as_string())
      smtp_obj.close()

  def _RunGsUtil(self, stdin, log_errors, *args):
    """Run gsutil as a subprocess.

    Args:
      stdin: If non-None, used as input to the process.
      log_errors: If True, write errors to stderr.
      *args: Arguments to pass to gsutil. The first argument should be an
          operation such as ls, cp or cat.
    Returns:
      The stdout from the process."""
    cmd = [self.gsutil] + list(args)
    logger.debug("Running: %s" % str(cmd))
    if stdin:
      stdin_pipe = subprocess.PIPE
    else:
      stdin_pipe = None

    try:
      process = subprocess.Popen(cmd, stdin=stdin_pipe, stdout=subprocess.PIPE,
          stderr=subprocess.PIPE)
      stdout, stderr = process.communicate(stdin)
    except OSError as e:
      raise manifest_util.Error("Unable to run '%s': %s" % (cmd[0], str(e)))

    if process.returncode:
      if log_errors:
        logger.error(stderr)
      raise subprocess.CalledProcessError(process.returncode, ' '.join(cmd))
    return stdout


class GsutilLoggingHandler(logging.handlers.BufferingHandler):
  def __init__(self, delegate):
    logging.handlers.BufferingHandler.__init__(self, capacity=0)
    self.delegate = delegate

  def shouldFlush(self, record):
    # BufferingHandler.shouldFlush automatically flushes if the length of the
    # buffer is greater than self.capacity. We don't want that behavior, so
    # return False here.
    return False

  def flush(self):
    # Do nothing here. We want to be explicit about uploading the log.
    pass

  def upload(self):
    output_list = []
    for record in self.buffer:
      output_list.append(self.format(record))
    output = '\n'.join(output_list)
    self.delegate.GsUtil_cp('-', GS_SDK_MANIFEST_LOG, stdin=output)

    logging.handlers.BufferingHandler.flush(self)


class NoSharedVersionException(Exception):
  pass


class VersionFinder(object):
  """Finds a version of a pepper bundle that all desired platforms share.

  Args:
    delegate: See Delegate class above.
    platforms: A sequence of platforms to consider, e.g.
        ('mac', 'linux', 'win')
    extra_archives: A sequence of tuples: (archive_basename, minimum_version),
        e.g. [('foo.tar.bz2', '18.0.1000.0'), ('bar.tar.bz2', '19.0.1100.20')]
        These archives must exist to consider a version for inclusion, as
        long as that version is greater than the archive's minimum version.
  """
  def __init__(self, delegate, platforms, extra_archives=None):
    self.delegate = delegate
    self.history = delegate.GetHistory()
    self.platforms = platforms
    self.extra_archives = extra_archives

  def GetMostRecentSharedVersion(self, major_version):
    """Returns the most recent version of a pepper bundle that exists on all
    given platforms.

    Specifically, the resulting version should be the most recently released
    (meaning closest to the top of the listing on
    omahaproxy.appspot.com/history) version that has a Chrome release on all
    given platforms, and has a pepper bundle archive for each platform as well.

    Args:
      major_version: The major version of the pepper bundle, e.g. 19.
    Returns:
      A tuple (version, channel, archives). The version is a string such as
      "19.0.1084.41". The channel is one of ('stable', 'beta', or 'dev').
      |archives| is a list of archive URLs."""
    def GetPlatformHistory(platform):
      return self._GetPlatformMajorVersionHistory(major_version, platform)

    shared_version_generator = self._FindNextSharedVersion(self.platforms,
                                                           GetPlatformHistory)
    return self._DoGetMostRecentSharedVersion(shared_version_generator)

  def GetMostRecentSharedCanary(self):
    """Returns the most recent version of a canary pepper bundle that exists on
    all given platforms.

    Canary is special-cased because we don't care about its major version; we
    always use the most recent canary, regardless of major version.

    Returns:
      A tuple (version, channel, archives). The version is a string such as
      "trunk.123456". The channel is always 'canary'. |archives| is a list of
      archive URLs."""
    version_generator = self._FindNextTrunkVersion()
    return self._DoGetMostRecentSharedVersion(version_generator)

  def GetAvailablePlatformArchivesFor(self, version):
    """Returns a sequence of archives that exist for a given version, on the
    given platforms.

    The second element of the returned tuple is a list of all platforms that do
    not have an archive for the given version.

    Args:
      version: The version to find archives for. (e.g. "18.0.1025.164")
    Returns:
      A tuple (archives, missing_archives). |archives| is a list of archive
      URLs, |missing_archives| is a list of archive names.
    """
    archive_urls = self._GetAvailableArchivesFor(version)

    expected_archives = set(GetPlatformArchiveName(p) for p in self.platforms)

    if self.extra_archives:
      for extra_archive, min_version, max_version in self.extra_archives:
        if (CompareVersions(version, min_version) >= 0 and
            CompareVersions(version, max_version) < 0):
          expected_archives.add(extra_archive)
    found_archives = set(GetCanonicalArchiveName(a) for a in archive_urls)
    missing_archives = expected_archives - found_archives

    # Only return archives that are "expected".
    def IsExpected(url):
      return GetCanonicalArchiveName(url) in expected_archives

    expected_archive_urls = [u for u in archive_urls if IsExpected(u)]
    return expected_archive_urls, missing_archives

  def _DoGetMostRecentSharedVersion(self, shared_version_generator):
    """Returns the most recent version of a pepper bundle that exists on all
    given platforms.

    This function does the real work for the public GetMostRecentShared* above.

    Args:
      shared_version_generator: A generator that will yield (version, channel)
          tuples in order of most recent to least recent.
    Returns:
      A tuple (version, channel, archives). The version is a string such as
      "19.0.1084.41". The channel is one of ('stable', 'beta', 'dev',
      'canary'). |archives| is a list of archive URLs."""
    version = None
    skipped_versions = []
    channel = ''
    while True:
      try:
        version, channel = shared_version_generator.next()
      except StopIteration:
        msg = 'No shared version for platforms: %s\n' % (
            ', '.join(self.platforms))
        msg += 'Last version checked = %s.\n' % (version,)
        if skipped_versions:
          msg += 'Versions skipped due to missing archives:\n'
          for version, channel, missing_archives in skipped_versions:
            archive_msg = '(missing %s)' % (', '.join(missing_archives))
          msg += '  %s (%s) %s\n' % (version, channel, archive_msg)
        raise NoSharedVersionException(msg)

      logger.info('Found shared version: %s, channel: %s' % (
          version, channel))

      archives, missing_archives = self.GetAvailablePlatformArchivesFor(version)

      if not missing_archives:
        return version, channel, archives

      logger.info('  skipping. Missing archives: %s' % (
          ', '.join(missing_archives)))

      skipped_versions.append((version, channel, missing_archives))

  def _GetPlatformMajorVersionHistory(self, with_major_version, with_platform):
    """Yields Chrome history for a given platform and major version.

    Args:
      with_major_version: The major version to filter for. If 0, match all
          versions.
      with_platform: The name of the platform to filter for.
    Returns:
      A generator that yields a tuple (channel, version) for each version that
      matches the platform and major version. The version returned is a tuple as
      returned from SplitVersion.
    """
    for platform, channel, version, _ in self.history:
      version = SplitVersion(version)
      if (with_platform == platform and
          (with_major_version == 0 or with_major_version == version[0])):
        yield channel, version

  def _FindNextSharedVersion(self, platforms, generator_func):
    """Yields versions of Chrome that exist on all given platforms, in order of
       newest to oldest.

    Versions are compared in reverse order of release. That is, the most
    recently updated version will be tested first.

    Args:
      platforms: A sequence of platforms to consider, e.g.
          ('mac', 'linux', 'win')
      generator_func: A function which takes a platform and returns a
          generator that yields (channel, version) tuples.
    Returns:
      A generator that yields a tuple (version, channel) for each version that
      matches all platforms and the major version. The version returned is a
      string (e.g. "18.0.1025.164").
    """
    platform_generators = []
    for platform in platforms:
      platform_generators.append(generator_func(platform))

    shared_version = None
    platform_versions = []
    for platform_gen in platform_generators:
      platform_versions.append(platform_gen.next())

    while True:
      if logger.isEnabledFor(logging.INFO):
        msg_info = []
        for i, platform in enumerate(platforms):
          msg_info.append('%s: %s' % (
              platform, JoinVersion(platform_versions[i][1])))
        logger.info('Checking versions: %s' % ', '.join(msg_info))

      shared_version = min((v for c, v in platform_versions))

      if all(v == shared_version for c, v in platform_versions):
        # grab the channel from an arbitrary platform
        first_platform = platform_versions[0]
        channel = first_platform[0]
        yield JoinVersion(shared_version), channel

        # force increment to next version for all platforms
        shared_version = None

      # Find the next version for any platform that isn't at the shared version.
      try:
        for i, platform_gen in enumerate(platform_generators):
          if platform_versions[i][1] != shared_version:
            platform_versions[i] = platform_gen.next()
      except StopIteration:
        return


  def _FindNextTrunkVersion(self):
    """Yields all trunk versions that exist in the cloud storage bucket, newest
    to oldest.

    Returns:
      A generator that yields a tuple (version, channel) for each version that
      matches all platforms and the major version. The version returned is a
      string (e.g. "trunk.123456").
    """
    files = self.delegate.GsUtil_ls(GS_BUCKET_PATH)
    assert all(f.startswith('gs://') for f in files)

    trunk_versions = []
    for f in files:
      match = re.search(r'(trunk\.\d+)', f)
      if match:
        trunk_versions.append(match.group(1))

    trunk_versions.sort(reverse=True)

    for version in trunk_versions:
      yield version, 'canary'


  def _GetAvailableArchivesFor(self, version_string):
    """Downloads a list of all available archives for a given version.

    Args:
      version_string: The version to find archives for. (e.g. "18.0.1025.164")
    Returns:
      A list of strings, each of which is a platform-specific archive URL. (e.g.
      "gs://nativeclient_mirror/nacl/nacl_sdk/18.0.1025.164/"
      "naclsdk_linux.tar.bz2").

      All returned URLs will use the gs:// schema."""
    files = self.delegate.GsUtil_ls(GS_BUCKET_PATH + version_string)

    assert all(f.startswith('gs://') for f in files)

    archives = [f for f in files if not f.endswith('.json')]
    manifests = [f for f in files if f.endswith('.json')]

    # don't include any archives that don't have an associated manifest.
    return filter(lambda a: a + '.json' in manifests, archives)


class UnknownLockedBundleException(Exception):
  pass


class Updater(object):
  def __init__(self, delegate):
    self.delegate = delegate
    self.versions_to_update = []
    self.locked_bundles = []
    self.online_manifest = manifest_util.SDKManifest()
    self._FetchOnlineManifest()

  def AddVersionToUpdate(self, bundle_name, version, channel, archives):
    """Add a pepper version to update in the uploaded manifest.

    Args:
      bundle_name: The name of the pepper bundle, e.g. 'pepper_18'
      version: The version of the pepper bundle, e.g. '18.0.1025.64'
      channel: The stability of the pepper bundle, e.g. 'beta'
      archives: A sequence of archive URLs for this bundle."""
    self.versions_to_update.append((bundle_name, version, channel, archives))

  def AddLockedBundle(self, bundle_name):
    """Add a "locked" bundle to the updater.

    A locked bundle is a bundle that wasn't found in the history. When this
    happens, the bundle is now "locked" to whatever was last found. We want to
    ensure that the online manifest has this bundle.

    Args:
      bundle_name: The name of the locked bundle.
    """
    self.locked_bundles.append(bundle_name)

  def Update(self, manifest):
    """Update a manifest and upload it.

    Note that bundles will not be updated if the current version is newer.
    That is, the updater will never automatically update to an older version of
    a bundle.

    Args:
      manifest: The manifest used as a template for updating. Only pepper
      bundles that contain no archives will be considered for auto-updating."""
    # Make sure there is only one stable branch: the one with the max version.
    # All others are post-stable.
    stable_major_versions = [GetMajorVersion(version) for _, version, channel, _
                             in self.versions_to_update if channel == 'stable']
    # Add 0 in case there are no stable versions.
    max_stable_version = max([0] + stable_major_versions)

    # Ensure that all locked bundles exist in the online manifest.
    for bundle_name in self.locked_bundles:
      online_bundle = self.online_manifest.GetBundle(bundle_name)
      if online_bundle:
        manifest.SetBundle(online_bundle)
      else:
        msg = ('Attempted to update bundle "%s", but no shared versions were '
            'found, and there is no online bundle with that name.')
        raise UnknownLockedBundleException(msg % bundle_name)

    if self.locked_bundles:
      # Send a nagging email that we shouldn't be wasting time looking for
      # bundles that are no longer in the history.
      scriptname = os.path.basename(sys.argv[0])
      subject = '[%s] Reminder: remove bundles from %s' % (scriptname,
                                                           MANIFEST_BASENAME)
      text = 'These bundles are not in the omahaproxy history anymore: ' + \
              ', '.join(self.locked_bundles)
      self.delegate.SendMail(subject, text)


    # Update all versions.
    logger.info('>>> Updating bundles...')
    for bundle_name, version, channel, archives in self.versions_to_update:
      logger.info('Updating %s to %s...' % (bundle_name, version))
      bundle = manifest.GetBundle(bundle_name)
      for archive in archives:
        platform_bundle = self._GetPlatformArchiveBundle(archive)
        # Normally the manifest snippet's bundle name matches our bundle name.
        # pepper_canary, however is called "pepper_###" in the manifest
        # snippet.
        platform_bundle.name = bundle_name
        bundle.MergeWithBundle(platform_bundle)

      # Fix the stability and recommended values
      major_version = GetMajorVersion(version)
      if major_version < max_stable_version:
        bundle.stability = 'post_stable'
      else:
        bundle.stability = channel
      # We always recommend the stable version.
      if bundle.stability == 'stable':
        bundle.recommended = 'yes'
      else:
        bundle.recommended = 'no'

      # Check to ensure this bundle is newer than the online bundle.
      online_bundle = self.online_manifest.GetBundle(bundle_name)
      if online_bundle:
        # This test used to be online_bundle.revision >= bundle.revision.
        # That doesn't do quite what we want: sometimes the metadata changes
        # but the revision stays the same -- we still want to push those
        # changes.
        if online_bundle.revision > bundle.revision or online_bundle == bundle:
          logger.info(
              '  Revision %s is not newer than than online revision %s. '
              'Skipping.' % (bundle.revision, online_bundle.revision))

          manifest.SetBundle(online_bundle)
          continue
    self._UploadManifest(manifest)
    logger.info('Done.')

  def _GetPlatformArchiveBundle(self, archive):
    """Downloads the manifest "snippet" for an archive, and reads it as a
       Bundle.

    Args:
      archive: A full URL of a platform-specific archive, using the gs schema.
    Returns:
      An object of type manifest_util.Bundle, read from a JSON file storing
      metadata for this archive.
    """
    stdout = self.delegate.GsUtil_cat(archive + '.json')
    bundle = manifest_util.Bundle('')
    bundle.LoadDataFromString(stdout)
    # Some snippets were uploaded with revisions and versions as strings. Fix
    # those here.
    bundle.revision = int(bundle.revision)
    bundle.version = int(bundle.version)

    # HACK. The naclports archive specifies host_os as linux. Change it to all.
    for archive in bundle.GetArchives():
      if NACLPORTS_ARCHIVE_NAME in archive.url:
        archive.host_os = 'all'
    return bundle

  def _UploadManifest(self, manifest):
    """Upload a serialized manifest_util.SDKManifest object.

    Upload one copy to gs://<BUCKET_PATH>/naclsdk_manifest2.json, and a copy to
    gs://<BUCKET_PATH>/manifest_backups/naclsdk_manifest2.<TIMESTAMP>.json.

    Args:
      manifest: The new manifest to upload.
    """
    new_manifest_string = manifest.GetDataAsString()
    online_manifest_string = self.online_manifest.GetDataAsString()

    if self.delegate.dryrun:
      logger.info(''.join(list(difflib.unified_diff(
          online_manifest_string.splitlines(1),
          new_manifest_string.splitlines(1)))))
      return
    else:
      online_manifest = manifest_util.SDKManifest()
      online_manifest.LoadDataFromString(online_manifest_string)

      if online_manifest == manifest:
        logger.info('New manifest doesn\'t differ from online manifest.'
            'Skipping upload.')
        return

    timestamp_manifest_path = GS_MANIFEST_BACKUP_DIR + \
        GetTimestampManifestName()
    self.delegate.GsUtil_cp('-', timestamp_manifest_path,
        stdin=manifest.GetDataAsString())

    # copy from timestampped copy over the official manifest.
    self.delegate.GsUtil_cp(timestamp_manifest_path, GS_SDK_MANIFEST)

  def _FetchOnlineManifest(self):
    try:
      online_manifest_string = self.delegate.GsUtil_cat(GS_SDK_MANIFEST)
    except subprocess.CalledProcessError:
      # It is not a failure if the online manifest doesn't exist.
      online_manifest_string = ''

    if online_manifest_string:
      self.online_manifest.LoadDataFromString(online_manifest_string)


def Run(delegate, platforms, extra_archives, fixed_bundle_versions=None):
  """Entry point for the auto-updater.

  Args:
    delegate: The Delegate object to use for reading Urls, files, etc.
    platforms: A sequence of platforms to consider, e.g.
        ('mac', 'linux', 'win')
      extra_archives: A sequence of tuples: (archive_basename, minimum_version,
          max_version), e.g. [('foo.tar.bz2', '18.0.1000.0', '19.0.0.0)]
          These archives must exist to consider a version for inclusion, as
          long as that version is greater than the archive's minimum version.
    fixed_bundle_versions: A sequence of tuples (bundle_name, version_string).
        e.g. ('pepper_21', '21.0.1145.0')
  """
  if fixed_bundle_versions:
    fixed_bundle_versions = dict(fixed_bundle_versions)
  else:
    fixed_bundle_versions = {}

  manifest = delegate.GetRepoManifest()
  auto_update_bundles = []
  for bundle in manifest.GetBundles():
    if not bundle.name.startswith('pepper_'):
      continue
    archives = bundle.GetArchives()
    if not archives:
      auto_update_bundles.append(bundle)

  if not auto_update_bundles:
    logger.info('No versions need auto-updating.')
    return

  updater = Updater(delegate)

  for bundle in auto_update_bundles:
    try:
      if bundle.name == CANARY_BUNDLE_NAME:
        logger.info('>>> Looking for most recent pepper_canary...')
        version_finder = VersionFinder(delegate, platforms, extra_archives)
        version, channel, archives = version_finder.GetMostRecentSharedCanary()
      else:
        logger.info('>>> Looking for most recent pepper_%s...' %
            bundle.version)
        version_finder = VersionFinder(delegate, platforms, extra_archives)
        version, channel, archives = version_finder.GetMostRecentSharedVersion(
            bundle.version)
    except NoSharedVersionException:
      # If we can't find a shared version, make sure that there is an uploaded
      # bundle with that name already.
      updater.AddLockedBundle(bundle.name)
      continue

    if bundle.name in fixed_bundle_versions:
      # Ensure this version is valid for all platforms.
      # If it is, use the channel found above (because the channel for this
      # version may not be in the history.)
      version = fixed_bundle_versions[bundle.name]
      logger.info('Fixed bundle version: %s, %s' % (bundle.name, version))
      archives, missing = \
          version_finder.GetAvailablePlatformArchivesFor(version)
      if missing:
        logger.warn(
            'Some archives for version %s of bundle %s don\'t exist: '
            'Missing %s' % (version, bundle.name, ', '.join(missing)))
        return

    updater.AddVersionToUpdate(bundle.name, version, channel, archives)

  updater.Update(manifest)


class CapturedFile(object):
  """A file-like object that captures text written to it, but also passes it
  through to an underlying file-like object."""
  def __init__(self, passthrough):
    self.passthrough = passthrough
    self.written = cStringIO.StringIO()

  def write(self, s):
    self.written.write(s)
    if self.passthrough:
      self.passthrough.write(s)

  def getvalue(self):
    return self.written.getvalue()


def main(args):
  parser = argparse.ArgumentParser()
  parser.add_argument('--gsutil', help='path to gsutil.')
  parser.add_argument('-d', '--debug', help='run in debug mode.',
      action='store_true')
  parser.add_argument('--mailfrom', help='email address of sender.')
  parser.add_argument('--mailto', help='send error mails to...',
      action='append')
  parser.add_argument('-n', '--dryrun', help="don't upload the manifest.",
      action='store_true')
  parser.add_argument('-v', '--verbose', help='print more diagnotic messages. '
      'Use more than once for more info.',
      action='count')
  parser.add_argument('--log-file', metavar='FILE', help='log to FILE')
  parser.add_argument('--upload-log', help='Upload log alongside the manifest.',
      action='store_true')
  parser.add_argument('--bundle-version',
      help='Manually set a bundle version. This can be passed more than once. '
      'format: --bundle-version pepper_24=24.0.1312.25', action='append')
  options = parser.parse_args(args)

  if (options.mailfrom is None) != (not options.mailto):
    options.mailfrom = None
    options.mailto = None
    logger.warning('Disabling email, one of --mailto or --mailfrom '
        'was missing.\n')

  if options.verbose >= 2:
    logging.basicConfig(level=logging.DEBUG, filename=options.log_file)
  elif options.verbose:
    logging.basicConfig(level=logging.INFO, filename=options.log_file)
  else:
    logging.basicConfig(level=logging.WARNING, filename=options.log_file)

  # Parse bundle versions.
  fixed_bundle_versions = {}
  if options.bundle_version:
    for bundle_version_string in options.bundle_version:
      bundle_name, version = bundle_version_string.split('=')
      fixed_bundle_versions[bundle_name] = version

  if options.mailfrom and options.mailto:
    # Capture stderr so it can be emailed, if necessary.
    sys.stderr = CapturedFile(sys.stderr)

  try:
    try:
      delegate = RealDelegate(options.dryrun, options.gsutil,
                              options.mailfrom, options.mailto)

      if options.upload_log:
        gsutil_logging_handler = GsutilLoggingHandler(delegate)
        logger.addHandler(gsutil_logging_handler)

      # Only look for naclports archives >= 27. The old ports bundles don't
      # include license information.  The naclports bundle was removed in
      # pepper_47.
      extra_archives = [(NACLPORTS_ARCHIVE_NAME, '27.0.0.0', '47.0.0.0')]
      Run(delegate, ('mac', 'win', 'linux'), extra_archives,
          fixed_bundle_versions)
    except Exception:
      if options.mailfrom and options.mailto:
        traceback.print_exc()
        scriptname = os.path.basename(sys.argv[0])
        subject = '[%s] Failed to update manifest' % (scriptname,)
        text = '%s failed.\n\nSTDERR:\n%s\n' % (scriptname,
                                                sys.stderr.getvalue())
        delegate.SendMail(subject, text)
        return 1
      else:
        raise
    finally:
      if options.upload_log:
        gsutil_logging_handler.upload()
  except manifest_util.Error as e:
    if options.debug:
      raise
    sys.stderr.write(str(e) + '\n')
    return 1

  return 0

if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
