# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json5_generator


def sort_keyword_properties_by_canonical_order(
        css_properties, css_value_keywords_file, json5_file_parameters):
    """Sort all keyword CSS properties by the order of the keyword in
       css_value_keywords.json5

    Args:
        css_properties: css_properties excluding extra fields.
        css_value_keywords_file: file containing all css keywords.
        json5_file_parameters: current context json5 parameters.

    Returns:
        New css_properties object with sorted keywords.
    """
    css_values_dictionary = json5_generator.Json5File.load_from_files(
        [css_value_keywords_file],
        default_parameters=json5_file_parameters).name_dictionaries
    css_values_dictionary = [x['name'].original for x in css_values_dictionary]
    name_to_position_dictionary = dict(
        zip(css_values_dictionary, range(len(css_values_dictionary))))
    for css_property in css_properties:
        if css_property.field_template == 'keyword' and len(
                css_property.include_paths) == 0:
            css_property.keywords = sorted(
                css_property.keywords,
                key=lambda x: name_to_position_dictionary[x])

    return css_properties
