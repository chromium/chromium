#!/usr/bin/env python
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import re


def basename(path):
    return os.path.splitext(os.path.basename(path))[0]


def char_escape(c):
    # Escaping suitable for Protobuf text format, which is C-like.
    if c in "'\"\\":
        return "\\" + c
    elif c == "\n":
        return "\\n"
    else:
        return c


def main():
    parser = argparse.ArgumentParser(
        description="Generate sanitizer_api_fuzzer seed corpus.")
    parser.add_argument("--outdir", required=True)
    parser.add_argument("--dictionary")
    parser.add_argument("inputs", nargs="+")
    args = parser.parse_args()

    # For simplicity, read all inputs into dictionary.
    inputs = {}
    for input_file in args.inputs:
        with open(input_file, "r") as f:
            inputs[input_file] = f.read()

    # Use file extensions to distinguish HTML from config inputs.
    htmls = [name for name in inputs if name.endswith(".html")]
    configs = [name for name in inputs if name.endswith(".txt")]

    # Generate each combo of html + config, and write it into --outdir.
    for html in htmls:
        for config in configs:
            name = "%s/%s-%s.txt" % (args.outdir, basename(html),
                                     basename(config))
            escaped_html = "".join(map(char_escape, inputs[html]))
            with open(name, "w") as f:
                f.write("html_string: \"%s\"\n%s\n" %
                        (escaped_html, inputs[config]))

    # Write a "dictionary" file with the element and attribute names.
    # Extract element and attribute names with simple regexps. It doesn't matter
    # if these will always match correctly, as long as the dictionary is mostly
    # sensible.
    if args.dictionary:
        seed_dictionary = set()
        for html in htmls:
            seed_dictionary.update(re.findall(r'(?<=<)\w+\b', inputs[html]))
            seed_dictionary.update(re.findall(r'\b\w+(?==)', inputs[html]))
        with open(args.dictionary, "w") as f:
            for word in seed_dictionary:
                f.write("\"%s\"\n" % word)


if __name__ == '__main__':
    main()
