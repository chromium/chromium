// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ZUCCHINI_DISASSEMBLER_DEX_H_
#define COMPONENTS_ZUCCHINI_DISASSEMBLER_DEX_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/zucchini/disassembler.h"
#include "components/zucchini/image_utils.h"
#include "components/zucchini/type_dex.h"

namespace zucchini {

// For consistency, let "canonical order" of DEX data types be the order defined
// in https://source.android.com/devices/tech/dalvik/dex-format "Type Codes"
// section.

class DisassemblerDex : public Disassembler {
 public:
  static constexpr uint16_t kVersion = 1;
  // Pools follow canonical order.
  enum ReferencePool : uint8_t {
    kStringId,
    kTypeId,
    kProtoId,
    kFieldId,
    kMethodId,
    // kClassDef,  // Unused
    kCallSiteId,
    kMethodHandle,
    kTypeList,
    kAnnotationSetRefList,
    kAnnotionSet,
    kClassData,
    kCode,
    kStringData,
    kAnnotation,
    kEncodedArray,
    kAnnotationsDirectory,
    kCallSite,
    kNumPools
  };

  // Types are grouped and ordered by target ReferencePool. This is required by
  // Zucchini-apply, which visits references by type order and sequentially
  // handles pools in the same order. Type-pool association is established in
  // MakeReferenceGroups(), and verified by a unit test.
  enum ReferenceType : uint8_t {
    kTypeIdToDescriptorStringId,  // kStringId
    kProtoIdToShortyStringId,
    kFieldIdToNameStringId,
    kMethodIdToNameStringId,
    kClassDefToSourceFileStringId,
    kCodeToStringId16,
    kCodeToStringId32,

    kProtoIdToReturnTypeId,  // kTypeId
    kFieldIdToClassTypeId,
    kFieldIdToTypeId,
    kMethodIdToClassTypeId,
    kClassDefToClassTypeId,
    kClassDefToSuperClassTypeId,
    kTypeListToTypeId,
    kCodeToTypeId,

    kCodeToProtoId,  // kProtoId
    kMethodIdToProtoId,

    kCodeToFieldId,  // kFieldId
    kMethodHandleToFieldId,
    kAnnotationsDirectoryToFieldId,

    kCodeToMethodId,  // kMethodId
    kMethodHandleToMethodId,
    kAnnotationsDirectoryToMethodId,
    kAnnotationsDirectoryToParameterMethodId,

    kCodeToCallSiteId,  // kCallSiteId

    kCodeToMethodHandle,  // kMethodHandle

    kProtoIdToParametersTypeList,  // kTypeList
    kClassDefToInterfacesTypeList,

    kAnnotationsDirectoryToParameterAnnotationSetRef,  // kAnnotationSetRef,

    kAnnotationSetRefListToAnnotationSet,  // kAnnotationSet,
    kAnnotationsDirectoryToClassAnnotationSet,
    kAnnotationsDirectoryToFieldAnnotationSet,
    kAnnotationsDirectoryToMethodAnnotationSet,

    kClassDefToClassData,  // kClassData

    kCodeToRelCode8,  // kCode
    kCodeToRelCode16,
    kCodeToRelCode32,

    kStringIdToStringData,  // kStringData

    kAnnotationSetToAnnotation,  // kAnnotation

    kClassDefToStaticValuesEncodedArray,  // kEncodedArrayItem

    kClassDefToAnnotationDirectory,  // kAnnotationsDirectory

    kCallSiteIdToCallSite,  // kCallSite

    kNumTypes
  };

  DisassemblerDex();
  DisassemblerDex(const DisassemblerDex&) = delete;
  const DisassemblerDex& operator=(const DisassemblerDex&) = delete;
  ~DisassemblerDex() override;

  // Applies quick checks to determine if |image| *may* point to the start of an
  // executable. Returns true on success.
  static bool QuickDetect(ConstBufferView image);

  // Disassembler:
  ExecutableType GetExeType() const override;
  std::string GetExeTypeString() const override;
  std::vector<ReferenceGroup> MakeReferenceGroups() const override;

