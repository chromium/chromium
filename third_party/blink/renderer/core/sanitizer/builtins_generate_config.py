# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Generate static sanitizer configuration. For use with the default config."""

import json
import optparse
import sys

# Mapping of (namespace, localname) to C++ name for elements and attributes.
# These are expected to be set once, at the beginning of the program, and be
# immutable for the entire remeining execution.
ELEMENT_CPP_MAP = {}
ATTRIBUTE_CPP_MAP = {}


def error(context, *infos):
    print("Error: " + context.strip() + "\n")
    for info in infos:
        print("\t" + str(info))
    sys.exit(1)


def lstrip(string):
    """Call str.lstrip on each line of text."""
    return "\n".join(map(str.lstrip, string.split("\n")))


def element(name):
    namespace = ("http://www.w3.org/1999/xhtml"
                 if isinstance(name, str) else name["namespace"])
    name = (name if isinstance(name, str) else name["name"])
    return ELEMENT_CPP_MAP[(namespace, name)]


def attribute(name):
    if isinstance(name, str):
        lname = name
        namespace = None
    else:
        lname = name["name"]
        namespace = name["namespace"]
    return ATTRIBUTE_CPP_MAP[(namespace, lname.lower())]


def bool(value):
    return "true" if value else "false"


def table_name(name):
    return f"k{name[0].upper()}{name[1:]}"


def generate_nameset_table(name, default_config, formatter_fn, output):
    if default_config.get(name):
        print(
            f"  static const QualifiedName* const {table_name(name)}[] = "
            "{",
            file=output)
        for item in default_config.get(name):
            print(f"      &{formatter_fn(item)},", file=output)
        print("  };\n", file=output)


def generate_nameset_arg(name, default_config, output):
    if name not in default_config:
        print(f"  /* {name} */ nullptr,", file=output)
    elif not default_config.get(name):
        print(f"  /* {name} */ std::make_unique<SanitizerNameSet>(),",
              file=output)
    else:
        print(f"  /* {name} */ MakeNameSet({table_name(name)}),", file=output)


def namemap_table_name(key, subkey):
    # Upper-case only the first letter. (str.capitalize() would lower-case
    # the remainder, e.g. "removeAttributes" -> "Removeattributes".)
    return f"k{key[0].upper()}{key[1:]}{subkey[0].upper()}{subkey[1:]}"


def generate_namemap_tables(key, subkey, default_config, output):
    entries = [elem for elem in default_config.get(key, []) if subkey in elem]
    if not entries:
        return
    table = namemap_table_name(key, subkey)
    for index, elem in enumerate(entries):
        if elem.get(subkey):
            print(f"  /* {elem['name']} */", file=output)
            print(
                f"  static const QualifiedName* const {table}_{index}[] = "
                "{",
                file=output)
            for attr in elem.get(subkey):
                print(f"      &{attribute(attr)},", file=output)
            print("  };\n", file=output)
    print(f"  static const ElementAttrs {table}[] = {{", file=output)
    for index, elem in enumerate(entries):
        if elem.get(subkey):
            print(f"      {{&{element(elem)}, {table}_{index}}},", file=output)
        else:
            print(f"      {{&{element(elem)}, {{}}}},", file=output)
    print("  };\n", file=output)


def generate_namemap_arg(key, subkey, default_config, output):
    entries = [elem for elem in default_config.get(key, []) if subkey in elem]
    if entries:
        print(
            f"  /* {key}[{subkey}] */ "
            f"MakeNameMap({namemap_table_name(key, subkey)}),",
            file=output)
    else:
        print(f"  /* {key}[{subkey}] */ SanitizerNameMap(),", file=output)


def generate_stringset(name, default_config, output):
    if name in default_config:
        print(f"  /* {name} */", file=output)
        print("  std::make_unique<HashSet<AtomicString>>(", file=output)
        print("    std::initializer_list<AtomicString>({", file=output)
        for item in default_config.get(name):
            print(f"      \"{item}\",", file=output)
        print("    })", file=output)
        print("  ),", file=output)
    else:
        print(f"  /* {name} */ nullptr,", file=output)


def generate_tables(default_config, output):
    generate_nameset_table("elements", default_config, element, output)
    generate_nameset_table("removeElements", default_config, element, output)
    generate_nameset_table("replaceWithChildrenElements", default_config,
                           element, output)
    generate_nameset_table("attributes", default_config, attribute, output)
    generate_nameset_table("removeAttributes", default_config, attribute,
                           output)
    generate_namemap_tables("elements", "attributes", default_config, output)
    generate_namemap_tables("elements", "removeAttributes", default_config,
                            output)


