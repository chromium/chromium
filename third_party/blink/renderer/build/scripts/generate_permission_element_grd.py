# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import glob
import os
import re
import sys
from xml.dom.minidom import parse, parseString

_SRC_PATH = os.path.abspath(
    os.path.join(os.path.dirname(__file__), '..', '..', '..', '..', '..'))
sys.path.append(os.path.join(_SRC_PATH, 'tools', 'grit'))
from grit.extern import tclib

kGrdTemplate = '''<?xml version="1.0" encoding="utf-8"?>
<!--
This file contains all Permission element strings in all locales.
This is a generated grd file.
The script to generate the grd file is located at
third_party/blink/renderer/build/scripts/generate_permission_element_grd.py
-->
<grit base_dir="." latest_public_release="0" current_release="1"
    source_lang_id="en" enc_check="möl">
<outputs>
    <output filename="grit/permission_element_generated_strings.h" type="rc_header">
      <emit emit_type='prepend'></emit>
    </output>
    <output filename="permission_element_generated_strings.pak" type="data_package" />
</outputs>
<release seq="1" allow_pseudo="false">
    <messages fallback_to_english="true">
    </messages>
</release>
</grit>
'''

kStringMapHeaderPrefix = '''/// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_PERMISSION_ELEMENT_STRINGS_MAP_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_PERMISSION_ELEMENT_STRINGS_MAP_H_

#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/public/strings/grit/permission_element_generated_strings.h"
#include "third_party/blink/public/strings/grit/permission_element_strings.h"

namespace blink {
  static void FillInPermissionElementTranslationsMap(HashMap<String, HashMap<int, int>>& strings_map) {
'''

kStringMapHeaderSuffix = '''
}
}  // namespace blink
#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_PERMISSION_ELEMENT_STRINGS_MAP_H_
'''


def get_message_id_map(input_base_dir):
    dom = parse(input_base_dir + "permission_element_strings.grd")
    dic = {}
    messages = dom.getElementsByTagName("message")
    for message in messages:
        dic[tclib.GenerateMessageId(
            message.firstChild.data.strip())] = message.getAttribute("name")
    return dic


def generate_grd_file(id_map, file_list, output_file_path):
    doc = parseString(kGrdTemplate)
    messages_node = doc.getElementsByTagName("messages")[0]
    for file in file_list:
        translated_file = parse(file)
        translated_messages = translated_file.getElementsByTagName(
            "translation")
        if translated_messages.length == 0:
            continue
        message_name_suffix = file.rsplit('.',
                                          1)[0].rsplit('_',
                                                       1)[1].replace('-', '_')
        for translated_message in translated_messages:
            message_name_prefix = id_map[translated_message.getAttribute("id")]
            generated_message_name = message_name_prefix + "_" + message_name_suffix
            message = translated_message.firstChild.data.strip()
            new_message_node = doc.createElement("message")
            new_message_node.setAttribute("name", generated_message_name)
            new_message_node.setAttribute("translateable", "false")
            messages_node.appendChild(new_message_node)
            new_message_node.appendChild(doc.createTextNode(message))
            messages_node.appendChild(doc.createTextNode('\n      '))

    with open(output_file_path, 'wb') as output_file:
        output_file.write(doc.toxml(encoding='UTF-8'))


def generate_cpp_mapping(input_file_path, output_file_path):
    doc = parse(input_file_path)
    messages = doc.getElementsByTagName("message")
    with open(output_file_path, 'w') as output_file:
        output_file.write(kStringMapHeaderPrefix)

        # This is to add language-only versions for the only three languages for
        # which we do not have language-only locales available in our translation
        # lists. The language only version of the string is needed for the case
        # when the combination of language and country is unknown. E.g. for the
        # `pt-AO` (Portuguese Angola) lang setting, we will use `pt`, which via
        # this code will use `pt-pt` (Portuguese from Portugal).
        custom_locale_mappings = {"en-gb": "en", "pt-pt": "pt", "zh-cn": "zh"}
        for message in messages:
            message_name = message.getAttribute('name')
            base_message = re.split('_[a-z]', message_name)[0]
            locale = message_name.split(base_message)[1].split(
                '_', 1)[1].lower().replace("_", "-")
            if locale in custom_locale_mappings:
                locale = custom_locale_mappings[locale]
            output_file.write(
                "    if (!strings_map.Contains(String(\"{0}\"))) strings_map.insert(String(\"{0}\"), HashMap<int, int>());\n"
                .format(locale))
            output_file.write(
                "    strings_map.find(String(\"{0}\"))->value.insert({1}, {2});\n"
                .format(locale, base_message, message_name))
        output_file.write(kStringMapHeaderSuffix)


def main(argv):
    output_grd_file_position = argv.index('--output_grd')
    output_map_file_position = argv.index('--output_map')
    input_base_dir_position = argv.index('--input_base_dir')
    input_base_dir = argv[input_base_dir_position + 1]
    id_map = get_message_id_map(input_base_dir)
    translated_files = list(
        glob.glob(input_base_dir +
                  "translations/permission_element_strings_*"))
    generate_grd_file(id_map, translated_files,
                      argv[output_grd_file_position + 1])
    generate_cpp_mapping(argv[output_grd_file_position + 1],
                         argv[output_map_file_position + 1])


if __name__ == '__main__':
    sys.exit(main(sys.argv))
