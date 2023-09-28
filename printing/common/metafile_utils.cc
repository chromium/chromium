// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/common/metafile_utils.h"

#include "base/check.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "printing/buildflags/buildflags.h"
#include "third_party/skia/include/codec/SkPngDecoder.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/core/SkString.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "third_party/skia/include/docs/SkPDFDocument.h"
#include "third_party/skia/include/encode/SkPngEncoder.h"
#include "third_party/skia/include/private/chromium/SkImageChromium.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_update.h"

#include <variant>

namespace {

// Table 333 in PDF 32000-1:2008 spec, section 14.8.4.2
const char kPDFStructureTypeDocument[] = "Document";
const char kPDFStructureTypeParagraph[] = "P";
const char kPDFStructureTypeDiv[] = "Div";
const char kPDFStructureTypeHeading[] = "H";
const char kPDFStructureTypeLink[] = "Link";
const char kPDFStructureTypeList[] = "L";
const char kPDFStructureTypeListItemLabel[] = "Lbl";
const char kPDFStructureTypeListItemBody[] = "LI";
const char kPDFStructureTypeTable[] = "Table";
const char kPDFStructureTypeTableRow[] = "TR";
const char kPDFStructureTypeTableHeader[] = "TH";
const char kPDFStructureTypeTableCell[] = "TD";
const char kPDFStructureTypeFigure[] = "Figure";
const char kPDFStructureTypeNonStruct[] = "NonStruct";

// Standard attribute owners from PDF 32000-1:2008 spec, section 14.8.5.2
// (Attribute owners are kind of like "categories" for structure node
// attributes.)
const char kPDFTableAttributeOwner[] = "Table";

// Table Attributes from PDF 32000-1:2008 spec, section 14.8.5.7
const char kPDFTableCellColSpanAttribute[] = "ColSpan";
const char kPDFTableCellHeadersAttribute[] = "Headers";
const char kPDFTableCellRowSpanAttribute[] = "RowSpan";
const char kPDFTableHeaderScopeAttribute[] = "Scope";
const char kPDFTableHeaderScopeColumn[] = "Column";
const char kPDFTableHeaderScopeRow[] = "Row";

SkString GetHeadingStructureType(int heading_level) {
  // From Table 333 in PDF 32000-1:2008 spec, section 14.8.4.2,
  // "H1"..."H6" are valid structure types.
  if (heading_level >= 1 && heading_level <= 6)
    return SkString(base::StringPrintf("H%d", heading_level).c_str());

  // If we don't have a valid heading level, use the generic heading role.
  return SkString(kPDFStructureTypeHeading);
}

SkPDF::DateTime TimeToSkTime(base::Time time) {
  base::Time::Exploded exploded;
  time.UTCExplode(&exploded);
  return SkPDF::DateTime{
      .fTimeZoneMinutes = 0,
      .fYear = static_cast<uint16_t>(exploded.year),
      .fMonth = static_cast<uint8_t>(exploded.month),
      .fDayOfWeek = static_cast<uint8_t>(exploded.day_of_week),
      .fDay = static_cast<uint8_t>(exploded.day_of_month),
      .fHour = static_cast<uint8_t>(exploded.hour),
      .fMinute = static_cast<uint8_t>(exploded.minute),
      .fSecond = static_cast<uint8_t>(exploded.second)};
}

sk_sp<SkPicture> GetEmptyPicture() {
  SkPictureRecorder rec;
  SkCanvas* canvas = rec.beginRecording(100, 100);
  // Add some ops whose net effects equal to a noop.
  canvas->save();
  canvas->restore();
  return rec.finishRecordingAsPicture();
}

// Convert an AXNode into a SkPDF::StructureElementNode in order to make a
// tagged (accessible) PDF. Returns true on success and false if we don't
// have enough data to build a valid tree.
bool RecursiveBuildStructureTree(const ui::AXNode* ax_node,
                                 SkPDF::StructureElementNode* tag) {
  bool valid = false;

  tag->fNodeId = ax_node->GetIntAttribute(ax::mojom::IntAttribute::kDOMNodeId);
  switch (ax_node->GetRole()) {
    case ax::mojom::Role::kRootWebArea:
      tag->fTypeString = kPDFStructureTypeDocument;
      break;
    case ax::mojom::Role::kParagraph:
      tag->fTypeString = kPDFStructureTypeParagraph;
      break;
    case ax::mojom::Role::kGenericContainer:
      tag->fTypeString = kPDFStructureTypeDiv;
      break;
    case ax::mojom::Role::kHeading:
      tag->fTypeString = GetHeadingStructureType(ax_node->GetIntAttribute(
          ax::mojom::IntAttribute::kHierarchicalLevel));
      break;
    case ax::mojom::Role::kLink:
      tag->fTypeString = kPDFStructureTypeLink;
      break;
    case ax::mojom::Role::kList:
      tag->fTypeString = kPDFStructureTypeList;
      break;
    case ax::mojom::Role::kListMarker:
      tag->fTypeString = kPDFStructureTypeListItemLabel;
      break;
    case ax::mojom::Role::kListItem:
      tag->fTypeString = kPDFStructureTypeListItemBody;
      break;
    case ax::mojom::Role::kTable:
      tag->fTypeString = kPDFStructureTypeTable;
      break;
    case ax::mojom::Role::kRow:
      tag->fTypeString = kPDFStructureTypeTableRow;
      break;
    case ax::mojom::Role::kColumnHeader:
      tag->fTypeString = kPDFStructureTypeTableHeader;
      tag->fAttributes.appendName(kPDFTableAttributeOwner,
                                  kPDFTableHeaderScopeAttribute,
                                  kPDFTableHeaderScopeColumn);
      break;
    case ax::mojom::Role::kRowHeader:
      tag->fTypeString = kPDFStructureTypeTableHeader;
      tag->fAttributes.appendName(kPDFTableAttributeOwner,
                                  kPDFTableHeaderScopeAttribute,
                                  kPDFTableHeaderScopeRow);
      break;
    case ax::mojom::Role::kCell: {
      tag->fTypeString = kPDFStructureTypeTableCell;

      // Append an attribute consisting of the string IDs of all of the
      // header cells that correspond to this table cell.
      std::vector<ui::AXNode*> header_nodes;
      ax_node->GetTableCellColHeaders(&header_nodes);
      ax_node->GetTableCellRowHeaders(&header_nodes);
      std::vector<int> header_ids;
      header_ids.reserve(header_nodes.size());
      for (ui::AXNode* header_node : header_nodes) {
        header_ids.push_back(header_node->GetIntAttribute(
            ax::mojom::IntAttribute::kDOMNodeId));
      }
      tag->fAttributes.appendNodeIdArray(
          kPDFTableAttributeOwner, kPDFTableCellHeadersAttribute, header_ids);
      break;
    }
    case ax::mojom::Role::kImage:
      // TODO(thestig): Figure out if the `ax::mojom::Role::kFigure` case should
      // share code with the `ax::mojom::Role::kImage` case, and if `valid`
      // should be set.
      valid = true;
      [[fallthrough]];
    case ax::mojom::Role::kFigure: {
      tag->fTypeString = kPDFStructureTypeFigure;
      std::string alt =
          ax_node->GetStringAttribute(ax::mojom::StringAttribute::kName);
      tag->fAlt = SkString(alt.c_str());
      break;
    }
    case ax::mojom::Role::kStaticText:
      tag->fTypeString = kPDFStructureTypeNonStruct;
      valid = true;
      break;
    default:
      tag->fTypeString = kPDFStructureTypeNonStruct;
      break;
  }

  if (ui::IsCellOrTableHeader(ax_node->GetRole())) {
    absl::optional<int> row_span = ax_node->GetTableCellRowSpan();
    if (row_span.has_value()) {
      tag->fAttributes.appendInt(kPDFTableAttributeOwner,
                                 kPDFTableCellRowSpanAttribute,
                                 row_span.value());
    }
    absl::optional<int> col_span = ax_node->GetTableCellColSpan();
    if (col_span.has_value()) {
      tag->fAttributes.appendInt(kPDFTableAttributeOwner,
                                 kPDFTableCellColSpanAttribute,
                                 col_span.value());
    }
  }

  std::string lang = ax_node->GetLanguage();
  std::string parent_lang =
      ax_node->parent() ? ax_node->parent()->GetLanguage() : "";
  if (!lang.empty() && lang != parent_lang)
    tag->fLang = lang.c_str();

  tag->fChildVector.resize(ax_node->GetUnignoredChildCount());
  for (size_t i = 0; i < tag->fChildVector.size(); i++) {
    tag->fChildVector[i] = std::make_unique<SkPDF::StructureElementNode>();
    valid |= RecursiveBuildStructureTree(ax_node->GetUnignoredChildAtIndex(i),
                                         tag->fChildVector[i].get());
  }

  return valid;
}

}  // namespace

