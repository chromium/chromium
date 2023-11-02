#!/usr/bin/env python
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import optparse
import sys

import web_idl


# Read the WebIDL database and write a list of all event handler attributes.
#
# Reads the WebIDL database (--webidl) and writes a C++ .h file with a macro
# containing all event handler names (to --out). All attributes declared as
# EventHandler or On(BeforeUnload|Error)EventHandler types are considered
# event handlers.
#
# The macro is called EVENT_HANDLER_LIST and follows the "X macro" model of
# macro lists [1], as its used elsewhere [2] in the Chromium code base.
#
# [1] https://en.wikipedia.org/wiki/X_Macro
# [2] https://source.chromium.org/search?q=%5E%23define%5C%20%5BA-Z_%5D*LIST%5C(%20file:v8
def main(argv):
    parser = optparse.OptionParser()
    parser.add_option("--out")
    parser.add_option("--webidl")
    options, args = parser.parse_args(argv[1:])

    for option in ("out", "webidl"):
        if not getattr(options, option):
            parser.error(f"--{option} is required.")
    if args:
        parser.error("No positional arguments supported.")

    event_handlers = set()

    web_idl_database = web_idl.Database.read_from_file(options.webidl)
    for interface in web_idl_database.interfaces:
        for attribute in interface.attributes:
            if attribute.idl_type.is_event_handler:
                event_handlers.add(attribute.identifier)

    license_and_header = """\
// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
"""

    with open(options.out, "w") as out:
        print(license_and_header, file=out)
        print("// Generated from WebIDL database. Don't edit, just generate.",
              file=out)
        print("//", file=out)
        print(f"// Generator: {argv[0]}", file=out)
        print("", file=out)
        print("#define EVENT_HANDLER_LIST(EH) \\", file=out)
        for event in sorted(event_handlers):
            print(f"  EH({event}) \\", file=out)
        print("\n", file=out)

    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
