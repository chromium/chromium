#!/usr/bin/env python3
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import filecmp
import os
import platform
import re
import subprocess
import sys
import typing

from xml.etree import ElementTree

_VK_XML_FILE = "third_party/vulkan-headers/src/registry/vk.xml"

_STRUCTS = [
  "VkExtensionProperties",
  "VkLayerProperties",
  "VkPhysicalDeviceProperties",
  "VkPhysicalDeviceFeatures",
  "VkQueueFamilyProperties",
]

_SELF_LOCATION = os.path.dirname(os.path.abspath(__file__))

_MOJO_TYPES = set([
  "uint8",
  "uint16",
  "uint32",
  "int8",
  "int16",
  "int32",
  "float",
  "string",
])

_VULKAN_BASIC_TYPE_MAP = set([
  "uint8_t",
  "uint16_t",
  "uint32_t",
  "uint64_t",
  "int8_t",
  "int16_t",
  "int32_t",
  "int64_t",
  "size_t",
  "VkBool32",
  "float",
  "char",
])

# types to mojo type
_type_map = {
  "uint8_t" : "uint8",
  "uint16_t" : "uint16",
  "uint32_t" : "uint32",
  "uint64_t" : "uint64",
  "int8_t" : "int8",
  "int16_t" : "int16",
  "int32_t" : "int32",
  "int64_t" : "int64",
  "size_t" : "uint64",
  "VkBool32" : "bool",
  "float" : "float",
  "char" : "char",
}

_structs = {}
_enums = {}
_defines = {}
_handles = set([])
_generated_types = []


def ValueNameToVALUE_NAME(name: str) -> str:
  return re.sub(
    r'(?<=[a-z])[A-Z]|(?<!^)[A-Z](?=[a-z])', r"_\g<0>", name).upper()


def ParseEnums(reg: re.Pattern) -> None:
  for type_elm in reg.findall("enums"):
    name = type_elm.get("name")
    if name == "API Constants":
      for enum in type_elm.findall("enum"):
        enum_name = enum.get("name")
        enum_value = enum.get("value")
        enum_alias = enum.get("alias")
        if enum_alias:
          continue
        _defines[enum_name] = enum_value
      continue

    # Skip VkResult and NameBits
    if name == "VkResult":
      value_name_prefix = "VK"
    elif name.endswith("FlagBits"):
      value_name_prefix = ValueNameToVALUE_NAME(name[:-len("FlagBits")])
    elif name.endswith("FlagBitsKHR"):
      value_name_prefix = ValueNameToVALUE_NAME(name[:-len("FlagBitsKHR")])
    else:
      value_name_prefix = ValueNameToVALUE_NAME(name)

    values = []
    for enum in type_elm.findall("enum"):
      enum_name = enum.get("name")
      enum_value = enum.get("value")
      mojom_name = enum_name[len(value_name_prefix) + 1:]
      values.append((enum_name, enum_value, mojom_name))

    _enums[name] = values


def ParseHandleElement(element: ElementTree.Element) -> None:
  name = element.get("name") or element.find("name").text
  _handles.add(name)


def ParseBaseTypeElement(element: ElementTree.Element) -> None:
  name = element.find("name").text
  _type = None if element.find("type") is None else element.find("type").text
  if name not in _type_map:
    _type_map[name] = _type


def ParseBitmaskElement(element: ElementTree.Element) -> None:
  name = element.find("name")
  if name is not None:
    name = name.text
    _type = None if element.find("type") is None else element.find("type").text
    _type_map[name] = _type


def ParseStructElement(element: ElementTree.Element) -> None:
  name = element.get("name") or element.find("name").text
  members = []
  for member in element.findall("member"):
    member_type = member.find("type").text
    member_name = member.find("name").text
    member_array_len = None
    for text in member.itertext():
      if text.startswith("["):
        member_array_len = text[1:-1]
        if not member_array_len:
          member_array_len = member.find("enum").text
        break
    members.append((member_name, member_type, member_array_len))
  _structs[name] = members


