# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Generate a Sanitizer "config" w/ all tag + attribute names the browser knows.

This is a terrible config, and we're not generating it to use it, but in order
to distinguish between browser-defined and browser-unknown/user-defined markup.

We also annotate all elements/attributes with their "cppnamespace, i.e. the
namespace that Blink's C++ implementation uses for this set of names.
"""

import json
import optparse
import sys
from pyjson5.src import json5


def error(context, *infos):
    print("Error: " + context + "\n")
    for info in infos:
        print("\t" + str(info))
    sys.exit(1)


def add_tag_names(data, result):
    namespace = data["metadata"]["namespaceURI"]
    for item in data["data"]:
        result["elements"].append({
            "namespace":
            namespace,
            "name":
            item if isinstance(item, str) else item["name"],
            "cppnamespace":
            data["metadata"]["namespace"]
        })


def add_attribute_names(data, result):
    namespace = (None if "attrsNullNamespace" in data["metadata"] else
                 data["metadata"]["namespaceURI"])
    for item in data["data"]:
        result["attributes"].append({
            "namespace":
            namespace,
            "name":
            item,
            "cppnamespace":
            data["metadata"]["namespace"]
        })


def add_aria_attribute_names(data, result):
    for item in data["attributes"]:
        result["attributes"].append({
            "namespace": None,
            "name": item["name"],
            "cppnamespace": "HTML"
        })


def main(argv):
    parser = optparse.OptionParser()
    parser.add_option("--out")
    options, args = parser.parse_args(argv)
    if not options.out:
        parser.error("No --out")

    result = {"elements": [], "attributes": []}
    for arg in args:
        try:
            data = json5.load(open(arg, "r"))
            if "tag_names" in arg:
                add_tag_names(data, result)
            elif "attribute_names" in arg:
                add_attribute_names(data, result)
            elif "aria_properties" in arg:
                add_aria_attribute_names(data, result)
            else:
                error("No idea what to do with this file.", arg)
        except BaseException as err:
            error("Cannot load json5 file", arg, err)

    try:
        json.dump(result, open(options.out, "w"), indent=2)
    except BaseException as err:
        error("Cannot open output file for writing.", options.out, err)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