  // Functions that return reference readers. These follow canonical order of
  // *locations* (unlike targets for ReferenceType). This allows functions with
  // similar parsing logic to appear togeter.
  std::unique_ptr<ReferenceReader> MakeReadStringIdToStringData(offset_t lo,
                                                                offset_t hi);
  std::unique_ptr<ReferenceReader> MakeReadTypeIdToDescriptorStringId32(
      offset_t lo,
      offset_t hi);
  std::unique_ptr<ReferenceReader> MakeReadProtoIdToShortyStringId32(
      offset_t lo,
      offset_t hi);
  std::unique_ptr<ReferenceReader> MakeReadProtoIdToReturnTypeId32(offset_t lo,
                                                                   offset_t hi);
  std::unique_ptr<ReferenceReader> MakeReadProtoIdToParametersTypeList(
      offset_t lo,
      offset_t hi);
  std::unique_ptr<ReferenceReader> MakeReadFieldToClassTypeId16(offset_t lo,
                                                                offset_t hi);
  std::unique_ptr<ReferenceReader> MakeReadFieldToTypeId16(offset_t lo,
                                                           offset_t hi);
  std::unique_ptr<ReferenceReader> MakeReadFieldToNameStringId32(offset_t lo,
                                                                 offset_t hi);
  std::unique_ptr<ReferenceReader> MakeReadMethodIdToClassTypeId16(offset_t lo,
                                                                   offset_t hi);
  std::unique_ptr<ReferenceReader> MakeReadMethodIdToProtoId16(offset_t lo,
                                                               offset_t hi);
  std::unique_ptr<ReferenceReader> MakeReadMethodIdToNameStringId32(
      offset_t lo,
      offset_t hi);
  std::unique_ptr<ReferenceReader> MakeReadClassDefToClassTypeId32(offset_t lo,
                                                                   offset_t hi);
  std::unique_ptr<ReferenceReader> MakeReadClassDefToSuperClassTypeId32(
      offset_t lo,
      offset_t hi);
  std::unique_ptr<ReferenceReader> MakeReadClassDefToInterfacesTypeList(
      offset_t lo,
      offset_t hi);
  std::unique_ptr<ReferenceReader> MakeReadClassDefToSourceFileStringId32(
      offset_t lo,
      offset_t hi);
  std::unique_ptr<ReferenceReader> MakeReadClassDefToAnnotationDirectory(
      offset_t lo,
      offset_t hi);
  std::unique_ptr<ReferenceReader> MakeReadClassDefToClassData(offset_t lo,
                                                               offset_t hi);
  std::unique_ptr<ReferenceReader> MakeReadClassDefToStaticValuesEncodedArray(
      offset_t lo,
      offset_t hi);
  std::unique_ptr<ReferenceReader> MakeReadCallSiteIdToCallSite32(offset_t lo,
                                                                  offset_t hi);
  std::unique_ptr<ReferenceReader> MakeReadMethodHandleToFieldId16(offset_t lo,
                                                                   offset_t hi);
  std::unique_ptr<ReferenceReader> MakeReadMethodHandleToMethodId16(
      offset_t lo,
      offset_t hi);
  std::unique_ptr<ReferenceReader> MakeReadTypeListToTypeId16(offset_t lo,
                                                              offset_t hi);
  std::unique_ptr<ReferenceReader> MakeReadAnnotationSetToAnnotation(
      offset_t lo,
      offset_t hi);
  std::unique_ptr<ReferenceReader> MakeReadAnnotationSetRefListToAnnotationSet(
      offset_t lo,
      offset_t hi);
  std::unique_ptr<ReferenceReader>
  MakeReadAnnotationsDirectoryToClassAnnotationSet(offset_t lo, offset_t hi);
  std::unique_ptr<ReferenceReader> MakeReadAnnotationsDirectoryToFieldId32(
      offset_t lo,
      offset_t hi);
  std::unique_ptr<ReferenceReader>
  MakeReadAnnotationsDirectoryToFieldAnnotationSet(offset_t lo, offset_t hi);
  std::unique_ptr<ReferenceReader> MakeReadAnnotationsDirectoryToMethodId32(
      offset_t lo,
      offset_t hi);
  std::unique_ptr<ReferenceReader>
  MakeReadAnnotationsDirectoryToMethodAnnotationSet(offset_t lo, offset_t hi);
  std::unique_ptr<ReferenceReader>
  MakeReadAnnotationsDirectoryToParameterMethodId32(offset_t lo, offset_t hi);
  std::unique_ptr<ReferenceReader>
  MakeReadAnnotationsDirectoryToParameterAnnotationSetRef(offset_t lo,
                                                          offset_t hi);
  std::unique_ptr<ReferenceReader> MakeReadCodeToStringId16(offset_t lo,
                                                            offset_t hi);
  std::unique_ptr<ReferenceReader> MakeReadCodeToStringId32(offset_t lo,
                                                            offset_t hi);
  std::unique_ptr<ReferenceReader> MakeReadCodeToTypeId16(offset_t lo,
                                                          offset_t hi);
  std::unique_ptr<ReferenceReader> MakeReadCodeToProtoId16(offset_t lo,
                                                           offset_t hi);
  std::unique_ptr<ReferenceReader> MakeReadCodeToFieldId16(offset_t lo,
                                                           offset_t hi);
  std::unique_ptr<ReferenceReader> MakeReadCodeToMethodId16(offset_t lo,
                                                            offset_t hi);
  std::unique_ptr<ReferenceReader> MakeReadCodeToCallSiteId16(offset_t lo,
                                                              offset_t hi);
  std::unique_ptr<ReferenceReader> MakeReadCodeToMethodHandle16(offset_t lo,
                                                                offset_t hi);
  std::unique_ptr<ReferenceReader> MakeReadCodeToRelCode8(offset_t lo,
                                                          offset_t hi);
  std::unique_ptr<ReferenceReader> MakeReadCodeToRelCode16(offset_t lo,
                                                           offset_t hi);
  std::unique_ptr<ReferenceReader> MakeReadCodeToRelCode32(offset_t lo,
                                                           offset_t hi);

