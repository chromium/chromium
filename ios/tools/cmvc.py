#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
r"""
Generates code/header skeletons for coordinator, coordinator configuration,
coordinator delegate, mediator, view controller, presentation delegate,
consumer, mutator and BUILD file. It can be called piecemeal with different
parameters to add a new component to existing files. Note that it will create
all related files, even if they are are not asked for. For example adding
a consumer will automatically update/create the view controller and mediator.
BUILD.gn file is always added or updated, regardless of parts selected.
This script should be run from chromium root src folder.
Example:

cmvc --out ios/chrome/browser/ui/choose_from_drive --cm ChooseFromDrive c m

Will create the `out` folder if needed then generate or update:
choose_from_drive_coordinator.h
choose_from_drive_coordinator.mm
choose_from_drive_coordinator_delegate.h
choose_from_drive_mediator.h
choose_from_drive_mediator.mm

cmvc --out ios/chrome/browser/ui/choose_from_drive --cm ChooseFromDrive \
  --vc FilePicker cons

Will add:

file_picker_consumer.h
file_picker_view_controller.h
file_picker_view_controller.mm

And update:

choose_from_drive_mediator.h
choose_from_drive_mediator.mm

Note: As usual it is suggested to backup, or be ready to do a git revert, on
the destination folder before running this script.
Note: Adding a configuration to an existing coordinator is not handled.
It would need updating the designated initializer with adding the configuration.
The configuration file skeleton will still be created, however.

"""

import argparse
import os
import re
import subprocess
import sys

# Character to set colors in terminal.
TERMINAL_ERROR_COLOR = "\033[1m\033[91m"
TERMINAL_INFO_COLOR = "\033[22m\033[92m"
TERMINAL_RESET_COLOR = "\033[0m"

# Static obj-c match regular expressions.
SNAKE_CASE_RE = re.compile(r"(?<=[a-z])(?=[A-Z])|(?<=[A-Z])(?=[A-Z][a-z])")
IMPORT_RE = re.compile(r"^#import\s")
FORWARDS_RE = re.compile(r"^(@class|@protocol|class|enum)\s+([A-Za-z0-9_]+);$")
OBJC_TYPE_START_RE = re.compile(r"^@(?:interface|protocol)\s+")
OBJC_TYPE_RE = re.compile(
    r"^@(interface|protocol)\s+([A-Za-z0-9_]+)[\s\n]+"
    r"(:[\s\n]*([A-Za-z0-9_]+))?[\s\n]*"
    r"(<[\s\n]*(,?[\s\n]*[A-Za-z0-9_]+[\s\n]*)+>)?[\s\n]*$")
END_RE = re.compile(r"^@end$")
PROPERTY_START_RE = re.compile(r"^@property\s*\(")
PROPERTY_RE = re.compile(
    r"^@property\s*(\([^\)]+\))[\s\n]*([a-zA-Z<>_,]+)[\s\n]*([a-zA-Z0-9_]+);$")
METHOD_START_RE = re.compile(r"^[-+]\s\(")
METHOD_H_END_RE = re.compile(r";$")
METHOD_HEAD_MM_RE = re.compile(r"^([-+]\s*[^{]+)")
IMPLEMENTATION_HEAD_RE = re.compile(
    r"^@implementation\s+([A-Za-z0-9_]+)\s*([{])?$")
IMPLEMENTATION_BLOCK_END_RE = re.compile(r"^}$")
PRAGMA_MARK_RE = re.compile(r"^#pragma\smark\s(-\s+)?(.+)$")
IVARS_RE = re.compile(r"^\s+(.+)\s+(.+);$")

GN_SOURCE_SET_START_RE = re.compile(r'^source_set\(\"([^\"]+)\"\)\s+{$')
GN_SOURCE_SET_END_RE = re.compile(r'^}$')
GN_BLOCK_ONE_LINE_RE = re.compile(
    r'^  (sources|deps|frameworks)\s*=\s*\[\s*\"([^\"]+)\"\s*\]$')
GN_BLOCK_START_RE = re.compile(r'^  (sources|deps|frameworks)\s*=\s*\[$')
GN_ITEM_RE = re.compile(r'^    \"([^\"]+)\",$')
GN_BLOCK_END_RE = re.compile(r'  \]$')

# Set to True for verbose output
DEBUG = False


def adapted_color_for_output(color_start, color_end):
    """Returns a the `color_start`, `color_end` tuple if the output is a
    terminal, or empty strings otherwise"""
    if not sys.stdout.isatty():
        return "", ""
    return color_start, color_end


def print_error(error_message, error_info=""):
    """Print the `error_message` with additional `error_info`"""
    color_start, color_end = adapted_color_for_output(TERMINAL_ERROR_COLOR,
                                                      TERMINAL_RESET_COLOR)

    error_message = color_start + "ERROR: " + error_message + color_end
    if len(error_info) > 0:
        error_message = error_message + "\n" + error_info
    print(error_message)


def print_info(info_message):
    """Print the `warning_message` with additional `warning_info`"""
    color_start, color_end = adapted_color_for_output(TERMINAL_INFO_COLOR,
                                                      TERMINAL_RESET_COLOR)

    info_message = color_start + "INFO: " + info_message + color_end
    print(info_message)


def print_debug(debug_message):
    """`print_info` `debug_message` if `DEBUG` is True."""
    if DEBUG:
        print_info(debug_message)


def to_variable_name(class_name):
    return "".join(c.lower() if i == 0 else c
                   for i, c in enumerate(class_name))


