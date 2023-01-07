# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import copy
import hashlib
import json
import string
import sys
import urllib2

MANIFEST_VERSION = 2

# Some commonly-used key names.
ARCHIVES_KEY = 'archives'
BUNDLES_KEY = 'bundles'
NAME_KEY = 'name'
REVISION_KEY = 'revision'
VERSION_KEY = 'version'

# Valid values for the archive.host_os field
HOST_OS_LITERALS = frozenset(['mac', 'win', 'linux', 'all'])

# Valid keys for various sdk objects, used for validation.
VALID_ARCHIVE_KEYS = frozenset(['host_os', 'size', 'checksum', 'url'])

# Valid values for bundle.stability field
STABILITY_LITERALS = [
    'obsolete', 'post_stable', 'stable', 'beta', 'dev', 'canary']

# Valid values for bundle-recommended field.
YES_NO_LITERALS = ['yes', 'no']
VALID_BUNDLES_KEYS = frozenset([
    ARCHIVES_KEY, NAME_KEY, VERSION_KEY, REVISION_KEY,
    'description', 'desc_url', 'stability', 'recommended', 'repath',
    'sdk_revision'
    ])

VALID_MANIFEST_KEYS = frozenset(['manifest_version', BUNDLES_KEY])


def GetHostOS():
  '''Returns the host_os value that corresponds to the current host OS'''
  return {
      'linux2': 'linux',
      'darwin': 'mac',
      'cygwin': 'win',
      'win32':  'win'
      }[sys.platform]


def DictToJSON(pydict):
  """Convert a dict to a JSON-formatted string."""
  pretty_string = json.dumps(pydict, sort_keys=True, indent=2)
  # json.dumps sometimes returns trailing whitespace and does not put
  # a newline at the end.  This code fixes these problems.
  pretty_lines = pretty_string.split('\n')
  return '\n'.join([line.rstrip() for line in pretty_lines]) + '\n'


def DownloadAndComputeHash(from_stream, to_stream=None, progress_func=None):
  '''Download the archive data from from-stream and generate sha1 and
  size info.

  Args:
    from_stream:   An input stream that supports read.
    to_stream:     [optional] the data is written to to_stream if it is
                   provided.
    progress_func: [optional] A function used to report download progress. If
                   provided, progress_func is called with progress=0 at the
                   beginning of the download, periodically with progress=1
                   during the download, and progress=100 at the end.

  Return
    A tuple (sha1, size) where sha1 is a sha1-hash for the archive data and
    size is the size of the archive data in bytes.'''
  # Use a no-op progress function if none is specified.
  def progress_no_op(progress):
    pass
  if not progress_func:
    progress_func = progress_no_op

  sha1_hash = hashlib.sha1()
  size = 0
  progress_func(progress=0)
  while(1):
    data = from_stream.read(32768)
    if not data:
      break
    sha1_hash.update(data)
    size += len(data)
    if to_stream:
      to_stream.write(data)
    progress_func(size)

  progress_func(progress=100)
  return sha1_hash.hexdigest(), size


class Error(Exception):
  """Generic error/exception for manifest_util module"""
  pass


