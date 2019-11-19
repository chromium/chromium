#!/usr/bin/env python
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import filecmp
import optparse
import os.path
import platform
import re
import subprocess
import sys

_VULKAN_HEADER_FILE = "third_party/vulkan/include/vulkan/vulkan_core.h"

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


def ValueNameToVALUE_NAME(name):
  return re.sub(
    r'(?<=[a-z])[A-Z]|(?<!^)[A-Z](?=[a-z])', r"_\g<0>", name).upper()


def ParseHandle(line):
  if line.startswith("VK_DEFINE_HANDLE("):
    name = line[len("VK_DEFINE_HANDLE("):-1]
  elif line.startswith("VK_DEFINE_NON_DISPATCHABLE_HANDLE("):
    name = line[len("VK_DEFINE_NON_DISPATCHABLE_HANDLE("):-1]
  elif line.startswith("VK_DEFINE_DISPATCHABLE_HANDLE("):
    name = line[len("VK_DEFINE_DISPATCHABLE_HANDLE("):-1]
  else:
    return
  _handles.add(name)


def ParseTypedef(line):
  # typedef Type1 Type1;
  line = line.rstrip(';')
  line = line.split()
  if len(line) == 3:
    typedef, t1, t2 = line
    assert typedef == "typedef"
    # We would like to use bool instead uint32 for VkBool32
    if t2 == "VkBool32":
      return
    if t1 in _type_map:
      _type_map[t2] = _type_map[t1]
    else:
      assert t1 in _structs or t1 in _enums or t1 in _handles, \
        "Undefined type '%s'" % t1
  else:
    pass
    # skip typdef for function pointer


def ParseEnum(line, header_file):
  # typedef enum kName {
  # ...
  # } kName;
  name = line.split()[2]

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
  while True:
    line = header_file.readline().strip()
    # } kName;
    if line == "} %s;" % name:
        break
    # VK_NAME = value,
    value_name, value = line.rstrip(',').split(" = ")
    if not value.isdigit():
      # Ignore VK_NAME_BEGIN_RANGE
      # Ignore VK_NAME_END_RANGE
      # Ignore VK_NAME_RANGE_SIZE
      # Ignore VK_NAME_MAX_ENUM = 0x7FFFFFFF
      continue
    assert len(value_name_prefix) + 1 < len(value_name), \
        "Wrong enum value name `%s`" % value_name
    mojom_value_name = value_name[len(value_name_prefix) + 1:]
    values.append((value_name, value, mojom_value_name))
  assert name not in _enums, "enum '%s' has been defined." % name
  _enums[name] = values


def ParseStruct(line, header_file):
  # typedef struct kName {
  # ...
  # } kName;
  name = line.split()[2]

  fields = []
  while True:
    line = header_file.readline().strip()
    # } kName;
    if line == "} %s;" % name:
        break
    # type name;
    # const type name;
    # type name[L];
    line = line.rstrip(";")
    field_type, field_name = line.rsplit(None, 1)
    array_len = None
    if '[' in field_name:
      assert ']' in field_name
      field_name, array_len = field_name.rstrip(']').split('[')
      assert array_len.isdigit() or array_len in _defines
    fields.append((field_name, field_type, array_len))
  assert name not in _structs, "struct '%s' has been defined." % name
  _structs[name] = fields


def ParseDefine(line):
  # not parse multi-line macros
  if line.endswith('\\'):
    return
  # not parse #define NAME() ...
  if '(' in line or ')' in line:
    return

  define, name, value = line.split()
  assert define == "#define"
  assert name not in _defines, "macro '%s' has been defined." % name
  _defines[name] = value


def ParseVulkanHeaderFile(path):
  with open(path) as header_file:
    while True:
      line = header_file.readline()
      if not line:
        break
      line = line.strip()

      if line.startswith("#define"):
        ParseDefine(line)
      elif line.startswith("typedef enum "):
        ParseEnum(line, header_file)
      elif line.startswith("typedef struct "):
        ParseStruct(line, header_file)
      elif line.startswith("typedef "):
        ParseTypedef(line)
      elif line.startswith("VK_DEFINE_"):
        ParseHandle(line)


def WriteMojomEnum(name, mojom_file):
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


def WriteMojomStruct(name, mojom_file):
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


def WriteMojomTypes(types, mojom_file):
  for t in types:
    if t in _structs:
      WriteMojomStruct(t, mojom_file)
    elif t in _enums:
      WriteMojomEnum(t, mojom_file)
    else:
      pass


