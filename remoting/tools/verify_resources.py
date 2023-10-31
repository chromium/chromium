#!/usr/bin/env python
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Verifies that GRD resource files define all the strings used by a given
set of source files. For file formats where it is not possible to infer which
strings represent message identifiers, localized strings should be explicitly
annotated with the string "i18n-content", for example:

  LocalizeString(/*i18n-content*/"PRODUCT_NAME");

This script also recognises localized strings in HTML and manifest.json files:

  HTML:          i18n-content="PRODUCT_NAME"
              or i18n-value-name-1="BUTTON_NAME"
              or i18n-title="TOOLTIP_NAME"
  manifest.json: __MSG_PRODUCT_NAME__

Note that these forms must be exact; extra spaces are not permitted, though
either single or double quotes are recognized.

In addition, the script checks that all the messages are still in use; if
this is not the case then a warning is issued, but the script still succeeds.
"""

from __future__ import print_function

import json
import os
import optparse
import re
import sys
import xml.dom.minidom as minidom

WARNING_MESSAGE = """
To remove this warning, either remove the unused tags from
resource files, add the files that use the tags listed above to
remoting.gyp, or annotate existing uses of those tags with the
prefix /*i18n-content*/
"""

def LoadTagsFromGrd(filename):
    xml = minidom.parse(filename)
    android_tags = []
    other_tags = []
    msgs_and_structs = xml.getElementsByTagName("message")
    msgs_and_structs.extend(xml.getElementsByTagName("structure"))
    for res in msgs_and_structs:
        name = res.getAttribute("name")
        if not name or not name.startswith("IDS_"):
            raise Exception("Tag name doesn't start with IDS_: %s" % name)
        name = name[4:]
        if 'android_java' in res.getAttribute('formatter_data'):
            android_tags.append(name)
        else:
            other_tags.append(name)
    return android_tags, other_tags


def ExtractTagFromLine(file_type, line):
    """Extract a tag from a line of HTML, C++, JS or JSON."""
    if file_type == "html":
        # HTML-style (tags)
        m = re.search(r'i18n-content=[\'"]([^\'"]*)[\'"]', line)
        if m: return m.group(1)
        # HTML-style (titles)
        m = re.search(r'i18n-title=[\'"]([^\'"]*)[\'"]', line)
        if m: return m.group(1)
        # HTML-style (substitutions)
        m = re.search(r'i18n-value-name-[1-9]=[\'"]([^\'"]*)[\'"]', line)
        if m: return m.group(1)
    elif file_type == 'js':
        # Javascript style
        m = re.search(r'/\*i18n-content\*/[\'"]([^\`"]*)[\'"]', line)
        if m: return m.group(1)
    elif file_type == 'cc' or file_type == 'mm':
        # C++ style
        m = re.search(r'IDS_([A-Z0-9_]*)', line)
        if m: return m.group(1)
        m = re.search(r'/\*i18n-content\*/["]([^\`"]*)["]', line)
        if m: return m.group(1)
    elif file_type == 'json.jinja2':
        # Manifest style
        m = re.search(r'__MSG_(.*)__', line)
        if m: return m.group(1)
    elif file_type == 'jinja2':
        # Jinja2 template file
        m = re.search(r'\{\%\s+trans\s+\%\}([A-Z0-9_]+)\{\%\s+endtrans\s+\%\}',
                      line)
        if m: return m.group(1)
    return None


def VerifyFile(filename, messages, used_tags):
    """
  Parse |filename|, looking for tags and report any that are not included in
  |messages|. Return True if all tags are present and correct, or False if
  any are missing.
  """

    base_name, file_type = os.path.splitext(filename)
    file_type = file_type[1:]
    if file_type == 'jinja2' and base_name.endswith('.json'):
        file_type = 'json.jinja2'
    if file_type not in ['js', 'cc', 'html', 'json.jinja2', 'jinja2', 'mm']:
        raise Exception("Unknown file type: %s" % file_type)

    result = True
    matches = False
    f = open(filename, 'r')
    lines = f.readlines()
    for i in range(0, len(lines)):
        tag = ExtractTagFromLine(file_type, lines[i])
        if tag:
            tag = tag.upper()
            used_tags.add(tag)
            matches = True
            if not tag in messages:
                result = False
                print('%s/%s:%d: error: Undefined tag: %s' %
                    (os.getcwd(), filename, i + 1, tag))
    f.close()
    return result


def main():
    parser = optparse.OptionParser(
        usage='Usage: %prog [options...] [source_file...]')
    parser.add_option('-t', '--touch', dest='touch',
                      help='File to touch when finished.')
    parser.add_option('-r', '--grd', dest='grd', action='append',
                      help='grd file')
    parser.add_option('--strict', dest='strict', action='store_true',
                      help='Use strict verification checks.')

    options, args = parser.parse_args()
    if not options.touch:
        print('-t is not specified.')
        return 1
    if len(options.grd) == 0 or len(args) == 0:
        print('At least one GRD file needs to be specified.')
        return 1

    all_resources = []
    non_android_resources = []
    for f in options.grd:
        android_tags, other_tags = LoadTagsFromGrd(f)
        all_resources.extend(android_tags + other_tags)
        non_android_resources.extend(other_tags)

    used_tags = set([])
    exit_code = 0
    for f in args:
        if not VerifyFile(f, all_resources, used_tags):
            exit_code = 1

    if options.strict:
        warnings = False
        # Determining if a resource is being used in the Android app is tricky
        # because it requires annotating and parsing Android XML layout files.
        # For now, exclude Android strings from this check.
        for tag in non_android_resources:
            if tag not in used_tags:
                print('%s/%s:0: warning: %s is defined but not used' %
                    (os.getcwd(), sys.argv[2], tag))
                warnings = True
        if warnings:
            print(WARNING_MESSAGE)

    if exit_code == 0:
        f = open(options.touch, 'a')
        f.close()
        os.utime(options.touch, None)

    return exit_code


if __name__ == '__main__':
    sys.exit(main())
