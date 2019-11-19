#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
IDLRelease for PPAPI

This file defines the behavior of the AST namespace which allows for resolving
a symbol as one or more AST nodes given a Release or range of Releases.
"""

from __future__ import print_function

import sys

from idl_log import ErrOut, InfoOut, WarnOut
from idl_option import GetOption, Option, ParseOptions

Option('release_debug', 'Debug Release data')
Option('wgap', 'Ignore Release gap warning')


#
# Module level functions and data used for testing.
#
error = None
warning = None
def ReportReleaseError(msg):
  global error
  error = msg

def ReportReleaseWarning(msg):
  global warning
  warning = msg

def ReportClear():
  global error, warning
  error = None
  warning = None

#
# IDLRelease
#
# IDLRelease is an object which stores the association of a given symbol
# name, with an AST node for a range of Releases for that object.
#
# A vmin value of None indicates that the object begins at the earliest
# available Release number.  The value of vmin is always inclusive.

# A vmax value of None indicates that the object is never deprecated, so
# it exists until it is overloaded or until the latest available Release.
# The value of vmax is always exclusive, representing the first Release
# on which the object is no longer valid.
class IDLRelease(object):
  def __init__(self, rmin, rmax):
    self.rmin = rmin
    self.rmax = rmax

  def __str__(self):
    if not self.rmin:
      rmin = '0'
    else:
      rmin = str(self.rmin)
    if not self.rmax:
      rmax = '+oo'
    else:
      rmax = str(self.rmax)
    return '[%s,%s)' % (rmin, rmax)

  def SetReleaseRange(self, rmin, rmax):
    self.rmin = rmin
    self.rmax = rmax

  # True, if Release falls within the interval [self.vmin, self.vmax)
  def IsRelease(self, release):
    if self.rmax and self.rmax <= release:
      return False
    if self.rmin and self.rmin > release:
      return False
    if GetOption('release_debug'):
      InfoOut.Log('%f is in %s' % (release, self))
    return True

  # True, if Release falls within the interval [self.vmin, self.vmax)
  def InReleases(self, releases):
    if not releases: return False

    # Check last release first, since InRange does not match last item
    if self.IsRelease(releases[-1]): return True
    if len(releases) > 1:
      return self.InRange(releases[0], releases[-1])
    return False

  # True, if interval [vmin, vmax) overlaps interval [self.vmin, self.vmax)
  def InRange(self, rmin, rmax):
    assert (rmin == None) or rmin < rmax

    # An min of None always passes a min bound test
    # An max of None always passes a max bound test
    if rmin is not None and self.rmax is not None:
      if self.rmax <= rmin:
        return False
    if rmax is not None and self.rmin is not None:
      if self.rmin >= rmax:
        return False

    if GetOption('release_debug'):
      InfoOut.Log('%f to %f is in %s' % (rmin, rmax, self))
    return True

  def GetMinMax(self, releases = None):
    if not releases:
      return self.rmin, self.rmax

    if not self.rmin:
      rmin = releases[0]
    else:
      rmin = str(self.rmin)
    if not self.rmax:
      rmax = releases[-1]
    else:
      rmax = str(self.rmax)
    return (rmin, rmax)

  def SetMin(self, release):
    assert not self.rmin
    self.rmin = release

  def Error(self, msg):
    ReportReleaseError(msg)

  def Warn(self, msg):
    ReportReleaseWarning(msg)


#
# IDLReleaseList
#
# IDLReleaseList is a list based container for holding IDLRelease
# objects in order.  The IDLReleaseList can be added to, and searched by
# range.  Objects are stored in order, and must be added in order.
#
class IDLReleaseList(object):
  def __init__(self):
    self._nodes = []

  def GetReleases(self):
    return self._nodes

  def FindRelease(self, release):
    for node in self._nodes:
      if node.IsRelease(release):
        return node
    return None

  def FindRange(self, rmin, rmax):
    assert (rmin == None) or rmin != rmax

    out = []
    for node in self._nodes:
      if node.InRange(rmin, rmax):
        out.append(node)
    return out

  def AddNode(self, node):
    if GetOption('release_debug'):
      InfoOut.Log('\nAdding %s %s' % (node.Location(), node))
    last = None

    # Check current releases in that namespace
    for cver in self._nodes:
      if GetOption('release_debug'): InfoOut.Log('  Checking %s' % cver)

      # We should only be missing a 'release' tag for the first item.
      if not node.rmin:
        node.Error('Missing release on overload of previous %s.' %
                   cver.Location())
        return False

      # If the node has no max, then set it to this one
      if not cver.rmax:
        cver.rmax = node.rmin
        if GetOption('release_debug'): InfoOut.Log('  Update %s' % cver)

      # if the max and min overlap, than's an error
      if cver.rmax > node.rmin:
        if node.rmax and cver.rmin >= node.rmax:
          node.Error('Declarations out of order.')
        else:
          node.Error('Overlap in releases: %s vs %s when adding %s' %
                     (cver.rmax, node.rmin, node))
        return False
      last = cver

    # Otherwise, the previous max and current min should match
    # unless this is the unlikely case of something being only
    # temporarily deprecated.
    if last and last.rmax != node.rmin:
      node.Warn('Gap in release numbers.')

    # If we made it here, this new node must be the 'newest'
    # and does not overlap with anything previously added, so
    # we can add it to the end of the list.
    if GetOption('release_debug'): InfoOut.Log('Done %s' % node)
    self._nodes.append(node)
    return True

#
# IDLReleaseMap
#
# A release map, can map from an float interface release, to a global
# release string.
#
class IDLReleaseMap(object):
  def __init__(self, release_info):
    self.version_to_release = {}
    self.release_to_version = {}
    self.release_to_channel = {}
    for release, version, channel in release_info:
      self.version_to_release[version] = release
      self.release_to_version[release] = version
      self.release_to_channel[release] = channel
    self.releases = sorted(self.release_to_version.keys())
    self.versions = sorted(self.version_to_release.keys())

  def GetVersion(self, release):
    return self.release_to_version.get(release, None)

  def GetVersions(self):
    return self.versions

  def GetRelease(self, version):
    return self.version_to_release.get(version, None)

  def GetReleases(self):
    return self.releases

  def GetReleaseRange(self):
    return (self.releases[0], self.releases[-1])

  def GetVersionRange(self):
    return (self.versions[0], self.version[-1])

  def GetChannel(self, release):
    return self.release_to_channel.get(release, None)

#
# Test Code
#
def TestReleaseNode():
  FooXX = IDLRelease(None, None)
  Foo1X = IDLRelease('M14', None)
  Foo23 = IDLRelease('M15', 'M16')

  assert FooXX.IsRelease('M13')
  assert FooXX.IsRelease('M14')
  assert FooXX.InRange('M13', 'M13A')
  assert FooXX.InRange('M14','M15')

  assert not Foo1X.IsRelease('M13')
  assert Foo1X.IsRelease('M14')
  assert Foo1X.IsRelease('M15')

  assert not Foo1X.InRange('M13', 'M14')
  assert not Foo1X.InRange('M13A', 'M14')
  assert Foo1X.InRange('M14', 'M15')
  assert Foo1X.InRange('M15', 'M16')

  assert not Foo23.InRange('M13', 'M14')
  assert not Foo23.InRange('M13A', 'M14')
  assert not Foo23.InRange('M14', 'M15')
  assert Foo23.InRange('M15', 'M16')
  assert Foo23.InRange('M14', 'M15A')
  assert Foo23.InRange('M15B', 'M17')
  assert not Foo23.InRange('M16', 'M17')
  print("TestReleaseNode - Passed")


def TestReleaseListWarning():
  FooXX = IDLRelease(None, None)
  Foo1X = IDLRelease('M14', None)
  Foo23 = IDLRelease('M15', 'M16')
  Foo45 = IDLRelease('M17', 'M18')

  # Add nodes out of order should fail
  ReportClear()
  releases = IDLReleaseList()
  assert releases.AddNode(Foo23)
  assert releases.AddNode(Foo45)
  assert warning
  print("TestReleaseListWarning - Passed")


def TestReleaseListError():
  FooXX = IDLRelease(None, None)
  Foo1X = IDLRelease('M14', None)
  Foo23 = IDLRelease('M15', 'M16')
  Foo45 = IDLRelease('M17', 'M18')

  # Add nodes out of order should fail
  ReportClear()
  releases = IDLReleaseList()
  assert releases.AddNode(FooXX)
  assert releases.AddNode(Foo23)
  assert not releases.AddNode(Foo1X)
  assert error
  print("TestReleaseListError - Passed")


def TestReleaseListOK():
  FooXX = IDLRelease(None, None)
  Foo1X = IDLRelease('M14', None)
  Foo23 = IDLRelease('M15', 'M16')
  Foo45 = IDLRelease('M17', 'M18')

  # Add nodes in order should work
  ReportClear()
  releases = IDLReleaseList()
  assert releases.AddNode(FooXX)
  assert releases.AddNode(Foo1X)
  assert releases.AddNode(Foo23)
  assert not error and not warning
  assert releases.AddNode(Foo45)
  assert warning

  assert releases.FindRelease('M13') == FooXX
  assert releases.FindRelease('M14') == Foo1X
  assert releases.FindRelease('M15') == Foo23
  assert releases.FindRelease('M16') == None
  assert releases.FindRelease('M17') == Foo45
  assert releases.FindRelease('M18') == None

  assert releases.FindRange('M13','M14') == [FooXX]
  assert releases.FindRange('M13','M17') == [FooXX, Foo1X, Foo23]
  assert releases.FindRange('M16','M17') == []
  assert releases.FindRange(None, None) == [FooXX, Foo1X, Foo23, Foo45]

  # Verify we can find the correct versions
  print("TestReleaseListOK - Passed")


def TestReleaseMap():
  print("TestReleaseMap- Passed")


def Main(args):
  TestReleaseNode()
  TestReleaseListWarning()
  TestReleaseListError()
  TestReleaseListOK()
  print("Passed")
  return 0


if __name__ == '__main__':
  sys.exit(Main(sys.argv[1:]))