def main(args):
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawTextHelpFormatter)
    parser.add_argument(
        'parts',
        nargs='+',
        help='list parts to generate: all [c]oordinator [m]ediator '
        '[mu]tator [pres]entation_delegate [conf]iguration '
        'coordinator_[d]elegate [cons]umer [v]iew_controller')
    parser.add_argument("--cm", help="Coordinator/Mediator prefix", default="")
    parser.add_argument("--vc", help="ViewController prefix", default="")
    parser.add_argument("--out", help="The destination dir", default=".")
    args = parser.parse_args()

    coordinator = ("all" in args.parts or "c" in args.parts
                   or "coordinator" in args.parts)
    mediator = ("all" in args.parts or "m" in args.parts
                or "mediator" in args.parts)
    mutator = ("all" in args.parts or "mu" in args.parts
               or "mutator" in args.parts)
    presentation_delegate = ("all" in args.parts or "pres" in args.parts
                             or "presentation_delegate" in args.parts)
    configuration = ("all" in args.parts or "conf" in args.parts
                     or "configuration" in args.parts)
    coordinator_delegate = ("all" in args.parts or "d" in args.parts
                            or "coordinator_delegate" in args.parts)
    consumer = ("all" in args.parts or "cons" in args.parts
                or "consumer" in args.parts)
    view_controller = ("all" in args.parts or "v" in args.parts
                       or "view_controller" in args.parts)

    out_path = args.out
    coordinator_prefix = args.cm
    vc_prefix = args.vc

    if not os.path.exists(out_path):
        print_info(f"creating folder: {out_path}")
        os.makedirs(out_path)

    # Object class names.
    coordinator_name = f"{coordinator_prefix}Coordinator"
    coordinator_delegate_name = f"{coordinator_prefix}CoordinatorDelegate"
    configuration_name = f"{coordinator_prefix}Configuration"
    mediator_name = f"{coordinator_prefix}Mediator"

    vc_name = f"{vc_prefix}ViewController"
    vc_presentation_delegate_name = f"{vc_prefix}PresentationDelegate"
    vc_consumer_name = f"{vc_prefix}Consumer"
    vc_mutator_name = f"{vc_prefix}Mutator"

    # File names.
    coordinator_snake_name = SNAKE_CASE_RE.sub("_", coordinator_prefix).lower()
    vc_snake_name = SNAKE_CASE_RE.sub("_", vc_prefix).lower()

    coordinator_h = os.path.join(out_path,
                                 f"{coordinator_snake_name}_coordinator.h")
    coordinator_mm = os.path.join(out_path,
                                  f"{coordinator_snake_name}_coordinator.mm")
    coordinator_delegate_h = os.path.join(
        out_path, f"{coordinator_snake_name}_coordinator_delegate.h")
    configuration_h = os.path.join(
        out_path, f"{coordinator_snake_name}_configuration.h")
    configuration_mm = os.path.join(
        out_path, f"{coordinator_snake_name}_configuration.mm")
    mediator_h = os.path.join(out_path, f"{coordinator_snake_name}_mediator.h")
    mediator_mm = os.path.join(out_path,
                               f"{coordinator_snake_name}_mediator.mm")

    vc_h = os.path.join(out_path, f"{vc_snake_name}_view_controller.h")
    vc_mm = os.path.join(out_path, f"{vc_snake_name}_view_controller.mm")
    vc_presentation_delegate_h = os.path.join(
        out_path, f"{vc_snake_name}_presentation_delegate.h")
    vc_consumer_h = os.path.join(out_path, f"{vc_snake_name}_consumer.h")
    vc_mutator_h = os.path.join(out_path, f"{vc_snake_name}_mutator.h")

    # BUILD.gn data
    cm_source_set_files = []
    vc_source_set_files = []
    cm_source_set_deps = ["//base"]
    vc_source_set_deps = ["//ui/base"]
    build_gn = os.path.join(out_path, "BUILD.gn")

    # Coordinator h and mm files
    if (coordinator or presentation_delegate or configuration
            or coordinator_delegate):
        cm_source_set_deps.append("//ios/chrome/browser/shared/coordinator/"
                                  "chrome_coordinator")
        cm_source_set_files.append(f"{coordinator_snake_name}_coordinator.h")
        cm_source_set_files.append(f"{coordinator_snake_name}_coordinator.mm")

        wanted_imports = {
            '#import "ios/chrome/browser/shared/coordinator/'
            'chrome_coordinator/chrome_coordinator.h"',
        }
        if presentation_delegate:
            wanted_imports.add(f'#import "{vc_presentation_delegate_h}"')

        wanted_forwards = set()
        if configuration:
            wanted_forwards.add(f"@class {configuration_name};")
        if coordinator_delegate:
            wanted_forwards.add(f"@protocol {coordinator_delegate_name};")

        wanted_obj_type_protocols = set()
        if presentation_delegate:
            wanted_obj_type_protocols.add(vc_presentation_delegate_name)

        wanted_properties = {}
        if coordinator_delegate:
            wanted_properties[f"id<{coordinator_delegate_name}>"] = (
                "(nonatomic, weak)",
                "delegate",
            )

        if configuration:
            wanted_methods = [
                "- (instancetype)initWithBaseViewController:"
                "(UIViewController*)viewController"
                " browser:(Browser*)browser"
                f"  configuration:({configuration_name}*)configuration"
                " NS_DESIGNATED_INITIALIZER;\n",
                "- (instancetype)initWithBaseViewController:"
                "(UIViewController*)viewController"
                " browser:(Browser*)browser NS_UNAVAILABLE;\n",
            ]
        else:
            wanted_methods = [
                "- (instancetype)initWithBaseViewController:"
                "(UIViewController*)viewController"
                " browser:(Browser*)browser NS_DESIGNATED_INITIALIZER;"
            ]

        update_h_file(
            coordinator_h,
            "interface",
            coordinator_name,
            "ChromeCoordinator",
            wanted_imports,
            wanted_forwards,
            wanted_obj_type_protocols,
            wanted_properties,
            wanted_methods,
        )

        wanted_imports = {
            f'#import "{coordinator_h}"',
        }
        if mediator:
            wanted_imports.add(f'#import "{mediator_h}"')
        if view_controller:
            wanted_imports.add(f'#import "{vc_h}"')

        wanted_ivars = {}
        if mediator:
            wanted_ivars["_mediator"] = f"{mediator_name}*"
        if view_controller:
            wanted_ivars["_viewController"] = f"{vc_name}*"
        if configuration:
            wanted_ivars["_configuration"] = f"__strong {configuration_name}*"

        wanted_methods = {}
        if configuration:
            wanted_methods["-"] = {
                f"- (instancetype)initWithBaseViewController:"
                f"(UIViewController*)viewController browser:(Browser*)browser "
                f"configuration:({configuration_name}*)configuration":
                "self = [super initWithBaseViewController:viewController "
                "browser:browser];\n"
                "if (self) {\n"
                "    _configuration = configuration;\n"
                "}\n"
                " return self;\n"
            }
        else:
            wanted_methods["-"] = {
                f"- (instancetype)initWithBaseViewController:"
                f"(UIViewController*)viewController browser:(Browser*)browser":
                "self = [super initWithBaseViewController:viewController "
                "browser:browser];\n"
                "if (self) {\n"
                "}\n"
                " return self;\n"
            }
        start_parts = []
        if mediator:
            start_parts.append(f"_mediator = [[{mediator_name} alloc] "
                               "initWithSomething:@\"something\"];")
        if view_controller:
            start_parts.append(f"_viewController = [[{vc_name} alloc] init];")
        if view_controller and mutator and mediator:
            start_parts.append(f"_viewController.mutator = _mediator;")
        if view_controller and presentation_delegate:
            start_parts.append(
                f"_mediator.{to_variable_name(vc_consumer_name)}"
                f" = _viewController;")
        stop_parts = []
        if mediator:
            stop_parts.append(f"[_mediator disconnect];")
            stop_parts.append(f"_mediator = nil;")
        if view_controller:
            stop_parts.append(f"[_viewController "
                              "willMoveToParentViewController:nil];")
            stop_parts.append(f"[_viewController "
                              "removeFromParentViewController];")
            stop_parts.append(f"_viewController = nil;")

        wanted_methods["ChromeCoordinator"] = {
            "- (void)start": "\n".join(start_parts),
            "- (void)stop": "\n".join(stop_parts),
        }

        if presentation_delegate:
            wanted_methods[f"{vc_presentation_delegate_name}"] = {
                f"- (void){to_variable_name(vc_name)}:({vc_name}*)"
                f"viewController dismissedAnimated:(BOOL)animated":
                "// TODO(crbug"
                ".com/_BUG_): do dismiss animated or remove."
            }

        update_mm_file(
            coordinator_mm,
            coordinator_name,
            wanted_imports,
            wanted_ivars,
            wanted_methods,
        )

    # Presentation delegate h file
    if presentation_delegate or view_controller:
        vc_source_set_files.append(f"{vc_snake_name}_presentation_delegate.h")

        wanted_imports = {"#import <Foundation/Foundation.h>"}
        wanted_forwards = {f"@class {vc_name};"}
        wanted_obj_type_protocols = set()
        wanted_properties = {}
        wanted_methods = [
            f"// Called when the user dismisses the VC.\n"
            f"- (void){to_variable_name(vc_name)}:({vc_name}*)viewController"
            f" dismissedAnimated:(BOOL)animated;\n"
        ]

        update_h_file(
            vc_presentation_delegate_h,
            "protocol",
            vc_presentation_delegate_name,
            None,
            wanted_imports,
            wanted_forwards,
            wanted_obj_type_protocols,
            wanted_properties,
            wanted_methods,
        )

    # Coordinator configuration h and mm files
    if configuration:
        cm_source_set_files.append(f"{coordinator_snake_name}_configuration.h")
        cm_source_set_files.append(
            f"{coordinator_snake_name}_configuration.mm")

        wanted_imports = set()
        wanted_imports.add("#import <Foundation/Foundation.h>")
        wanted_forwards = set()
        wanted_obj_type_protocols = set()
        wanted_properties = {}
        wanted_methods = []

        update_h_file(
            configuration_h,
            "interface",
            configuration_name,
            "NSObject",
            wanted_imports,
            wanted_forwards,
            wanted_obj_type_protocols,
            wanted_properties,
            wanted_methods,
        )

        wanted_imports = {
            f'#import "{configuration_h}"',
        }
        wanted_ivars = {}
        wanted_methods = {}

        update_mm_file(
            configuration_mm,
            configuration_name,
            wanted_imports,
            wanted_ivars,
            wanted_methods,
        )

    # Coordinator delegate h file
    if coordinator_delegate or coordinator:
        cm_source_set_files.append(f"{coordinator_snake_name}"
                                   "_coordinator_delegate.h")

        wanted_imports = set()
        wanted_imports.add("#import <Foundation/Foundation.h>")
        wanted_forwards = set()
        wanted_forwards.add(f"@class {coordinator_name};")
        wanted_obj_type_protocols = set()
        wanted_obj_type_protocols.add("NSObject")
        wanted_properties = {}
        wanted_methods = []

        update_h_file(
            coordinator_delegate_h,
            "protocol",
            coordinator_delegate_name,
            None,
            wanted_imports,
            wanted_forwards,
            wanted_obj_type_protocols,
            wanted_properties,
            wanted_methods,
        )

    # Mediator h and mm files
    if mediator or mutator or consumer:
        cm_source_set_files.append(f"{coordinator_snake_name}_mediator.h")
        cm_source_set_files.append(f"{coordinator_snake_name}_mediator.mm")

        wanted_imports = set()
        wanted_imports.add("#import <Foundation/Foundation.h>")
        if mutator:
            wanted_imports.add(f'#import "{vc_mutator_h}"')

        wanted_forwards = set()
        if consumer:
            wanted_forwards.add(f"@protocol {vc_consumer_name};")

        wanted_obj_type_protocols = set()
        if mutator:
            wanted_obj_type_protocols.add(vc_mutator_name)

        wanted_properties = {}
        if consumer:
            wanted_properties[f"id<{vc_consumer_name}>"] = (
                "(nonatomic, weak)",
                to_variable_name(vc_consumer_name),
            )

        wanted_methods = [
            "- (instancetype)initWithSomething:(NSString*)something"
            " NS_DESIGNATED_INITIALIZER;\n",
            "- (instancetype)init NS_UNAVAILABLE;\n\n",
            "- (void)disconnect;\n",
        ]

        update_h_file(
            mediator_h,
            "interface",
            mediator_name,
            "NSObject",
            wanted_imports,
            wanted_forwards,
            wanted_obj_type_protocols,
            wanted_properties,
            wanted_methods,
        )

        wanted_imports = {
            f'#import "{mediator_h}"',
        }
        if consumer:
            wanted_imports.add(f'#import "{vc_consumer_h}"')

        wanted_ivars = {"_something": "NSString*"}

        wanted_methods = {}

        wanted_methods["-"] = {
            f"- (instancetype)initWithSomething:(NSString*)something":
            "self = [super init];\n"
            "if (self) {\n"
            "  _something = something;\n"
            "}\n"
            "return self;\n",
        }

        if consumer:
            wanted_methods["Properties getters/setters"] = {
                f"- (void)set{vc_consumer_name}:(id<{vc_consumer_name}>)"
                "consumer":
                f"_{to_variable_name(vc_consumer_name)} = consumer;\n"
                f"[_{to_variable_name(vc_consumer_name)} "
                f"setAnotherSomething:_something];\n",
            }

        wanted_methods["Public"] = {
            "- (void)disconnect": "_something = nil;\n",
        }

        update_mm_file(
            mediator_mm,
            mediator_name,
            wanted_imports,
            wanted_ivars,
            wanted_methods,
        )

    # Consumer h file
    if consumer:
        vc_source_set_files.append(f"{vc_snake_name}_consumer.h")

        wanted_imports = set()
        wanted_imports.add("#import <Foundation/Foundation.h>")
        wanted_forwards = set()
        wanted_obj_type_protocols = set()
        wanted_properties = {}
        wanted_methods = [
            "// Called when the the data model has a new something.\n"
            "- (void)setAnotherSomething:(NSString*)something;\n"
        ]

        update_h_file(
            vc_consumer_h,
            "protocol",
            vc_consumer_name,
            None,
            wanted_imports,
            wanted_forwards,
            wanted_obj_type_protocols,
            wanted_properties,
            wanted_methods,
        )

    # Mutator h file
    if mutator:
        vc_source_set_files.append(f"{vc_snake_name}_mutator.h")

        wanted_imports = set()
        wanted_imports.add("#import <Foundation/Foundation.h>")
        wanted_forwards = set()
        wanted_obj_type_protocols = set()
        wanted_properties = {}
        wanted_methods = []

        update_h_file(
            vc_mutator_h,
            "protocol",
            vc_mutator_name,
            None,
            wanted_imports,
            wanted_forwards,
            wanted_obj_type_protocols,
            wanted_properties,
            wanted_methods,
        )

    # View controller h and mm files
    if view_controller or consumer or mutator or presentation_delegate:
        vc_source_set_files.append(f"{vc_snake_name}_view_controller.h")
        vc_source_set_files.append(f"{vc_snake_name}_view_controller.mm")

        wanted_imports = {
            "#import <UIKit/UIKit.h>",
        }
        if consumer:
            wanted_imports.add(f'#import "{vc_consumer_h}"')
        if mutator:
            wanted_imports.add(f'#import "{vc_mutator_h}"')

        wanted_forwards = set()
        if coordinator_delegate:
            wanted_forwards.add(f"@protocol {vc_presentation_delegate_name};")

        wanted_obj_type_protocols = set()
        if consumer:
            wanted_obj_type_protocols.add(vc_consumer_name)

        wanted_properties = {}
        if mutator:
            wanted_properties[f"id<{vc_mutator_name}>"] = (
                "(nonatomic, weak)",
                "mutator",
            )
        if presentation_delegate:
            wanted_properties[f"id<{vc_presentation_delegate_name}>"] = (
                "(nonatomic, weak)",
                "presentationDelegate",
            )

        wanted_methods = [
            "- (instancetype)init NS_DESIGNATED_INITIALIZER;\n",
            "- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;\n",
            "- (instancetype)initWithNibName:(NSString*)nibNameOrNil",
            " bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;\n",
        ]

        update_h_file(
            vc_h,
            "interface",
            vc_name,
            "UIViewController",
            wanted_imports,
            wanted_forwards,
            wanted_obj_type_protocols,
            wanted_properties,
            wanted_methods,
        )

        wanted_imports = {
            f'#import "{vc_h}"',
        }
        if consumer:
            wanted_imports.add(f'#import "{vc_consumer_h}"')
        if presentation_delegate:
            wanted_imports.add(f'#import "{vc_presentation_delegate_h}"')

        wanted_ivars = {}
        wanted_methods = {}

        wanted_methods["-"] = {
            "- (instancetype)init":
            "self = [super initWithNibName:nil bundle:nil];\n"
            "if (self) {\n"
            "}\n"
            "return self;\n",
        }

        wanted_methods["UIViewController"] = {
            "- (void)viewDidLoad":
            "[super viewDidLoad];\n"
            "// TODO(crbug"
            ".com/_BUG_): setup;\n",
        }

        if consumer:
            wanted_methods[f"{vc_consumer_name}"] = {
                "- (void)setAnotherSomething:(NSString*)something": "",
            }

        update_mm_file(
            vc_mm,
            vc_name,
            wanted_imports,
            wanted_ivars,
            wanted_methods,
        )

    # Create or update BUILD.gn file
    if len(vc_source_set_files):
        cm_source_set_deps.append(f":{vc_snake_name}")

    wanted_source_sets = {}
    if len(cm_source_set_files):
        wanted_source_sets[coordinator_snake_name] = {
            "sources": cm_source_set_files,
            "deps": cm_source_set_deps,
            "frameworks": ["UIKit.framework"],
        }
    if len(cm_source_set_files):
        wanted_source_sets[vc_snake_name] = {
            "sources": vc_source_set_files,
            "deps": vc_source_set_deps,
            "frameworks": ["UIKit.framework"],
        }

    if len(wanted_source_sets):
        update_build_file(build_gn, wanted_source_sets)


