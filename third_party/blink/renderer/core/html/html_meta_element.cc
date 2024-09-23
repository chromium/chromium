/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2010 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/html/html_meta_element.h"

#include "base/metrics/histogram_macros.h"
#include "base/trace_event/typed_macros.h"
#include "third_party/blink/public/mojom/frame/color_scheme.mojom-blink.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/viewport_data.h"
#include "third_party/blink/renderer/core/html/client_hints_util.h"
#include "third_party/blink/renderer/core/html/html_head_element.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/loader/frame_client_hints_preferences_context.h"
#include "third_party/blink/renderer/core/loader/frame_fetch_context.h"
#include "third_party/blink/renderer/core/loader/http_equiv.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/speculation_rules/document_speculation_rules.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/loader/fetch/client_hints_preferences.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"
#include "third_party/blink/renderer/platform/wtf/text/character_visitor.h"
#include "third_party/blink/renderer/platform/wtf/text/string_to_number.h"

namespace blink {

HTMLMetaElement::HTMLMetaElement(Document& document,
                                 const CreateElementFlags flags)
    : HTMLElement(html_names::kMetaTag, document),
      is_sync_parser_(flags.IsCreatedByParser() &&
                      !flags.IsAsyncCustomElements() &&
                      !document.IsInDocumentWrite()) {}

static bool IsInvalidSeparator(UChar c) {
  return c == ';';
}

// Though absl::ascii_isspace() considers \t and \v to be whitespace, Win IE
// doesn't.
static bool IsSeparator(UChar c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '=' ||
         c == ',' || c == '\0';
}

void HTMLMetaElement::ParseViewportContentAttribute(
    const String& content,
    ViewportDescription& viewport_description,
    Document* document,
    bool viewport_meta_zero_values_quirk) {
  bool has_invalid_separator = false;

  // Tread lightly in this code -- it was specifically designed to mimic Win
  // IE's parsing behavior.
  unsigned key_begin, key_end;
  unsigned value_begin, value_end;

  String buffer = content.LowerASCII();
  unsigned length = buffer.length();
  for (unsigned i = 0; i < length; /* no increment here */) {
    // skip to first non-separator, but don't skip past the end of the string
    while (IsSeparator(buffer[i])) {
      if (i >= length)
        break;
      i++;
    }
    key_begin = i;

    // skip to first separator
    while (!IsSeparator(buffer[i])) {
      has_invalid_separator |= IsInvalidSeparator(buffer[i]);
      if (i >= length)
        break;
      i++;
    }
    key_end = i;

    // skip to first '=', but don't skip past a ',' or the end of the string
    while (buffer[i] != '=') {
      has_invalid_separator |= IsInvalidSeparator(buffer[i]);
      if (buffer[i] == ',' || i >= length)
        break;
      i++;
    }

    // Skip to first non-separator, but don't skip past a ',' or the end of the
    // string.
    while (IsSeparator(buffer[i])) {
      if (buffer[i] == ',' || i >= length)
        break;
      i++;
    }
    value_begin = i;

    // skip to first separator
    while (!IsSeparator(buffer[i])) {
      has_invalid_separator |= IsInvalidSeparator(buffer[i]);
      if (i >= length)
        break;
      i++;
    }
    value_end = i;

    SECURITY_DCHECK(i <= length);

    String key_string = buffer.Substring(key_begin, key_end - key_begin);
    String value_string =
        buffer.Substring(value_begin, value_end - value_begin);
    ProcessViewportKeyValuePair(document, !has_invalid_separator, key_string,
                                value_string, viewport_meta_zero_values_quirk,
                                viewport_description);
  }
  if (has_invalid_separator && document) {
    String message =
        "Error parsing a meta element's content: ';' is not a valid key-value "
        "pair separator. Please use ',' instead.";
    document->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::ConsoleMessageSource::kRendering,
        mojom::ConsoleMessageLevel::kWarning, message));
  }
}

static inline float ClampLengthValue(float value) {
  // Limits as defined in the css-device-adapt spec.
  if (value != ViewportDescription::kValueAuto)
    return std::min(float(10000), std::max(value, float(1)));
  return value;
}

