# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os.path
import posixpath

import web_idl

from . import name_style
from .blink_v8_bridge import blink_class_name


class PathManager(object):
    """
    Provides a variety of paths such as Blink headers and output files.  Unless
    explicitly specified, returned paths are relative to the project's root
    directory or the root directory of generated files, e.g.
    "third_party/blink/renderer/..."

    Relative paths are represented in POSIX style so that it fits nicely in
    generated code, e.g. #include "third_party/blink/renderer/...", while
    absolute paths are represented in a platform-specific style so that it works
    well with a platform-specific notion, e.g. a drive letter in Windows path
    such as "C:\\chromium\\src\\...".

    About output files, there are two cases.
    - cross-components case:
        APIs are generated in 'core' and implementations are generated in
        'modules'.
    - single component case:
        Everything is generated in a single component.
    """

    _REQUIRE_INIT_MESSAGE = ("PathManager.init must be called in advance.")
    _is_initialized = False

    @classmethod
    def init(cls, root_src_dir, root_gen_dir, component_reldirs):
        """
        Args:
            root_src_dir: Project's root directory, which corresponds to "//"
                in GN.
            root_gen_dir: Root directory of generated files, which corresponds
                to "//out/Default/gen" in GN.
            component_reldirs: Pairs of component and output directory relative
                to |root_gen_dir|.
        """
        assert not cls._is_initialized
        assert isinstance(root_src_dir, str)
        assert isinstance(root_gen_dir, str)
        assert isinstance(component_reldirs, dict)

        cls._blink_path_prefix = posixpath.sep + posixpath.join(
            "third_party", "blink", "renderer", "")

        cls._root_src_dir = os.path.abspath(root_src_dir)
        cls._root_gen_dir = os.path.abspath(root_gen_dir)
        cls._component_reldirs = {
            component: posixpath.normpath(rel_dir)
            for component, rel_dir in component_reldirs.items()
        }
        cls._is_initialized = True

    @classmethod
    def component_path(cls, component, filepath):
        """
        Returns the relative path to |filepath| in |component|'s directory.
        """
        assert cls._is_initialized, cls._REQUIRE_INIT_MESSAGE
        return posixpath.join(cls._component_reldirs[component], filepath)

    @classmethod
    def gen_path_to(cls, path):
        """
        Returns the absolute path of |path| that must be relative to the root
        directory of generated files.
        """
        assert cls._is_initialized, cls._REQUIRE_INIT_MESSAGE
        return os.path.abspath(os.path.join(cls._root_gen_dir, path))

    @classmethod
    def src_path_to(cls, path):
        """
        Returns the absolute path of |path| that must be relative to the
        project root directory.
        """
        assert cls._is_initialized, cls._REQUIRE_INIT_MESSAGE
        return os.path.abspath(os.path.join(cls._root_src_dir, path))

    def __init__(self, idl_definition):
        assert self._is_initialized, self._REQUIRE_INIT_MESSAGE

        components = sorted(idl_definition.components)  # "core" < "modules"

        if len(components) == 0:
            assert isinstance(idl_definition,
                              (web_idl.ObservableArray, web_idl.Union))
            # Compound types of built-in types, e.g. ObservableArray<long> and
            # (double or DOMString), do not have a component.
            self._is_cross_components = False
            default_component = web_idl.Component("core")
            self._api_component = default_component
            self._impl_component = default_component
        elif len(components) == 1:
            component = components[0]
            self._is_cross_components = False
            self._api_component = component
            self._impl_component = component
        elif len(components) == 2:
            assert components[0] == "core"
            assert components[1] == "modules"
            # ObservableArray and union types do not support cross-component
            # code generation because clients of IDL observable array and IDL
            # union types must be on an upper or same layer to any of element
            # type and union members.
            if isinstance(idl_definition,
                          (web_idl.ObservableArray, web_idl.Union)):
                self._is_cross_components = False
                self._api_component = components[1]
                self._impl_component = components[1]
            else:
                self._is_cross_components = True
                self._api_component = components[0]
                self._impl_component = components[1]
        else:
            assert False

        self._api_dir = self._component_reldirs[self._api_component]
        self._impl_dir = self._component_reldirs[self._impl_component]
        if isinstance(idl_definition, web_idl.ObservableArray):
            self._api_basename = name_style.file("v8",
                                                 idl_definition.identifier)
            self._impl_basename = name_style.file("v8",
                                                  idl_definition.identifier)
            self._blink_dir = None
            self._blink_basename = None
        elif isinstance(idl_definition, web_idl.Union):
            # In case of IDL unions, underscore is used as a separator of union
            # members, so we don't want any underscore inside a union member.
            # For example, (Foo or Bar or Baz) and (FooBar or Baz) are defined
            # in v8_union_foo_bar_baz.ext and v8_union_foobar_baz.ext
            # respectively.
            #
            # Avoid name_style.file not to make "Int32Array" into
            # "int_32_array".
            filename = "v8_union_{}".format("_".join(
                idl_definition.member_tokens)).lower()
            self._api_basename = filename
            self._impl_basename = filename
            self._blink_dir = None
            self._blink_basename = None
        else:
            self._api_basename = name_style.file("v8",
                                                 idl_definition.identifier)
            self._impl_basename = name_style.file("v8",
                                                  idl_definition.identifier)
            idl_path = idl_definition.debug_info.location.filepath
            self._blink_dir = posixpath.dirname(idl_path)
            self._blink_basename = name_style.file(
                blink_class_name(idl_definition))

    @property
    def is_cross_components(self):
        return self._is_cross_components

    @property
    def api_component(self):
        return self._api_component

    @property
    def api_dir(self):
        return self._api_dir

    def api_path(self, filename=None, ext=None):
        return self._join(
            dirpath=self.api_dir,
            filename=(filename or self._api_basename),
            ext=ext)

    @property
    def impl_component(self):
        return self._impl_component

    @property
    def impl_dir(self):
        return self._impl_dir

    def impl_path(self, filename=None, ext=None):
        return self._join(
            dirpath=self.impl_dir,
            filename=(filename or self._impl_basename),
            ext=ext)

    @property
    def blink_dir(self):
        return self._blink_dir

    def blink_path(self, filename=None, ext=None):
        return self._join(
            dirpath=self.blink_dir,
            filename=(filename or self._blink_basename),
            ext=ext)

    @staticmethod
    def _join(dirpath, filename, ext=None):
        if ext is not None:
            filename = posixpath.extsep.join([filename, ext])
        return posixpath.join(dirpath, filename)