  // Functions that return reference writers. Different readers may share a
  // common writer. Therefore these loosely follow canonical order of locations,
  std::unique_ptr<ReferenceWriter> MakeWriteStringId16(MutableBufferView image);
  std::unique_ptr<ReferenceWriter> MakeWriteStringId32(MutableBufferView image);
  std::unique_ptr<ReferenceWriter> MakeWriteTypeId16(MutableBufferView image);
  std::unique_ptr<ReferenceWriter> MakeWriteTypeId32(MutableBufferView image);
  std::unique_ptr<ReferenceWriter> MakeWriteProtoId16(MutableBufferView image);
  std::unique_ptr<ReferenceWriter> MakeWriteFieldId16(MutableBufferView image);
  std::unique_ptr<ReferenceWriter> MakeWriteFieldId32(MutableBufferView image);
  std::unique_ptr<ReferenceWriter> MakeWriteMethodId16(MutableBufferView image);
  std::unique_ptr<ReferenceWriter> MakeWriteMethodId32(MutableBufferView image);
  std::unique_ptr<ReferenceWriter> MakeWriteCallSiteId16(
      MutableBufferView image);
  std::unique_ptr<ReferenceWriter> MakeWriteMethodHandle16(
      MutableBufferView image);
  std::unique_ptr<ReferenceWriter> MakeWriteRelCode8(MutableBufferView image);
  std::unique_ptr<ReferenceWriter> MakeWriteRelCode16(MutableBufferView image);
  std::unique_ptr<ReferenceWriter> MakeWriteRelCode32(MutableBufferView image);
  std::unique_ptr<ReferenceWriter> MakeWriteAbs32(MutableBufferView image);

 private:
  friend Disassembler;
  using MapItemMap =
      std::map<uint16_t, raw_ptr<const dex::MapItem, CtnExperimental>>;

  // Disassembler:
  bool Parse(ConstBufferView image) override;

  bool ParseHeader();

  raw_ptr<const dex::HeaderItem> header_ = nullptr;
  int dex_version_ = 0;
  MapItemMap map_item_map_ = {};
  dex::MapItem string_map_item_ = {};
  dex::MapItem type_map_item_ = {};
  dex::MapItem proto_map_item_ = {};
  dex::MapItem field_map_item_ = {};
  dex::MapItem method_map_item_ = {};
  dex::MapItem class_def_map_item_ = {};
  dex::MapItem call_site_map_item_ = {};
  dex::MapItem method_handle_map_item_ = {};
  dex::MapItem type_list_map_item_ = {};
  dex::MapItem annotation_set_ref_list_map_item_ = {};
  dex::MapItem annotation_set_map_item_ = {};
  dex::MapItem code_map_item_ = {};
  dex::MapItem annotations_directory_map_item_ = {};

  // Sorted list of offsets of parsed items in |image_|.
  std::vector<offset_t> code_item_offsets_;
  std::vector<offset_t> type_list_offsets_;
  std::vector<offset_t> annotation_set_ref_list_offsets_;
  std::vector<offset_t> annotation_set_offsets_;
  std::vector<offset_t> annotations_directory_item_offsets_;
  std::vector<offset_t> annotations_directory_item_field_annotation_offsets_;
  std::vector<offset_t> annotations_directory_item_method_annotation_offsets_;
  std::vector<offset_t>
      annotations_directory_item_parameter_annotation_offsets_;
};

}  // namespace zucchini

#endif  // COMPONENTS_ZUCCHINI_DISASSEMBLER_DEX_H_