static inline float ClampScaleValue(float value) {
  // Limits as defined in the css-device-adapt spec.
  if (value != ViewportDescription::kValueAuto)
    return std::min(float(10), std::max(value, float(0.1)));
  return value;
}

float HTMLMetaElement::ParsePositiveNumber(Document* document,
                                           bool report_warnings,
                                           const String& key_string,
                                           const String& value_string,
                                           bool* ok) {
  size_t parsed_length;
  float value = WTF::VisitCharacters(value_string, [&](auto chars) {
    return CharactersToFloat(chars, parsed_length);
  });
  if (!parsed_length) {
    if (report_warnings)
      ReportViewportWarning(document, kUnrecognizedViewportArgumentValueError,
                            value_string, key_string);
    if (ok)
      *ok = false;
    return 0;
  }
  if (parsed_length < value_string.length() && report_warnings)
    ReportViewportWarning(document, kTruncatedViewportArgumentValueError,
                          value_string, key_string);
  if (ok)
    *ok = true;
  return value;
}

Length HTMLMetaElement::ParseViewportValueAsLength(Document* document,
                                                   bool report_warnings,
                                                   const String& key_string,
                                                   const String& value_string) {
  // 1) Non-negative number values are translated to px lengths.
  // 2) Negative number values are translated to auto.
  // 3) device-width and device-height are used as keywords.
  // 4) Other keywords and unknown values translate to auto.

  if (EqualIgnoringASCIICase(value_string, "device-width"))
    return Length::DeviceWidth();
  if (EqualIgnoringASCIICase(value_string, "device-height"))
    return Length::DeviceHeight();

  bool ok;

  float value = ParsePositiveNumber(document, report_warnings, key_string,
                                    value_string, &ok);

  if (!ok)
    return Length();  // auto

  if (value < 0)
    return Length();  // auto

  value = ClampLengthValue(value);
  if (document && document->GetPage()) {
    value = document->GetPage()->GetChromeClient().WindowToViewportScalar(
        document->GetFrame(), value);
  }
  return Length::Fixed(value);
}

float HTMLMetaElement::ParseViewportValueAsZoom(
    Document* document,
    bool report_warnings,
    const String& key_string,
    const String& value_string,
    bool& computed_value_matches_parsed_value,
    bool viewport_meta_zero_values_quirk) {
  // 1) Non-negative number values are translated to <number> values.
  // 2) Negative number values are translated to auto.
  // 3) yes is translated to 1.0.
  // 4) device-width and device-height are translated to 10.0.
  // 5) no and unknown values are translated to 0.0

  computed_value_matches_parsed_value = false;
  if (EqualIgnoringASCIICase(value_string, "yes"))
    return 1;
  if (EqualIgnoringASCIICase(value_string, "no"))
    return 0;
  if (EqualIgnoringASCIICase(value_string, "device-width"))
    return 10;
  if (EqualIgnoringASCIICase(value_string, "device-height"))
    return 10;

  float value =
      ParsePositiveNumber(document, report_warnings, key_string, value_string);

  if (value < 0)
    return ViewportDescription::kValueAuto;

  if (value > 10.0 && report_warnings)
    ReportViewportWarning(document, kMaximumScaleTooLargeError, String(),
                          String());

  if (!value && viewport_meta_zero_values_quirk)
    return ViewportDescription::kValueAuto;

  float clamped_value = ClampScaleValue(value);
  if (clamped_value == value)
    computed_value_matches_parsed_value = true;

  return clamped_value;
}

bool HTMLMetaElement::ParseViewportValueAsUserZoom(
    Document* document,
    bool report_warnings,
    const String& key_string,
    const String& value_string,
    bool& computed_value_matches_parsed_value) {
  // yes and no are used as keywords.
  // Numbers >= 1, numbers <= -1, device-width and device-height are mapped to
  // yes.
  // Numbers in the range <-1, 1>, and unknown values, are mapped to no.

  computed_value_matches_parsed_value = false;
  if (EqualIgnoringASCIICase(value_string, "yes")) {
    computed_value_matches_parsed_value = true;
    return true;
  }
  if (EqualIgnoringASCIICase(value_string, "no")) {
    computed_value_matches_parsed_value = true;
    return false;
  }
  if (EqualIgnoringASCIICase(value_string, "device-width"))
    return true;
  if (EqualIgnoringASCIICase(value_string, "device-height"))
    return true;

  float value =
      ParsePositiveNumber(document, report_warnings, key_string, value_string);
  if (fabs(value) < 1)
    return false;

  return true;
}