def update_build_file(filename, wanted_source_sets):
    """Updates or creates a build file of given`filename` with the content
    request in dictionary `wanted_source_sets`. The dictionary contains keys to
    lists for array elements of BUILD.gn (sources, deps, frameworks)"""
    print_info(filename)

    if not os.path.isfile(filename):
        subprocess.check_call(["tools/boilerplate.py", filename])

    surface_first_line = -1

    found_source_sets = []
    source_set = None

    with open(filename, "r") as body:
        all_lines = body.readlines()

        currently_in = [filename, "license"]

        for line_index, line in enumerate(all_lines):
            if currently_in[-1] == "license":
                if line[0] == "#":
                    continue
                currently_in.pop()
                surface_first_line = line_index
                # fallthrough

            if currently_in[-1] == "source_set":
                if GN_SOURCE_SET_END_RE.search(line):
                    source_set["last_line"] = line_index + 1
                    currently_in.pop()
                    continue
                match = GN_BLOCK_ONE_LINE_RE.search(line)
                if match:
                    block = match.groups()[0]
                    item = match.groups()[1]
                    source_set[f"{block}_first_line"] = line_index
                    source_set[f"{block}_last_line"] = line_index + 1
                    source_set[block].append(item)
                    source_set["block_order"].append(block)
                    continue
                match = GN_BLOCK_START_RE.search(line)
                if match:
                    block = match.groups()[0]
                    source_set[f"{block}_first_line"] = line_index
                    source_set["block_order"].append(block)
                    currently_in.append(block)
                continue

            if currently_in[-1] == "sources" or currently_in[
                    -1] == "deps" or currently_in[-1] == "frameworks":
                block = currently_in[-1]
                if GN_BLOCK_END_RE.search(line):
                    source_set[f"{block}_last_line"] = line_index + 1
                    currently_in.pop()
                    continue
                match = GN_ITEM_RE.search(line)
                if match:
                    source_set[block].append(match.groups()[0])
                    continue

            match = GN_SOURCE_SET_START_RE.search(line)
            if match:
                source_set = {
                    "name": match.groups()[0],
                    "first_line": line_index,
                    "last_line": -1,
                    "sources": [],
                    "sources_first_line": len(all_lines),
                    "sources_last_line": -1,
                    "deps": [],
                    "deps_first_line": len(all_lines),
                    "deps_last_line": -1,
                    "frameworks": [],
                    "frameworks_first_line": len(all_lines),
                    "frameworks_last_line": -1,
                    "block_order": []
                }
                found_source_sets.append(source_set)
                currently_in.append("source_set")
                continue

        # Remove items already in any existing source set
        for block in ['sources']:
            existing_items = set()
            for source_set in found_source_sets:
                for source in source_set[block]:
                    existing_items.add(source)

            for source_set, content in wanted_source_sets.items():
                new_items = []
                for source in content[block]:
                    if source not in existing_items:
                        new_items.append(source)
                content[block] = new_items

        # Remove items in matching source set
        for block in ['deps', 'frameworks']:
            for source_set in found_source_sets:
                if source_set["name"] in wanted_source_sets:
                    wanted_source_set = wanted_source_sets[source_set["name"]]
                    existing_items = set(source_set[block])
                    new_items = []
                    for source in wanted_source_set[block]:
                        if source not in existing_items:
                            new_items.append(source)
                    wanted_source_set[block] = new_items

        print_debug(f"surface: {surface_first_line}")
        print_debug(f"found_source_sets: {found_source_sets}")
        print_debug(f"wanted_source_sets: {wanted_source_sets}")

        # rewrite
        content = []
        changed = False
        cursor = 0

        # Copy until surface start.
        while cursor < len(all_lines) and cursor < surface_first_line:
            content += [all_lines[cursor]]
            cursor += 1

        # Update existing source sets
        for source_set in found_source_sets:
            name = source_set["name"]
            # Copy until first line inside source_set start.
            while (cursor < len(all_lines) and
                   cursor < source_set["first_line"] + 1):
                content += [all_lines[cursor]]
                cursor += 1
            if name in wanted_source_sets:
                wanted_source_set = wanted_source_sets[name]
                # Update existing source set, following block_order
                for block in source_set["block_order"]:
                    if len(wanted_source_set[block]) == 0:
                        continue
                    print_info(f"  updating {name}:{block}")
                    first_line = source_set[f"{block}_first_line"]
                    last_line = source_set[f"{block}_last_line"]
                    # Copy until block start.
                    while cursor < len(all_lines) and cursor < first_line:
                        content += [all_lines[cursor]]
                        cursor += 1
                    # rewrite
                    content += [f"  {block} = [\n"]
                    items = set(source_set[block]).union(
                        wanted_source_set[block])
                    for item in items:
                        content += [f'    "{item}",\n']
                    content += [f"  ]\n"]
                    # Skip lines until end of block
                    cursor = last_line
                    changed = True
                # Add other blocks that are in wanted_source_set.
                for block, items in wanted_source_set.items():
                    if block not in source_set["block_order"]:
                        print_info(f"  adding {name}:{block}")
                        content += [f"  {block} = [\n"]
                        for item in items:
                            content += [f'    "{item}",\n']
                        content += [f"  ]\n"]
                        changed = True
                del wanted_source_sets[name]

        # copy remaining lines
        while cursor < len(all_lines):
            content += [all_lines[cursor]]
            cursor += 1
        content += ["\n"]

        # Add remaining source sets
        for name, source_set in wanted_source_sets.items():
            if len(source_set["sources"]) == 0:
                continue
            print_info(f"  adding {name}")
            content += [f'source_set("{name}") {{\n']
            for block, items in source_set.items():
                content += [f"  {block} = [\n"]
                for item in items:
                    content += [f'    "{item}",\n']
                content += [f"  ]\n"]
            content += ["}\n\n"]
            changed = True

        if not changed:
            print_info(" nothing to do")
            return

        with open(filename, "w") as output:
            output.write("".join(content))

        subprocess.check_call(["gn", "format", filename])


