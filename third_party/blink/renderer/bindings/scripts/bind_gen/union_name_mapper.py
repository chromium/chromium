# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import web_idl
from . import name_style


class UnionNameMapper(object):
    """
    Manages overrides for generated union class and file names, which is
    occasionally needed for unions with large number of variants.
    See union_name_map.conf for documentation on file format.
    """
    _instance = None

    class Entry:

        def __init__(self, alias, class_name, file_name):
            self.class_name = class_name or ("V8" + alias)
            self.file_name = file_name or ("v8_union_" +
                                           name_style.raw.snake_case(alias))

    @classmethod
    def init(cls, filename, database):
        assert not cls._instance
        config_text = ""
        with open(filename) as config:
            config_text = config.read()
        cls._instance = cls.init_with_config_text(config_text, database)
        return cls._instance

    @classmethod
    def instance(cls):
        return cls._instance

    @classmethod
    def init_with_config_text(cls, config_text, database):
        assert isinstance(config_text, str)
        assert isinstance(database, web_idl.Database)
        map = dict()

        # This is only called from the config being parsed.
        def MapUnionName(idl_name, class_name=None, file_name=None):
            assert (isinstance(idl_name, str))
            assert (class_name is None or isinstance(class_name, str))
            assert (file_name is None or isinstance(file_name, str))
            idl_typedef = database.find(idl_name)
            if not idl_typedef:
                raise KeyError(idl_typedef)
            idl_type = idl_typedef.idl_type.unwrap(typedef=True)
            if not idl_type.is_union:
                raise TypeError(idl_typedef + " does not refer to a union")
            union = idl_type.union_definition_object
            assert union
            map[id(union)] = UnionNameMapper.Entry(idl_name, class_name,
                                                   file_name)

        globals = {
            '__builtins__': None,
            'MapUnionName': MapUnionName,
        }
        exec(config_text, globals)
        return UnionNameMapper(map)

    def __init__(self, map):
        assert isinstance(map, dict)
        self._map = map

    def class_name(self, idl_union):
        """
        Returns class name override for a union if present, otherwise None.
        """
        assert isinstance(idl_union, web_idl.Union)
        entry = self._map.get(id(idl_union))
        return entry.class_name if entry else None

    def file_name(self, idl_union):
        """
        Returns file name override for a union if present, otherwise None.
        """
        assert isinstance(idl_union, web_idl.Union)
        entry = self._map.get(id(idl_union))
        return entry.file_name if entry else None
