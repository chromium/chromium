# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import posixpath

import web_idl

from . import name_style
from .blink_v8_bridge import blink_class_name


class PathManager(object):
    """
    Provides a variety of paths such as Blink headers and output files.

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
    def init(cls, output_dirs):
        """
        Args:
            output_dirs: Pairs of component and output directory.
        """
        assert not cls._is_initialized
        assert isinstance(output_dirs, dict)
        cls._output_dirs = output_dirs
        cls._blink_path_prefix = posixpath.sep + posixpath.join(
            "third_party", "blink", "renderer", "")
        cls._is_initialized = True

    @classmethod
    def relpath_to_project_root(cls, path):
        index = path.find(cls._blink_path_prefix)
        if index < 0:
            assert path.startswith(cls._blink_path_prefix[1:])
            return path
        return path[index + 1:]

    def __init__(self, idl_definition):
        assert self._is_initialized, self._REQUIRE_INIT_MESSAGE

        idl_path = idl_definition.debug_info.location.filepath
        self._idl_basepath, _ = posixpath.splitext(idl_path)
        self._idl_dir, self._idl_basename = posixpath.split(self._idl_basepath)

        components = sorted(idl_definition.components)

        if len(components) == 1:
            component = components[0]
            self._is_cross_components = False
            self._api_component = component
            self._impl_component = component
        elif len(components) == 2:
            assert components[0] == "core"
            assert components[1] == "modules"
            self._is_cross_components = True
            self._api_component = "core"
            self._impl_component = "modules"
        else:
            assert False

        self._api_dir = self._output_dirs[self._api_component]
        self._impl_dir = self._output_dirs[self._impl_component]
        self._out_basename = name_style.file("v8", idl_definition.identifier)

        if isinstance(idl_definition,
                      (web_idl.CallbackFunction, web_idl.CallbackInterface)):
            self._blink_dir = self._api_dir
        else:
            self._blink_dir = self._idl_dir
        self._blink_basename = name_style.file(
            blink_class_name(idl_definition))

    @property
    def idl_dir(self):
        return self._idl_dir

    def blink_path(self, filename=None, ext=None):
        """
        Returns a path to a Blink implementation file relative to the project
        root directory, e.g. "third_party/blink/renderer/..."
        """
        return self.relpath_to_project_root(
            self._join(
                dirpath=self._blink_dir,
                filename=(filename or self._blink_basename),
                ext=ext))

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
            filename=(filename or self._out_basename),
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
            filename=(filename or self._out_basename),
            ext=ext)

    @staticmethod
    def _join(dirpath, filename, ext=None):
        if ext is not None:
            filename = posixpath.extsep.join([filename, ext])
        return posixpath.join(dirpath, filename)
