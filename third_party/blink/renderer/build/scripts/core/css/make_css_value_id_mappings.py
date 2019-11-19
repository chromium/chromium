#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from blinkbuild.name_style_converter import NameStyleConverter
import json5_generator
import template_expander
import make_style_builder
import keyword_utils
from name_utilities import enum_key_for_css_keyword


def _find_continuous_segment(numbers):
    """Find the longest continuous segment in a list of numbers.
    For example:
        input:
                1, 2, 3, 4, 5, 6
               22,70,23,24,25,26
        output:
            number_list_sorted:
                1, 3, 4, 5, 6, 2
               22,23,24,25,26,70
            segments:
                0, 1, 5, 6
            which means there are 3 segment with start and end indices
            on the number_list_sorted to be: (0, 1), (1, 5), (5, 6)

    Args:
        numbers: List of pairs of number

    Returns:
        segment: a list containing the indices of the segment start point
            and end point.
        number_list: sorted by the first element version of the input.

    """
    segments = [0]
    number_list_sorted = sorted(numbers, key=lambda elem: elem[0])
    for i in range(len(number_list_sorted) - 1):
        # continuous segment is a segment which the number in pair is 1 unit
        # more than the previous pair
        if (number_list_sorted[i + 1][0] - number_list_sorted[i][0] != 1
                or number_list_sorted[i + 1][1] - number_list_sorted[i][1] != 1):
            segments.append(i + 1)
    segments.append(len(number_list_sorted))
    return segments, number_list_sorted


def _find_largest_segment(segments):
    """Find the largest segment given a list of start and end
    indices of segments

    Args:
        segments: a list of start and end indices

    Returns:
        longest_segment: the start and end indices of the longest segment

    """
    segment_list = zip(segments[:-1], segments[1:])
    return max(segment_list, key=lambda x: x[1] - x[0])


def _find_enum_longest_continuous_segment(property_, name_to_position_dictionary):
    """Find the longest continuous segment in the list of keywords
    Finding the continuous segment will allows us to do the subtraction
    between keywords so that the distance between 2 keywords in this
    enum is equal to the distance of corresponding keywords in another
    enum.

    Step 1:
        Convert keyword enums into number.
        Sort and find all continuous segment in the list of enums.

    Step 2:
        Get the longest segment.

    Step 3:
        Compose a list of keyword enums and their respective numbers
        in the sorted order.

    Step 4:
        Build the switch case statements of other enums not in the
        segment. Enums in the segment will be computed in default clause.
    """
    property_enum_order = range(len(property_['keywords']))
    css_enum_order = [name_to_position_dictionary[x] for x in property_['keywords']]
    enum_pair_list = zip(css_enum_order, property_enum_order)
    enum_segment, enum_pair_list = _find_continuous_segment(enum_pair_list)
    longest_segment = _find_largest_segment(enum_segment)

    enum_tuple_list = []
    for x in enum_pair_list:
        keyword = NameStyleConverter(property_['keywords'][x[1]])
        enum_tuple_list.append((enum_key_for_css_keyword(keyword), x[1], x[0]))
    return enum_tuple_list, enum_segment, longest_segment


class CSSValueIDMappingsWriter(make_style_builder.StyleBuilderWriter):
    def __init__(self, json5_file_paths, output_dir):
        super(CSSValueIDMappingsWriter, self).__init__(json5_file_paths, output_dir)
        self._outputs = {
            'css_value_id_mappings_generated.h': self.generate_css_value_mappings,
        }
        self.css_values_dictionary_file = json5_file_paths[2]
        css_properties = self.css_properties.longhands
        # We sort the enum values based on each value's position in
        # the keywords as listed in css_properties.json5. This will ensure that if there is a continuous
        # segment in css_properties.json5 matching the segment in this enum then
        # the generated enum will have the same order and continuity as
        # css_properties.json5 and we can get the longest continuous segment.
        # Thereby reduce the switch case statement to the minimum.
        css_properties = keyword_utils.sort_keyword_properties_by_canonical_order(
            css_properties, json5_file_paths[2], self.default_parameters)

    @template_expander.use_jinja('core/css/templates/css_value_id_mappings_generated.h.tmpl')
    def generate_css_value_mappings(self):
        mappings = {}
        include_paths = set()
        css_values_dictionary = json5_generator.Json5File.load_from_files(
            [self.css_values_dictionary_file],
            default_parameters=self.default_parameters
        ).name_dictionaries
        name_to_position_dictionary = dict(zip([x['name'].original for x in css_values_dictionary],
                                               range(len(css_values_dictionary))))

        for property_ in self.css_properties.properties_including_aliases:
            include_paths.update(property_['include_paths'])
            if property_['field_template'] == 'multi_keyword':
                mappings[property_['type_name']] = {
                    'default_value': property_['default_value'],
                    'mapping': [enum_key_for_css_keyword(k)
                                for k in property_['keywords']],
                }
            elif property_['field_template'] == 'keyword':
                enum_pair_list, enum_segment, p_segment = _find_enum_longest_continuous_segment(
                    property_, name_to_position_dictionary)
                mappings[property_['type_name']] = {
                    'default_value': property_['default_value'],
                    'mapping': enum_pair_list,
                    'segment': enum_segment,
                    'longest_segment_length': p_segment[1] - p_segment[0],
                    'start_segment': enum_pair_list[p_segment[0]],
                    'end_segment': enum_pair_list[p_segment[1] - 1],
                }

        return {
            'include_paths': list(sorted(include_paths)),
            'input_files': self._input_files,
            'mappings': mappings,
        }

if __name__ == '__main__':
    json5_generator.Maker(CSSValueIDMappingsWriter).main()
