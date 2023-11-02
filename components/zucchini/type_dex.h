// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ZUCCHINI_TYPE_DEX_H_
#define COMPONENTS_ZUCCHINI_TYPE_DEX_H_

#include <stdint.h>

namespace zucchini {
namespace dex {
// Contains types that models DEX executable format data structures.
// See https://source.android.com/devices/tech/dalvik/dex-format

// The supported versions are 035, 037, 038, and 039.

enum class FormatId : uint8_t {
  b,  // 22b.
  c,  // 21c, 22c, 31c, 35c, 3rc, 45cc, 4rcc.
  h,  // 21h.
  i,  // 31i.
  l,  // 51l.
  n,  // 11n.
  s,  // 21s, 22s.
  t,  // 10t, 20t, 21t, 22t, 30t, 31t.
  x,  // 10x, 11x, 12x, 22x, 23x, 32x.
};

struct Instruction {
  Instruction() = default;
  constexpr Instruction(uint8_t opcode_in,
                        uint8_t layout_in,
                        FormatId format_in,
                        uint8_t variant_in = 1)
      : opcode(opcode_in),
        layout(layout_in),
        format(format_in),
        variant(variant_in) {}

  // The opcode that identifies the instruction.
  uint8_t opcode;
  // Number of uint16_t units for the instruction.
  uint8_t layout;
  // Identifier that groups similar instructions, as quick filter.
  FormatId format;
  // Number of successive opcodes that have the same format.
  uint8_t variant = 1;
};

constexpr Instruction kByteCode[] = {
    {0x00, 1, FormatId::x},
    {0x01, 1, FormatId::x},
    {0x02, 2, FormatId::x},
    {0x03, 3, FormatId::x},
    {0x04, 1, FormatId::x},
    {0x05, 2, FormatId::x},
    {0x06, 3, FormatId::x},
    {0x07, 1, FormatId::x},
    {0x08, 2, FormatId::x},
    {0x09, 3, FormatId::x},
    {0x0A, 1, FormatId::x},
    {0x0B, 1, FormatId::x},
    {0x0C, 1, FormatId::x},
    {0x0D, 1, FormatId::x},
    {0x0E, 1, FormatId::x},
    {0x0F, 1, FormatId::x},
    {0x10, 1, FormatId::x},
    {0x11, 1, FormatId::x},
    {0x12, 1, FormatId::n},
    {0x13, 2, FormatId::s},
    {0x14, 3, FormatId::i},
    {0x15, 2, FormatId::h},
    {0x16, 2, FormatId::s},
    {0x17, 3, FormatId::i},
    {0x18, 5, FormatId::l},
    {0x19, 2, FormatId::h},
    {0x1A, 2, FormatId::c},
    {0x1B, 3, FormatId::c},
    {0x1C, 2, FormatId::c},
    {0x1D, 1, FormatId::x},
    {0x1E, 1, FormatId::x},
    {0x1F, 2, FormatId::c},
    {0x20, 2, FormatId::c},
    {0x21, 1, FormatId::x},
    {0x22, 2, FormatId::c},
    {0x23, 2, FormatId::c},
    {0x24, 3, FormatId::c},
    {0x25, 3, FormatId::c},
    {0x26, 3, FormatId::t},
    {0x27, 1, FormatId::x},
    {0x28, 1, FormatId::t},
    {0x29, 2, FormatId::t},
    {0x2A, 3, FormatId::t},
    {0x2B, 3, FormatId::t},
    {0x2C, 3, FormatId::t},
    {0x2D, 2, FormatId::x, 5},
    {0x32, 2, FormatId::t, 6},
    {0x38, 2, FormatId::t, 6},
    // {0x3E, 1, FormatId::x, 6}, unused
    {0x44, 2, FormatId::x, 14},
    {0x52, 2, FormatId::c, 14},
    {0x60, 2, FormatId::c, 14},
    {0x6E, 3, FormatId::c, 5},
    // {0x73, 1, FormatId::x}, unused
    {0x74, 3, FormatId::c, 5},
    // {0x79, 1, FormatId::x, 2}, unused
    {0x7B, 1, FormatId::x, 21},
    {0x90, 2, FormatId::x, 32},
    {0xB0, 1, FormatId::x, 32},
    {0xD0, 2, FormatId::s, 8},
    {0xD8, 2, FormatId::b, 11},
    // {0xE3, 1, FormatId::x, 29}, unused
    {0xFA, 4, FormatId::c},
    {0xFB, 4, FormatId::c},
    {0xFC, 3, FormatId::c},
    {0xFD, 3, FormatId::c},
    {0xFE, 2, FormatId::c},
    {0xFF, 2, FormatId::c},
};

// Supported by MSVC, g++, and clang++. Ensures no gaps in packing.
#pragma pack(push, 1)

// header_item: Appears in the header section.
struct HeaderItem {
  uint8_t magic[8];
  uint32_t checksum;
  uint8_t signature[20];
  uint32_t file_size;
  uint32_t header_size;
  uint32_t endian_tag;
  uint32_t link_size;
  uint32_t link_off;
  uint32_t map_off;
  uint32_t string_ids_size;
  uint32_t string_ids_off;
  uint32_t type_ids_size;
  uint32_t type_ids_off;
  uint32_t proto_ids_size;
  uint32_t proto_ids_off;
  uint32_t field_ids_size;
  uint32_t field_ids_off;
  uint32_t method_ids_size;
  uint32_t method_ids_off;
  uint32_t class_defs_size;
  uint32_t class_defs_off;
  uint32_t data_size;
  uint32_t data_off;
};

// string_id_item: String identifiers list.
struct StringIdItem {
  uint32_t string_data_off;
};

// type_id_item: Type identifiers list.
struct TypeIdItem {
  uint32_t descriptor_idx;
};

// proto_id_item: Method prototype identifiers list.
struct ProtoIdItem {
  uint32_t shorty_idx;
  uint32_t return_type_idx;
  uint32_t parameters_off;
};

// field_id_item: Field identifiers list.
struct FieldIdItem {
  uint16_t class_idx;
  uint16_t type_idx;
  uint32_t name_idx;
};

// method_id_item: Method identifiers list.
struct MethodIdItem {
  uint16_t class_idx;
  uint16_t proto_idx;
  uint32_t name_idx;
};

// class_def_item: Class definitions list.
struct ClassDefItem {
  uint32_t class_idx;
  uint32_t access_flags;
  uint32_t superclass_idx;
  uint32_t interfaces_off;
  uint32_t source_file_idx;
  uint32_t annotations_off;
  uint32_t class_data_off;
  uint32_t static_values_off;
};

// call_site_id_item: Call site identifiers list.
struct CallSiteIdItem {
  uint32_t call_site_off;
};

// method_handle_type: Determines the behavior of the MethodHandleItem.
enum class MethodHandleType : uint16_t {
  // FieldId
  kStaticPut = 0x00,
  kStaticGet = 0x01,
  kInstancePut = 0x02,
  kInstanceGet = 0x03,
  // MethodId
  kInvokeStatic = 0x04,
  kInvokeInstance = 0x05,
  kInvokeConstructor = 0x06,
  kInvokeDirect = 0x07,
  kInvokeInterface = 0x08,
  // Sentinel. If new types are added put them before this and increment.
  kMaxMethodHandleType = 0x09
};

// method_handle_item: Method handles referred within the Dex file.
struct MethodHandleItem {
  uint16_t method_handle_type;
  uint16_t unused_1;
  uint16_t field_or_method_id;
  uint16_t unused_2;
};

// code_item: Header of a code item.
struct CodeItem {
  uint16_t registers_size;
  uint16_t ins_size;
  uint16_t outs_size;
  uint16_t tries_size;
  uint32_t debug_info_off;
  uint32_t insns_size;
  // Variable length data follow for complete code item.
};

// Number of valid type codes for map_item elements in map_list.
// See: https://source.android.com/devices/tech/dalvik/dex-format#type-codes
constexpr uint32_t kMaxItemListSize = 21;

constexpr uint16_t kTypeHeaderItem = 0x0000;
constexpr uint16_t kTypeStringIdItem = 0x0001;
constexpr uint16_t kTypeTypeIdItem = 0x0002;
constexpr uint16_t kTypeProtoIdItem = 0x0003;
constexpr uint16_t kTypeFieldIdItem = 0x0004;
constexpr uint16_t kTypeMethodIdItem = 0x0005;
constexpr uint16_t kTypeClassDefItem = 0x0006;
constexpr uint16_t kTypeCallSiteIdItem = 0x0007;
constexpr uint16_t kTypeMethodHandleItem = 0x0008;
constexpr uint16_t kTypeMapList = 0x1000;
constexpr uint16_t kTypeTypeList = 0x1001;
constexpr uint16_t kTypeAnnotationSetRefList = 0x1002;
constexpr uint16_t kTypeAnnotationSetItem = 0x1003;
constexpr uint16_t kTypeClassDataItem = 0x2000;
constexpr uint16_t kTypeCodeItem = 0x2001;
constexpr uint16_t kTypeStringDataItem = 0x2002;
constexpr uint16_t kTypeDebugInfoItem = 0x2003;
constexpr uint16_t kTypeAnnotationItem = 0x2004;
constexpr uint16_t kTypeEncodedArrayItem = 0x2005;
constexpr uint16_t kTypeAnnotationsDirectoryItem = 0x2006;
constexpr uint16_t kTypeHiddenApiClassDataItem = 0xF000;

// map_item
struct MapItem {
  uint16_t type;
  uint16_t unused;
  uint32_t size;
  uint32_t offset;
};

// map_list
struct MapList {
  uint32_t size;
  MapItem list[kMaxItemListSize];
};

// type_item
struct TypeItem {
  uint16_t type_idx;
};

// annotation_set_ref_item
struct AnnotationSetRefItem {
  uint32_t annotations_off;
};

// annotation_off_item
struct AnnotationOffItem {
  uint32_t annotation_off;
};

// field_annotation
struct FieldAnnotation {
  uint32_t field_idx;
  uint32_t annotations_off;
};

// method_annotation
struct MethodAnnotation {
  uint32_t method_idx;
  uint32_t annotations_off;
};

// parameter_annotation
struct ParameterAnnotation {
  uint32_t method_idx;
  uint32_t annotations_off;
};

// annotations_directory_item
struct AnnotationsDirectoryItem {
  uint32_t class_annotations_off;
  uint32_t fields_size;
  uint32_t annotated_methods_size;
  uint32_t annotated_parameters_size;
  // FieldAnnotation field_annotations[fields_size];
  // MethodAnnotation method_annotations[annotated_methods_size];
  // ParameterAnnotation parameter_annotations[annotated_parameters_size];
  // All *Annotation are 8 bytes each.
};

// try_item
struct TryItem {
  uint32_t start_addr;
  uint16_t insn_count;
  uint16_t handler_off;
};

#pragma pack(pop)

}  // namespace dex
}  // namespace zucchini

#endif  // COMPONENTS_ZUCCHINI_TYPE_DEX_H_