def ParseTypes(reg: re.Pattern) -> None:
  for type_elm in reg.findall("types/type"):
    category = type_elm.get("category")
    if category == "handle":
      ParseHandleElement(type_elm)
    elif category == "basetype":
      ParseBaseTypeElement(type_elm)
    elif category == "bitmask":
      ParseBitmaskElement(type_elm)
    elif category == "struct":
      ParseStructElement(type_elm)


def ParseVkXMLFile(path: str) -> None:
  tree = ElementTree.parse(path)
  reg = tree.getroot()
  ParseEnums(reg)
  ParseTypes(reg)


def WriteMojomEnum(name: str, mojom_file: typing.IO) -> None:
  if name in _generated_types:
    return
  _generated_types.append(name)

  values = _enums[name]
  mojom_file.write("\n")
  mojom_file.write("enum %s {\n" % name)
  for _, value, mojom_value_name in values:
    mojom_file.write("  %s = %s,\n" % (mojom_value_name, value))
  mojom_file.write("  INVALID_VALUE = -1,\n")
  mojom_file.write("};\n")


def WriteMojomStruct(name: str, mojom_file: typing.IO) -> None:
  if name in _generated_types:
    return
  _generated_types.append(name)

  fields = _structs[name]
  deps = []
  for field_name, field_type, array_len in fields:
    if field_type in _structs or field_type in _enums:
      deps.append(field_type)
  WriteMojomTypes(deps, mojom_file)

  mojom_file.write("\n")
  mojom_file.write("struct %s {\n" % name)
  for field_name, field_type, array_len in fields:
    if field_type in _type_map:
      while field_type in _type_map and field_type != _type_map[field_type]:
        field_type = _type_map[field_type]
    else:
      assert field_type in _structs or field_type in _enums or \
        field_type in _handles, "Undefine type: '%s'" % field_type
    if field_type == "char":
      assert array_len
      array_len = _defines[array_len]
      mojom_file.write("  string  %s;\n" % field_name)
    elif not array_len:
      mojom_file.write("  %s %s;\n" % (field_type, field_name))
    else:
      if not array_len.isdigit():
        array_len = _defines[array_len]
        assert array_len.isdigit(), "%s is not a digit." % array_len
      mojom_file.write(
        "  array<%s, %s> %s;\n" % (field_type, array_len, field_name))
  mojom_file.write("};\n")


def WriteMojomTypes(types: typing.Iterable[str], mojom_file: typing.IO) -> None:
  for t in types:
    if t in _structs:
      WriteMojomStruct(t, mojom_file)
    elif t in _enums:
      WriteMojomEnum(t, mojom_file)
    else:
      pass


def GenerateMojom(mojom_file: typing.IO) -> None:
  mojom_file.write(
'''// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/ipc/common/generate_vulkan_types.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

module gpu.mojom;
''')
  WriteMojomTypes(_STRUCTS, mojom_file)


def NormalizedCamelCase(identifier: str) -> None:
  result = identifier[0].upper()
  lowercase_next = True
  for i in range(1, len(identifier)):
    if identifier[i].isupper():
      if lowercase_next:
        result += identifier[i].lower()
      else:
        result += identifier[i]
      lowercase_next = True
    else:
      lowercase_next = False
      result += identifier[i]
  return result


