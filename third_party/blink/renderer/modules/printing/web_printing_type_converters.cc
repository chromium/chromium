// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/printing/web_printing_type_converters.h"

#include "third_party/blink/public/mojom/printing/web_printing.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_unsignedlong_webprintingrange.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_print_color_mode.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_print_job_template_attributes.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_printer_attributes.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_printer_state.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_printing_media_collection.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_printing_media_collection_requested.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_printing_media_size.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_printing_media_size_requested.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_printing_mime_media_type.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_printing_multiple_document_handling.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_printing_orientation_requested.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_printing_range.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_printing_resolution.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_printing_resolution_units.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_printing_sides.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/resolution_units.h"

namespace {
// copies:
using BlinkRange = blink::WebPrintingRange;
using MojomRangePtr = blink::mojom::blink::WebPrintingRangePtr;

// sides:
using V8Sides = blink::V8WebPrintingSides;
using MojomSides = blink::mojom::blink::WebPrintingSides;

// media-col:
using BlinkMediaCollection = blink::WebPrintingMediaCollection;
using BlinkMediaCollectionRequested =
    blink::WebPrintingMediaCollectionRequested;
using V8MediaSizeDimension = blink::V8UnionUnsignedLongOrWebPrintingRange;
using MojomMediaCollection = blink::mojom::blink::WebPrintingMediaCollection;
using MojomMediaCollectionRequested =
    blink::mojom::blink::WebPrintingMediaCollectionRequested;
using MojomMediaCollectionRequestedPtr =
    blink::mojom::blink::WebPrintingMediaCollectionRequestedPtr;
using MojomMediaSizeDimensionPtr =
    blink::mojom::blink::WebPrintingMediaSizeDimensionPtr;

// multiple-document-handling:
using V8MultipleDocumentHandling = blink::V8WebPrintingMultipleDocumentHandling;
using MojomMultipleDocumentHandling =
    blink::mojom::blink::WebPrintingMultipleDocumentHandling;

// orientation-requested:
using V8OrientationRequested = blink::V8WebPrintingOrientationRequested;
using MojomOrientationRequested =
    blink::mojom::blink::WebPrintingOrientationRequested;

// state:
using V8JobState = blink::V8WebPrintJobState;
using MojomJobState = blink::mojom::blink::WebPrintJobState;

// print-color-mode:
using V8ColorMode = blink::V8WebPrintColorMode;
using MojomColorMode = blink::mojom::blink::WebPrintColorMode;

// printer-state:
using V8PrinterState = blink::V8WebPrinterState;
using MojomPrinterState = blink::mojom::blink::WebPrinterState;

// printer-state-reason:
using V8PrinterStateReason = blink::V8WebPrinterStateReason;
using MojomPrinterStateReason = blink::mojom::blink::WebPrinterStateReason;
}  // namespace