float HTMLMetaElement::ParseViewportValueAsDPI(Document* document,
                                               bool report_warnings,
                                               const String& key_string,
                                               const String& value_string) {
  if (EqualIgnoringASCIICase(value_string, "device-dpi"))
    return ViewportDescription::kValueDeviceDPI;
  if (EqualIgnoringASCIICase(value_string, "low-dpi"))
    return ViewportDescription::kValueLowDPI;
  if (EqualIgnoringASCIICase(value_string, "medium-dpi"))
    return ViewportDescription::kValueMediumDPI;
  if (EqualIgnoringASCIICase(value_string, "high-dpi"))
    return ViewportDescription::kValueHighDPI;

  bool ok;
  float value = ParsePositiveNumber(document, report_warnings, key_string,
                                    value_string, &ok);
  if (!ok || value < 70 || value > 400)
    return ViewportDescription::kValueAuto;

  return value;
}

blink::mojom::ViewportFit HTMLMetaElement::ParseViewportFitValueAsEnum(
    bool& unknown_value,
    const String& value_string) {
  if (EqualIgnoringASCIICase(value_string, "auto"))
    return mojom::ViewportFit::kAuto;
  if (EqualIgnoringASCIICase(value_string, "contain"))
    return mojom::ViewportFit::kContain;
  if (EqualIgnoringASCIICase(value_string, "cover"))
    return mojom::ViewportFit::kCover;

  unknown_value = true;
  return mojom::ViewportFit::kAuto;
}

// static
std::optional<ui::mojom::blink::VirtualKeyboardMode>
HTMLMetaElement::ParseVirtualKeyboardValueAsEnum(const String& value) {
  if (EqualIgnoringASCIICase(value, "resizes-content"))
    return ui::mojom::blink::VirtualKeyboardMode::kResizesContent;
  else if (EqualIgnoringASCIICase(value, "resizes-visual"))
    return ui::mojom::blink::VirtualKeyboardMode::kResizesVisual;
  else if (EqualIgnoringASCIICase(value, "overlays-content"))
    return ui::mojom::blink::VirtualKeyboardMode::kOverlaysContent;

  return std::nullopt;
}