def generate_config(default_config, output):
    generate_nameset_arg("elements", default_config, output)
    generate_nameset_arg("removeElements", default_config, output)
    generate_nameset_arg("replaceWithChildrenElements", default_config, output)
    generate_nameset_arg("attributes", default_config, output)
    generate_nameset_arg("removeAttributes", default_config, output)
    generate_namemap_arg("elements", "attributes", default_config, output)
    generate_namemap_arg("elements", "removeAttributes", default_config,
                         output)
    generate_stringset("processingInstructions", default_config, output)
    generate_stringset("removeProcessingInstructions", default_config, output)
    print(f"  /* comments */ {bool(default_config.get('comments'))},",
          file=output)
    print(
        "  /* dataAttributes */ "
        f"{bool(default_config.get('dataAttributes'))}",
        file=output)


def generate_file(name, default_config, output):
    print(lstrip(f"""
        // Copyright 2024 The Chromium Authors
        // Use of this source code is governed by a BSD-style license that can be
        // found in the LICENSE file.

        // This file is automatically generated. Do not edit. Just generate.
        //
        // Manually re-generate:
        // $ ninja -C ... third_party/blink/renderer/core/sanitizer:generated

        #include "third_party/blink/renderer/core/sanitizer/sanitizer_builtins.h"
        #include "third_party/blink/renderer/core/sanitizer/sanitizer.h"

        #include "third_party/blink/renderer/core/html_names.h"
        #include "third_party/blink/renderer/core/mathml_names.h"
        #include "third_party/blink/renderer/core/svg_names.h"
        #include "third_party/blink/renderer/core/xlink_names.h"
        #include "third_party/blink/renderer/core/xml_names.h"
        #include "third_party/blink/renderer/core/xmlns_names.h"

        namespace blink {{
        namespace sanitizer_generated_builtins {{

        Sanitizer* {name}() {{"""),
          file=output)
    generate_tables(default_config, output)
    print("  Sanitizer* sanitizer = MakeGarbageCollected<Sanitizer>(",
          file=output)
    generate_config(default_config, output)
    print(lstrip("""
          );
          return sanitizer;
        }
        }  // namespace sanitizer_generated_builtins
        }  // namespace blink"""),
          file=output)


def set_elements_cpp_mapping(all_known):
    CPP_NAME_EXCEPTIONS = {
        "html": "HTML",
        "rtc": "RTC",
        "iframe": "IFrame",
        "annotation-xml": "AnnotationXml",
        "mpath": "MPath",
        "svg": "SVG",
        "tspan": "TSpan"
    }
    for elem in all_known["elements"]:
        cppname = elem["name"]
        # Normalize to match C++ style. Unfortunately, some C++ names are not
        # regularly formed so we use a small table of exceptions.
        cppname = CPP_NAME_EXCEPTIONS.get(cppname, cppname)
        if cppname.startswith("fe") and cppname not in ("fencedframe", ):
            cppname = "FE" + cppname[2:]
        cppname = cppname[0].upper() + cppname[1:]
        ELEMENT_CPP_MAP[(elem["namespace"], elem["name"])] = (
            f"{elem['cppnamespace'].lower()}_names::k{cppname}Tag")


def set_attributes_cpp_mapping(all_known):
    for attr in all_known["attributes"]:
        cppname = attr["name"]

        # Normalize name to match C++ style, by up-casing the first letter
        # and every letter following a dash. And remove the dashes.
        cppname = cppname[0].upper() + cppname[1:]
        while '-' in cppname:
            pos = cppname.index("-")
            cppname = cppname[0:pos] + cppname[pos + 1].upper() + cppname[pos +
                                                                          2:]
        ATTRIBUTE_CPP_MAP[(attr["namespace"], attr["name"].lower())] = (
            f"{attr['cppnamespace'].lower()}_names::k{cppname}Attr")


def set_cpp_mapping(all_known):
    set_elements_cpp_mapping(all_known)
    set_attributes_cpp_mapping(all_known)


def main(argv):
    parser = optparse.OptionParser()
    parser.add_option("--out")
    parser.add_option("--name")
    parser.add_option("--default-configuration")
    parser.add_option("--all-known")
    options, args = parser.parse_args(argv)
    if not options.out:
        parser.error("No --out")
    if not options.name:
        parser.error("No --name")
    if not options.default_configuration:
        parser.error("No --default-configuration")
    if not options.all_known:
        parser.error("No --all-known")
    if args:
        parser.error("Unknown argument: " + args)

    try:
        all_known = json.load(open(options.all_known, "r"))
        set_cpp_mapping(all_known)
    except BaseException as err:
        error("Cannod load table of all known elements/attribuutes.",
              options.allknown, err)

    try:
        default_config = json.load(open(options.default_configuration, "r"))
    except BaseException as err:
        error("Cannot load default config as JSON.",
              options.default_configuration, err)
    try:
        with open(options.out, "w") as output:
            try:
                generate_file(options.name, default_config, output)
            except BaseException as err:
                error("Cannot generate output content", err)
    except BaseException as err:
        error("Cannot open output file for writing.", options.out, err)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
