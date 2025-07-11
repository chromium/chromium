#!/usr/bin/env python
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
IDLNamespace for PPAPI

This file defines the behavior of the AST namespace which allows for resolving
a symbol as one or more AST nodes given a release or range of releases.
"""

from __future__ import print_function

import sys

from idl_option import GetOption, Option, ParseOptions
from idl_log import ErrOut, InfoOut, WarnOut
from idl_release import IDLRelease, IDLReleaseList

Option('label', 'Use the specifed label blocks.', default='Chrome')
Option('namespace_debug', 'Use the specified release')


#
# IDLNamespace
#
# IDLNamespace provides a mapping between a symbol name and an IDLReleaseList
# which contains IDLRelease objects.  It provides an interface for fetching
# one or more IDLNodes based on a release or range of releases.
#
class IDLNamespace(object):
  def __init__(self, parent):
    self._name_to_releases = {}
    self._parent = parent

  def Dump(self):
    for name in self._name_to_releases:
      InfoOut.Log('NAME=%s' % name)
      for cver in self._name_to_releases[name].GetReleases():
        InfoOut.Log('  %s' % cver)
      InfoOut.Log('')

  def FindRelease(self, name, release):
    verlist = self._name_to_releases.get(name, None)
    if verlist == None:
      if self._parent:
        return self._parent.FindRelease(name, release)
      else:
        return None
    return verlist.FindRelease(release)

  def FindRange(self, name, rmin, rmax):
    verlist = self._name_to_releases.get(name, None)
    if verlist == None:
      if self._parent:
        return self._parent.FindRange(name, rmin, rmax)
      else:
        return []
    return verlist.FindRange(rmin, rmax)

  def FindList(self, name):
    verlist = self._name_to_releases.get(name, None)
    if verlist == None:
      if self._parent:
        return self._parent.FindList(name)
    return verlist

  def AddNode(self, node):
    name = node.GetName()
    verlist = self._name_to_releases.setdefault(name,IDLReleaseList())
    if GetOption('namespace_debug'):
      print("Adding to namespace: %s" % node)
    return verlist.AddNode(node)


#
# Testing Code
#

#
# MockNode
#
# Mocks the IDLNode to support error, warning handling, and string functions.
#
class MockNode(IDLRelease):
  def __init__(self, name, rmin, rmax):
    self.name = name
    self.rmin = rmin
    self.rmax = rmax
    self.errors = []
    self.warns = []
    self.properties = {
        'NAME': name,
        'release': rmin,
        'deprecate' : rmax
        }

  def __str__(self):
    return '%s (%s : %s)' % (self.name, self.rmin, self.rmax)

  def GetName(self):
    return self.name

  def Error(self, msg):
    if GetOption('release_debug'):
      print('Error: %s' % msg)
    self.errors.append(msg)

  def Warn(self, msg):
    if GetOption('release_debug'):
      print('Warn: %s' % msg)
    self.warns.append(msg)

  def GetProperty(self, name):
    return self.properties.get(name, None)

errors = 0
#
# DumpFailure
#
# Dumps all the information relevant  to an add failure.
def DumpFailure(namespace, node, msg):
  global errors
  print('\n******************************')
  print('Failure: %s %s' % (node, msg))
  for warn in node.warns:
    print('  WARN: %s' % warn)
  for err in node.errors:
    print('  ERROR: %s' % err)
  print('\n')
  namespace.Dump()
  print('******************************\n')
  errors += 1

# Add expecting no errors or warnings
def AddOkay(namespace, node):
  okay = namespace.AddNode(node)
  if not okay or node.errors or node.warns:
    DumpFailure(namespace, node, 'Expected success')

# Add expecting a specific warning
def AddWarn(namespace, node, msg):
  okay = namespace.AddNode(node)
  if not okay or node.errors or not node.warns:
    DumpFailure(namespace, node, 'Expected warnings')
  if msg not in node.warns:
    DumpFailure(namespace, node, 'Expected warning: %s' % msg)

# Add expecting a specific error any any number of warnings
def AddError(namespace, node, msg):
  okay = namespace.AddNode(node)
  if okay or not node.errors:
    DumpFailure(namespace, node, 'Expected errors')
  if msg not in node.errors:
    DumpFailure(namespace, node, 'Expected error: %s' % msg)
    print(">>%s<<\n>>%s<<\n" % (node.errors[0], msg))

# Verify that a FindRelease call on the namespace returns the expected node.
def VerifyFindOne(namespace, name, release, node):
  global errors
  if (namespace.FindRelease(name, release) != node):
    print("Failed to find %s as release %f of %s" % (node, release, name))
    namespace.Dump()
    print("\n")
    errors += 1

# Verify that a FindRage call on the namespace returns a set of expected nodes.
def VerifyFindAll(namespace, name, rmin, rmax, nodes):
  global errors
  out = namespace.FindRange(name, rmin, rmax)
  if (out != nodes):
    print("Found [%s] instead of[%s] for releases %f to %f of %s" % (' '.join([
        str(x) for x in out
    ]), ' '.join([str(x) for x in nodes]), rmin, rmax, name))
    namespace.Dump()
    print("\n")
    errors += 1

def Main(args):
  global errors
  ParseOptions(args)

  InfoOut.SetConsole(True)

  namespace = IDLNamespace(None)

  FooXX = MockNode('foo', None, None)
  Foo1X = MockNode('foo', 1.0, None)
  Foo2X = MockNode('foo', 2.0, None)
  Foo3X = MockNode('foo', 3.0, None)

  # Verify we succeed with undeprecated adds
  AddOkay(namespace, FooXX)
  AddOkay(namespace, Foo1X)
  AddOkay(namespace, Foo3X)
  # Verify we fail to add a node between undeprecated releases
  AddError(namespace, Foo2X,
           'Overlap in releases: 3.0 vs 2.0 when adding foo (2.0 : None)')

  BarXX = MockNode('bar', None, None)
  Bar12 = MockNode('bar', 1.0, 2.0)
  Bar23 = MockNode('bar', 2.0, 3.0)
  Bar34 = MockNode('bar', 3.0, 4.0)


  # Verify we succeed with fully qualified releases
  namespace = IDLNamespace(namespace)
  AddOkay(namespace, BarXX)
  AddOkay(namespace, Bar12)
  # Verify we warn when detecting a gap
  AddWarn(namespace, Bar34, 'Gap in release numbers.')
  # Verify we fail when inserting into this gap
  # (NOTE: while this could be legal, it is sloppy so we disallow it)
  AddError(namespace, Bar23, 'Declarations out of order.')

  # Verify local namespace
  VerifyFindOne(namespace, 'bar', 0.0, BarXX)
  VerifyFindAll(namespace, 'bar', 0.5, 1.5, [BarXX, Bar12])

  # Verify the correct release of the object is found recursively
  VerifyFindOne(namespace, 'foo', 0.0, FooXX)
  VerifyFindOne(namespace, 'foo', 0.5, FooXX)
  VerifyFindOne(namespace, 'foo', 1.0, Foo1X)
  VerifyFindOne(namespace, 'foo', 1.5, Foo1X)
  VerifyFindOne(namespace, 'foo', 3.0, Foo3X)
  VerifyFindOne(namespace, 'foo', 100.0, Foo3X)

  # Verify the correct range of objects is found
  VerifyFindAll(namespace, 'foo', 0.0, 1.0, [FooXX])
  VerifyFindAll(namespace, 'foo', 0.5, 1.0, [FooXX])
  VerifyFindAll(namespace, 'foo', 1.0, 1.1, [Foo1X])
  VerifyFindAll(namespace, 'foo', 0.5, 1.5, [FooXX, Foo1X])
  VerifyFindAll(namespace, 'foo', 0.0, 3.0, [FooXX, Foo1X])
  VerifyFindAll(namespace, 'foo', 3.0, 100.0, [Foo3X])

  FooBar = MockNode('foobar', 1.0, 2.0)
  namespace = IDLNamespace(namespace)
  AddOkay(namespace, FooBar)

  if errors:
    print('Test failed with %d errors.' % errors)
  else:
    print('Passed.')
  return errors


if __name__ == '__main__':
  sys.exit(Main(sys.argv[1:]))