class Archive(dict):
  """A placeholder for sdk archive information. We derive Archive from
     dict so that it is easily serializable. """

  def __init__(self, host_os_name):
    """ Create a new archive for the given host-os name. """
    super(Archive, self).__init__()
    self['host_os'] = host_os_name

  def CopyFrom(self, src):
    """Update the content of the archive by copying values from the given
       dictionary.

    Args:
      src: The dictionary whose values must be copied to the archive."""
    for key, value in src.items():
      self[key] = value

  def Validate(self, error_on_unknown_keys=False):
    """Validate the content of the archive object. Raise an Error if
       an invalid or missing field is found.

    Args:
      error_on_unknown_keys: If True, raise an Error when unknown keys are
      found in the archive.
    """
    host_os = self.get('host_os', None)
    if host_os and host_os not in HOST_OS_LITERALS:
      raise Error('Invalid host-os name in archive')
    # Ensure host_os has a valid string. We'll use it for pretty printing.
    if not host_os:
      host_os = 'all (default)'
    if not self.get('url', None):
      raise Error('Archive "%s" has no URL' % host_os)
    if not self.get('size', None):
      raise Error('Archive "%s" has no size' % host_os)
    checksum = self.get('checksum', None)
    if not checksum:
      raise Error('Archive "%s" has no checksum' % host_os)
    elif not isinstance(checksum, dict):
      raise Error('Archive "%s" has a checksum, but it is not a dict' % host_os)
    elif not len(checksum):
      raise Error('Archive "%s" has an empty checksum dict' % host_os)
    # Verify that all key names are valid.
    if error_on_unknown_keys:
      for key in self:
        if key not in VALID_ARCHIVE_KEYS:
          raise Error('Archive "%s" has invalid attribute "%s"' % (
              host_os, key))

  def UpdateVitals(self, revision):
    """Update the size and checksum information for this archive
    based on the content currently at the URL.

    This allows the template manifest to be maintained without
    the need to size and checksums to be present.
    """
    template = string.Template(self['url'])
    self['url'] = template.substitute({'revision': revision})
    from_stream = urllib2.urlopen(self['url'])
    sha1_hash, size = DownloadAndComputeHash(from_stream)
    self['size'] = size
    self['checksum'] = { 'sha1': sha1_hash }

  def __getattr__(self, name):
    """Retrieve values from this dict using attributes.

    This allows for foo.bar instead of foo['bar'].

    Args:
      name: the name of the key, 'bar' in the example above.
    Returns:
      The value associated with that key."""
    if name not in self:
      raise AttributeError(name)
    # special case, self.checksum returns the sha1, not the checksum dict.
    if name == 'checksum':
      return self.GetChecksum()
    return self.__getitem__(name)

  def __setattr__(self, name, value):
    """Set values in this dict using attributes.

    This allows for foo.bar instead of foo['bar'].

    Args:
      name: The name of the key, 'bar' in the example above.
      value: The value to associate with that key."""
    # special case, self.checksum returns the sha1, not the checksum dict.
    if name == 'checksum':
      self.setdefault('checksum', {})['sha1'] = value
      return
    return self.__setitem__(name, value)

  def GetChecksum(self, hash_type='sha1'):
    """Returns a given cryptographic checksum of the archive"""
    return self['checksum'][hash_type]


