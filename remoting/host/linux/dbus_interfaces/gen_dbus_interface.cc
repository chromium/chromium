// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gio/gio.h>
#include <glib.h>

#include <fstream>
#include <iostream>
#include <ostream>
#include <string>
#include <string_view>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/string_util.h"

// This utility handles conversion from an XML-format D-Bus interface definition
// to a header file containing method, property, and signal spec classes for use
// with GDBusConnectionRef. It should be run from the src/ directory with the
// output specified with a relative path. (Otherwise the generated header guard
// will be incorrect.) Sample usage:
//
//     gen_dbus_interface --input=/path/to/com.example.Interface.xml \
//         --output=remoting/host/linux/dbus_interfaces/com_example_Interface.h
//
// Typically, the XML interface definition will come from one of three sources:
//
//  1. Installed by service: Services providing public D-Bus APIs may install
//     interface definitions in /usr/share/dbus-1/interfaces/.
//  2. From the service's source tree: Even if not installed, services may
//     include an interface XML file in their source tree.
//  3. Via D-Bus introspection: If the service supports introspection, an XML
//     interface definition can be obtained via busctl:
//
//         busctl --user introspect --xml-interface org.foo.Bar /org/foo/Bar
//
//     The output will include all interfaces implemented by the `/org/foo/Bar`
//     object exported by the service at `org.foo.Bar`. Edit it to include only
//     the interface of interest and feed the result to this utility.
//
// After generation, the result should be formatted and an appropriate copyright
// header added.
//
// If no XML interface definition is available, a header file can be created
// manually following the style of the existing header files.

// Converts a relative path from src/ to an appropriate header guard.
std::string HeaderGuard(const base::FilePath& header) {
  // For now assumes command is being run from src/ and header is a relative
  // path from there. Can be made smarter if needed.

  std::string result = base::ToUpperASCII(header.MaybeAsASCII());
  base::ReplaceChars(result, "/.", "_", &result);
  return result + "_";
}

// Converts a D-Bus interface name into a valid C++ namespace identifier.
std::string Namespace(std::string_view interface) {
  std::string result;
  base::ReplaceChars(interface, ".", "_", &result);
  return result;
}

// Writes a tuple of parameters, with each parameter on its own line followed by
// a line comment giving the parameter name.
void WriteParameters(std::ostream& output, GDBusArgInfo** args) {
  if (args == nullptr || *args == nullptr) {
    output << "      \"()\"" << std::endl;
    return;
  }
  output << "      \"(\"" << std::endl;
  for (GDBusArgInfo** arg = args; *arg != nullptr; ++arg) {
    output << "      \"" << (**arg).signature << "\"  // " << (**arg).name
           << std::endl;
  }
  output << "      \")\"" << std::endl;
}

int main(int argc, char* argv[]) {
  base::CommandLine::Init(argc, argv);
  base::FilePath input_path =
      base::CommandLine::ForCurrentProcess()->GetSwitchValuePath("input");
  base::FilePath output_path =
      base::CommandLine::ForCurrentProcess()->GetSwitchValuePath("output");

  if (input_path.empty() || output_path.empty()) {
    std::cerr << "Usage: gen_dbus_interface --input=com.example.Interface.xml "
                 "--output=remoting/host/linux/dbus_interfaces/"
                 "com_example_Interface.h"
              << std::endl;
    return 1;
  }

  std::string header_guard = HeaderGuard(output_path);
  if (header_guard.empty()) {
    std::cerr << "Output name is not ASCII" << std::endl;
    return 1;
  }

  std::string xml;
  if (!base::ReadFileToString(input_path, &xml)) {
    std::cerr << "Failed to read input file.";
  }

  GError* error = nullptr;
  GDBusNodeInfo* node = g_dbus_node_info_new_for_xml(xml.c_str(), &error);
  if (error) {
    std::cerr << "Error parsing xml: " << error->message << std::endl;
    g_error_free(error);
    return 1;
  }

  std::ofstream output(output_path.value());
  if (!output) {
    std::cerr << "Failed to open output file for writing." << std::endl;
    g_dbus_node_info_unref(node);
    return 1;
  }

  output << "// This file was generated from " << input_path.BaseName().value()
         << std::endl
         << std::endl;

  output << "#ifndef " << header_guard << std::endl;
  output << "#define " << header_guard << std::endl << std::endl;

  output << "#include \"remoting/host/linux/gvariant_type.h\"" << std::endl
         << std::endl;

  output << "namespace remoting {" << std::endl << std::endl;

  for (GDBusInterfaceInfo** interface = node->interfaces;
       interface != nullptr && *interface != nullptr; ++interface) {
    std::string namespace_name = Namespace((**interface).name);
    output << "namespace " << namespace_name << " {" << std::endl << std::endl;

    for (GDBusMethodInfo** method = (**interface).methods;
         method != nullptr && *method != nullptr; ++method) {
      output << "// method" << std::endl;
      output << "struct " << (**method).name << " {" << std::endl;
      output << "  static constexpr char kInterfaceName[] = \""
             << (**interface).name << "\";" << std::endl;
      output << "  static constexpr char kMethodName[] = \"" << (**method).name
             << "\";" << std::endl;
      output << "  static constexpr gvariant::Type kInType{" << std::endl;
      WriteParameters(output, (**method).in_args);
      output << "  };" << std::endl;
      output << "  static constexpr gvariant::Type kOutType{" << std::endl;
      WriteParameters(output, (**method).out_args);
      output << "  };" << std::endl;
      output << "};" << std::endl << std::endl;
    }

    for (GDBusPropertyInfo** property = (**interface).properties;
         property != nullptr && *property != nullptr; ++property) {
      output << "// property" << std::endl;
      output << "struct " << (**property).name << " {" << std::endl;
      output << "  static constexpr char kInterfaceName[] = \""
             << (**interface).name << "\";" << std::endl;
      output << "  static constexpr char kPropertyName[] = \""
             << (**property).name << "\";" << std::endl;
      output << "  static constexpr gvariant::Type kType{\""
             << (**property).signature << "\"};" << std::endl;
      output << "  static constexpr bool kReadable = "
             << ((**property).flags & G_DBUS_PROPERTY_INFO_FLAGS_READABLE
                     ? "true;"
                     : "false;")
             << std::endl;
      output << "  static constexpr bool kWritable = "
             << ((**property).flags & G_DBUS_PROPERTY_INFO_FLAGS_WRITABLE
                     ? "true;"
                     : "false;")
             << std::endl;
      output << "};" << std::endl << std::endl;
    }

    for (GDBusSignalInfo** signal = (**interface).signals;
         signal != nullptr && *signal != nullptr; ++signal) {
      output << "// signal" << std::endl;
      output << "struct " << (**signal).name << " {" << std::endl;
      output << "  static constexpr char kInterfaceName[] = \""
             << (**interface).name << "\";" << std::endl;
      output << "  static constexpr char kSignalName[] = \"" << (**signal).name
             << "\";" << std::endl;
      output << "  static constexpr gvariant::Type kType{" << std::endl;
      WriteParameters(output, (**signal).args);
      output << "  };" << std::endl;
      output << "};" << std::endl << std::endl;
    }
    output << "}  // namespace " << namespace_name << std::endl << std::endl;
  }

  output << "}  // namespace remoting" << std::endl << std::endl;

  output << "#endif  // " << header_guard << std::endl;

  g_dbus_node_info_unref(node);
}
