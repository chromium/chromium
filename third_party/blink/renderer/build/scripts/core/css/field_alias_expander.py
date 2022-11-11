# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json5_generator


class FieldAliasExpander(object):
    """
    A helper for expanding the "field_template" parameter in css_properties.json5

    It takes the list of aliases and expansions from the given file_path, (it
    should point to core/css/computed_style_field_aliases.json5) and uses that to
    inform which fields in a given property should be set.
    """

    def __init__(self, file_path):
        loaded_file = json5_generator.Json5File.load_from_files([file_path])
        self._field_aliases = dict([
            (alias["name"], alias) for alias in loaded_file.name_dictionaries
        ])

    def expand_field_alias(self, property_):
        """
        Does expansion based on the value of field_template of a given property.
        """
        if property_.field_template in self._field_aliases:
            alias_template = property_.field_template
            for field in self._field_aliases[alias_template]:
                if field == 'name':
                    continue
                assert hasattr(property_, field)
                setattr(property_, field,
                        self._field_aliases[alias_template][field])