void HTMLMetaElement::ProcessViewportKeyValuePair(
    Document* document,
    bool report_warnings,
    const String& key_string,
    const String& value_string,
    bool viewport_meta_zero_values_quirk,
    ViewportDescription& description) {
  if (key_string == "width") {
    const Length& width = ParseViewportValueAsLength(document, report_warnings,
                                                     key_string, value_string);
    if (!width.IsAuto()) {
      description.min_width = Length::ExtendToZoom();
      description.max_width = width;
    }
  } else if (key_string == "height") {
    const Length& height = ParseViewportValueAsLength(document, report_warnings,
                                                      key_string, value_string);
    if (!height.IsAuto()) {
      description.min_height = Length::ExtendToZoom();
      description.max_height = height;
    }
  } else if (key_string == "initial-scale") {
    description.zoom = ParseViewportValueAsZoom(
        document, report_warnings, key_string, value_string,
        description.zoom_is_explicit, viewport_meta_zero_values_quirk);
  } else if (key_string == "minimum-scale") {
    description.min_zoom = ParseViewportValueAsZoom(
        document, report_warnings, key_string, value_string,
        description.min_zoom_is_explicit, viewport_meta_zero_values_quirk);
  } else if (key_string == "maximum-scale") {
    description.max_zoom = ParseViewportValueAsZoom(
        document, report_warnings, key_string, value_string,
        description.max_zoom_is_explicit, viewport_meta_zero_values_quirk);
  } else if (key_string == "user-scalable") {
    description.user_zoom = ParseViewportValueAsUserZoom(
        document, report_warnings, key_string, value_string,
        description.user_zoom_is_explicit);
  } else if (key_string == "target-densitydpi") {
    description.deprecated_target_density_dpi = ParseViewportValueAsDPI(
        document, report_warnings, key_string, value_string);
    if (report_warnings)
      ReportViewportWarning(document, kTargetDensityDpiUnsupported, String(),
                            String());
  } else if (key_string == "minimal-ui") {
    // Ignore vendor-specific argument.
  } else if (key_string == "viewport-fit") {
    if (RuntimeEnabledFeatures::DisplayCutoutAPIEnabled()) {
      bool unknown_value = false;
      description.SetViewportFit(
          ParseViewportFitValueAsEnum(unknown_value, value_string));

      // If we got an unknown value then report a warning.
      if (unknown_value) {
        ReportViewportWarning(document, kViewportFitUnsupported, value_string,
                              String());
      }
    }
  } else if (key_string == "shrink-to-fit") {
    // Ignore vendor-specific argument.
  } else if (key_string == "interactive-widget") {
    std::optional<ui::mojom::blink::VirtualKeyboardMode> resize_type =
        ParseVirtualKeyboardValueAsEnum(value_string);

    if (resize_type) {
      description.virtual_keyboard_mode = resize_type.value();
      switch (resize_type.value()) {
        case ui::mojom::blink::VirtualKeyboardMode::kOverlaysContent: {
          UseCounter::Count(document,
                            WebFeature::kInteractiveWidgetOverlaysContent);
        } break;
        case ui::mojom::blink::VirtualKeyboardMode::kResizesContent: {
          UseCounter::Count(document,
                            WebFeature::kInteractiveWidgetResizesContent);
        } break;
        case ui::mojom::blink::VirtualKeyboardMode::kResizesVisual: {
          UseCounter::Count(document,
                            WebFeature::kInteractiveWidgetResizesVisual);
        } break;
        case ui::mojom::blink::VirtualKeyboardMode::kUnset: {
          NOTREACHED_IN_MIGRATION();
        } break;
      }
    } else {
      description.virtual_keyboard_mode =
          ui::mojom::blink::VirtualKeyboardMode::kUnset;
      ReportViewportWarning(document, kUnrecognizedViewportArgumentValueError,
                            value_string, key_string);
    }
  } else if (report_warnings) {
    ReportViewportWarning(document, kUnrecognizedViewportArgumentKeyError,
                          key_string, String());
  }
}

static const char* ViewportErrorMessageTemplate(ViewportErrorCode error_code) {
  static const char* const kErrors[] = {
      "The key \"%replacement1\" is not recognized and ignored.",
      "The value \"%replacement1\" for key \"%replacement2\" is invalid, and "
      "has been ignored.",
      "The value \"%replacement1\" for key \"%replacement2\" was truncated to "
      "its numeric prefix.",
      "The value for key \"maximum-scale\" is out of bounds and the value has "
      "been clamped.",
      "The key \"target-densitydpi\" is not supported.",
      "The value \"%replacement1\" for key \"viewport-fit\" is not supported.",
  };

  return kErrors[error_code];
}

static mojom::ConsoleMessageLevel ViewportErrorMessageLevel(
    ViewportErrorCode error_code) {
  switch (error_code) {
    case kTruncatedViewportArgumentValueError:
    case kTargetDensityDpiUnsupported:
    case kUnrecognizedViewportArgumentKeyError:
    case kUnrecognizedViewportArgumentValueError:
    case kMaximumScaleTooLargeError:
    case kViewportFitUnsupported:
      return mojom::ConsoleMessageLevel::kWarning;
  }

  NOTREACHED_IN_MIGRATION();
  return mojom::ConsoleMessageLevel::kError;
}

void HTMLMetaElement::ReportViewportWarning(Document* document,
                                            ViewportErrorCode error_code,
                                            const String& replacement1,
                                            const String& replacement2) {
  if (!document || !document->GetFrame())
    return;

  String message = ViewportErrorMessageTemplate(error_code);
  if (!replacement1.IsNull())
    message.Replace("%replacement1", replacement1);
  if (!replacement2.IsNull())
    message.Replace("%replacement2", replacement2);

  // FIXME: This message should be moved off the console once a solution to
  // https://bugs.webkit.org/show_bug.cgi?id=103274 exists.
  document->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
      mojom::ConsoleMessageSource::kRendering,
      ViewportErrorMessageLevel(error_code), message));
}