def GenerateMojom(mojom_file):
  mojom_file.write(
'''// Copyright 2019 The Chromium Authors. All rights reserved.
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


def WriteStructTraits(name, traits_header_file, traits_source_file):
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
  static base::StringPiece %s(const %s& input) {
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
      read_method = "Read%s%s" % (field_name[0].upper(), field_name[1:])
      traits_source_file.write(
"""
  base::StringPiece %s;
  if (!data.%s(&%s))
    return false;
  %s.copy(out->%s, sizeof(out->%s));
""" % (field_name, read_method, field_name, field_name, field_name, field_name))
    elif array_len:
      read_method = "Read%s%s" % (field_name[0].upper(), field_name[1:])
      traits_source_file.write(
"""
  base::span<%s> %s(out->%s);
  if (!data.%s(&%s))
    return false;
""" % (field_type, field_name, field_name, read_method, field_name))
    elif field_type in _structs or field_type in _enums:
      traits_source_file.write(
"""
  if (!data.Read%s%s(&out->%s))
    return false;
""" % (field_name[0].upper(), field_name[1:], field_name))
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


def WriteEnumTraits(name, traits_header_file):
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
        NOTREACHED();
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
        NOTREACHED();
        return false;

    }
    NOTREACHED();
    return false;
  }
};""" % name)



def GenerateTraitsFile(traits_header_file, traits_source_file):
  traits_header_file.write(
"""// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/ipc/common/generate_vulkan_types.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

#ifndef GPU_IPC_COMMON_VULKAN_TYPES_MOJOM_TRAITS_H_
#define GPU_IPC_COMMON_VULKAN_TYPES_MOJOM_TRAITS_H_

#include "base/containers/span.h"
#include "base/strings/string_piece.h"
#include "gpu/ipc/common/vulkan_types.h"
#include "gpu/ipc/common/vulkan_types.mojom-shared.h"

namespace mojo {
""")

  traits_source_file.write(
"""// Copyright 2019 The Chromium Authors. All rights reserved.
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


def GenerateTypemapFile(typemap_file):
  typemap_file.write(
"""# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file is auto-generated from
# gpu/ipc/common/generate_vulkan_types.py
# DO NOT EDIT!

mojom = "//gpu/ipc/common/vulkan_types.mojom"
public_headers = [ "//gpu/ipc/common/vulkan_types.h" ]
traits_headers = [ "//gpu/ipc/common/vulkan_types_mojom_traits.h" ]
sources = [
  "//gpu/ipc/common/vulkan_types_mojom_traits.cc",
]
public_deps = [
  "//gpu/ipc/common:vulkan_types",
]
type_mappings = [
""")
  for t in _generated_types:
    typemap_file.write("  \"gpu.mojom.%s=::%s\",\n" % (t, t))
  typemap_file.write("]\n")


def main(argv):
  """This is the main function."""

  parser = optparse.OptionParser()
  parser.add_option(
      "--output-dir",
      help="Output directory for generated files. Defaults to this script's "
      "directory.")
  parser.add_option(
      "-c", "--check", action="store_true",
      help="Check if output files match generated files in chromium root "
      "directory. Use this in PRESUBMIT scripts with --output-dir.")

  (options, _) = parser.parse_args(args=argv)

  # Support generating files for PRESUBMIT.
  if options.output_dir:
    output_dir = options.output_dir
  else:
    output_dir = _SELF_LOCATION

  def ClangFormat(filename):
    formatter = "clang-format"
    if platform.system() == "Windows":
      formatter += ".bat"
    subprocess.call([formatter, "-i", "-style=chromium", filename])

  vulkan_header_file_path = os.path.join(
    _SELF_LOCATION, "../../..", _VULKAN_HEADER_FILE)
  ParseVulkanHeaderFile(vulkan_header_file_path)

  mojom_file_name = "vulkan_types.mojom"
  mojom_file = open(
      os.path.join(output_dir, mojom_file_name), 'wb')
  GenerateMojom(mojom_file)
  mojom_file.close()
  ClangFormat(mojom_file.name)

  traits_header_file_name = "vulkan_types_mojom_traits.h"
  traits_header_file = \
      open(os.path.join(output_dir, traits_header_file_name), 'wb')
  traits_source_file_name = "vulkan_types_mojom_traits.cc"
  traits_source_file = \
      open(os.path.join(output_dir, traits_source_file_name), 'wb')
  GenerateTraitsFile(traits_header_file, traits_source_file)
  traits_header_file.close()
  ClangFormat(traits_header_file.name)
  traits_source_file.close()
  ClangFormat(traits_source_file.name)

  typemap_file_name = "vulkan_types.typemap"
  typemap_file = open(
      os.path.join(output_dir, typemap_file_name), 'wb')
  GenerateTypemapFile(typemap_file)
  typemap_file.close()

  check_failed_filenames = []
  if options.check:
    for filename in [mojom_file_name, traits_header_file_name,
                     traits_source_file_name, typemap_file_name]:
      if not filecmp.cmp(os.path.join(output_dir, filename),
                         os.path.join(_SELF_LOCATION, filename)):
        check_failed_filenames.append(filename)

  if len(check_failed_filenames) > 0:
    print 'Please run gpu/ipc/common/generate_vulkan_types.py'
    print 'Failed check on generated files:'
    for filename in check_failed_filenames:
      print filename
    return 1

  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv))