class Bundle(dict):
  """A placeholder for sdk bundle information. We derive Bundle from
     dict so that it is easily serializable."""

  def __init__(self, obj):
    """ Create a new bundle with the given bundle name."""
    if isinstance(obj, str) or isinstance(obj, unicode):
      dict.__init__(self, [(ARCHIVES_KEY, []), (NAME_KEY, obj)])
    else:
      dict.__init__(self, obj)

  def MergeWithBundle(self, bundle):
    """Merge this bundle with |bundle|.

    Merges dict in |bundle| with this one in such a way that keys are not
    duplicated: the values of the keys in |bundle| take precedence in the
    resulting dictionary.

    Archives in |bundle| will be appended to archives in self.

    Args:
      bundle: The other bundle.  Must be a dict.
    """
    assert self is not bundle

    for k, v in bundle.iteritems():
      if k == ARCHIVES_KEY:
        for archive in v:
          self.get(k, []).append(archive)
      else:
        self[k] = v

  def __str__(self):
    return self.GetDataAsString()

  def GetDataAsString(self):
    """Returns the JSON bundle object, pretty-printed"""
    return DictToJSON(self)

  def LoadDataFromString(self, json_string):
    """Load a JSON bundle string. Raises an exception if json_string
       is not well-formed JSON.

    Args:
      json_string: a JSON-formatted string containing the bundle
    """
    self.CopyFrom(json.loads(json_string))

  def CopyFrom(self, source):
    """Update the content of the bundle by copying values from the given
       dictionary.

    Args:
      source: The dictionary whose values must be copied to the bundle."""
    for key, value in source.items():
      if key == ARCHIVES_KEY:
        archives = []
        for a in value:
          new_archive = Archive(a['host_os'])
          new_archive.CopyFrom(a)
          archives.append(new_archive)
        self[ARCHIVES_KEY] = archives
      else:
        self[key] = value

  def Validate(self, add_missing_info=False, error_on_unknown_keys=False):
    """Validate the content of the bundle. Raise an Error if an invalid or
       missing field is found.

    Args:
      error_on_unknown_keys: If True, raise an Error when unknown keys are
      found in the bundle.
    """
    # Check required fields.
    if not self.get(NAME_KEY):
      raise Error('Bundle has no name')
    if self.get(REVISION_KEY) == None:
      raise Error('Bundle "%s" is missing a revision number' % self[NAME_KEY])
    if self.get(VERSION_KEY) == None:
      raise Error('Bundle "%s" is missing a version number' % self[NAME_KEY])
    if not self.get('description'):
      raise Error('Bundle "%s" is missing a description' % self[NAME_KEY])
    if not self.get('stability'):
      raise Error('Bundle "%s" is missing stability info' % self[NAME_KEY])
    if self.get('recommended') == None:
      raise Error('Bundle "%s" is missing the recommended field' %
                  self[NAME_KEY])
    # Check specific values
    if self['stability'] not in STABILITY_LITERALS:
      raise Error('Bundle "%s" has invalid stability field: "%s"' %
                  (self[NAME_KEY], self['stability']))
    if self['recommended'] not in YES_NO_LITERALS:
      raise Error(
          'Bundle "%s" has invalid recommended field: "%s"' %
          (self[NAME_KEY], self['recommended']))
    # Verify that all key names are valid.
    if error_on_unknown_keys:
      for key in self:
        if key not in VALID_BUNDLES_KEYS:
          raise Error('Bundle "%s" has invalid attribute "%s"' %
                      (self[NAME_KEY], key))
    # Validate the archives
    for archive in self[ARCHIVES_KEY]:
      if add_missing_info and 'size' not in archive:
        archive.UpdateVitals(self[REVISION_KEY])
      archive.Validate(error_on_unknown_keys)

  def GetArchive(self, host_os_name):
    """Retrieve the archive for the given host os.

    Args:
      host_os_name: name of host os whose archive must be retrieved.
    Return:
      An Archive instance or None if it doesn't exist."""
    for archive in self[ARCHIVES_KEY]:
      if archive.host_os == host_os_name or archive.host_os == 'all':
        return archive
    return None

  def GetHostOSArchive(self):
    """Retrieve the archive for the current host os."""
    return self.GetArchive(GetHostOS())

  def GetHostOSArchives(self):
    """Retrieve all archives for the current host os, or marked all.
    """
    return [archive for archive in self.GetArchives()
        if archive.host_os in (GetHostOS(), 'all')]

  def GetArchives(self):
    """Returns all the archives in this bundle"""
    return self[ARCHIVES_KEY]

  def AddArchive(self, archive):
    """Add an archive to this bundle."""
    self[ARCHIVES_KEY].append(archive)

  def RemoveAllArchives(self):
    """Remove all archives from this Bundle."""
    del self[ARCHIVES_KEY][:]

  def RemoveAllArchivesForHostOS(self, host_os_name):
    """Remove an archive from this Bundle."""
    if host_os_name == 'all':
      del self[ARCHIVES_KEY][:]
    else:
      for i, archive in enumerate(self[ARCHIVES_KEY]):
        if archive.host_os == host_os_name:
          del self[ARCHIVES_KEY][i]

  def __getattr__(self, name):
    """Retrieve values from this dict using attributes.

    This allows for foo.bar instead of foo['bar'].

    Args:
      name: the name of the key, 'bar' in the example above.
    Returns:
      The value associated with that key."""
    if name not in self:
      raise AttributeError(name)
    return self.__getitem__(name)

  def __setattr__(self, name, value):
    """Set values in this dict using attributes.

    This allows for foo.bar instead of foo['bar'].

    Args:
      name: The name of the key, 'bar' in the example above.
      value: The value to associate with that key."""
    self.__setitem__(name, value)

  def __eq__(self, bundle):
    """Test if two bundles are equal.

    Normally the default comparison for two dicts is fine, but in this case we
    don't care about the list order of the archives.

    Args:
      bundle: The other bundle to compare against.
    Returns:
      True if the bundles are equal."""
    if not isinstance(bundle, Bundle):
      return False
    if len(self.keys()) != len(bundle.keys()):
      return False
    for key in self.keys():
      if key not in bundle:
        return False
      # special comparison for ARCHIVE_KEY because we don't care about the list
      # ordering.
      if key == ARCHIVES_KEY:
        if len(self[key]) != len(bundle[key]):
          return False
        for archive in self[key]:
          if archive != bundle.GetArchive(archive.host_os):
            return False
      elif self[key] != bundle[key]:
        return False
    return True

  def __ne__(self, bundle):
    """Test if two bundles are unequal.

    See __eq__ for more info."""
    return not self.__eq__(bundle)


