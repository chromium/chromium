# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import hashlib
import copy
import logging
import os
import subprocess
import sys
import urlparse
import urllib2

import command_common
import download
from sdk_update_common import Error
import sdk_update_common

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PARENT_DIR = os.path.dirname(SCRIPT_DIR)
sys.path.append(PARENT_DIR)
try:
  import cygtar
except ImportError:
  # Try to find this in the Chromium repo.
  CHROME_SRC_DIR = os.path.abspath(
      os.path.join(PARENT_DIR, '..', '..', '..', '..'))
  sys.path.append(os.path.join(CHROME_SRC_DIR, 'native_client', 'build'))
  import cygtar


RECOMMENDED = 'recommended'
SDK_TOOLS = 'sdk_tools'
HTTP_CONTENT_LENGTH = 'Content-Length'  # HTTP Header field for content length
DEFAULT_CACHE_SIZE = 512 * 1024 * 1024  # 1/2 Gb cache by default


class UpdateDelegate(object):
  def BundleDirectoryExists(self, bundle_name):
    raise NotImplementedError()

  def DownloadToFile(self, url, dest_filename):
    raise NotImplementedError()

  def ExtractArchives(self, archives, extract_dir, rename_from_dir,
                      rename_to_dir):
    raise NotImplementedError()


class RealUpdateDelegate(UpdateDelegate):
  def __init__(self, user_data_dir, install_dir, cfg):
    UpdateDelegate.__init__(self)
    self.archive_cache = os.path.join(user_data_dir, 'archives')
    self.install_dir = install_dir
    self.cache_max = getattr(cfg, 'cache_max', DEFAULT_CACHE_SIZE)

  def BundleDirectoryExists(self, bundle_name):
    bundle_path = os.path.join(self.install_dir, bundle_name)
    return os.path.isdir(bundle_path)

  def VerifyDownload(self, filename, archive):
    """Verify that a local filename in the cache matches the given
    online archive.

    Returns True if both size and sha1 match, False otherwise.
    """
    filename = os.path.join(self.archive_cache, filename)
    if not os.path.exists(filename):
      logging.info('File does not exist: %s.' % filename)
      return False
    size = os.path.getsize(filename)
    if size != archive.size:
      logging.info('File size does not match (%d vs %d): %s.' % (size,
          archive.size, filename))
      return False
    sha1_hash = hashlib.sha1()
    with open(filename, 'rb') as f:
      sha1_hash.update(f.read())
    if sha1_hash.hexdigest() != archive.GetChecksum():
      logging.info('File hash does not match: %s.' % filename)
      return False
    return True

  def BytesUsedInCache(self):
    """Determine number of bytes currently be in local archive cache."""
    total = 0
    for root, _, files in os.walk(self.archive_cache):
      for filename in files:
        total += os.path.getsize(os.path.join(root, filename))
    return total

  def CleanupCache(self):
    """Remove archives from the local filesystem cache until the
    total size is below cache_max.

    This is done my deleting the oldest archive files until the
    condition is satisfied.  If cache_max is zero then the entire
    cache will be removed.
    """
    used = self.BytesUsedInCache()
    logging.info('Cache usage: %d / %d' % (used, self.cache_max))
    if used <= self.cache_max:
      return
    clean_bytes = used - self.cache_max

    logging.info('Clearing %d bytes in archive cache' % clean_bytes)
    file_timestamps = []
    for root, _, files in os.walk(self.archive_cache):
      for filename in files:
        fullname = os.path.join(root, filename)
        file_timestamps.append((os.path.getmtime(fullname), fullname))

    file_timestamps.sort()
    while clean_bytes > 0:
      assert(file_timestamps)
      filename_to_remove = file_timestamps[0][1]
      clean_bytes -= os.path.getsize(filename_to_remove)
      logging.info('Removing from cache: %s' % filename_to_remove)
      os.remove(filename_to_remove)
      # Also remove resulting empty parent directory structure
      while True:
        filename_to_remove = os.path.dirname(filename_to_remove)
        if not os.listdir(filename_to_remove):
          os.rmdir(filename_to_remove)
        else:
          break
      file_timestamps = file_timestamps[1:]

  def DownloadToFile(self, url, dest_filename):
    dest_path = os.path.join(self.archive_cache, dest_filename)
    sdk_update_common.MakeDirs(os.path.dirname(dest_path))

    out_stream = None
    url_stream = None
    try:
      out_stream = open(dest_path, 'wb')
      url_stream = download.UrlOpen(url)
      content_length = int(url_stream.info()[HTTP_CONTENT_LENGTH])
      progress = download.MakeProgressFunction(content_length)
      sha1, size = download.DownloadAndComputeHash(url_stream, out_stream,
                                                   progress)
      return sha1, size
    except urllib2.URLError as e:
      raise Error('Unable to read from URL "%s".\n  %s' % (url, e))
    except IOError as e:
      raise Error('Unable to write to file "%s".\n  %s' % (dest_filename, e))
    finally:
      if url_stream:
        url_stream.close()
      if out_stream:
        out_stream.close()

  def ExtractArchives(self, archives, extract_dir, rename_from_dir,
                      rename_to_dir):
    tar_file = None

    extract_path = os.path.join(self.install_dir, extract_dir)
    rename_from_path = os.path.join(self.install_dir, rename_from_dir)
    rename_to_path = os.path.join(self.install_dir, rename_to_dir)

    # Extract to extract_dir, usually "<bundle name>_update".
    # This way if the extraction fails, we haven't blown away the old bundle
    # (if it exists).
    sdk_update_common.RemoveDir(extract_path)
    sdk_update_common.MakeDirs(extract_path)
    curpath = os.getcwd()
    tar_file = None

    try:
      try:
        logging.info('Changing the directory to %s' % (extract_path,))
        os.chdir(extract_path)
      except Exception as e:
        raise Error('Unable to chdir into "%s".\n  %s' % (extract_path, e))

      for i, archive in enumerate(archives):
        archive_path = os.path.join(self.archive_cache, archive)

        if len(archives) > 1:
          print '(file %d/%d - "%s")' % (
             i + 1, len(archives), os.path.basename(archive_path))
        logging.info('Extracting to %s' % (extract_path,))

        if sys.platform == 'win32':
          try:
            logging.info('Opening file %s (%d/%d).' % (archive_path, i + 1,
                len(archives)))
            try:
              tar_file = cygtar.CygTar(archive_path, 'r', verbose=True)
            except Exception as e:
              raise Error("Can't open archive '%s'.\n  %s" % (archive_path, e))

            tar_file.Extract()
          finally:
            if tar_file:
              tar_file.Close()
        else:
          try:
            subprocess.check_call(['tar', 'xf', archive_path])
          except subprocess.CalledProcessError:
            raise Error('Error extracting archive: %s' % archive_path)

      logging.info('Changing the directory to %s' % (curpath,))
      os.chdir(curpath)

      logging.info('Renaming %s->%s' % (rename_from_path, rename_to_path))
      sdk_update_common.RenameDir(rename_from_path, rename_to_path)
    finally:
      # Change the directory back so we can remove the update directory.
      os.chdir(curpath)

      # Clean up the ..._update directory.
      try:
        sdk_update_common.RemoveDir(extract_path)
      except Exception as e:
        logging.error('Failed to remove directory \"%s\".  %s' % (
            extract_path, e))