void HTMLMetaElement::GetViewportDescriptionFromContentAttribute(
    const String& content,
    ViewportDescription& description,
    Document* document,
    bool viewport_meta_zero_values_quirk) {
  ParseViewportContentAttribute(content, description, document,
                                viewport_meta_zero_values_quirk);

  if (description.min_zoom == ViewportDescription::kValueAuto)
    description.min_zoom = 0.25;

  if (description.max_zoom == ViewportDescription::kValueAuto) {
    description.max_zoom = 5;
    description.min_zoom = std::min(description.min_zoom, float(5));
  }
}

void HTMLMetaElement::ProcessViewportContentAttribute(
    const String& content,
    ViewportDescription::Type origin) {
  DCHECK(!content.IsNull());

  ViewportData& viewport_data = GetDocument().GetViewportData();
  if (!viewport_data.ShouldOverrideLegacyDescription(origin))
    return;

  ViewportDescription description_from_legacy_tag(origin);
  if (viewport_data.ShouldMergeWithLegacyDescription(origin))
    description_from_legacy_tag = viewport_data.GetViewportDescription();

  GetViewportDescriptionFromContentAttribute(
      content, description_from_legacy_tag, &GetDocument(),
      GetDocument().GetSettings() &&
          GetDocument().GetSettings()->GetViewportMetaZeroValuesQuirk());

  viewport_data.SetViewportDescription(description_from_legacy_tag);

  TRACE_EVENT_INSTANT(
      TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "ParseMetaViewport",
      "data", [&](perfetto::TracedValue context) {
        auto dict = std::move(context).WriteDictionary();
        if (GetDocument().GetFrame()) {
          dict.Add("frame", GetDocument().GetFrame()->GetFrameIdForTracing());
        }
        dict.Add("node_id", GetDomNodeId());
        dict.Add("content", content);
      });
}

void HTMLMetaElement::NameRemoved(const AtomicString& name_value) {
  const AtomicString& content_value =
      FastGetAttribute(html_names::kContentAttr);
  if (content_value.IsNull())
    return;
  if (EqualIgnoringASCIICase(name_value, "theme-color") &&
      GetDocument().GetFrame()) {
    GetDocument().GetFrame()->DidChangeThemeColor(
        /*update_theme_color_cache=*/true);
  } else if (EqualIgnoringASCIICase(name_value, keywords::kColorScheme)) {
    GetDocument().ColorSchemeMetaChanged();
  } else if (EqualIgnoringASCIICase(name_value, "supports-reduced-motion")) {
    GetDocument().SupportsReducedMotionMetaChanged();
  } else if (RuntimeEnabledFeatures::AppTitleEnabled(GetExecutionContext()) &&
             EqualIgnoringASCIICase(name_value, "app-title")) {
    GetDocument().UpdateAppTitle();
  }
}

void HTMLMetaElement::ParseAttribute(
    const AttributeModificationParams& params) {
  if (params.name == html_names::kNameAttr) {
    if (IsInDocumentTree())
      NameRemoved(params.old_value);
    ProcessContent();
  } else if (params.name == html_names::kContentAttr) {
    ProcessContent();
    ProcessHttpEquiv();
  } else if (params.name == html_names::kHttpEquivAttr) {
    ProcessHttpEquiv();
  } else if (params.name == html_names::kMediaAttr) {
    ProcessContent();
  } else {
    HTMLElement::ParseAttribute(params);
  }
}

Node::InsertionNotificationRequest HTMLMetaElement::InsertedInto(
    ContainerNode& insertion_point) {
  HTMLElement::InsertedInto(insertion_point);
  return kInsertionShouldCallDidNotifySubtreeInsertions;
}

void HTMLMetaElement::DidNotifySubtreeInsertionsToDocument() {
  ProcessContent();
  ProcessHttpEquiv();
}

void HTMLMetaElement::RemovedFrom(ContainerNode& insertion_point) {
  HTMLElement::RemovedFrom(insertion_point);
  if (!insertion_point.IsInDocumentTree())
    return;
  const AtomicString& name_value = FastGetAttribute(html_names::kNameAttr);
  if (!name_value.empty())
    NameRemoved(name_value);
}

