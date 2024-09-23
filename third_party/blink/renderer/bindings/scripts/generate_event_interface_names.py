# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Generate event interfaces .json5 file (e.g. event_interface_names.json5).

The event interfaces .json5 file contains a list of all Event interfaces, i.e.,
all interfaces that inherit from Event, including Event itself,
together with certain extended attributes.

Paths are in POSIX format, and relative to the repository root.

This list is used to generate `EventFactory` and `event_interface_names`.
The .json5 format is documented in build/scripts/json5_generator.py.
"""

import json
import optparse
import os

import web_idl


def parse_options():
    parser = optparse.OptionParser()
    parser.add_option("--web_idl_database",
                      type="string",
                      help="filepath of the input database")
    parser.add_option(
        "--component",
        type="string",
        help="component to be processed, e.g. 'core' or 'modules'")
    parser.add_option(
        "--export-macro",
        type="string",
        help="C++ export macro, e.g. 'CORE_EXPORT' or 'MODULES_EXPORT'")
    parser.add_option("--suffix",
                      type="string",
                      default="",
                      help="'metadata.suffix' entry in the output .json5 file")
    parser.add_option("--output",
                      type="string",
                      help="filepath of the output .json5 file")
    options, args = parser.parse_args()

    required_option_names = [
        "web_idl_database",
        "component",
        "export_macro",
        "output",
    ]
    for required_option_name in required_option_names:
        if getattr(options, required_option_name) is None:
            parser.error(
                "--{} is a required option.".format(required_option_name))

    return options, args


def main():
    options, args = parse_options()

    web_idl_database = web_idl.file_io.read_pickle_file(
        options.web_idl_database)

    metadata = {
        "namespace": "event_interface_names",
        "suffix": options.suffix,
        "export": options.export_macro,
    }
    data = []
    event_interface = web_idl_database.find("Event")
    for interface in sorted(web_idl_database.interfaces,
                            key=lambda x: x.identifier):
        if interface.components[0] != options.component:
            continue
        if event_interface not in interface.inclusive_inherited_interfaces:
            continue
        entry = {
            "name":
            interface.identifier,
            "interfaceHeaderDir":
            os.path.dirname(interface.code_generator_info.blink_headers[0]),
        }
        runtime_enabled_values = interface.extended_attributes.values_of(
            "RuntimeEnabled")
        if runtime_enabled_values:
            assert len(runtime_enabled_values) == 1
            entry["RuntimeEnabled"] = runtime_enabled_values[0]
        data.append(entry)
    event_interface_names = {
        "metadata": metadata,
        "data": data,
    }

    with open(options.output, mode="w") as file_obj:
        file_obj.write(json.dumps(event_interface_names, indent=2))


if __name__ == '__main__':
    main()
