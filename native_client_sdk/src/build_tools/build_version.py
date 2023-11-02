# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Small utility library of python functions used during SDK building.
"""

import os
import re
import sys

# pylint: disable=E0602

# Reuse last change utility code.
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
SRC_DIR = os.path.dirname(os.path.dirname(os.path.dirname(SCRIPT_DIR)))
sys.path.append(os.path.join(SRC_DIR, 'build/util'))
import lastchange


# Location of chrome's version file.
VERSION_PATH = os.path.join(SRC_DIR, 'chrome', 'VERSION')


def ChromeVersion():
  '''Extract chrome version from src/chrome/VERSION + svn.

  Returns:
    Chrome version string or trunk + svn rev.
  '''
  info = FetchCommitPosition()
  _, ref, revision = ParseCommitPosition(info.revision)
  if ref in ['refs/heads/master', 'refs/heads/main']:
    return 'trunk.%s' % revision
  return ChromeVersionNoTrunk()


def ChromeVersionNoTrunk():
  '''Extract the chrome version from src/chrome/VERSION.
  Ignore whether this is a trunk build.

  Returns:
    Chrome version string.
  '''
  exec(open(VERSION_PATH).read())
  return '%s.%s.%s.%s' % (MAJOR, MINOR, BUILD, PATCH)


def ChromeMajorVersion():
  '''Extract chrome major version from src/chrome/VERSION.

  Returns:
    Chrome major version.
  '''
  exec(open(VERSION_PATH, 'r').read())
  return str(MAJOR)


def ChromeRevision():
  '''Extract chrome revision from svn.

     Now that the Chrome source-of-truth is git, this will return the
     Cr-Commit-Position instead. Fortunately, this value is equal to the SVN
     revision if one exists.

  Returns:
    The Chrome revision as a string. e.g. "12345"
  '''
  version = FetchCommitPosition()
  return ParseCommitPosition(version.revision)[2]


def ChromeCommitPosition():
  '''Return the full git sha and commit position.

  Returns:
    A value like:
    0178d4831bd36b5fb9ff477f03dc43b11626a6dc-refs/heads/main@{#292238}
  '''
  return FetchCommitPosition().revision


def NaClRevision():
  '''Extract NaCl revision from svn.

  Returns:
    The NaCl revision as a string. e.g. "12345"
  '''
  nacl_dir = os.path.join(SRC_DIR, 'native_client')
  return lastchange.FetchVersionInfo(nacl_dir).revision


def FetchCommitPosition(directory=None):
  '''Return the "commit-position" of the Chromium git repo. This should be
  equivalent to the SVN revision if one exists.
  '''
  SEARCH_LIMIT = 100
  for i in xrange(SEARCH_LIMIT):
    cmd = ['show', '-s', '--format=%H%n%B', 'HEAD~%d' % i]
    proc = lastchange.RunGitCommand(directory, cmd)
    if not proc:
      break

    output = proc.communicate()[0]
    if not (proc.returncode == 0 and output):
      break

    lines = output.splitlines()

    # First line is the hash.
    hsh = lines[0]
    if not re.match(r'[0-9a-fA-F]+', hsh):
      break

    for line in reversed(lines):
      if line.startswith('Cr-Commit-Position:'):
        pos = line.rsplit()[-1].strip()
        return lastchange.VersionInfo(hsh, '%s-%s' % (hsh, pos))

  raise Exception('Unable to fetch a Git Commit Position.')



def ParseCommitPosition(commit_position):
  '''Parse a Chrome commit position into its components.

  Given a commit position like:
    0178d4831bd36b5fb9ff477f03dc43b11626a6dc-refs/heads/main@{#292238}
  Returns:
    ("0178d4831bd36b5fb9ff477f03dc43b11626a6dc", "refs/heads/main", "292238")
  '''
  m = re.match(r'([0-9a-fA-F]+)(?:-([^@]+)@{#(\d+)})?', commit_position)
  if m:
    return m.groups()
  return None