namespace mojo {

template <>
struct TypeConverter<V8Sides, MojomSides> {
  static V8Sides Convert(const MojomSides& sides) {
    switch (sides) {
      case MojomSides::kOneSided:
        return V8Sides(V8Sides::Enum::kOneSided);
      case MojomSides::kTwoSidedShortEdge:
        return V8Sides(V8Sides::Enum::kTwoSidedShortEdge);
      case MojomSides::kTwoSidedLongEdge:
        return V8Sides(V8Sides::Enum::kTwoSidedLongEdge);
    }
  }
};

template <>
struct TypeConverter<MojomSides, V8Sides> {
  static MojomSides Convert(const V8Sides& sides) {
    switch (sides.AsEnum()) {
      case V8Sides::Enum::kOneSided:
        return MojomSides::kOneSided;
      case V8Sides::Enum::kTwoSidedShortEdge:
        return MojomSides::kTwoSidedShortEdge;
      case V8Sides::Enum::kTwoSidedLongEdge:
        return MojomSides::kTwoSidedLongEdge;
    }
  }
};

template <>
struct TypeConverter<V8MultipleDocumentHandling,
                     MojomMultipleDocumentHandling> {
  static V8MultipleDocumentHandling Convert(
      const MojomMultipleDocumentHandling& mdh) {
    switch (mdh) {
      case MojomMultipleDocumentHandling::kSeparateDocumentsCollatedCopies:
        return V8MultipleDocumentHandling(
            V8MultipleDocumentHandling::Enum::kSeparateDocumentsCollatedCopies);
      case MojomMultipleDocumentHandling::kSeparateDocumentsUncollatedCopies:
        return V8MultipleDocumentHandling(
            V8MultipleDocumentHandling::Enum::
                kSeparateDocumentsUncollatedCopies);
    }
  }
};

template <>
struct TypeConverter<MojomMultipleDocumentHandling,
                     V8MultipleDocumentHandling> {
  static MojomMultipleDocumentHandling Convert(
      const V8MultipleDocumentHandling& mdh) {
    switch (mdh.AsEnum()) {
      case V8MultipleDocumentHandling::Enum::kSeparateDocumentsCollatedCopies:
        return MojomMultipleDocumentHandling::kSeparateDocumentsCollatedCopies;
      case V8MultipleDocumentHandling::Enum::kSeparateDocumentsUncollatedCopies:
        return MojomMultipleDocumentHandling::
            kSeparateDocumentsUncollatedCopies;
    }
  }
};

template <>
struct TypeConverter<V8OrientationRequested, MojomOrientationRequested> {
  static V8OrientationRequested Convert(
      const MojomOrientationRequested& orientation) {
    switch (orientation) {
      case MojomOrientationRequested::kPortrait:
        return V8OrientationRequested(V8OrientationRequested::Enum::kPortrait);
      case MojomOrientationRequested::kLandscape:
        return V8OrientationRequested(V8OrientationRequested::Enum::kLandscape);
    }
  }
};

template <>
struct TypeConverter<MojomOrientationRequested, V8OrientationRequested> {
  static MojomOrientationRequested Convert(
      const V8OrientationRequested& orientation) {
    switch (orientation.AsEnum()) {
      case V8OrientationRequested::Enum::kPortrait:
        return MojomOrientationRequested::kPortrait;
      case V8OrientationRequested::Enum::kLandscape:
        return MojomOrientationRequested::kLandscape;
    }
  }
};

template <>
struct TypeConverter<BlinkRange*, MojomRangePtr> {
  static BlinkRange* Convert(const MojomRangePtr& range_in) {
    auto* range = blink::MakeGarbageCollected<BlinkRange>();
    range->setFrom(range_in->from);
    range->setTo(range_in->to);
    return range;
  }
};

template <>
struct TypeConverter<V8MediaSizeDimension*, MojomMediaSizeDimensionPtr> {
  static V8MediaSizeDimension* Convert(
      const MojomMediaSizeDimensionPtr& dimension) {
    return dimension->is_range()
               ? blink::MakeGarbageCollected<V8MediaSizeDimension>(
                     mojo::ConvertTo<BlinkRange*>(dimension->get_range()))
               : blink::MakeGarbageCollected<V8MediaSizeDimension>(
                     dimension->get_value());
  }
};

template <>
struct TypeConverter<BlinkMediaCollection*, MojomMediaCollection*> {
  static BlinkMediaCollection* Convert(
      const MojomMediaCollection* media_col_in) {
    auto* media_col = blink::MakeGarbageCollected<BlinkMediaCollection>();
    media_col->setMediaSizeName(media_col_in->media_size_name);
    auto* media_size =
        blink::MakeGarbageCollected<blink::WebPrintingMediaSize>();
    media_size->setYDimension(mojo::ConvertTo<V8MediaSizeDimension*>(
        media_col_in->media_size->y_dimension));
    media_size->setXDimension(mojo::ConvertTo<V8MediaSizeDimension*>(
        media_col_in->media_size->x_dimension));
    media_col->setMediaSize(media_size);
    return media_col;
  }
};

template <>
struct TypeConverter<MojomMediaCollectionRequestedPtr,
                     BlinkMediaCollectionRequested*> {
  static MojomMediaCollectionRequestedPtr Convert(
      const BlinkMediaCollectionRequested* media_col_in) {
    auto media_col = MojomMediaCollectionRequested::New();
    media_col->media_size = {
        base::checked_cast<int32_t>(media_col_in->mediaSize()->xDimension()),
        base::checked_cast<int32_t>(media_col_in->mediaSize()->yDimension())};
    return media_col;
  }
};

template <>
struct TypeConverter<gfx::Size, blink::WebPrintingResolution*> {
  static gfx::Size Convert(
      const blink::WebPrintingResolution* printer_resolution) {
    CHECK(printer_resolution->hasCrossFeedDirectionResolution());
    CHECK(printer_resolution->hasFeedDirectionResolution());
    if (printer_resolution->hasUnits() &&
        printer_resolution->units() ==
            blink::V8WebPrintingResolutionUnits::Enum::kDotsPerCentimeter) {
      return gfx::Size(printer_resolution->crossFeedDirectionResolution() *
                           blink::kCentimetersPerInch,
                       printer_resolution->feedDirectionResolution() *
                           blink::kCentimetersPerInch);
    }
    return gfx::Size(printer_resolution->crossFeedDirectionResolution(),
                     printer_resolution->feedDirectionResolution());
  }
};

template <>
struct TypeConverter<blink::WebPrintingResolution*, gfx::Size> {
  static blink::WebPrintingResolution* Convert(
      const gfx::Size& printer_resolution) {
    auto* output_resolution =
        blink::MakeGarbageCollected<blink::WebPrintingResolution>();
    output_resolution->setCrossFeedDirectionResolution(
        printer_resolution.width());
    output_resolution->setFeedDirectionResolution(printer_resolution.height());
    output_resolution->setUnits(
        blink::V8WebPrintingResolutionUnits::Enum::kDotsPerInch);
    return output_resolution;
  }
};

template <>
struct TypeConverter<V8ColorMode, MojomColorMode> {
  static V8ColorMode Convert(const MojomColorMode& color_mode) {
    switch (color_mode) {
      case MojomColorMode::kColor:
        return V8ColorMode(V8ColorMode::Enum::kColor);
      case MojomColorMode::kMonochrome:
        return V8ColorMode(V8ColorMode::Enum::kMonochrome);
    }
  }
};

template <>
struct TypeConverter<MojomColorMode, V8ColorMode> {
  static MojomColorMode Convert(const V8ColorMode& color_mode) {
    switch (color_mode.AsEnum()) {
      case V8ColorMode::Enum::kColor:
        return MojomColorMode::kColor;
      case V8ColorMode::Enum::kMonochrome:
        return MojomColorMode::kMonochrome;
    }
  }
};

template <>
struct TypeConverter<V8PrinterState::Enum, MojomPrinterState> {
  static V8PrinterState::Enum Convert(const MojomPrinterState& printer_state) {
    switch (printer_state) {
      case MojomPrinterState::kIdle:
        return V8PrinterState::Enum::kIdle;
      case MojomPrinterState::kProcessing:
        return V8PrinterState::Enum::kProcessing;
      case MojomPrinterState::kStopped:
        return V8PrinterState::Enum::kStopped;
    }
  }
};

template <>
struct TypeConverter<V8PrinterStateReason, MojomPrinterStateReason> {
  static V8PrinterStateReason Convert(
      const MojomPrinterStateReason& printer_state_reason) {
    switch (printer_state_reason) {
      case MojomPrinterStateReason::kNone:
        return V8PrinterStateReason(V8PrinterStateReason::Enum::kNone);
      case MojomPrinterStateReason::kOther:
        return V8PrinterStateReason(V8PrinterStateReason::Enum::kOther);
      case MojomPrinterStateReason::kConnectingToDevice:
        return V8PrinterStateReason(
            V8PrinterStateReason::Enum::kConnectingToDevice);
      case MojomPrinterStateReason::kCoverOpen:
        return V8PrinterStateReason(V8PrinterStateReason::Enum::kCoverOpen);
      case MojomPrinterStateReason::kDeveloperEmpty:
        return V8PrinterStateReason(
            V8PrinterStateReason::Enum::kDeveloperEmpty);
      case MojomPrinterStateReason::kDeveloperLow:
        return V8PrinterStateReason(V8PrinterStateReason::Enum::kDeveloperLow);
      case MojomPrinterStateReason::kDoorOpen:
        return V8PrinterStateReason(V8PrinterStateReason::Enum::kDoorOpen);
      case MojomPrinterStateReason::kFuserOverTemp:
        return V8PrinterStateReason(V8PrinterStateReason::Enum::kFuserOverTemp);
      case MojomPrinterStateReason::kFuserUnderTemp:
        return V8PrinterStateReason(
            V8PrinterStateReason::Enum::kFuserUnderTemp);
      case MojomPrinterStateReason::kInputTrayMissing:
        return V8PrinterStateReason(
            V8PrinterStateReason::Enum::kInputTrayMissing);
      case MojomPrinterStateReason::kInterlockOpen:
        return V8PrinterStateReason(V8PrinterStateReason::Enum::kInterlockOpen);
      case MojomPrinterStateReason::kInterpreterResourceUnavailable:
        return V8PrinterStateReason(
            V8PrinterStateReason::Enum::kInterpreterResourceUnavailable);
      case MojomPrinterStateReason::kMarkerSupplyEmpty:
        return V8PrinterStateReason(
            V8PrinterStateReason::Enum::kMarkerSupplyEmpty);
      case MojomPrinterStateReason::kMarkerSupplyLow:
        return V8PrinterStateReason(
            V8PrinterStateReason::Enum::kMarkerSupplyLow);
      case MojomPrinterStateReason::kMarkerWasteAlmostFull:
        return V8PrinterStateReason(
            V8PrinterStateReason::Enum::kMarkerWasteAlmostFull);
      case MojomPrinterStateReason::kMarkerWasteFull:
        return V8PrinterStateReason(
            V8PrinterStateReason::Enum::kMarkerWasteFull);
      case MojomPrinterStateReason::kMediaEmpty:
        return V8PrinterStateReason(V8PrinterStateReason::Enum::kMediaEmpty);
      case MojomPrinterStateReason::kMediaJam:
        return V8PrinterStateReason(V8PrinterStateReason::Enum::kMediaJam);
      case MojomPrinterStateReason::kMediaLow:
        return V8PrinterStateReason(V8PrinterStateReason::Enum::kMediaLow);
      case MojomPrinterStateReason::kMediaNeeded:
        return V8PrinterStateReason(V8PrinterStateReason::Enum::kMediaNeeded);
      case MojomPrinterStateReason::kMovingToPaused:
        return V8PrinterStateReason(
            V8PrinterStateReason::Enum::kMovingToPaused);
      case MojomPrinterStateReason::kOpcLifeOver:
        return V8PrinterStateReason(V8PrinterStateReason::Enum::kOpcLifeOver);
      case MojomPrinterStateReason::kOpcNearEol:
        return V8PrinterStateReason(V8PrinterStateReason::Enum::kOpcNearEol);
      case MojomPrinterStateReason::kOutputAreaAlmostFull:
        return V8PrinterStateReason(
            V8PrinterStateReason::Enum::kOutputAreaAlmostFull);
      case MojomPrinterStateReason::kOutputAreaFull:
        return V8PrinterStateReason(
            V8PrinterStateReason::Enum::kOutputAreaFull);
      case MojomPrinterStateReason::kOutputTrayMissing:
        return V8PrinterStateReason(
            V8PrinterStateReason::Enum::kOutputTrayMissing);
      case MojomPrinterStateReason::kPaused:
        return V8PrinterStateReason(V8PrinterStateReason::Enum::kPaused);
      case MojomPrinterStateReason::kShutdown:
        return V8PrinterStateReason(V8PrinterStateReason::Enum::kShutdown);
      case MojomPrinterStateReason::kSpoolAreaFull:
        return V8PrinterStateReason(V8PrinterStateReason::Enum::kSpoolAreaFull);
      case MojomPrinterStateReason::kStoppedPartly:
        return V8PrinterStateReason(V8PrinterStateReason::Enum::kStoppedPartly);
      case MojomPrinterStateReason::kStopping:
        return V8PrinterStateReason(V8PrinterStateReason::Enum::kStopping);
      case MojomPrinterStateReason::kTimedOut:
        return V8PrinterStateReason(V8PrinterStateReason::Enum::kTimedOut);
      case MojomPrinterStateReason::kTonerEmpty:
        return V8PrinterStateReason(V8PrinterStateReason::Enum::kTonerEmpty);
      case MojomPrinterStateReason::kTonerLow:
        return V8PrinterStateReason(V8PrinterStateReason::Enum::kTonerLow);
      case MojomPrinterStateReason::kCupsPkiExpired:
        return V8PrinterStateReason(
            V8PrinterStateReason::Enum::kCupsPkiExpired);
    }
  }
};

}  // namespace mojo

