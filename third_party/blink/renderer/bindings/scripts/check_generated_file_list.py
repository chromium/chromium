# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Checks the file list of generated bindings defined in generated_in_*.gni
if there is no missing entry.
"""

import argparse
import itertools
import sys

import bind_gen
import web_idl

from bind_gen.path_manager import PathManager


def parse_output_reldirs(reldirs):
    required = ["core", "modules"]
    valid = required + ["extensions_chromeos", "extensions_webview"]
    result = {}
    for key_value_pair in reldirs:
        key, value = key_value_pair.split("=", 1)
        key = key.strip()
        result[key] = value

    for c in required:
        assert c in result, "missing required --output_reldir \"{0}\"".format(
            c)

    for c in result.keys():
        assert c in valid, "invalid --output_reldir \"{0}\"".format(c)

    return result


def parse_options():
    parser = argparse.ArgumentParser(
        description="Check the generated file list of Blink bindings")
    parser.add_argument("--web_idl_database",
                        required=True,
                        type=str,
                        help="filepath of the input database")
    parser.add_argument("--root_src_dir",
                        required=True,
                        type=str,
                        help="root directory of chromium project, i.e. \"//\"")
    parser.add_argument("--root_gen_dir",
                        required=True,
                        type=str,
                        help="root directory of generated code files, i.e. "
                        "\"//out/Default/gen\"")
    parser.add_argument(
        "--output_reldir",
        metavar="KEY=VALUE",
        action="append",
        help="output directory of KEY component relative to root_gen_dir.")
    parser.add_argument("--generated_file_list",
                        required=True,
                        type=str,
                        help="filepath of the generated file list")
    parser.add_argument("--output",
                        required=True,
                        type=str,
                        help="filepath of the check results")

    options = parser.parse_args()

    return options


def main():
    options = parse_options()

    output_reldirs = parse_output_reldirs(options.output_reldir)
    component_reldirs = {}
    for component, reldir in output_reldirs.items():
        component_reldirs[web_idl.Component(component)] = reldir

    bind_gen.init(web_idl_database_path=options.web_idl_database,
                  root_src_dir=options.root_src_dir,
                  root_gen_dir=options.root_gen_dir,
                  component_reldirs=component_reldirs)
    web_idl_database = bind_gen.package_initializer.package_initializer(
    ).web_idl_database()
    idl_definitions = {
        "async_iterator": [
            interface.async_iterator
            for interface in web_idl_database.interfaces
            if interface.async_iterator
        ],
        "callback_function":
        web_idl_database.callback_functions,
        "callback_interface":
        web_idl_database.callback_interfaces,
        "dictionary":
        web_idl_database.dictionaries,
        "enumeration":
        web_idl_database.enumerations,
        "interface":
        web_idl_database.interfaces,
        "namespace":
        web_idl_database.namespaces,
        "observable_array":
        web_idl_database.observable_arrays,
        "sync_iterator": [
            interface.sync_iterator
            for interface in web_idl_database.interfaces
            if interface.sync_iterator
        ],
        "union":
        web_idl_database.union_types,
    }

    error_log = []

    # Read generated_file_list into `filepaths`.
    filepaths = {}  # {kind: set([(filepath, for_testing), ...])}
    for_testing = False
    kind = None
    with open(options.generated_file_list) as input:
        for token in itertools.chain.from_iterable(line.split()
                                                   for line in input):
            if token == "--for_prod":
                for_testing = False
                continue
            if token == "--for_testing":
                for_testing = True
                continue
            if token == "--kind":
                kind = None
                continue
            if token.startswith("--"):
                raise KeyError("Unknown keyword: {}".format(token))
            if kind is None:
                kind = token
                filepaths.setdefault(kind, set())
                continue
            filepaths[kind].add((token, for_testing))

    def check_if_listed(file_set, path, for_testing, component, kind):
        try:
            file_set.remove((path, for_testing))
        except KeyError:
            error_log.append(
                "\"{path}\" is generated but not listed in the file list of "
                "\"{kind}\" in generated_in_{component}.gni.\n".format(
                    path=path, component=component, kind=kind))

    # Check whether all generated files are listed appropriately.
    for kind, file_set in filepaths.items():
        for idl_definition in idl_definitions.get(kind, []):
            if kind == "callback_function" and idl_definition.identifier in (
                    "OnErrorEventHandlerNonNull",
                    "OnBeforeUnloadEventHandlerNonNull"):
                # OnErrorEventHandlerNonNull and
                # OnBeforeUnloadEventHandlerNonNull are unified into
                # EventHandlerNonNull, and they won't be used.
                continue

            path_manager = PathManager(idl_definition)
            for_testing = idl_definition.code_generator_info.for_testing
            check_if_listed(file_set, path_manager.api_path(ext="cc"),
                            for_testing, path_manager.api_component, kind)
            check_if_listed(file_set, path_manager.api_path(ext="h"),
                            for_testing, path_manager.api_component, kind)
            if path_manager.is_cross_components:
                check_if_listed(file_set, path_manager.impl_path(ext="cc"),
                                for_testing, path_manager.impl_component, kind)
                check_if_listed(file_set, path_manager.impl_path(ext="h"),
                                for_testing, path_manager.impl_component, kind)
        for path, _ in file_set:
            error_log.append(
                "\"{path}\" is listed in the file list of \"{kind}\", but "
                "the file is not generated as {kind}.\n".format(path=path,
                                                                kind=kind))

    with open(options.output, mode="w") as output:
        for message in error_log:
            output.write(message)

    if error_log:
        sys.stderr.write(
            "Error: {} errors were detected in file listing of the generated "
            "Blink-V8 bindings files.\n\n".format(len(error_log)))
        for message in error_log:
            sys.stderr.write(message)
        sys.stderr.write("\n")
        sys.exit(1)

    if sys.stdout.isatty():
        sys.stdout.write("No error was detected.\n")


if __name__ == "__main__":
    main()
