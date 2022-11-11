#!/usr/bin/env python
# Copyright (C) 2013 Intel Corporation. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

from core.css import css_properties
from collections import defaultdict
import json5_generator
from name_utilities import enum_key_for_css_property
import template_expander


def collect_runtime_flags(properties):
    """Returns a list of unique runtime flags used by the properties"""
    flags = {p.runtime_flag for p in properties if p.runtime_flag}
    return sorted(flags)


class Expansion(object):
    """A specific (longhand) expansion of a shorthand.

    A shorthand may have multiple expansions, because some of the longhands
    might be behind runtime flags.

    The enabled_mask represents which flags are enabled/disabled for this
    specific expansion. For example, if flags contains three elements,
    and enabled_mask is 0b100, then flags[0] is disabled, flags[1] is disabled,
    and flags[2] is enabled. This information is used to produce the correct
    list of longhands corresponding to the runtime flags that are enabled/
    disabled.
    """

    def __init__(self, longhands, flags, enabled_mask):
        super(Expansion, self).__init__()
        self._longhands = longhands
        self._flags = flags
        self._enabled_mask = enabled_mask

    def is_enabled(self, flag):
        return (1 << self._flags.index(flag)) & self._enabled_mask

    @property
    def is_empty(self):
        return len(self.enabled_longhands) == 0

    @property
    def enabled_longhands(self):
        include = lambda longhand: not longhand.runtime_flag or self.is_enabled(
            longhand.runtime_flag)
        return list(filter(include, self._longhands))

    @property
    def index(self):
        return self._enabled_mask

    @property
    def flags(self):
        return [
            dict(name=flag, enabled=self.is_enabled(flag))
            for flag in self._flags
        ]


def create_expansions(longhands):
    flags = collect_runtime_flags(longhands)
    expansions = list(
        map(lambda mask: Expansion(longhands, flags, mask),
            range(1 << len(flags))))
    assert len(expansions) > 0
    # We generate 2^N expansions for N flags, so enforce some limit.
    assert len(flags) <= 4, 'Too many runtime flags for a single shorthand'
    return expansions


class StylePropertyShorthandWriter(json5_generator.Writer):
    class_name = 'StylePropertyShorthand'
    _FILE_BASENAME = 'style_property_shorthand'

    def __init__(self, json5_file_paths, output_dir):
        super(StylePropertyShorthandWriter, self).__init__([], output_dir)
        self._input_files = json5_file_paths
        self._outputs = {
            (self._FILE_BASENAME + '.cc'):
            self.generate_style_property_shorthand_cpp,
            (self._FILE_BASENAME + '.h'):
            self.generate_style_property_shorthand_h
        }

        json5_properties = css_properties.CSSProperties(json5_file_paths)
        self._shorthands = json5_properties.shorthands

        self._longhand_dictionary = defaultdict(list)
        for property_ in json5_properties.shorthands:
            longhand_enum_keys = list(
                map(enum_key_for_css_property, property_.longhands))

            longhands = list(
                map(lambda name: json5_properties.properties_by_name[name],
                    property_.longhands))
            property_.expansions = create_expansions(longhands)
            for longhand_enum_key in longhand_enum_keys:
                self._longhand_dictionary[longhand_enum_key].append(property_)

        for longhands in self._longhand_dictionary.values():
            # Sort first by number of longhands in decreasing order, then
            # alphabetically
            longhands.sort(key=lambda property_: (-len(property_.longhands),
                                                  property_.name.original))

    @template_expander.use_jinja(
        'core/css/templates/style_property_shorthand.cc.tmpl')
    def generate_style_property_shorthand_cpp(self):
        return {
            'input_files': self._input_files,
            'properties': self._shorthands,
            'longhands_dictionary': self._longhand_dictionary,
        }

    @template_expander.use_jinja(
        'core/css/templates/style_property_shorthand.h.tmpl')
    def generate_style_property_shorthand_h(self):
        return {
            'input_files':
            self._input_files,
            'properties':
            self._shorthands,
            'header_guard':
            self.make_header_guard(self._relative_output_dir +
                                   self._FILE_BASENAME + '.h')
        }


if __name__ == '__main__':
    json5_generator.Maker(StylePropertyShorthandWriter).main()