class SDKManifest(object):
  """This class contains utilities for manipulation an SDK manifest string

  For ease of unit-testing, this class should not contain any file I/O.
  """

  def __init__(self):
    """Create a new SDKManifest object with default contents"""
    self._manifest_data = {
        "manifest_version": MANIFEST_VERSION,
        "bundles": [],
        }

  def Validate(self, add_missing_info=False):
    """Validate the Manifest file and raises an exception for problems"""
    # Validate the manifest top level
    if self._manifest_data["manifest_version"] > MANIFEST_VERSION:
      raise Error("Manifest version too high: %s" %
                  self._manifest_data["manifest_version"])
    # Verify that all key names are valid.
    for key in self._manifest_data:
      if key not in VALID_MANIFEST_KEYS:
        raise Error('Manifest has invalid attribute "%s"' % key)
    # Validate each bundle
    for bundle in self._manifest_data[BUNDLES_KEY]:
      bundle.Validate(add_missing_info)

  def GetBundle(self, name):
    """Get a bundle from the array of bundles.

    Args:
      name: the name of the bundle to return.
    Return:
      The first bundle with the given name, or None if it is not found."""
    if not BUNDLES_KEY in self._manifest_data:
      return None
    bundles = [bundle for bundle in self._manifest_data[BUNDLES_KEY]
               if bundle[NAME_KEY] == name]
    if len(bundles) > 1:
      sys.stderr.write("WARNING: More than one bundle with name"
                       "'%s' exists.\n" % name)
    return bundles[0] if len(bundles) > 0 else None

  def GetBundles(self):
    """Return all the bundles in the manifest."""
    return self._manifest_data[BUNDLES_KEY]

  def SetBundle(self, new_bundle):
    """Add or replace a bundle in the manifest.

    Note: If a bundle in the manifest already exists with this name, it will be
    overwritten with a copy of this bundle, at the same index as the original.

    Args:
      bundle: The bundle.
    """
    name = new_bundle[NAME_KEY]
    bundles = self.GetBundles()
    new_bundle_copy = copy.deepcopy(new_bundle)
    for i, bundle in enumerate(bundles):
      if bundle[NAME_KEY] == name:
        bundles[i] = new_bundle_copy
        return
    # Bundle not already in list, append it.
    bundles.append(new_bundle_copy)

  def RemoveBundle(self, name):
    """Remove a bundle by name.

    Args:
      name: the name of the bundle to remove.
    Return:
      True if the bundle was removed, False if there is no bundle with that
      name.
    """
    if not BUNDLES_KEY in self._manifest_data:
      return False
    bundles = self._manifest_data[BUNDLES_KEY]
    for i, bundle in enumerate(bundles):
      if bundle[NAME_KEY] == name:
        del bundles[i]
        return True
    return False

  def BundleNeedsUpdate(self, bundle):
    """Decides if a bundle needs to be updated.

    A bundle needs to be updated if it is not installed (doesn't exist in this
    manifest file) or if its revision is later than the revision in this file.

    Args:
      bundle: The Bundle to test.
    Returns:
      True if Bundle needs to be updated.
    """
    if NAME_KEY not in bundle:
      raise KeyError("Bundle must have a 'name' key.")
    local_bundle = self.GetBundle(bundle[NAME_KEY])
    return (local_bundle == None) or (
           (local_bundle[VERSION_KEY], local_bundle[REVISION_KEY]) <
           (bundle[VERSION_KEY], bundle[REVISION_KEY]))

  def MergeBundle(self, bundle, allow_existing=True):
    """Merge a Bundle into this manifest.

    The new bundle is added if not present, or merged into the existing bundle.

    Args:
      bundle: The bundle to merge.
    """
    if NAME_KEY not in bundle:
      raise KeyError("Bundle must have a 'name' key.")
    local_bundle = self.GetBundle(bundle.name)
    if not local_bundle:
      self.SetBundle(bundle)
    else:
      if not allow_existing:
        raise Error('cannot merge manifest bundle \'%s\', it already exists'
                    % bundle.name)
      local_bundle.MergeWithBundle(bundle)

  def MergeManifest(self, manifest):
    '''Merge another manifest into this manifest, disallowing overriding.

    Args
      manifest: The manifest to merge.
    '''
    for bundle in manifest.GetBundles():
      self.MergeBundle(bundle, allow_existing=False)

  def FilterBundles(self, predicate):
    """Filter the list of bundles by |predicate|.

    For all bundles in this manifest, if predicate(bundle) is False, the bundle
    is removed from the manifest.

    Args:
      predicate: a function that take a bundle and returns whether True to keep
      it or False to remove it.
    """
    self._manifest_data[BUNDLES_KEY] = filter(predicate, self.GetBundles())

  def LoadDataFromString(self, json_string, add_missing_info=False):
    """Load a JSON manifest string. Raises an exception if json_string
       is not well-formed JSON.

    Args:
      json_string: a JSON-formatted string containing the previous manifest
      all_hosts: True indicates that we should load bundles for all hosts.
          False (default) says to only load bundles for the current host"""
    new_manifest = json.loads(json_string)
    for key, value in new_manifest.items():
      if key == BUNDLES_KEY:
        # Remap each bundle in |value| to a Bundle instance
        bundles = []
        for b in value:
          new_bundle = Bundle(b[NAME_KEY])
          new_bundle.CopyFrom(b)
          bundles.append(new_bundle)
        self._manifest_data[key] = bundles
      else:
        self._manifest_data[key] = value
    self.Validate(add_missing_info)

  def __str__(self):
    return self.GetDataAsString()

  def __eq__(self, other):
    # Access to protected member _manifest_data of a client class
    # pylint: disable=W0212
    if (self._manifest_data['manifest_version'] !=
        other._manifest_data['manifest_version']):
      return False

    self_bundle_names = set(b.name for b in self.GetBundles())
    other_bundle_names = set(b.name for b in other.GetBundles())
    if self_bundle_names != other_bundle_names:
      return False

    for bundle_name in self_bundle_names:
      if self.GetBundle(bundle_name) != other.GetBundle(bundle_name):
        return False

    return True

  def __ne__(self, other):
    return not (self == other)

  def GetDataAsString(self):
    """Returns the current JSON manifest object, pretty-printed"""
    return DictToJSON(self._manifest_data)