namespace blink {

namespace {

void ProcessCopies(const mojom::blink::WebPrinterAttributes& new_attributes,
                   WebPrinterAttributes* current_attributes) {
  current_attributes->setCopiesDefault(new_attributes.copies_default);
  current_attributes->setCopiesSupported(
      mojo::ConvertTo<BlinkRange*>(new_attributes.copies_supported));
}

void ProcessDocumentFormat(
    const mojom::blink::WebPrinterAttributes& new_attributes,
    WebPrinterAttributes* current_attributes) {
  current_attributes->setDocumentFormatDefault(
      V8WebPrintingMimeMediaType::Enum::kApplicationPdf);
  current_attributes->setDocumentFormatSupported({V8WebPrintingMimeMediaType(
      V8WebPrintingMimeMediaType::Enum::kApplicationPdf)});
}

void ProcessMediaCollection(
    const mojom::blink::WebPrinterAttributes& new_attributes,
    WebPrinterAttributes* current_attributes) {
  current_attributes->setMediaColDefault(
      mojo::ConvertTo<BlinkMediaCollection*>(new_attributes.media_col_default));
  current_attributes->setMediaColDatabase(
      mojo::ConvertTo<HeapVector<Member<BlinkMediaCollection>>>(
          new_attributes.media_col_database));
}

void ProcessMediaSource(
    const mojom::blink::WebPrinterAttributes& new_attributes,
    WebPrinterAttributes* current_attributes) {
  if (new_attributes.media_source_default) {
    current_attributes->setMediaSourceDefault(
        new_attributes.media_source_default);
  }
  if (!new_attributes.media_source_supported.empty()) {
    current_attributes->setMediaSourceSupported(
        new_attributes.media_source_supported);
  }
}

void ProcessMultipleDocumentHandling(
    const mojom::blink::WebPrinterAttributes& new_attributes,
    WebPrinterAttributes* current_attributes) {
  current_attributes->setMultipleDocumentHandlingDefault(
      mojo::ConvertTo<V8MultipleDocumentHandling>(
          new_attributes.multiple_document_handling_default));
  current_attributes->setMultipleDocumentHandlingSupported(
      mojo::ConvertTo<Vector<V8MultipleDocumentHandling>>(
          new_attributes.multiple_document_handling_supported));
}

void ProcessOrientationRequested(
    const mojom::blink::WebPrinterAttributes& new_attributes,
    WebPrinterAttributes* current_attributes) {
  current_attributes->setOrientationRequestedDefault(
      mojo::ConvertTo<V8OrientationRequested>(
          new_attributes.orientation_requested_default));
  current_attributes->setOrientationRequestedSupported(
      mojo::ConvertTo<Vector<V8OrientationRequested>>(
          new_attributes.orientation_requested_supported));
}

void ProcessPrinterResolution(
    const mojom::blink::WebPrinterAttributes& new_attributes,
    WebPrinterAttributes* current_attributes) {
  current_attributes->setPrinterResolutionDefault(
      mojo::ConvertTo<blink::WebPrintingResolution*>(
          new_attributes.printer_resolution_default));
  current_attributes->setPrinterResolutionSupported(
      mojo::ConvertTo<HeapVector<Member<blink::WebPrintingResolution>>>(
          new_attributes.printer_resolution_supported));
}

void ProcessPrintColorMode(
    const mojom::blink::WebPrinterAttributes& new_attributes,
    WebPrinterAttributes* current_attributes) {
  current_attributes->setPrintColorModeDefault(
      mojo::ConvertTo<V8ColorMode>(new_attributes.print_color_mode_default));
  current_attributes->setPrintColorModeSupported(
      mojo::ConvertTo<Vector<V8ColorMode>>(
          new_attributes.print_color_mode_supported));
}

void ProcessSides(const mojom::blink::WebPrinterAttributes& new_attributes,
                  WebPrinterAttributes* current_attributes) {
  if (new_attributes.sides_default) {
    current_attributes->setSidesDefault(
        mojo::ConvertTo<V8Sides>(*new_attributes.sides_default));
  }
  if (!new_attributes.sides_supported.empty()) {
    current_attributes->setSidesSupported(
        mojo::ConvertTo<Vector<V8Sides>>(new_attributes.sides_supported));
  }
}

}  // namespace

}  // namespace blink

