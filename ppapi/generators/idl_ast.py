# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Nodes for PPAPI IDL AST."""

from __future__ import print_function

from idl_namespace import IDLNamespace
from idl_node import IDLNode
from idl_option import GetOption
from idl_visitor import IDLVisitor
from idl_release import IDLReleaseMap

#
# IDLLabelResolver
#
# A specialized visitor which traverses the AST, building a mapping of
# Release names to Versions numbers and calculating a min version.
# The mapping is applied to the File nodes within the AST.
#
class IDLLabelResolver(IDLVisitor):
  def Depart(self, node, ignore, childdata):
    # Build list of Release=Version
    if node.IsA('LabelItem'):
      channel = node.GetProperty('channel')
      if not channel:
        channel = 'stable'
      return (node.GetName(), node.GetProperty('VALUE'), channel)

    # On completion of the Label, apply to the parent File if the
    # name of the label matches the generation label.
    if node.IsA('Label') and node.GetName() == GetOption('label'):
      try:
        node.parent.release_map = IDLReleaseMap(childdata)
      except Exception as err:
        node.Error('Unable to build release map: %s' % str(err))

    # For File objects, set the minimum version
    if node.IsA('File'):
      file_min, _ = node.release_map.GetReleaseRange()
      node.SetMin(file_min)

    return None


#
# IDLNamespaceVersionResolver
#
# A specialized visitor which traverses the AST, building a namespace tree
# as it goes.  The namespace tree is mapping from a name to a version list.
# Labels must already be resolved to use.
#
class IDLNamespaceVersionResolver(IDLVisitor):
  NamespaceSet = set(['AST', 'Callspec', 'Interface', 'Member', 'Struct'])
  #
  # When we arrive at a node we must assign it a namespace and if the
  # node is named, then place it in the appropriate namespace.
  #
  def Arrive(self, node, parent_namespace):
    # If we are a File, grab the Min version and replease mapping
    if node.IsA('File'):
      self.rmin = node.GetMinMax()[0]
      self.release_map = node.release_map

    # Set the min version on any non Label within the File
    if not node.IsA('AST', 'File', 'Label', 'LabelItem'):
      my_min, _ = node.GetMinMax()
      if not my_min:
        node.SetMin(self.rmin)

    # If this object is not a namespace aware object, use the parent's one
    if node.cls not in self.NamespaceSet:
      node.namespace = parent_namespace
    else:
      # otherwise create one.
      node.namespace = IDLNamespace(parent_namespace)

    # If this node is named, place it in its parent's namespace
    if parent_namespace and node.cls in IDLNode.NamedSet:
      # Set version min and max based on properties
      if self.release_map:
        vmin = node.GetProperty('dev_version')
        if vmin == None:
          vmin = node.GetProperty('version')
        vmax = node.GetProperty('deprecate')
        # If no min is available, the use the parent File's min
        if vmin == None:
          rmin = self.rmin
        else:
          rmin = self.release_map.GetRelease(vmin)
        rmax = self.release_map.GetRelease(vmax)
        node.SetReleaseRange(rmin, rmax)
      parent_namespace.AddNode(node)

    # Pass this namespace to each child in case they inherit it
    return node.namespace


#
# IDLFileTypeRessolver
#
# A specialized visitor which traverses the AST and sets a FILE property
# on all file nodes.  In addition, searches the namespace resolving all
# type references.  The namespace tree must already have been populated
# before this visitor is used.
#
class IDLFileTypeResolver(IDLVisitor):
  def VisitFilter(self, node, data):
    return not node.IsA('Comment', 'Copyright')

  def Arrive(self, node, filenode):
    # Track the file node to update errors
    if node.IsA('File'):
      node.SetProperty('FILE', node)
      filenode = node

    if not node.IsA('AST'):
      file_min, _ = filenode.release_map.GetReleaseRange()
      if not file_min:
        print('Resetting min on %s to %s' % (node, file_min))
        node.SetMinRange(file_min)

    # If this node has a TYPEREF, resolve it to a version list
    typeref = node.GetPropertyLocal('TYPEREF')
    if typeref:
      node.typelist = node.parent.namespace.FindList(typeref)
      if not node.typelist:
        node.Error('Could not resolve %s.' % typeref)
    else:
      node.typelist = None
    return filenode

#
# IDLReleaseResolver
#
# A specialized visitor which will traverse the AST, and generate a mapping
# from any release to the first release in which that version of the object
# was generated.  Types must already be resolved to use.
#
class IDLReleaseResolver(IDLVisitor):
  def Arrive(self, node, releases):
    node.BuildReleaseMap(releases)
    return releases


#
# IDLAst
#
# A specialized version of the IDLNode for containing the whole of the
# AST.  Construction of the AST object will cause resolution of the
# tree including versions, types, etc...  Errors counts will be collected
# both per file, and on the AST itself.
#
class IDLAst(IDLNode):
  def __init__(self, children):
    IDLNode.__init__(self, 'AST', 'BuiltIn', 1, 0, children)
    self.Resolve()

  def Resolve(self):
    # Set the appropriate Release=Version mapping for each File
    IDLLabelResolver().Visit(self, None)

    # Generate the Namesapce Tree
    self.namespace = IDLNamespace(None)
    IDLNamespaceVersionResolver().Visit(self, self.namespace)

    # Using the namespace, resolve type references
    IDLFileTypeResolver().Visit(self, None)

    # Build an ordered list of all releases
    releases = set()
    for filenode in self.GetListOf('File'):
      releases |= set(filenode.release_map.GetReleases())

    # Generate a per node list of releases and release mapping
    IDLReleaseResolver().Visit(self, sorted(releases))

    for filenode in self.GetListOf('File'):
      errors = filenode.GetProperty('ERRORS')
      if errors:
        self.errors += errors
