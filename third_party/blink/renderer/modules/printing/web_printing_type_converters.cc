// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/printing/web_printing_type_converters.h"

#include "third_party/blink/public/mojom/printing/web_printing.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_printer_attributes.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_printing_mime_media_type.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_printing_multiple_document_handling.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_printing_range.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_printing_sides.h"

namespace {
// sides:
using V8Sides = blink::V8WebPrintingSides;
using MojomSides = blink::mojom::blink::WebPrintingSides;

// multiple-document-handling:
using V8MultipleDocumentHandling = blink::V8WebPrintingMultipleDocumentHandling;
using MojomMultipleDocumentHandling =
    blink::mojom::blink::WebPrintingMultipleDocumentHandling;
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

}  // namespace mojo

namespace blink {

namespace {

void ProcessCopies(const mojom::blink::WebPrinterAttributes& new_attributes,
                   WebPrinterAttributes* current_attributes) {
  current_attributes->setCopiesDefault(new_attributes.copies_default);
  auto* copies_range = WebPrintingRange::Create();
  copies_range->setFrom(new_attributes.copies_supported->from);
  copies_range->setTo(new_attributes.copies_supported->to);
  current_attributes->setCopiesSupported(copies_range);
}

void ProcessDocumentFormat(
    const mojom::blink::WebPrinterAttributes& new_attributes,
    WebPrinterAttributes* current_attributes) {
  current_attributes->setDocumentFormatDefault(
      V8WebPrintingMimeMediaType::Enum::kApplicationPdf);
  current_attributes->setDocumentFormatSupported({V8WebPrintingMimeMediaType(
      V8WebPrintingMimeMediaType::Enum::kApplicationPdf)});
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
  blink::ProcessMultipleDocumentHandling(*printer_attributes, attributes);
  blink::ProcessSides(*printer_attributes, attributes);

  return attributes;
}

}  // namespace mojo