def Update(delegate, remote_manifest, local_manifest, bundle_names, force):
  valid_bundles = set([bundle.name for bundle in remote_manifest.GetBundles()])
  requested_bundles = _GetRequestedBundleNamesFromArgs(remote_manifest,
                                                       bundle_names)
  invalid_bundles = requested_bundles - valid_bundles
  if invalid_bundles:
    logging.warn('Ignoring unknown bundle(s): %s' % (
        ', '.join(invalid_bundles)))
    requested_bundles -= invalid_bundles

  if SDK_TOOLS in requested_bundles:
    logging.warn('Updating sdk_tools happens automatically. '
                 'Ignoring manual update request.')
    requested_bundles.discard(SDK_TOOLS)

  if requested_bundles:
    for bundle_name in requested_bundles:
      logging.info('Trying to update %s' % (bundle_name,))
      UpdateBundleIfNeeded(delegate, remote_manifest, local_manifest,
          bundle_name, force)
  else:
    logging.warn('No bundles to update.')


def Reinstall(delegate, local_manifest, bundle_names):
  valid_bundles, invalid_bundles = \
      command_common.GetValidBundles(local_manifest, bundle_names)
  if invalid_bundles:
    logging.warn('Unknown bundle(s): %s\n' % (', '.join(invalid_bundles)))

  if not valid_bundles:
    logging.warn('No bundles to reinstall.')
    return

  for bundle_name in valid_bundles:
    bundle = copy.deepcopy(local_manifest.GetBundle(bundle_name))

    # HACK(binji): There was a bug where we'd merge the bundles from the old
    # archive and the new archive when updating. As a result, some users may
    # have a cache manifest that contains duplicate archives. Remove all
    # archives with the same basename except for the most recent.
    # Because the archives are added to a list, we know the most recent is at
    # the end.
    archives = {}
    for archive in bundle.GetArchives():
      url = archive.url
      path = urlparse.urlparse(url)[2]
      basename = os.path.basename(path)
      archives[basename] = archive

    # Update the bundle with these new archives.
    bundle.RemoveAllArchives()
    for _, archive in archives.iteritems():
      bundle.AddArchive(archive)

    _UpdateBundle(delegate, bundle, local_manifest)


