# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from . import file_io
from .composition_parts import Component


class AstGroup(object):
    """A set of Web IDL ASTs grouped by component."""

    def __init__(self, component, for_testing=False):
        assert isinstance(component, Component)
        assert isinstance(for_testing, bool)
        self._nodes = []
        self._component = component
        self._for_testing = for_testing

    def __iter__(self):
        return self._nodes.__iter__()

    @staticmethod
    def read_from_file(filepath):
        ast_group = file_io.read_pickle_file(filepath)
        assert isinstance(ast_group, AstGroup)
        return ast_group

    def write_to_file(self, filepath):
        return file_io.write_pickle_file_if_changed(filepath, self)

    def add_ast_node(self, node):
        assert node.GetClass() == 'File', (
            'Root node of an AST must be a File node, but is %s.' %
            node.GetClass())
        self._nodes.append(node)

    @property
    def component(self):
        return self._component

    @property
    def for_testing(self):
        return self._for_testing