def update_mm_file(
    filename,
    type_name,
    wanted_imports,
    wanted_ivars,
    wanted_methods,
):
    """Creates or updates a objc mm file with `filename`.
    Inserts or adds (in a sorted fashion) all #import pre-processor lines in
    the sets of `wanted_imports`, all ivars and methods in `wanted_ivars` in
    `wanted_methods` to the implementation of class with `type_name`
    Note: Methods are not sorted, but are added at the end of the specified
    section. Sections in the code are determined by `#pragma mark` tags."""

    print_info(filename)

    if not os.path.isfile(filename):
        subprocess.check_call(["tools/boilerplate.py", filename])

    with open(filename, "r") as body:
        all_lines = body.readlines()

        currently_in = [filename, "license"]
        surface_first_line = -1
        import_first_line = len(all_lines)
        import_last_line = -1
        implementation_head_first_line = len(all_lines)
        ivars_first_line = len(all_lines)
        ivars_last_line = -1
        implementation_head_last_line = -1
        implementation_last_line = -1

        found_sections = {}
        found_methods = {}
        found_imports = set()
        found_ivars = {}

        signature = ""

        section = "-"

        skip_lines = 0
        for line_index, line in enumerate(all_lines):
            if skip_lines > 0:
                skip_lines = skip_lines - 1
                continue

            if currently_in[-1] == "license":
                if line.startswith("//"):
                    continue
                else:
                    surface_first_line = line_index
                    currently_in.pop()
                    # fall through

            if currently_in[-1] == "objc_block_ignore":
                if END_RE.search(line):
                    currently_in.pop()
                    continue

            if currently_in[-1] == "ivars":
                match = IVARS_RE.search(line)
                if match:
                    found_ivars[match.groups()[1]] = match.groups()[0]
                    ivars_first_line = min(line_index, ivars_first_line)
                if IMPLEMENTATION_BLOCK_END_RE.search(line):
                    ivars_last_line = line_index
                    implementation_head_last_line = line_index + 1
                    found_sections[section] = {'first_line': line_index + 1}
                    currently_in.pop()
                continue

            if currently_in[-1] == "implementation":
                match = PRAGMA_MARK_RE.search(line)
                if match:
                    found_sections[section]['last_line'] = line_index
                    section = match.groups()[1]
                    found_sections[section] = {'first_line': line_index + 1}
                elif END_RE.search(line):
                    found_sections[section]['last_line'] = line_index
                    implementation_last_line = line_index
                    currently_in.pop()
                elif METHOD_START_RE.search(line):
                    current_line = line
                    index = 0
                    next_line = all_lines[line_index + 1]
                    multiline = current_line.strip()
                    while not current_line.strip()[-1] == "{":
                        separator = '' if multiline[-1] == ':' else ' '
                        multiline = multiline + separator + next_line.strip()
                        index = index + 1
                        current_line = next_line
                        next_line = all_lines[line_index + index + 1]
                    skip_lines = index
                    match = METHOD_HEAD_MM_RE.match(multiline)
                    if match:
                        signature = match.groups()[0].strip()
                        found_methods[signature] = {
                            "section": section,
                            "header_first_line": line_index,
                            "body_first_line": line_index + index + 1,
                        }
                        currently_in.append("method")
                continue

            if currently_in[-1] == "method":
                if IMPLEMENTATION_BLOCK_END_RE.search(line):
                    found_methods[signature]["body_last_line"] = line_index + 1
                    currently_in.pop()
                continue

            if IMPORT_RE.search(line):
                import_first_line = min(line_index, import_first_line)
                import_last_line = max(line_index + 1, import_last_line)
                found_imports.add(line.strip())
                continue

            match = IMPLEMENTATION_HEAD_RE.search(line)
            if match:
                if match.groups()[0] != type_name:
                    currently_in.append("objc_block_ignore")
                    continue
                implementation_head_first_line = line_index
                implementation_head_last_line = line_index + 1
                currently_in.append("implementation")
                if match.groups()[1] == "{":
                    currently_in.append("ivars")
                else:
                    found_sections[section] = {'first_line': line_index + 1}
                continue

        new_imports = wanted_imports.difference(found_imports)
        new_ivars = dict(wanted_ivars)
        for ivar in found_ivars:
            if ivar in new_ivars:
                del new_ivars[ivar]

        # For all methods wanted, remove them in any section where they exist.
        new_methods = {}
        if len(wanted_methods):
            for section, methods in wanted_methods.items():
                new_section = {}
                for signature, value in methods.items():
                    if signature not in found_methods:
                        new_section[signature] = value
                if len(new_section):
                    new_methods[section] = new_section

        print_debug(f"surface: {surface_first_line + 1}")
        print_debug(f"import: {import_first_line + 1} -"
                    f" {import_last_line + 1}")
        if len(new_imports):
            print_debug(f"  new_imports: {new_imports}")
        print_debug(
            f"implementation head: {implementation_head_first_line + 1}"
            f" - {implementation_head_last_line + 1}")
        print_debug(f"implementation: - {implementation_last_line + 1}")
        print_debug(f"ivars: {ivars_first_line + 1} -"
                    f" {ivars_last_line + 1}")
        if len(found_ivars):
            print_debug(f"  {found_ivars}")
        if len(new_ivars):
            print_debug(f"  new_ivars: {new_ivars}")
        print_debug("sections:")
        for section, value in found_sections.items():
            print_debug(f"  '{section}': {value['first_line'] + 1} - "
                        f"{value['last_line'] + 1}")
        print_debug("methods:")
        for signature, value in found_methods.items():
            print_debug(f"  '{signature}': {value}")
        if len(new_methods):
            print_debug(f"  new_methods: {new_methods}")

        # rewrite
        content = []
        changed = False
        cursor = 0

        # Copy until surface start.
        while cursor < len(all_lines) and cursor < surface_first_line:
            content += [all_lines[cursor]]
            cursor += 1

        # Imports.
        if len(new_imports) > 0:
            if import_first_line > import_last_line:
                print_info(" adding import section")
                if content[-1] != "\n":
                    content += ["\n"]
                sorted_imports = list(new_imports)
                sorted_imports.sort()
                for new_import in sorted_imports:
                    content += [f"{new_import}\n"]
                changed = True
            else:
                # Copy until imports start.
                while cursor < len(all_lines) and cursor < import_first_line:
                    content += all_lines[cursor]
                    cursor += 1

                # Insert missing imports if and where needed.
                print_info(" merging import section")
                while cursor < import_last_line:
                    line = all_lines[cursor].strip()
                    sorted_imports = list(new_imports)
                    sorted_imports.sort()
                    for new_import in sorted_imports:
                        if new_import < line:
                            content += [f"{new_import}\n"]
                            changed = True
                            new_imports.remove(new_import)
                    content += [all_lines[cursor]]
                    cursor += 1
                sorted_imports = list(new_imports)
                sorted_imports.sort()
                for new_import in sorted_imports:
                    content += [f"{new_import}\n"]
                    changed = True

        if implementation_head_first_line > implementation_head_last_line:
            # Copy until end.
            while cursor < len(all_lines):
                content += all_lines[cursor]
                cursor += 1

            print_info(f" adding implementation/ivars/methods sections")
            # Add Implementation header.
            content += ["\n"]
            content += [f"@implementation {type_name}"]
            # Add ivars.
            if len(new_ivars):
                content += [" {\n"]
                for ivar, type in new_ivars.items():
                    content += [f"  {type} {ivar};\n"]
                content += ["}"]
            content += ["\n\n"]

            if len(new_methods):
                content += ["\n"]
                for section, methods in new_methods.items():
                    if section != "-":
                        content += [f"#pragma mark - {section}\n\n"]
                    for signature, body in methods.items():
                        content += [f"{signature} {{\n{body}\n}}\n\n"]

            content += ["\n@end\n\n"]
            changed = True
        else:
            # Copy until implementation_head_first_line.
            while (cursor < len(all_lines)
                   and cursor < implementation_head_first_line):
                content += all_lines[cursor]
                cursor += 1

            # If ivars changed.
            if len(new_ivars):
                if ivars_first_line < ivars_last_line:
                    # Copy existing ivars
                    while cursor < len(all_lines) and cursor < ivars_last_line:
                        content += all_lines[cursor]
                        cursor += 1
                    # Add new ivars
                    for ivar, type in new_ivars.items():
                        content += [f"  {type} {ivar};\n"]
                    changed = True
                else:
                    # Copy until implementation_head_last_line.
                    while (cursor < len(all_lines)
                           and cursor < implementation_head_last_line):
                        content += all_lines[cursor]
                        cursor += 1
                    # Add new ivars.
                    content += ["{\n"]
                    for ivar, type in new_ivars.items():
                        content += [f"  {type} {ivar};\n"]
                    content += ["}\n"]
                    changed = True

            # If new methods / sections to add.
            if len(new_methods):
                for section, range in found_sections.items():
                    # Copy lines until end of section.
                    while (cursor < len(all_lines)
                           and cursor < range['last_line']):
                        content += all_lines[cursor]
                        cursor += 1
                    for method_section, methods in new_methods.items():
                        if section != method_section:
                            continue
                        for signature, body in methods.items():
                            content += [f"{signature} {{\n{body}\n}}\n\n"]
                        changed = True

                # Copy until implementation_last_line.
                while (cursor < len(all_lines)
                       and cursor < implementation_last_line):
                    content += all_lines[cursor]
                    cursor += 1
                # Add remaining sections, if any.
                for section, methods in new_methods.items():
                    if section in found_sections:
                        continue
                    if section != "-":
                        content += [f"#pragma mark - {section}\n\n"]
                    for signature, body in methods.items():
                        content += [f"{signature} {{\n{body}\n}}\n\n"]
                    changed = True

        if not changed:
            print_info(" nothing to do")
            return

        # copy remaining lines
        while cursor < len(all_lines):
            content += [all_lines[cursor]]
            cursor += 1

        with open(filename, "w") as output:
            output.write("".join(content))

        subprocess.check_call(["clang-format", "-i", filename])