def UpdateBundleIfNeeded(delegate, remote_manifest, local_manifest,
                         bundle_name, force):
  bundle = remote_manifest.GetBundle(bundle_name)
  if bundle:
    if _BundleNeedsUpdate(delegate, local_manifest, bundle):
      # TODO(binji): It would be nicer to detect whether the user has any
      # modifications to the bundle. If not, we could update with impunity.
      if not force and delegate.BundleDirectoryExists(bundle_name):
        print ('%s already exists, but has an update available.\n'
            'Run update with the --force option to overwrite the '
            'existing directory.\nWarning: This will overwrite any '
            'modifications you have made within this directory.'
            % (bundle_name,))
        return

      _UpdateBundle(delegate, bundle, local_manifest)
    else:
      print '%s is already up to date.' % (bundle.name,)
  else:
    logging.error('Bundle %s does not exist.' % (bundle_name,))


def _GetRequestedBundleNamesFromArgs(remote_manifest, requested_bundles):
  requested_bundles = set(requested_bundles)
  if RECOMMENDED in requested_bundles:
    requested_bundles.discard(RECOMMENDED)
    requested_bundles |= set(_GetRecommendedBundleNames(remote_manifest))

  return requested_bundles


def _GetRecommendedBundleNames(remote_manifest):
  result = []
  for bundle in remote_manifest.GetBundles():
    if bundle.recommended == 'yes' and bundle.name != SDK_TOOLS:
      result.append(bundle.name)
  return result


def _BundleNeedsUpdate(delegate, local_manifest, bundle):
  # Always update the bundle if the directory doesn't exist;
  # the user may have deleted it.
  if not delegate.BundleDirectoryExists(bundle.name):
    return True

  return local_manifest.BundleNeedsUpdate(bundle)


def _UpdateBundle(delegate, bundle, local_manifest):
  archives = bundle.GetHostOSArchives()
  if not archives:
    logging.warn('Bundle %s does not exist for this platform.' % (bundle.name,))
    return

  archive_filenames = []

  shown_banner = False
  for i, archive in enumerate(archives):
    archive_filename = _GetFilenameFromURL(archive.url)
    archive_filename = os.path.join(bundle.name, archive_filename)

    if not delegate.VerifyDownload(archive_filename, archive):
      if not shown_banner:
        shown_banner = True
        print 'Downloading bundle %s' % (bundle.name,)
      if len(archives) > 1:
        print '(file %d/%d - "%s")' % (
            i + 1, len(archives), os.path.basename(archive.url))
      sha1, size = delegate.DownloadToFile(archive.url, archive_filename)
      _ValidateArchive(archive, sha1, size)

    archive_filenames.append(archive_filename)

  print 'Updating bundle %s to version %s, revision %s' % (
      bundle.name, bundle.version, bundle.revision)
  extract_dir = bundle.name + '_update'

  repath_dir = bundle.get('repath', None)
  if repath_dir:
    # If repath is specified:
    # The files are extracted to nacl_sdk/<bundle.name>_update/<repath>/...
    # The destination directory is nacl_sdk/<bundle.name>/...
    rename_from_dir = os.path.join(extract_dir, repath_dir)
  else:
    # If no repath is specified:
    # The files are extracted to nacl_sdk/<bundle.name>_update/...
    # The destination directory is nacl_sdk/<bundle.name>/...
    rename_from_dir = extract_dir

  rename_to_dir = bundle.name

  delegate.ExtractArchives(archive_filenames, extract_dir, rename_from_dir,
                           rename_to_dir)

  logging.info('Updating local manifest to include bundle %s' % (bundle.name))
  local_manifest.RemoveBundle(bundle.name)
  local_manifest.SetBundle(bundle)
  delegate.CleanupCache()


def _GetFilenameFromURL(url):
  path = urlparse.urlparse(url)[2]
  return os.path.basename(path)


def _ValidateArchive(archive, actual_sha1, actual_size):
  if actual_size != archive.size:
    raise Error('Size mismatch on "%s".  Expected %s but got %s bytes' % (
        archive.url, archive.size, actual_size))
  if actual_sha1 != archive.GetChecksum():
    raise Error('SHA1 checksum mismatch on "%s".  Expected %s but got %s' % (
        archive.url, archive.GetChecksum(), actual_sha1))