static bool InDocumentHead(HTMLMetaElement* element) {
  if (!element->isConnected())
    return false;

  return Traversal<HTMLHeadElement>::FirstAncestor(*element);
}

void HTMLMetaElement::ProcessHttpEquiv() {
  if (!IsInDocumentTree())
    return;
  const AtomicString& content_value =
      FastGetAttribute(html_names::kContentAttr);
  if (content_value.IsNull())
    return;
  const AtomicString& http_equiv_value =
      FastGetAttribute(html_names::kHttpEquivAttr);
  if (http_equiv_value.empty())
    return;
  HttpEquiv::Process(GetDocument(), http_equiv_value, content_value,
                     InDocumentHead(this), is_sync_parser_, this);
}

// Open Graph Protocol Content Classification types used for logging.
enum class ContentClassificationOpenGraph {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  kUnknown = 0,
  kWebsite = 1,
  kMusic = 2,
  kVideo = 3,
  kArticle = 4,
  kBook = 5,
  kProfile = 6,
  kMaxValue = kProfile
};

ContentClassificationOpenGraph GetContentClassification(
    const AtomicString& open_graph_type) {
  const AtomicString lowercase_type(open_graph_type.LowerASCII());
  if (lowercase_type.StartsWithIgnoringASCIICase("website")) {
    return ContentClassificationOpenGraph::kWebsite;
  } else if (lowercase_type.StartsWithIgnoringASCIICase("music")) {
    return ContentClassificationOpenGraph::kMusic;
  } else if (lowercase_type.StartsWithIgnoringASCIICase("video")) {
    return ContentClassificationOpenGraph::kVideo;
  } else if (lowercase_type.StartsWithIgnoringASCIICase("article")) {
    return ContentClassificationOpenGraph::kArticle;
  } else if (lowercase_type.StartsWithIgnoringASCIICase("book")) {
    return ContentClassificationOpenGraph::kBook;
  } else if (lowercase_type.StartsWithIgnoringASCIICase("profile")) {
    return ContentClassificationOpenGraph::kProfile;
  }
  return ContentClassificationOpenGraph::kUnknown;
}

void HTMLMetaElement::ProcessContent() {
  if (!IsInDocumentTree())
    return;

  const AtomicString& property_value =
      FastGetAttribute(html_names::kPropertyAttr);
  const AtomicString& content_value =
      FastGetAttribute(html_names::kContentAttr);

  if (EqualIgnoringASCIICase(property_value, "og:type")) {
    UMA_HISTOGRAM_ENUMERATION("Content.Classification.OpenGraph",
                              GetContentClassification(content_value));
  }

  const AtomicString& name_value = FastGetAttribute(html_names::kNameAttr);
  if (name_value.empty())
    return;

  if (EqualIgnoringASCIICase(name_value, "theme-color") &&
      GetDocument().GetFrame()) {
    GetDocument().GetFrame()->DidChangeThemeColor(
        /*update_theme_color_cache=*/true);
    return;
  }
  if (EqualIgnoringASCIICase(name_value, keywords::kColorScheme)) {
    GetDocument().ColorSchemeMetaChanged();
    return;
  }

  if (EqualIgnoringASCIICase(name_value, "supports-reduced-motion")) {
    GetDocument().SupportsReducedMotionMetaChanged();
    return;
  }

  // All situations below require a content attribute (which can be the empty
  // string).
  if (content_value.IsNull())
    return;

  if (EqualIgnoringASCIICase(name_value, "viewport")) {
    ProcessViewportContentAttribute(content_value,
                                    ViewportDescription::kViewportMeta);
  } else if (EqualIgnoringASCIICase(name_value, "referrer") &&
             GetExecutionContext()) {
    UseCounter::Count(&GetDocument(),
                      WebFeature::kHTMLMetaElementReferrerPolicy);
    if (!IsDescendantOf(GetDocument().head())) {
      UseCounter::Count(&GetDocument(),
                        WebFeature::kHTMLMetaElementReferrerPolicyOutsideHead);
    }
    network::mojom::ReferrerPolicy old_referrer_policy =
        GetExecutionContext()->GetReferrerPolicy();
    GetExecutionContext()->ParseAndSetReferrerPolicy(content_value,
                                                     kPolicySourceMetaTag);
    network::mojom::ReferrerPolicy new_referrer_policy =
        GetExecutionContext()->GetReferrerPolicy();
    if (old_referrer_policy != new_referrer_policy) {
      if (auto* document_rules =
              DocumentSpeculationRules::FromIfExists(GetDocument())) {
        document_rules->DocumentReferrerPolicyChanged();
      }
    }
  } else if (EqualIgnoringASCIICase(name_value, "handheldfriendly") &&
             EqualIgnoringASCIICase(content_value, "true")) {
    ProcessViewportContentAttribute("width=device-width",
                                    ViewportDescription::kHandheldFriendlyMeta);
  } else if (EqualIgnoringASCIICase(name_value, "mobileoptimized")) {
    ProcessViewportContentAttribute("width=device-width, initial-scale=1",
                                    ViewportDescription::kMobileOptimizedMeta);
  } else if (EqualIgnoringASCIICase(name_value, "monetization")) {
    // TODO(1031476): The Web Monetization specification is an unofficial draft,
    // available at https://webmonetization.org/specification.html
    // For now, only use counters are implemented in Blink.
    if (GetDocument().IsInOutermostMainFrame()) {
      UseCounter::Count(&GetDocument(),
                        WebFeature::kHTMLMetaElementMonetization);
    }
  } else if (RuntimeEnabledFeatures::AppTitleEnabled(GetExecutionContext()) &&
             EqualIgnoringASCIICase(name_value, "app-title")) {
    UseCounter::Count(&GetDocument(), WebFeature::kWebAppTitle);
    GetDocument().UpdateAppTitle();
  }
}

