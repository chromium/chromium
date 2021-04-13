# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


class TargetType(object):
    def __init__(self,
                 name,
                 get_target_objects,
                 get_debug_info=lambda x: x.debug_info):
        """
        Args:
          name:
            A string of the name of the target.
          get_target_objects:
            A function that takes a TargetStore and returns a list of the
            target objects.
          get_debug_info:
            A function to get the DebugInfo from a target object.

        `target_store` calls `get_target_objects` to get and cache the
        target objects, so `get_target_objects` won't be called twice on
        the same `target_store`.
        """
        self._name = name
        self._get_target_objects = get_target_objects
        self._get_debug_info = get_debug_info

    @property
    def name(self):
        return self._name

    def get_target_objects(self, target_store):
        """
        Returns a list of target objects from `target_store`.
        ('target' means this class.)
        """
        return self._get_target_objects(target_store)

    def get_debug_info(self, target):
        return self._get_debug_info(target)