namespace mojo {

blink::WebPrinterAttributes*
TypeConverter<blink::WebPrinterAttributes*,
              blink::mojom::blink::WebPrinterAttributesPtr>::
    Convert(const blink::mojom::blink::WebPrinterAttributesPtr&
                printer_attributes) {
  auto* attributes = blink::WebPrinterAttributes::Create();

  blink::ProcessCopies(*printer_attributes, attributes);
  blink::ProcessDocumentFormat(*printer_attributes, attributes);
  blink::ProcessMediaCollection(*printer_attributes, attributes);
  blink::ProcessMediaSource(*printer_attributes, attributes);
  blink::ProcessMultipleDocumentHandling(*printer_attributes, attributes);
  blink::ProcessOrientationRequested(*printer_attributes, attributes);
  blink::ProcessPrinterResolution(*printer_attributes, attributes);
  blink::ProcessPrintColorMode(*printer_attributes, attributes);
  blink::ProcessSides(*printer_attributes, attributes);

  attributes->setPrinterState(
      mojo::ConvertTo<V8PrinterState::Enum>(printer_attributes->printer_state));
  attributes->setPrinterStateReasons(
      mojo::ConvertTo<Vector<V8PrinterStateReason>>(
          printer_attributes->printer_state_reasons));
  attributes->setPrinterStateMessage(printer_attributes->printer_state_message);

  return attributes;
}

blink::mojom::blink::WebPrintJobTemplateAttributesPtr
TypeConverter<blink::mojom::blink::WebPrintJobTemplateAttributesPtr,
              blink::WebPrintJobTemplateAttributes*>::
    Convert(const blink::WebPrintJobTemplateAttributes* pjt_attributes) {
  auto attributes = blink::mojom::blink::WebPrintJobTemplateAttributes::New();

  attributes->copies = pjt_attributes->getCopiesOr(1);
  if (pjt_attributes->hasMediaCol()) {
    attributes->media_col = mojo::ConvertTo<MojomMediaCollectionRequestedPtr>(
        pjt_attributes->mediaCol());
  }
  if (pjt_attributes->hasMediaSource()) {
    attributes->media_source = pjt_attributes->mediaSource();
  }
  if (pjt_attributes->hasMultipleDocumentHandling()) {
    attributes->multiple_document_handling =
        mojo::ConvertTo<MojomMultipleDocumentHandling>(
            pjt_attributes->multipleDocumentHandling());
  }
  if (pjt_attributes->hasOrientationRequested()) {
    attributes->orientation_requested =
        mojo::ConvertTo<MojomOrientationRequested>(
            pjt_attributes->orientationRequested());
  }
  if (pjt_attributes->hasPrinterResolution()) {
    attributes->printer_resolution =
        mojo::ConvertTo<gfx::Size>(pjt_attributes->printerResolution());
  }
  if (pjt_attributes->hasPrintColorMode()) {
    attributes->print_color_mode =
        mojo::ConvertTo<MojomColorMode>(pjt_attributes->printColorMode());
  }
  if (pjt_attributes->hasSides()) {
    attributes->sides = mojo::ConvertTo<MojomSides>(pjt_attributes->sides());
  }

  return attributes;
}

V8JobState::Enum TypeConverter<V8JobState::Enum, MojomJobState>::Convert(
    const MojomJobState& state) {
  switch (state) {
    case MojomJobState::kPending:
      return V8JobState::Enum::kPending;
    case MojomJobState::kProcessing:
      return V8JobState::Enum::kProcessing;
    case MojomJobState::kCompleted:
      return V8JobState::Enum::kCompleted;
    case MojomJobState::kCanceled:
      return V8JobState::Enum::kCanceled;
    case MojomJobState::kAborted:
      return V8JobState::Enum::kAborted;
  }
}

}  // namespace mojo