def update_h_file(
    filename,
    type,
    type_name,
    type_super_name,
    wanted_imports,
    wanted_forwards,
    wanted_obj_type_protocols,
    wanted_properties,
    wanted_methods,
):
    """Creates or updates a objc header file with `filename`.
    Inserts or adds (in a sorted fashion) all #import pre-processor lines in
    the sets of `wanted_imports`, all forward declarations `wanted_forwards` in
    a sorted fashion. Inserts or updates an obj block of `type`
    (protocol or interface) with `type_name` (and with superclass
    `type_super_name` if type is interface) and with the
    `wanted_obj_type_protocols`. Inserts or adds `wanted_properties` and
    `wanted_methods`.
    Note: Properties are not sorted, new ones are added at the end."""

    print_info(filename)

    if not os.path.isfile(filename):
        subprocess.check_call(["tools/boilerplate.py", filename])

    guard = filename.upper() + "_"
    for char in "/\\.+":
        guard = guard.replace(char, "_")

    guard_start_re = re.compile(r"^#define %s" % guard)
    guard_end_re = re.compile(r"^#endif  // %s" % guard)

    with open(filename, "r") as body:
        all_lines = body.readlines()

        currently_in = [filename]
        surface_first_line = len(all_lines)
        surface_last_line = -1
        import_first_line = len(all_lines)
        import_last_line = -1
        forward_first_line = len(all_lines)
        forward_last_line = -1
        obj_type_head_first_line = len(all_lines)
        obj_type_head_last_line = -1
        obj_type_first_line = len(all_lines)
        obj_type_last_line = -1
        properties_first_line = len(all_lines)
        properties_last_line = -1
        method_first_line = len(all_lines)
        method_last_line = -1

        found_imports = set()
        found_forwards = set()
        found_obj_type_protocols = set()

        skip_lines = 0
        for line_index, line in enumerate(all_lines):
            if skip_lines > 0:
                skip_lines = skip_lines - 1
                continue

            if currently_in[-1] == "objc_block_ignore":
                if END_RE.search(line):
                    currently_in.pop()
                    continue

            if currently_in[-1] == "interface":
                if END_RE.search(line):
                    obj_type_last_line = line_index
                    currently_in.pop()
                    continue
                if METHOD_START_RE.search(line):
                    method_first_line = min(line_index, method_first_line)
                    if METHOD_H_END_RE.search(line):
                        method_last_line = max(line_index + 1,
                                               method_last_line)
                    else:
                        currently_in.append("method")
                    continue
                if PROPERTY_START_RE.search(line):
                    current_line = line
                    index = 0
                    next_line = all_lines[line_index + 1]
                    multiline = current_line
                    while (len(next_line.strip()) > 0
                           and not current_line.strip()[-1] == ";"):
                        multiline = multiline + next_line
                        index = index + 1
                        current_line = next_line
                        next_line = all_lines[line_index + index + 1]
                    skip_lines = index
                    match = PROPERTY_RE.match(multiline)
                    if match:
                        wanted_properties.pop(match.groups()[1], None)
                        properties_first_line = min(line_index,
                                                    properties_first_line)
                        properties_last_line = max(line_index + 1,
                                                   properties_last_line)
                continue

            if currently_in[-1] == "method":
                if METHOD_H_END_RE.search(line):
                    method_last_line = max(line_index + 1, method_last_line)
                    currently_in.pop()
                continue

            if guard_start_re.search(line):
                surface_first_line = line_index + 1
                currently_in.append("guard")
                continue
            if guard_end_re.search(line):
                surface_last_line = line_index
                if currently_in.pop() != "guard":
                    print_error("found unexpected h file guard:",
                                f"{line_index}: {line}")
                continue

            if IMPORT_RE.search(line):
                import_first_line = min(line_index, import_first_line)
                import_last_line = max(line_index + 1, import_last_line)
                found_imports.add(line.strip())
                continue

            if FORWARDS_RE.search(line):
                forward_first_line = min(line_index, forward_first_line)
                forward_last_line = max(line_index + 1, forward_last_line)
                found_forwards.add(line.strip())
                continue

            if OBJC_TYPE_START_RE.search(line):
                current_line = line
                index = 0
                next_line = all_lines[line_index + 1]
                multiline = current_line
                while len(next_line.strip()) > 0 and (
                        next_line.strip()[0] in ":<,"
                        or current_line.strip()[-1] in ":<,"):
                    multiline = multiline + next_line
                    index = index + 1
                    current_line = next_line
                    next_line = all_lines[line_index + index + 1]
                skip_lines = index
                match = OBJC_TYPE_RE.match(multiline)
                if (match and match.groups()[0] == type
                        and match.groups()[1] == type_name):
                    obj_type_head_first_line = line_index
                    obj_type_head_last_line = line_index + index + 1
                    obj_type_first_line = obj_type_head_last_line
                    if match.groups()[4]:
                        found_obj_type_protocols = set(
                            map(
                                lambda s: s.strip(),
                                match.groups()[4].strip("<>").split(","),
                            ))
                    currently_in.append("interface")
                else:
                    currently_in.append("objc_block_ignore")
                continue

        if surface_first_line >= surface_last_line:
            print_error("h file surface not found:")

        print_debug(f"surface: {surface_first_line + 1} -"
                    f" {surface_last_line + 1}")
        print_debug(f"import: {import_first_line + 1} -"
                    f" {import_last_line + 1}")
        print_debug(f"forward: {forward_first_line + 1} -"
                    f" {forward_last_line + 1}")
        print_debug(f"{type} head: {obj_type_head_first_line + 1} -"
                    f" {obj_type_head_last_line + 1}")
        print_debug(f"{type} body: {obj_type_first_line + 1} -"
                    f" {obj_type_last_line + 1}")
        print_debug(f"properties: {properties_first_line + 1} -"
                    f" {properties_last_line + 1}")
        print_debug(f"methods: {method_first_line + 1} -"
                    f" {method_last_line + 1}")

        # TODO: assert that if sections exist they are in the order expected.

        # rewrite
        content = []
        changed = False
        cursor = 0

        new_imports = wanted_imports.difference(found_imports)
        new_forwards = wanted_forwards.difference(found_forwards)
        obj_type_protocols = wanted_obj_type_protocols.union(
            found_obj_type_protocols)

        if len(obj_type_protocols) != len(found_obj_type_protocols):
            changed = True

        # Copy until surface start.
        while cursor < len(all_lines) and cursor < surface_first_line:
            content += [all_lines[cursor]]
            cursor += 1

        # Imports.
        if len(new_imports) > 0:
            if import_first_line > import_last_line:
                print_info(" adding import section")
                if content[-1] != "\n":
                    content += ["\n"]
                sorted_imports = list(new_imports)
                sorted_imports.sort()
                for new_import in sorted_imports:
                    content += [f"{new_import}\n"]
                changed = True
            else:
                # Copy until imports start.
                while cursor < len(all_lines) and cursor < import_first_line:
                    content += all_lines[cursor]
                    cursor += 1

                # Insert missing imports if and where needed.
                print_info(" merging import section")
                while cursor < import_last_line:
                    line = all_lines[cursor].strip()
                    sorted_imports = list(new_imports)
                    sorted_imports.sort()
                    for new_import in sorted_imports:
                        if new_import < line:
                            content += [f"{new_import}\n"]
                            changed = True
                            new_imports.remove(new_import)
                    content += [all_lines[cursor]]
                    cursor += 1
                sorted_imports = list(new_imports)
                sorted_imports.sort()
                for new_import in sorted_imports:
                    content += [f"{new_import}\n"]
                    changed = True

        # Forward declarations.
        if len(new_forwards) > 0:

            def sort_by_forward_name(s):
                return FORWARDS_RE.search(s).groups()[0]

            if forward_first_line > forward_last_line:
                print_info(" adding forward declarations section")
                if content[-1] != "\n":
                    content += ["\n"]
                sorted_forwards = list(new_forwards)
                sorted_forwards.sort(key=sort_by_forward_name)
                for new_forward in sorted_forwards:
                    content += [f"{new_forward}\n"]
                changed = True
            else:
                # Copy until forwards start.
                while cursor < len(all_lines) and cursor < forward_first_line:
                    content += all_lines[cursor]
                    cursor += 1

                # Insert missing forwards if and where needed.
                print_info(" merging forward section")
                while cursor < forward_last_line:
                    line = all_lines[cursor].strip()
                    sorted_forwards = list(new_forwards)
                    sorted_forwards.sort(key=sort_by_forward_name)
                    for new_forward in sorted_forwards:
                        if new_forward < line:
                            content += [f"{new_forward}\n"]
                            changed = True
                            new_forwards.remove(new_forward)
                    content += [all_lines[cursor]]
                    cursor += 1
                sorted_forwards = list(new_forwards)
                sorted_forwards.sort(key=sort_by_forward_name)
                for new_forward in sorted_forwards:
                    content += [f"{new_forward}\n"]
                    changed = True

        type_super = f" : {type_super_name}" if type_super_name else ""

        if obj_type_head_first_line > obj_type_head_last_line:
            # Copy until surface end.
            while cursor < len(all_lines) and cursor < surface_last_line:
                content += all_lines[cursor]
                cursor += 1

            print_info(f" adding {type}/property/methods sections")
            content += ["\n"]
            protocols_string = ""
            if len(obj_type_protocols):
                lst = list(obj_type_protocols)
                lst.sort()
                protocols_string = f"<{', '.join(lst)}>"
            content += [
                f"@{type} {type_name}{type_super} {protocols_string}"
                "\n\n"
            ]
            # Add properties.
            if len(wanted_properties):
                content += ["\n"]
                for type in wanted_properties:
                    attributes, name = wanted_properties[type]
                    content += [f"@property{attributes} {type} {name};\n\n"]
            # Add methods.
            if len(wanted_methods):
                for method in wanted_methods:
                    content += [method, "\n"]
            content += ["@end\n\n"]
            changed = True
        else:
            # Copy until type head start.
            while cursor < len(
                    all_lines) and cursor < obj_type_head_first_line:
                content += all_lines[cursor]
                cursor += 1

            if changed:
                print_info(" rewriting {type} head")

            # Rewrite interface/protocol header.
            protocols_string = ""
            if len(obj_type_protocols):
                lst = list(obj_type_protocols)
                lst.sort()
                protocols_string = f"<{', '.join(lst)}>"
            content += [
                f"@{type} {type_name}{type_super} {protocols_string}"
                "\n\n"
            ]

            # Skip existing head data
            while cursor < len(all_lines) and cursor < obj_type_head_last_line:
                cursor += 1

            # Copy existing properties
            if properties_first_line < properties_last_line:
                while cursor < len(
                        all_lines) and cursor < properties_last_line:
                    content += all_lines[cursor]
                    cursor += 1

            # Add properties.
            if len(wanted_properties):
                print_info(" adding properties section")
                content += ["\n"]
                for type in wanted_properties:
                    attributes, name = wanted_properties[type]
                    content += [f"@property{attributes} {type} {name};\n"]
                    content += ["\n"]
                changed = True

            # Add methods section if none there already.
            if method_first_line > method_last_line:
                content += ["\n"]
                if len(wanted_methods):
                    print_info(" adding methods section")
                    for methods in wanted_methods:
                        content += [methods, "\n"]
                    changed = True

        if not changed:
            print_info(" nothing to do")
            return

        # copy remaining lines
        while cursor < len(all_lines):
            content += [all_lines[cursor]]
            cursor += 1

        with open(filename, "w") as output:
            output.write("".join(content))

        subprocess.check_call(["clang-format", "-i", filename])


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
