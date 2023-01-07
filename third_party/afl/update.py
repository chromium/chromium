#! /usr/bin/env python
# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script for updating AFL. Also updates AFL version in README.chromium.
"""

import argparse
import cStringIO
import datetime
import os
import re
import subprocess
import sys
import tarfile
import urllib2


VERSION_REGEX = r'(?P<version>([0-9]*[.])?[0-9]+b)'
PATH_REGEX = r'(afl-)' + VERSION_REGEX


class ChromiumReadme(object):
  """Class that handles reading from and updating the README.chromium"""

  README_FILE_PATH = 'third_party/afl/README.chromium'
  README_VERSION_REGEX = r'Version: ' + VERSION_REGEX

  def __init__(self):
    """
    Inits the ChromiumReadme.
    """
    with open(self.README_FILE_PATH) as readme_file_handle:
      self.readme_contents = readme_file_handle.read()

  def get_current_version(self):
    """
    Get the current version of AFL according to the README.chromium
    """
    match = re.search(self.README_VERSION_REGEX, self.readme_contents)
    if not match:
      raise Exception('Could not determine current AFL version')
    return match.groupdict()['version']

  def update(self, new_version):
    """
    Update the readme to reflect the new version that has been downloaded.
    """
    new_readme = self.readme_contents
    subsitutions = [(VERSION_REGEX, new_version),  # Update the version.
                    (r'Date: .*',
                     'Date: ' + datetime.date.today().strftime("%B %d, %Y")),
                    # Update the Local Modifications.
                    (PATH_REGEX + r'/', 'afl-' + new_version + '/')]

    for regex, replacement in subsitutions:
      new_readme = re.subn(regex, replacement, new_readme, 1)[0]

    self.readme_contents = new_readme
    with open(self.README_FILE_PATH, 'w+') as readme_file_handle:
      readme_file_handle.write(self.readme_contents)


class AflTarball(object):
  """
  Class that handles the afl-latest.tgz tarball.
  """
  # Regexes that match files that we don't want to extract.
  # Note that you should add these removals to "Local Modifications" in
  # the README.chromium.
  UNWANTED_FILE_REGEX = '|'.join([
      r'(.*\.elf)',  # presubmit complains these aren't marked executable.
      r'(.*others/elf)',  # We don't need this if we have no elfs.
      # checkdeps complains about #includes.
      r'(.*afl-llvm-pass\.so\.cc)',
      r'(.*argv.*)',  # Delete the demo's directory as well.

      r'(.*dictionaries.*)',  # Including these make builds fail.
  ])
  AFL_SRC_DIR = 'third_party/afl/src'

  def __init__(self, version):
    """
    Init this AFL tarball.
    """
    release_name = 'afl-{0}'.format(version)
    filename = '{0}.tgz'.format(release_name)

    # Note: lcamtuf.coredump.cx does not support TLS connections. The "http://"
    # protocol is intentional.
    self.url = "http://lcamtuf.coredump.cx/afl/releases/{0}".format(filename)
    self.tarball = None
    self.real_version = version if version != 'latest' else None

  def download(self):
    """Download the tarball version from
    http://lcamtuf.coredump.cx/afl/releases/
    """
    tarball_contents = urllib2.urlopen(self.url).read()
    tarball_file = cStringIO.StringIO(tarball_contents)
    self.tarball = tarfile.open(fileobj=tarball_file, mode="r:gz")
    if self.real_version is None:
      regex_match = re.search(VERSION_REGEX, self.tarball.members[0].path)
      self.real_version = regex_match.groupdict()['version']

  def extract(self):
    """
    Extract the files and folders from the tarball we have downloaded while
    skipping unwanted ones.
    """

    for member in self.tarball.getmembers():
      member.path = re.sub(PATH_REGEX, self.AFL_SRC_DIR, member.path)
      if re.match(self.UNWANTED_FILE_REGEX, member.path):
        print 'skipping unwanted file: {0}'.format(member.path)
        continue
      self.tarball.extract(member)


def version_to_float(version):
  """
  Convert version string to float.
  """
  if version.endswith('b'):
    return float(version[:-1])

  return float(version)


def apply_patches():
  afl_dir = os.path.join('third_party', 'afl')
  patch_dir = os.path.join(afl_dir, 'patches')
  src_dir = os.path.join(afl_dir, 'src')
  for patch_file in os.listdir(patch_dir):
    subprocess.check_output(
        ['patch', '-i',
         os.path.join('..', 'patches', patch_file)], cwd=src_dir)


def update_afl(new_version):
  """
  Update this version of AFL to newer version, new_version.
  """
  readme = ChromiumReadme()
  old_version = readme.get_current_version()
  if new_version != 'latest':
    new_float = version_to_float(new_version)
    assert version_to_float(old_version) < new_float, (
        'Trying to update from version {0} to {1}'.format(old_version,
                                                          new_version))

  # Extract the tarball.
  tarball = AflTarball(new_version)
  tarball.download()
  tarball.extract()

  apply_patches()

  readme.update(tarball.real_version)


def main():
  """
  Update AFL if possible.
  """
  parser = argparse.ArgumentParser('Update AFL.')
  parser.add_argument('version', metavar='version', default='latest', nargs='?',
                      help='(optional) Version to update AFL to.')

  args = parser.parse_args()
  version = args.version
  if version != 'latest' and not version.endswith('b'):
    version += 'b'

  in_correct_directory = (os.path.basename(os.getcwd()) == 'src' and
                          os.path.exists('third_party'))

  assert in_correct_directory, (
      '{0} must be run from the repo\'s root'.format(sys.argv[0]))

  update_afl(version)
  print ("Run git diff third_party/afl/src/docs/ChangeLog to see changes to AFL"
         " since the last roll")


if __name__ == '__main__':
  main()