def WriteStructTraits(name: str,
                      traits_header_file: typing.IO,
                      traits_source_file: typing.IO) -> None:
  traits_header_file.write(
"""
template <>
struct StructTraits<gpu::mojom::%sDataView, %s> {
""" % (name, name)
  )

  fields = _structs[name]
  for field_name, field_type, array_len in fields:
    if field_type == "VkBool32":
      field_type = "bool"
    elif field_type == "VkDeviceSize":
      field_type = "bool"

    if field_type == "char":
      assert array_len
      traits_header_file.write(
"""
  static std::string_view %s(const %s& input) {
    return input.%s;
  }
""" % (field_name, name, field_name))
    elif array_len:
      traits_header_file.write(
"""
  static base::span<const %s> %s(const %s& input) {
    return input.%s;
  }
""" % (field_type, field_name, name, field_name))
    elif field_type in _structs:
      traits_header_file.write(
"""
  static const %s& %s(const %s& input) {
    return input.%s;
  }
""" % (field_type, field_name, name, field_name))
    else:
      traits_header_file.write(
"""
  static %s %s(const %s& input) {
    return input.%s;
  }
""" % (field_type, field_name, name, field_name))

  traits_header_file.write(
"""
  static bool Read(gpu::mojom::%sDataView data, %s* out);
""" % (name, name))

  traits_source_file.write(
"""
// static
bool StructTraits<gpu::mojom::%sDataView, %s>::Read(
    gpu::mojom::%sDataView data, %s* out) {
""" % (name, name, name, name))

  fields = _structs[name]
  for field_name, field_type, array_len in fields:
    if field_type == "VkBool32":
      field_type = "bool"
    elif field_type == "VkDeviceSize":
      field_type = "bool"

    if field_type == "char":
      assert array_len
      read_method = "Read%s" % (NormalizedCamelCase(field_name))
      traits_source_file.write(
"""
  std::string_view %s;
  if (!data.%s(&%s))
    return false;
  %s.copy(out->%s, sizeof(out->%s));
""" % (field_name, read_method, field_name, field_name, field_name, field_name))
    elif array_len:
      read_method = "Read%s" % (NormalizedCamelCase(field_name))
      traits_source_file.write(
"""
  base::span<%s> %s(out->%s);
  if (!data.%s(&%s))
    return false;
""" % (field_type, field_name, field_name, read_method, field_name))
    elif field_type in _structs or field_type in _enums:
      traits_source_file.write(
"""
  if (!data.Read%s(&out->%s))
    return false;
""" % (NormalizedCamelCase(field_name), field_name))
    else:
      traits_source_file.write(
"""
  out->%s = data.%s();
""" % (field_name, field_name))


  traits_source_file.write(
"""
  return true;
}
""")


  traits_header_file.write("};\n")


def WriteEnumTraits(name: str, traits_header_file: typing.IO) -> None:
  traits_header_file.write(
"""
template <>
struct EnumTraits<gpu::mojom::%s, %s> {
  static gpu::mojom::%s ToMojom(%s input) {
    switch (input) {
""" % (name, name, name, name))

  for value_name, _, mojom_value_name in _enums[name]:
    traits_header_file.write(
"""
     case %s::%s:
       return gpu::mojom::%s::%s;"""
       % (name, value_name, name, mojom_value_name))

  traits_header_file.write(
"""
      default:
        NOTREACHED_IN_MIGRATION();
        return gpu::mojom::%s::INVALID_VALUE;
    }
  }

  static bool FromMojom(gpu::mojom::%s input, %s* out) {
    switch (input) {
""" % (name, name, name))

  for value_name, _, mojom_value_name in _enums[name]:
    traits_header_file.write(
"""
     case gpu::mojom::%s::%s:
       *out = %s::%s;
       return true;""" % (name, mojom_value_name, name, value_name))

  traits_header_file.write(
"""
      case gpu::mojom::%s::INVALID_VALUE:
        NOTREACHED_IN_MIGRATION();
        return false;

    }
    NOTREACHED_IN_MIGRATION();
    return false;
  }
};""" % name)