namespace printing {

sk_sp<SkDocument> MakePdfDocument(base::StringPiece creator,
                                  const ui::AXTreeUpdate& accessibility_tree,
                                  SkWStream* stream) {
  SkPDF::Metadata metadata;
  SkPDF::DateTime now = TimeToSkTime(base::Time::Now());
  metadata.fCreation = now;
  metadata.fModified = now;
  // TODO(crbug.com/691162): Switch to SkString's string_view constructor when
  // possible.
  metadata.fCreator = creator.empty()
                          ? SkString("Chromium")
                          : SkString(creator.data(), creator.size());
  metadata.fRasterDPI = 300.0f;

  SkPDF::StructureElementNode tag_root = {};
  if (!accessibility_tree.nodes.empty()) {
    ui::AXTree tree(accessibility_tree);
    if (RecursiveBuildStructureTree(tree.root(), &tag_root))
      metadata.fStructureElementTreeRoot = &tag_root;
  }

  return SkPDF::MakeDocument(stream, metadata);
}

sk_sp<SkData> SerializeOopPicture(SkPicture* pic, void* ctx) {
  const auto* context = reinterpret_cast<const ContentToProxyTokenMap*>(ctx);
  uint32_t pic_id = pic->uniqueID();
  auto iter = context->find(pic_id);
  if (iter == context->end())
    return nullptr;

  return SkData::MakeWithCopy(&pic_id, sizeof(pic_id));
}

sk_sp<SkPicture> DeserializeOopPicture(const void* data,
                                       size_t length,
                                       void* ctx) {
  uint32_t pic_id;
  if (length < sizeof(pic_id)) {
    NOTREACHED();  // Should not happen if the content is as written.
    return GetEmptyPicture();
  }
  memcpy(&pic_id, data, sizeof(pic_id));

  auto* context = reinterpret_cast<PictureDeserializationContext*>(ctx);
  auto iter = context->find(pic_id);
  if (iter == context->end() || !iter->second) {
    // When we don't have the out-of-process picture available, we return
    // an empty picture. Returning a nullptr will cause the deserialization
    // crash.
    return GetEmptyPicture();
  }
  return iter->second;
}

sk_sp<SkData> SerializeOopTypeface(SkTypeface* typeface, void* ctx) {
  auto* context = reinterpret_cast<TypefaceSerializationContext*>(ctx);
  SkTypefaceID typeface_id = typeface->uniqueID();
  bool data_included = context->insert(typeface_id).second;

  // Need the typeface ID to identify the desired typeface.  Include an
  // indicator for when typeface data actually follows vs. when the typeface
  // should already exist in a cache when deserializing.
  SkDynamicMemoryWStream stream;
  stream.write32(typeface_id);
  stream.writeBool(data_included);
  if (data_included) {
    typeface->serialize(&stream, SkTypeface::SerializeBehavior::kDoIncludeData);
  }
  return stream.detachAsData();
}

sk_sp<SkTypeface> DeserializeOopTypeface(const void* data,
                                         size_t length,
                                         void* ctx) {
  SkStream* stream = *(reinterpret_cast<SkStream**>(const_cast<void*>(data)));
  if (length < sizeof(stream)) {
    NOTREACHED();  // Should not happen if the content is as written.
    return nullptr;
  }

  SkTypefaceID id;
  if (!stream->readU32(&id)) {
    return nullptr;
  }
  bool data_included;
  if (!stream->readBool(&data_included)) {
    return nullptr;
  }

  auto* context = reinterpret_cast<TypefaceDeserializationContext*>(ctx);
  auto iter = context->find(id);
  if (iter != context->end()) {
    DCHECK(!data_included);
    return iter->second;
  }

  // Typeface not encountered before, expect it to be present in the stream.
  DCHECK(data_included);
  sk_sp<SkTypeface> typeface = SkTypeface::MakeDeserialize(stream);
  context->emplace(id, typeface);
  return typeface;
}

sk_sp<SkData> SerializeRasterImage(SkImage* img, void*) {
  if (!img) {
    return nullptr;
  }
  // TODO(crbug.com/1486503)
  DUMP_WILL_BE_CHECK(!img->isTextureBacked());
  if (img->isTextureBacked()) {
    GrDirectContext* ctx = SkImages::GetContext(img);
    return SkPngEncoder::Encode(ctx, img, SkPngEncoder::Options{});
  }
  return SkPngEncoder::Encode(nullptr, img, SkPngEncoder::Options{});
}

sk_sp<SkImage> DeserializeRasterImage(const void* bytes, size_t length, void*) {
  auto data = SkData::MakeWithoutCopy(bytes, length);
  auto codec = SkPngDecoder::Decode(data, nullptr);
  if (codec) {
    return std::get<0>(codec->getImage());
  }
  return nullptr;
}

SkSerialProcs SerializationProcs(PictureSerializationContext* picture_ctx,
                                 TypefaceSerializationContext* typeface_ctx) {
  SkSerialProcs procs;
  procs.fImageProc = SerializeRasterImage;
  procs.fPictureProc = SerializeOopPicture;
  procs.fPictureCtx = picture_ctx;
  procs.fTypefaceProc = SerializeOopTypeface;
  procs.fTypefaceCtx = typeface_ctx;
  return procs;
}

SkDeserialProcs DeserializationProcs(
    PictureDeserializationContext* picture_ctx,
    TypefaceDeserializationContext* typeface_ctx) {
  SkDeserialProcs procs;
  procs.fImageProc = DeserializeRasterImage;
  procs.fPictureProc = DeserializeOopPicture;
  procs.fPictureCtx = picture_ctx;
  procs.fTypefaceProc = DeserializeOopTypeface;
  procs.fTypefaceCtx = typeface_ctx;
  return procs;
}

}  // namespace printing
