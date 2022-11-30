# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import web_idl


class TargetStore(object):
    """
    Gets the target objects from a web_idl.Database and caches them.
    """

    def __init__(self, web_idl_database):
        assert isinstance(web_idl_database, web_idl.Database)
        self._web_idl_database = web_idl_database
        """
        _cache = {
            FUNCTION_LIKES: [function_like1, function_like2, ... ],
            INTERFACES: [interface1, interface2, ... ],
            ...
        }
        """
        self._cache = {}

    @property
    def web_idl_database(self):
        return self._web_idl_database

    def get(self, target_type):
        """
        Returns a list of target objects.

        Example: get(target.INTERFACES) -> [interface1, interface2, ... ]
        """
        if target_type not in self._cache:
            self._cache[target_type] = target_type.get_target_objects(self)
        return self._cache[target_type]