WTF::TextEncoding HTMLMetaElement::ComputeEncoding() const {
  HTMLAttributeList attribute_list;
  for (const Attribute& attr : Attributes())
    attribute_list.push_back(
        std::make_pair(attr.GetName().LocalName(), attr.Value().GetString()));
  return EncodingFromMetaAttributes(attribute_list);
}

const AtomicString& HTMLMetaElement::Content() const {
  return FastGetAttribute(html_names::kContentAttr);
}

const AtomicString& HTMLMetaElement::HttpEquiv() const {
  return FastGetAttribute(html_names::kHttpEquivAttr);
}

const AtomicString& HTMLMetaElement::Media() const {
  return FastGetAttribute(html_names::kMediaAttr);
}

const AtomicString& HTMLMetaElement::GetName() const {
  return FastGetAttribute(html_names::kNameAttr);
}

const AtomicString& HTMLMetaElement::Property() const {
  return FastGetAttribute(html_names::kPropertyAttr);
}

const AtomicString& HTMLMetaElement::Itemprop() const {
  return FastGetAttribute(html_names::kItempropAttr);
}

// static
void HTMLMetaElement::ProcessMetaCH(Document& document,
                                    const AtomicString& content,
                                    network::MetaCHType type,
                                    bool is_doc_preloader,
                                    bool is_sync_parser) {

  LocalFrame* frame = document.GetFrame();
  if (!frame)
    return;

  if (!frame->IsMainFrame()) {
    return;
  }

  if (!frame->ScriptEnabled()) {
    // Do not allow configuring client hints if JavaScript is disabled.
    return;
  }

  switch (type) {
    case network::MetaCHType::HttpEquivAcceptCH:
      UseCounter::Count(document,
                        WebFeature::kClientHintsMetaHTTPEquivAcceptCH);
      break;
    case network::MetaCHType::HttpEquivDelegateCH:
      UseCounter::Count(document, WebFeature::kClientHintsMetaEquivDelegateCH);
      break;
  }
  FrameClientHintsPreferencesContext hints_context(frame);
  UpdateWindowPermissionsPolicyWithDelegationSupportForClientHints(
      frame->GetClientHintsPreferences(), document.domWindow(), content,
      document.Url(), &hints_context, type, is_doc_preloader, is_sync_parser);
}

void HTMLMetaElement::FinishParsingChildren() {
  // Flag the tag was parsed so if it's re-read we know it was modified.
  is_sync_parser_ = false;
  HTMLElement::FinishParsingChildren();
}

}  // namespace blink
