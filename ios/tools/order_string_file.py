# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Script to alphabetically order string messages

This script alphabetically orders the messages in the
.grd file passed in argument.
"""

import xml.etree.ElementTree as ElementTree
import sys


# Alphabetically sort the file given as first argument
def SortFile(file_path):
    parser = ElementTree.XMLParser(target=ElementTree.TreeBuilder(
        insert_comments=True))
    with open(file_path, 'r') as xml_file:
        try:
            tree = ElementTree.parse(xml_file, parser)
        except ElementTree.ParseError:
            print("ERROR while parsing (wrongly formatted file?):\n" +
                  file_path)
            return -1

    root = tree.getroot()
    messages_element = tree.findall('.//messages')[0]
    messages = messages_element.findall('message')
    messages.sort(key=lambda message: message.attrib["name"])
    for message in messages:
        messages_element.remove(message)
    for message in messages:
        messages_element.append(message)
    tree.write(file_path, encoding="UTF-8", xml_declaration=True)


def main(args):
    if len(args) == 0:
        print("ERROR: Pass the path to the file to order.")
        return -1
    for paths in args:
        SortFile(paths)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
