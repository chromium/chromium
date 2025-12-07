# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Script to alphabetically order string messages

This script alphabetically orders the messages in the
.grd file passed in argument.
"""

import xml.etree.ElementTree as ElementTree
import sys
import io
import re

# Sorts the "messages" tags in the file in the given `file_path` in
# alphabetical order of their "name" attribute.
# This method preserve the HTML entities to avoid replacing $ in strings
# to avoid seeing it as a placeholder. See crbug.com/424146226.
def SortFile(file_path):
    parser = ElementTree.XMLParser(target=ElementTree.TreeBuilder(
        insert_comments=True))
    with open(file_path, 'r', encoding='utf-8') as f:
        raw_xml = f.read()

    # --- Placeholder logic for all entities ---
    # 1. Find all unique entities (e.g., '&quot;', '&amp;')
    # Sorting by length descending ensures that longer entities are replaced
    # first (e.g., '&amp;amp;' before '&amp;').
    entities = re.findall(r'&[a-zA-Z0-9#]+;', raw_xml)
    unique_entities = sorted(list(set(entities)), key=len, reverse=True)

    placeholders = {}
    reverse_placeholders = {}
    xml_to_parse = raw_xml

    # 2. Replace each unique entity with a unique placeholder
    for i, entity in enumerate(unique_entities):
        placeholder = f"___ENTITY_PLACEHOLDER_{i}___"
        placeholders[entity] = placeholder
        reverse_placeholders[placeholder] = entity
        xml_to_parse = xml_to_parse.replace(entity, placeholder)

    try:
        # --- XML Parsing and Sorting ---
        # 3. Use io.StringIO to treat the modified string as a file for parse()
        parser = ElementTree.XMLParser(target=ElementTree.TreeBuilder(
            insert_comments=True))
        xml_stream = io.StringIO(xml_to_parse)
        tree = ElementTree.parse(xml_stream, parser=parser)
        root = tree.getroot()
    except ElementTree.ParseError:
        print("ERROR while parsing (wrongly formatted file?):\n" + file_path)
        return -1

    # 4. Get all <message> elements and sort them based on the 'name' attribute.
    messages_element = tree.find('.//messages')
    messages = messages_element.findall('message')
    messages.sort(key=lambda message: message.attrib["name"])
    for message in messages:
        messages_element.remove(message)
    messages_element.extend(messages)

    # --- Serialization and Restoration ---
    # 5. Serialize the tree back to a string/bytes
    output_buffer = io.BytesIO()
    tree.write(output_buffer, encoding='utf-8', xml_declaration=True)
    output_xml_bytes = output_buffer.getvalue()

    output_xml_str = output_xml_bytes.decode('utf-8')

    # 6. Restore placeholders back to their original entities
    final_xml_str = output_xml_str
    for placeholder, entity in reverse_placeholders.items():
        final_xml_str = final_xml_str.replace(placeholder, entity)

    final_xml_bytes = final_xml_str.encode('utf-8')

    # 7. Write the final bytes to the output file
    with open(file_path, 'wb') as f:
        f.write(final_xml_bytes)
        f.write(b'\n')

def main(args):
    if len(args) == 0:
        print("ERROR: Pass the path to the file to order.")
        return -1
    for paths in args:
        SortFile(paths)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
