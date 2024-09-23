# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from core.css import css_properties
import json5_generator
import template_expander


class CSSProtoWriter(json5_generator.Writer):
    def __init__(self, json5_file_paths, output_dir):
        super(CSSProtoWriter, self).__init__(None, output_dir)
        assert len(json5_file_paths) == 4, \
            "Needs css_properties.json5, computed_style_field_aliases.json5," \
            " runtime_enabled_features.json5, and css_value_keywords.json5."
        self._input_files = json5_file_paths
        self._outputs = {
            'css.proto': self.generate_proto,
            'css_proto_converter_generated.h': self.generate_cc,
        }
        self._all_properties = css_properties.CSSProperties(
            json5_file_paths[:3]).properties_including_aliases
        self._keywords = sorted([
            keyword['name']
            for keyword in json5_generator.Json5File.load_from_files(
                [json5_file_paths[3]]).name_dictionaries
        ])

    @template_expander.use_jinja(
        'core/css/parser/templates/css_proto_converter_generated.h.tmpl')
    def generate_cc(self):
        return {
            'input_files':
            self._input_files,
            'property_names':
            '\n'.join(f'  "{property.name.original}",'
                      for property in self._all_properties),
            'value_keywords':
            '\n'.join(f'  "{keyword.original}",'
                      for keyword in self._keywords),
        }

    @template_expander.use_jinja('core/css/parser/templates/css.proto.tmpl')
    def generate_proto(self):
        property_symbols = []
        for i, property in enumerate(self._all_properties):
            symbol = property.name.to_macro_case()
            if symbol == 'OVERFLOW':  # Conflicts with a system header
                symbol = 'OVERFLOW_'
            property_symbols.append('    %s = %d;' % (symbol, i + 1))
        property_symbols.append('    INVALID_PROPERTY = %d;' %
                                (len(self._all_properties) + 1))

        keyword_symbols = []
        for i, keyword in enumerate(self._keywords):
            symbol = keyword.to_macro_case()
            if keyword.original == '-infinity':
                # Conflicts with a system header
                symbol = 'NEGATIVE_INFINITY'
            elif keyword.original == 'infinity':
                # Conflicts with a system header
                symbol = 'POSITIVE_INFINITY'
            elif keyword.original == 'unset':
                # Conflicts with PropertyAndValue::Prio::UNSET.
                symbol = 'UNSET_'
            elif keyword.original == 'nan':
                # Conflicts with a system header
                symbol = 'NOT_A_NUMBER'
            elif keyword.original == 'unicode':
                # Conflicts with UNICODE macro.
                symbol = 'UNICODE_'
            keyword_symbols.append('    %s = %d;' % (symbol, i + 1))
        keyword_symbols.append('    INVALID_VALUE = %d;' %
                               (len(self._keywords) + 1))

        return {
            'property_proto_enums': '\n'.join(property_symbols),
            'value_proto_enums': '\n'.join(keyword_symbols),
        }


if __name__ == "__main__":
    json5_generator.Maker(CSSProtoWriter).main()
