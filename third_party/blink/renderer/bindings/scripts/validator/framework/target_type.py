# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


def _get_target_path(target):
    target_path = []
    visited_objects = set()
    while True:
        if target in visited_objects:
            break
        visited_objects.add(target)
        if hasattr(target, "identifier"):
            target_path.append(target.identifier)
        if not hasattr(target, "owner"):
            break
        target = target.owner
    target_path_str = ".".join(reversed(target_path))
    if not target_path_str:
        return "NO_IDENTIFIER_FOUND"
    return target_path_str


def _get_debug_info_list(target):
    debug_infos = []
    visited_objects = set()
    visited_filepaths = set()
    while True:
        if target in visited_objects:
            break
        visited_objects.add(target)
        if hasattr(target, "debug_info") and (
                target.debug_info.location.filepath not in visited_filepaths):
            debug_infos.append(target.debug_info)
            visited_filepaths.add(target.debug_info.location.filepath)
        if not hasattr(target, "owner"):
            break
        target = target.owner
    return debug_infos


class TargetType(object):
    def __init__(self,
                 name,
                 get_target_objects,
                 get_target_path=_get_target_path,
                 get_debug_info_list=_get_debug_info_list):
        """
        Args:
          name:
            A string of the name of the target.
          get_target_objects:
            A function that takes a TargetStore and returns a list of the
            target objects.
          get_target_path:
            A function that takes a target object and returns a string
            made by connecting identifiers of IDL fragments with a period.
          get_debug_info_list:
            A function that takes a target object and returns
            a list of DebugInfo which is related to the target object.

        `target_store` calls `get_target_objects` to get and cache the
        target objects, so `get_target_objects` won't be called twice on
        the same `target_store`.
        """
        self._name = name
        self._get_target_objects = get_target_objects
        self._get_target_path = get_target_path
        self._get_debug_info_list = get_debug_info_list

    @property
    def name(self):
        return self._name

    def get_target_objects(self, target_store):
        """
        Returns a list of target objects from `target_store`.
        ('target' means this class.)
        """
        return self._get_target_objects(target_store)

    def get_target_path(self, target):
        return self._get_target_path(target)

    def get_debug_info_list(self, target):
        return self._get_debug_info_list(target)
