#!/usr/bin/env python

import subprocess
from name_utilities import enum_key_for_css_keyword
import json5_generator

import template_expander
import gperf


class CSSValueKeywordsWriter(json5_generator.Writer):
    _FILE_BASENAME = 'css_value_keywords'

    def __init__(self, file_paths, output_dir):
        json5_generator.Writer.__init__(self, file_paths, output_dir)
        self._outputs = {
            (self._FILE_BASENAME + '.h'): self.generate_header,
            (self._FILE_BASENAME + '.cc'): self.generate_implementation,
        }

        self._value_keywords = self.json5_file.name_dictionaries
        first_keyword_id = 1
        for offset, keyword in enumerate(self._value_keywords):
            keyword['lower_name'] = keyword['name'].original.lower()
            keyword['enum_name'] = enum_key_for_css_keyword(keyword['name'])
            keyword['enum_value'] = first_keyword_id + offset
            if keyword['name'].original.startswith('-internal-'):
                assert keyword['mode'] is None, 'Can\'t specify mode for ' \
                    'value keywords with the prefix "-internal-".'
                keyword['mode'] = 'UASheet'
            else:
                assert keyword['mode'] != 'UASheet', 'UASheet mode only ' \
                    'value keywords should have the prefix "-internal-".'
        self._keyword_count = len(self._value_keywords) + first_keyword_id

    @template_expander.use_jinja(
        'core/css/templates/css_value_keywords.h.tmpl')
    def generate_header(self):
        return {
            'value_keywords':
            self._value_keywords,
            'value_keywords_count':
            self._keyword_count,
            'max_value_keyword_length':
            max(
                len(keyword['name'].original)
                for keyword in self._value_keywords),
            'header_guard':
            self.make_header_guard(self._relative_output_dir +
                                   self._FILE_BASENAME + '.h')
        }

    def _value_keywords_with_mode(self, mode):
        return [
            keyword for keyword in self._value_keywords
            if keyword['mode'] == mode
        ]

    @gperf.use_jinja_gperf_template(
        'core/css/templates/css_value_keywords.cc.tmpl',
        ['-Q', 'CSSValueStringPool'])
    def generate_implementation(self):
        keyword_offsets = []
        current_offset = 0
        for keyword in self._value_keywords:
            keyword_offsets.append(current_offset)
            current_offset += len(keyword["name"].original) + 1

        return {
            'value_keywords':
            self._value_keywords,
            'value_keyword_offsets':
            keyword_offsets,
            'ua_sheet_mode_values_keywords':
            self._value_keywords_with_mode('UASheet'),
            'quirks_mode_or_ua_sheet_mode_values_keywords':
            self._value_keywords_with_mode('QuirksOrUASheet'),
            'gperf_path':
            self.gperf_path,
        }


if __name__ == "__main__":
    json5_generator.Maker(CSSValueKeywordsWriter).main()