def GenerateTraitsFile(traits_header_file: typing.IO,
                       traits_source_file: typing.IO) -> None:
  traits_header_file.write(
"""// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/ipc/common/generate_vulkan_types.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

#ifndef GPU_IPC_COMMON_VULKAN_TYPES_MOJOM_TRAITS_H_
#define GPU_IPC_COMMON_VULKAN_TYPES_MOJOM_TRAITS_H_

#include <string_view>

#include "base/containers/span.h"
#include "gpu/ipc/common/vulkan_types.h"
#include "gpu/ipc/common/vulkan_types.mojom-shared.h"

namespace mojo {
""")

  traits_source_file.write(
"""// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/ipc/common/generate_vulkan_types.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

#include "gpu/ipc/common/vulkan_info_mojom_traits.h"

namespace mojo {
""")

  for t in _generated_types:
    if t in _structs:
      WriteStructTraits(t, traits_header_file, traits_source_file)
    elif t in _enums:
      WriteEnumTraits(t, traits_header_file)

  traits_header_file.write(
"""
}  // namespace mojo

#endif  // GPU_IPC_COMMON_VULKAN_TYPES_MOJOM_TRAITS_H_""")

  traits_source_file.write(
"""
}  // namespace mojo""")


def GenerateTypemapFile(typemap_file: typing.IO) -> None:
  typemap_file.write(
"""# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file is auto-generated from
# gpu/ipc/common/generate_vulkan_types.py
# DO NOT EDIT!

generated_vulkan_type_mappings = [""")
  for t in _generated_types:
    typemap_file.write(
"""
  {
    mojom = "gpu.mojom.%s"
    cpp = "::%s"
  },""" % (t, t))
  typemap_file.write("\n]\n")


def main() -> int:
  """This is the main function."""

  parser = argparse.ArgumentParser()
  parser.add_argument(
      "--output-dir",
      help="Output directory for generated files. Defaults to this script's "
      "directory.")
  parser.add_argument(
      "-c", "--check", action="store_true",
      help="Check if output files match generated files in chromium root "
      "directory. Use this in PRESUBMIT scripts with --output-dir.")

  args = parser.parse_args()

  # Support generating files for PRESUBMIT.
  if args.output_dir:
    output_dir = args.output_dir
  else:
    output_dir = _SELF_LOCATION

  def ClangFormat(filename):
    formatter = "clang-format"
    if platform.system() == "Windows":
      formatter += ".bat"
    subprocess.call([formatter, "-i", "-style=chromium", filename])

  vk_xml_file_path = os.path.join(
    _SELF_LOCATION, "../../..", _VK_XML_FILE)
  ParseVkXMLFile(vk_xml_file_path)


  mojom_file_name = "vulkan_types.mojom"
  mojom_file = open(
      os.path.join(output_dir, mojom_file_name), 'w', newline='')
  GenerateMojom(mojom_file)
  mojom_file.close()
  ClangFormat(mojom_file.name)

  traits_header_file_name = "vulkan_types_mojom_traits.h"
  traits_header_file = \
      open(os.path.join(output_dir, traits_header_file_name), 'w', newline='')
  traits_source_file_name = "vulkan_types_mojom_traits.cc"
  traits_source_file = \
      open(os.path.join(output_dir, traits_source_file_name), 'w', newline='')
  GenerateTraitsFile(traits_header_file, traits_source_file)
  traits_header_file.close()
  ClangFormat(traits_header_file.name)
  traits_source_file.close()
  ClangFormat(traits_source_file.name)

  typemap_file_name = "generated_vulkan_type_mappings.gni"
  typemap_file = open(
      os.path.join(output_dir, typemap_file_name), 'w', newline='')
  GenerateTypemapFile(typemap_file)
  typemap_file.close()

  check_failed_filenames = []
  if args.check:
    for filename in [mojom_file_name, traits_header_file_name,
                     traits_source_file_name, typemap_file_name]:
      if not filecmp.cmp(os.path.join(output_dir, filename),
                         os.path.join(_SELF_LOCATION, filename)):
        check_failed_filenames.append(filename)

  if len(check_failed_filenames) > 0:
    print('Please run gpu/ipc/common/generate_vulkan_types.py')
    print('Failed check on generated files:')
    for filename in check_failed_filenames:
      print(filename)
    return 1

  return 0


if __name__ == '__main__':
  sys.exit(main())
