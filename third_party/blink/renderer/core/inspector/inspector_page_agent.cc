/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/inspector/inspector_page_agent.h"

#include <memory>

#include "base/containers/span.h"
#include "build/build_config.h"
#include "third_party/blink/public/common/widget/screen_info.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/script_regexp.h"
#include "third_party/blink/renderer/bindings/core/v8/script_source_code.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_timing.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/report.h"
#include "third_party/blink/renderer/core/frame/reporting_context.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/test_report_body.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/html/imports/html_import_loader.h"
#include "third_party/blink/renderer/core/html/imports/html_imports_controller.h"
#include "third_party/blink/renderer/core/html/parser/text_resource_decoder.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/core/inspector/inspected_frames.h"
#include "third_party/blink/renderer/core/inspector/inspector_css_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_resource_content_loader.h"
#include "third_party/blink/renderer/core/inspector/protocol/Page.h"
#include "third_party/blink/renderer/core/inspector/v8_inspector_string.h"
#include "third_party/blink/renderer/core/layout/adjust_for_absolute_zoom.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/core/loader/idleness_detector.h"
#include "third_party/blink/renderer/core/loader/resource/css_style_sheet_resource.h"
#include "third_party/blink/renderer/core/loader/resource/script_resource.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/script/classic_script.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/text_resource_decoder_options.h"
#include "third_party/blink/renderer/platform/network/mime/mime_type_registry.h"
#include "third_party/blink/renderer/platform/network/network_utils.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/text/base64.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "v8/include/v8-inspector.h"

namespace blink {

using protocol::Response;

namespace {

String ClientNavigationReasonToProtocol(ClientNavigationReason reason) {
  namespace ReasonEnum = protocol::Page::ClientNavigationReasonEnum;
  switch (reason) {
    case ClientNavigationReason::kAnchorClick:
      return ReasonEnum::AnchorClick;
    case ClientNavigationReason::kFormSubmissionGet:
      return ReasonEnum::FormSubmissionGet;
    case ClientNavigationReason::kFormSubmissionPost:
      return ReasonEnum::FormSubmissionPost;
    case ClientNavigationReason::kHttpHeaderRefresh:
      return ReasonEnum::HttpHeaderRefresh;
    case ClientNavigationReason::kFrameNavigation:
      return ReasonEnum::ScriptInitiated;
    case ClientNavigationReason::kMetaTagRefresh:
      return ReasonEnum::MetaTagRefresh;
    case ClientNavigationReason::kPageBlock:
      return ReasonEnum::PageBlockInterstitial;
    case ClientNavigationReason::kReload:
      return ReasonEnum::Reload;
    default:
      NOTREACHED();
  }
  return ReasonEnum::Reload;
}

String NavigationPolicyToProtocol(NavigationPolicy policy) {
  namespace DispositionEnum = protocol::Page::ClientNavigationDispositionEnum;
  switch (policy) {
    case kNavigationPolicyDownload:
      return DispositionEnum::Download;
    case kNavigationPolicyCurrentTab:
      return DispositionEnum::CurrentTab;
    case kNavigationPolicyNewBackgroundTab:
      return DispositionEnum::NewTab;
    case kNavigationPolicyNewForegroundTab:
      return DispositionEnum::NewTab;
    case kNavigationPolicyNewWindow:
      return DispositionEnum::NewWindow;
    case kNavigationPolicyNewPopup:
      return DispositionEnum::NewWindow;
  }
  return DispositionEnum::CurrentTab;
}

Resource* CachedResource(LocalFrame* frame,
                         const KURL& url,
                         InspectorResourceContentLoader* loader) {
  Document* document = frame->GetDocument();
  if (!document)
    return nullptr;
  Resource* cached_resource = document->Fetcher()->CachedResource(url);
  if (!cached_resource) {
    HeapVector<Member<Document>> all_imports =
        InspectorPageAgent::ImportsForFrame(frame);
    for (Document* import : all_imports) {
      cached_resource = import->Fetcher()->CachedResource(url);
      if (cached_resource)
        break;
    }
  }
  if (!cached_resource) {
    cached_resource = GetMemoryCache()->ResourceForURL(
        url, document->Fetcher()->GetCacheIdentifier(url));
  }
  if (!cached_resource)
    cached_resource = loader->ResourceForURL(url);
  return cached_resource;
}

std::unique_ptr<protocol::Array<String>> GetEnabledWindowFeatures(
    const WebWindowFeatures& window_features) {
  auto feature_strings = std::make_unique<protocol::Array<String>>();
  if (window_features.x_set) {
    feature_strings->emplace_back(
        String::Format("left=%d", static_cast<int>(window_features.x)));
  }
  if (window_features.y_set) {
    feature_strings->emplace_back(
        String::Format("top=%d", static_cast<int>(window_features.y)));
  }
  if (window_features.width_set) {
    feature_strings->emplace_back(
        String::Format("width=%d", static_cast<int>(window_features.width)));
  }
  if (window_features.height_set) {
    feature_strings->emplace_back(
        String::Format("height=%d", static_cast<int>(window_features.height)));
  }
  if (window_features.menu_bar_visible)
    feature_strings->emplace_back("menubar");
  if (window_features.tool_bar_visible)
    feature_strings->emplace_back("toolbar");
  if (window_features.status_bar_visible)
    feature_strings->emplace_back("status");
  if (window_features.scrollbars_visible)
    feature_strings->emplace_back("scrollbars");
  if (window_features.resizable)
    feature_strings->emplace_back("resizable");
  if (window_features.noopener)
    feature_strings->emplace_back("noopener");
  if (window_features.background)
    feature_strings->emplace_back("background");
  if (window_features.persistent)
    feature_strings->emplace_back("persistent");
  return feature_strings;
}

}  // namespace

static bool PrepareResourceBuffer(const Resource* cached_resource,
                                  bool* has_zero_size) {
  if (!cached_resource)
    return false;

  if (cached_resource->GetDataBufferingPolicy() == kDoNotBufferData)
    return false;

  // Zero-sized resources don't have data at all -- so fake the empty buffer,
  // instead of indicating error by returning 0.
  if (!cached_resource->EncodedSize()) {
    *has_zero_size = true;
    return true;
  }

  *has_zero_size = false;
  return true;
}

static bool HasTextContent(const Resource* cached_resource) {
  ResourceType type = cached_resource->GetType();
  return type == ResourceType::kCSSStyleSheet ||
         type == ResourceType::kXSLStyleSheet ||
         type == ResourceType::kScript || type == ResourceType::kRaw ||
         type == ResourceType::kImportResource;
}

static std::unique_ptr<TextResourceDecoder> CreateResourceTextDecoder(
    const String& mime_type,
    const String& text_encoding_name) {
  if (!text_encoding_name.IsEmpty()) {
    return std::make_unique<TextResourceDecoder>(TextResourceDecoderOptions(
        TextResourceDecoderOptions::kPlainTextContent,
        WTF::TextEncoding(text_encoding_name)));
  }
  if (MIMETypeRegistry::IsXMLMIMEType(mime_type)) {
    TextResourceDecoderOptions options(TextResourceDecoderOptions::kXMLContent);
    options.SetUseLenientXMLDecoding();
    return std::make_unique<TextResourceDecoder>(options);
  }
  if (EqualIgnoringASCIICase(mime_type, "text/html")) {
    return std::make_unique<TextResourceDecoder>(TextResourceDecoderOptions(
        TextResourceDecoderOptions::kHTMLContent, UTF8Encoding()));
  }
  if (MIMETypeRegistry::IsSupportedJavaScriptMIMEType(mime_type) ||
      MIMETypeRegistry::IsJSONMimeType(mime_type)) {
    return std::make_unique<TextResourceDecoder>(TextResourceDecoderOptions(
        TextResourceDecoderOptions::kPlainTextContent, UTF8Encoding()));
  }
  if (MIMETypeRegistry::IsPlainTextMIMEType(mime_type)) {
    return std::make_unique<TextResourceDecoder>(TextResourceDecoderOptions(
        TextResourceDecoderOptions::kPlainTextContent,
        WTF::TextEncoding("ISO-8859-1")));
  }
  return std::unique_ptr<TextResourceDecoder>();
}

static void MaybeEncodeTextContent(const String& text_content,
                                   const char* buffer_data,
                                   wtf_size_t buffer_size,
                                   String* result,
                                   bool* base64_encoded) {
  if (!text_content.IsNull()) {
    *result = text_content;
    *base64_encoded = false;
  } else if (buffer_data) {
    *result =
        Base64Encode(base::as_bytes(base::make_span(buffer_data, buffer_size)));
    *base64_encoded = true;
  } else if (text_content.IsNull()) {
    *result = "";
    *base64_encoded = false;
  } else {
    DCHECK(!text_content.Is8Bit());
    *result = Base64Encode(
        base::as_bytes(base::make_span(StringUTF8Adaptor(text_content))));
    *base64_encoded = true;
  }
}

static void MaybeEncodeTextContent(const String& text_content,
                                   scoped_refptr<const SharedBuffer> buffer,
                                   String* result,
                                   bool* base64_encoded) {
  if (!buffer) {
    return MaybeEncodeTextContent(text_content, nullptr, 0, result,
                                  base64_encoded);
  }

  const SharedBuffer::DeprecatedFlatData flat_buffer(std::move(buffer));
  return MaybeEncodeTextContent(text_content, flat_buffer.Data(),
                                SafeCast<wtf_size_t>(flat_buffer.size()),
                                result, base64_encoded);
}

// static
KURL InspectorPageAgent::UrlWithoutFragment(const KURL& url) {
  KURL result = url;
  result.RemoveFragmentIdentifier();
  return result;
}

// static
bool InspectorPageAgent::SharedBufferContent(
    scoped_refptr<const SharedBuffer> buffer,
    const String& mime_type,
    const String& text_encoding_name,
    String* result,
    bool* base64_encoded) {
  if (!buffer)
    return false;

  String text_content;
  std::unique_ptr<TextResourceDecoder> decoder =
      CreateResourceTextDecoder(mime_type, text_encoding_name);
  WTF::TextEncoding encoding(text_encoding_name);

  const SharedBuffer::DeprecatedFlatData flat_buffer(std::move(buffer));
  if (decoder) {
    text_content = decoder->Decode(flat_buffer.Data(), flat_buffer.size());
    text_content = text_content + decoder->Flush();
  } else if (encoding.IsValid()) {
    text_content = encoding.Decode(flat_buffer.Data(),
                                   SafeCast<wtf_size_t>(flat_buffer.size()));
  }

  MaybeEncodeTextContent(text_content, flat_buffer.Data(),
                         SafeCast<wtf_size_t>(flat_buffer.size()), result,
                         base64_encoded);
  return true;
}

// static
bool InspectorPageAgent::CachedResourceContent(const Resource* cached_resource,
                                               String* result,
                                               bool* base64_encoded) {
  bool has_zero_size;
  if (!PrepareResourceBuffer(cached_resource, &has_zero_size))
    return false;

  if (!HasTextContent(cached_resource)) {
    scoped_refptr<const SharedBuffer> buffer =
        has_zero_size ? SharedBuffer::Create()
                      : cached_resource->ResourceBuffer();
    if (!buffer)
      return false;

    const SharedBuffer::DeprecatedFlatData flat_buffer(std::move(buffer));
    *result = Base64Encode(base::as_bytes(
        base::make_span(flat_buffer.Data(), flat_buffer.size())));
    *base64_encoded = true;
    return true;
  }

  if (has_zero_size) {
    *result = "";
    *base64_encoded = false;
    return true;
  }

  DCHECK(cached_resource);
  switch (cached_resource->GetType()) {
    case blink::ResourceType::kCSSStyleSheet:
      MaybeEncodeTextContent(
          ToCSSStyleSheetResource(cached_resource)
              ->SheetText(nullptr, CSSStyleSheetResource::MIMETypeCheck::kLax),
          cached_resource->ResourceBuffer(), result, base64_encoded);
      return true;
    case blink::ResourceType::kScript:
      MaybeEncodeTextContent(
          ToScriptResource(cached_resource)->TextForInspector(),
          cached_resource->ResourceBuffer(), result, base64_encoded);
      return true;
    default:
      String text_encoding_name =
          cached_resource->GetResponse().TextEncodingName();
      if (text_encoding_name.IsEmpty() &&
          cached_resource->GetType() != blink::ResourceType::kRaw)
        text_encoding_name = "WinLatin1";
      return InspectorPageAgent::SharedBufferContent(
          cached_resource->ResourceBuffer(),
          cached_resource->GetResponse().MimeType(), text_encoding_name, result,
          base64_encoded);
  }
}

String InspectorPageAgent::ResourceTypeJson(
    InspectorPageAgent::ResourceType resource_type) {
  switch (resource_type) {
    case kDocumentResource:
      return protocol::Network::ResourceTypeEnum::Document;
    case kFontResource:
      return protocol::Network::ResourceTypeEnum::Font;
    case kImageResource:
      return protocol::Network::ResourceTypeEnum::Image;
    case kMediaResource:
      return protocol::Network::ResourceTypeEnum::Media;
    case kScriptResource:
      return protocol::Network::ResourceTypeEnum::Script;
    case kStylesheetResource:
      return protocol::Network::ResourceTypeEnum::Stylesheet;
    case kTextTrackResource:
      return protocol::Network::ResourceTypeEnum::TextTrack;
    case kXHRResource:
      return protocol::Network::ResourceTypeEnum::XHR;
    case kFetchResource:
      return protocol::Network::ResourceTypeEnum::Fetch;
    case kEventSourceResource:
      return protocol::Network::ResourceTypeEnum::EventSource;
    case kWebSocketResource:
      return protocol::Network::ResourceTypeEnum::WebSocket;
    case kManifestResource:
      return protocol::Network::ResourceTypeEnum::Manifest;
    case kSignedExchangeResource:
      return protocol::Network::ResourceTypeEnum::SignedExchange;
    case kOtherResource:
      return protocol::Network::ResourceTypeEnum::Other;
  }
  return protocol::Network::ResourceTypeEnum::Other;
}

InspectorPageAgent::ResourceType InspectorPageAgent::ToResourceType(
    const blink::ResourceType resource_type) {
  switch (resource_type) {
    case blink::ResourceType::kImage:
      return InspectorPageAgent::kImageResource;
    case blink::ResourceType::kFont:
      return InspectorPageAgent::kFontResource;
    case blink::ResourceType::kAudio:
    case blink::ResourceType::kVideo:
      return InspectorPageAgent::kMediaResource;
    case blink::ResourceType::kManifest:
      return InspectorPageAgent::kManifestResource;
    case blink::ResourceType::kTextTrack:
      return InspectorPageAgent::kTextTrackResource;
    case blink::ResourceType::kCSSStyleSheet:
    // Fall through.
    case blink::ResourceType::kXSLStyleSheet:
      return InspectorPageAgent::kStylesheetResource;
    case blink::ResourceType::kScript:
      return InspectorPageAgent::kScriptResource;
    case blink::ResourceType::kImportResource:
      return InspectorPageAgent::kDocumentResource;
    default:
      break;
  }
  return InspectorPageAgent::kOtherResource;
}

String InspectorPageAgent::CachedResourceTypeJson(
    const Resource& cached_resource) {
  return ResourceTypeJson(ToResourceType(cached_resource.GetType()));
}

InspectorPageAgent::InspectorPageAgent(
    InspectedFrames* inspected_frames,
    Client* client,
    InspectorResourceContentLoader* resource_content_loader,
    v8_inspector::V8InspectorSession* v8_session)
    : inspected_frames_(inspected_frames),
      v8_session_(v8_session),
      client_(client),
      inspector_resource_content_loader_(resource_content_loader),
      resource_content_loader_client_id_(
          resource_content_loader->CreateClientId()),
      enabled_(&agent_state_, /*default_value=*/false),
      screencast_enabled_(&agent_state_, /*default_value=*/false),
      lifecycle_events_enabled_(&agent_state_, /*default_value=*/false),
      bypass_csp_enabled_(&agent_state_, /*default_value=*/false),
      scripts_to_evaluate_on_load_(&agent_state_,
                                   /*default_value=*/WTF::String()),
      worlds_to_evaluate_on_load_(&agent_state_,
                                  /*default_value=*/WTF::String()),
      standard_font_family_(&agent_state_, /*default_value=*/WTF::String()),
      fixed_font_family_(&agent_state_, /*default_value=*/WTF::String()),
      serif_font_family_(&agent_state_, /*default_value=*/WTF::String()),
      sans_serif_font_family_(&agent_state_, /*default_value=*/WTF::String()),
      cursive_font_family_(&agent_state_, /*default_value=*/WTF::String()),
      fantasy_font_family_(&agent_state_, /*default_value=*/WTF::String()),
      pictograph_font_family_(&agent_state_, /*default_value=*/WTF::String()),
      standard_font_size_(&agent_state_, /*default_value=*/0),
      fixed_font_size_(&agent_state_, /*default_value=*/0),
      produce_compilation_cache_(&agent_state_, /*default_value=*/false) {}

void InspectorPageAgent::Restore() {
  if (enabled_.Get())
    enable();
  if (bypass_csp_enabled_.Get())
    setBypassCSP(true);
  // Re-apply generic fonts overrides.
  bool notifyGenericFontFamilyChange = false;
  LocalFrame* frame = inspected_frames_->Root();
  auto* settings = frame->GetSettings();
  if (settings) {
    auto& family_settings = settings->GetGenericFontFamilySettings();
    if (!standard_font_family_.Get().IsNull()) {
      family_settings.UpdateStandard(AtomicString(standard_font_family_.Get()));
      notifyGenericFontFamilyChange = true;
    }
    if (!fixed_font_family_.Get().IsNull()) {
      family_settings.UpdateFixed(AtomicString(fixed_font_family_.Get()));
      notifyGenericFontFamilyChange = true;
    }
    if (!serif_font_family_.Get().IsNull()) {
      family_settings.UpdateSerif(AtomicString(serif_font_family_.Get()));
      notifyGenericFontFamilyChange = true;
    }
    if (!sans_serif_font_family_.Get().IsNull()) {
      family_settings.UpdateSansSerif(
          AtomicString(sans_serif_font_family_.Get()));
      notifyGenericFontFamilyChange = true;
    }
    if (!cursive_font_family_.Get().IsNull()) {
      family_settings.UpdateCursive(AtomicString(cursive_font_family_.Get()));
      notifyGenericFontFamilyChange = true;
    }
    if (!fantasy_font_family_.Get().IsNull()) {
      family_settings.UpdateFantasy(AtomicString(fantasy_font_family_.Get()));
      notifyGenericFontFamilyChange = true;
    }
    if (!pictograph_font_family_.Get().IsNull()) {
      family_settings.UpdatePictograph(
          AtomicString(pictograph_font_family_.Get()));
      notifyGenericFontFamilyChange = true;
    }
    if (notifyGenericFontFamilyChange)
      settings->NotifyGenericFontFamilyChange();
  }

  // Re-apply default font size overrides.
  if (settings) {
    if (standard_font_size_.Get() != 0)
      settings->SetDefaultFontSize(standard_font_size_.Get());
    if (fixed_font_size_.Get() != 0)
      settings->SetDefaultFixedFontSize(fixed_font_size_.Get());
  }
}

Response InspectorPageAgent::enable() {
  enabled_.Set(true);
  instrumenting_agents_->AddInspectorPageAgent(this);
  return Response::Success();
}

Response InspectorPageAgent::disable() {
  agent_state_.ClearAllFields();
  script_to_evaluate_on_load_once_ = String();
  pending_script_to_evaluate_on_load_once_ = String();
  instrumenting_agents_->RemoveInspectorPageAgent(this);
  inspector_resource_content_loader_->Cancel(
      resource_content_loader_client_id_);

  stopScreencast();

  return Response::Success();
}

Response InspectorPageAgent::addScriptToEvaluateOnNewDocument(
    const String& source,
    Maybe<String> world_name,
    String* identifier) {
  Vector<WTF::String> keys = scripts_to_evaluate_on_load_.Keys();
  auto* result = std::max_element(
      keys.begin(), keys.end(), [](const WTF::String& a, const WTF::String& b) {
        return Decimal::FromString(a) < Decimal::FromString(b);
      });
  if (result == keys.end()) {
    *identifier = String::Number(1);
  } else {
    *identifier = String::Number(Decimal::FromString(*result).ToDouble() + 1);
  }

  scripts_to_evaluate_on_load_.Set(*identifier, source);
  worlds_to_evaluate_on_load_.Set(*identifier, world_name.fromMaybe(""));
  return Response::Success();
}

Response InspectorPageAgent::removeScriptToEvaluateOnNewDocument(
    const String& identifier) {
  if (scripts_to_evaluate_on_load_.Get(identifier).IsNull())
    return Response::ServerError("Script not found");
  scripts_to_evaluate_on_load_.Clear(identifier);
  worlds_to_evaluate_on_load_.Clear(identifier);
  return Response::Success();
}

Response InspectorPageAgent::addScriptToEvaluateOnLoad(const String& source,
                                                       String* identifier) {
  return addScriptToEvaluateOnNewDocument(source, Maybe<String>(""),
                                          identifier);
}

Response InspectorPageAgent::removeScriptToEvaluateOnLoad(
    const String& identifier) {
  return removeScriptToEvaluateOnNewDocument(identifier);
}

Response InspectorPageAgent::setLifecycleEventsEnabled(bool enabled) {
  lifecycle_events_enabled_.Set(enabled);
  if (!enabled)
    return Response::Success();

  for (LocalFrame* frame : *inspected_frames_) {
    Document* document = frame->GetDocument();
    DocumentLoader* loader = frame->Loader().GetDocumentLoader();
    if (!document || !loader)
      continue;

    DocumentLoadTiming& timing = loader->GetTiming();
    base::TimeTicks commit_timestamp = timing.ResponseEnd();
    if (!commit_timestamp.is_null()) {
      LifecycleEvent(frame, loader, "commit",
                     commit_timestamp.since_origin().InSecondsF());
    }

    base::TimeTicks domcontentloaded_timestamp =
        document->GetTiming().DomContentLoadedEventEnd();
    if (!domcontentloaded_timestamp.is_null()) {
      LifecycleEvent(frame, loader, "DOMContentLoaded",
                     domcontentloaded_timestamp.since_origin().InSecondsF());
    }

    base::TimeTicks load_timestamp = timing.LoadEventEnd();
    if (!load_timestamp.is_null()) {
      LifecycleEvent(frame, loader, "load",
                     load_timestamp.since_origin().InSecondsF());
    }

    IdlenessDetector* idleness_detector = frame->GetIdlenessDetector();
    base::TimeTicks network_almost_idle_timestamp =
        idleness_detector->GetNetworkAlmostIdleTime();
    if (!network_almost_idle_timestamp.is_null()) {
      LifecycleEvent(frame, loader, "networkAlmostIdle",
                     network_almost_idle_timestamp.since_origin().InSecondsF());
    }
    base::TimeTicks network_idle_timestamp =
        idleness_detector->GetNetworkIdleTime();
    if (!network_idle_timestamp.is_null()) {
      LifecycleEvent(frame, loader, "networkIdle",
                     network_idle_timestamp.since_origin().InSecondsF());
    }
  }

  return Response::Success();
}

Response InspectorPageAgent::setAdBlockingEnabled(bool enable) {
  return Response::Success();
}

Response InspectorPageAgent::reload(
    Maybe<bool> optional_bypass_cache,
    Maybe<String> optional_script_to_evaluate_on_load) {
  pending_script_to_evaluate_on_load_once_ =
      optional_script_to_evaluate_on_load.fromMaybe("");
  v8_session_->setSkipAllPauses(true);
  return Response::Success();
}

Response InspectorPageAgent::stopLoading() {
  return Response::Success();
}

static void CachedResourcesForDocument(Document* document,
                                       HeapVector<Member<Resource>>& result,
                                       bool skip_xhrs) {
  const ResourceFetcher::DocumentResourceMap& all_resources =
      document->Fetcher()->AllResources();
  for (const auto& resource : all_resources) {
    Resource* cached_resource = resource.value.Get();
    if (!cached_resource)
      continue;

    // Skip images that were not auto loaded (images disabled in the user
    // agent), fonts that were referenced in CSS but never used/downloaded, etc.
    if (cached_resource->StillNeedsLoad())
      continue;
    if (cached_resource->GetType() == ResourceType::kRaw && skip_xhrs)
      continue;
    result.push_back(cached_resource);
  }
}

// static
HeapVector<Member<Document>> InspectorPageAgent::ImportsForFrame(
    LocalFrame* frame) {
  HeapVector<Member<Document>> result;
  Document* root_document = frame->GetDocument();

  if (HTMLImportsController* controller = root_document->ImportsController()) {
    for (wtf_size_t i = 0; i < controller->LoaderCount(); ++i) {
      if (Document* document = controller->LoaderAt(i)->GetDocument())
        result.push_back(document);
    }
  }

  return result;
}

static HeapVector<Member<Resource>> CachedResourcesForFrame(LocalFrame* frame,
                                                            bool skip_xhrs) {
  HeapVector<Member<Resource>> result;
  Document* root_document = frame->GetDocument();
  HeapVector<Member<Document>> loaders =
      InspectorPageAgent::ImportsForFrame(frame);

  CachedResourcesForDocument(root_document, result, skip_xhrs);
  for (wtf_size_t i = 0; i < loaders.size(); ++i)
    CachedResourcesForDocument(loaders[i], result, skip_xhrs);

  return result;
}

Response InspectorPageAgent::getResourceTree(
    std::unique_ptr<protocol::Page::FrameResourceTree>* object) {
  *object = BuildObjectForResourceTree(inspected_frames_->Root());
  return Response::Success();
}

Response InspectorPageAgent::getFrameTree(
    std::unique_ptr<protocol::Page::FrameTree>* object) {
  *object = BuildObjectForFrameTree(inspected_frames_->Root());
  return Response::Success();
}

void InspectorPageAgent::GetResourceContentAfterResourcesContentLoaded(
    const String& frame_id,
    const String& url,
    std::unique_ptr<GetResourceContentCallback> callback) {
  LocalFrame* frame =
      IdentifiersFactory::FrameById(inspected_frames_, frame_id);
  if (!frame) {
    callback->sendFailure(Response::ServerError("No frame for given id found"));
    return;
  }
  String content;
  bool base64_encoded;
  if (InspectorPageAgent::CachedResourceContent(
          CachedResource(frame, KURL(url), inspector_resource_content_loader_),
          &content, &base64_encoded)) {
    callback->sendSuccess(content, base64_encoded);
  } else {
    callback->sendFailure(
        Response::ServerError("No resource with given URL found"));
  }
}

void InspectorPageAgent::getResourceContent(
    const String& frame_id,
    const String& url,
    std::unique_ptr<GetResourceContentCallback> callback) {
  if (!enabled_.Get()) {
    callback->sendFailure(Response::ServerError("Agent is not enabled."));
    return;
  }
  inspector_resource_content_loader_->EnsureResourcesContentLoaded(
      resource_content_loader_client_id_,
      WTF::Bind(
          &InspectorPageAgent::GetResourceContentAfterResourcesContentLoaded,
          WrapPersistent(this), frame_id, url,
          WTF::Passed(std::move(callback))));
}

void InspectorPageAgent::SearchContentAfterResourcesContentLoaded(
    const String& frame_id,
    const String& url,
    const String& query,
    bool case_sensitive,
    bool is_regex,
    std::unique_ptr<SearchInResourceCallback> callback) {
  LocalFrame* frame =
      IdentifiersFactory::FrameById(inspected_frames_, frame_id);
  if (!frame) {
    callback->sendFailure(Response::ServerError("No frame for given id found"));
    return;
  }
  String content;
  bool base64_encoded;
  if (!InspectorPageAgent::CachedResourceContent(
          CachedResource(frame, KURL(url), inspector_resource_content_loader_),
          &content, &base64_encoded)) {
    callback->sendFailure(
        Response::ServerError("No resource with given URL found"));
    return;
  }

  auto matches = v8_session_->searchInTextByLines(
      ToV8InspectorStringView(content), ToV8InspectorStringView(query),
      case_sensitive, is_regex);
  callback->sendSuccess(
      std::make_unique<
          protocol::Array<v8_inspector::protocol::Debugger::API::SearchMatch>>(
          std::move(matches)));
}

void InspectorPageAgent::searchInResource(
    const String& frame_id,
    const String& url,
    const String& query,
    Maybe<bool> optional_case_sensitive,
    Maybe<bool> optional_is_regex,
    std::unique_ptr<SearchInResourceCallback> callback) {
  if (!enabled_.Get()) {
    callback->sendFailure(Response::ServerError("Agent is not enabled."));
    return;
  }
  inspector_resource_content_loader_->EnsureResourcesContentLoaded(
      resource_content_loader_client_id_,
      WTF::Bind(&InspectorPageAgent::SearchContentAfterResourcesContentLoaded,
                WrapPersistent(this), frame_id, url, query,
                optional_case_sensitive.fromMaybe(false),
                optional_is_regex.fromMaybe(false),
                WTF::Passed(std::move(callback))));
}

Response InspectorPageAgent::setBypassCSP(bool enabled) {
  LocalFrame* frame = inspected_frames_->Root();
  frame->GetSettings()->SetBypassCSP(enabled);
  bypass_csp_enabled_.Set(enabled);
  return Response::Success();
}

Response InspectorPageAgent::setDocumentContent(const String& frame_id,
                                                const String& html) {
  LocalFrame* frame =
      IdentifiersFactory::FrameById(inspected_frames_, frame_id);
  if (!frame)
    return Response::ServerError("No frame for given id found");

  Document* document = frame->GetDocument();
  if (!document)
    return Response::ServerError("No Document instance to set HTML for");
  document->SetContent(html);
  return Response::Success();
}

void InspectorPageAgent::DidNavigateWithinDocument(LocalFrame* frame) {
  Document* document = frame->GetDocument();
  if (document) {
    return GetFrontend()->navigatedWithinDocument(
        IdentifiersFactory::FrameId(frame), document->Url());
  }
}

scoped_refptr<DOMWrapperWorld> InspectorPageAgent::EnsureDOMWrapperWorld(
    LocalFrame* frame,
    const String& world_name,
    bool grant_universal_access) {
  if (!isolated_worlds_.Contains(frame))
    isolated_worlds_.Set(frame, FrameIsolatedWorlds());
  FrameIsolatedWorlds& frame_worlds = isolated_worlds_.find(frame)->value;

  auto world_it = frame_worlds.find(world_name);
  if (world_it != frame_worlds.end())
    return world_it->value;
  LocalDOMWindow* window = frame->DomWindow();
  scoped_refptr<DOMWrapperWorld> world =
      window->GetScriptController().CreateNewInspectorIsolatedWorld(world_name);
  if (!world)
    return nullptr;
  frame_worlds.Set(world_name, world);
  scoped_refptr<SecurityOrigin> security_origin =
      window->GetSecurityOrigin()->IsolatedCopy();
  if (grant_universal_access)
    security_origin->GrantUniversalAccess();
  DOMWrapperWorld::SetIsolatedWorldSecurityOrigin(world->GetWorldId(),
                                                  security_origin);
  return world;
}

void InspectorPageAgent::DidClearDocumentOfWindowObject(LocalFrame* frame) {
  if (!GetFrontend())
    return;
  Vector<WTF::String> keys = scripts_to_evaluate_on_load_.Keys();
  std::sort(keys.begin(), keys.end(),
            [](const WTF::String& a, const WTF::String& b) {
              return Decimal::FromString(a) < Decimal::FromString(b);
            });

  for (const WTF::String& key : keys) {
    const String source = scripts_to_evaluate_on_load_.Get(key);
    const String world_name = worlds_to_evaluate_on_load_.Get(key);
    if (world_name.IsEmpty()) {
      ClassicScript::CreateUnspecifiedScript(ScriptSourceCode(source))
          ->RunScript(frame,
                      ScriptController::kExecuteScriptWhenScriptsDisabled);
      continue;
    }

    scoped_refptr<DOMWrapperWorld> world = EnsureDOMWrapperWorld(
        frame, world_name, true /* grant_universal_access */);
    if (!world)
      continue;

    // Note: An error event in an isolated world will never be dispatched to
    // a foreign world.
    v8::HandleScope handle_scope(V8PerIsolateData::MainThreadIsolate());
    ClassicScript::CreateUnspecifiedScript(ScriptSourceCode(source))
        ->RunScriptInIsolatedWorldAndReturnValue(frame, world->GetWorldId());
  }

  if (!script_to_evaluate_on_load_once_.IsEmpty()) {
    ClassicScript::CreateUnspecifiedScript(
        ScriptSourceCode(script_to_evaluate_on_load_once_))
        ->RunScript(frame, ScriptController::kExecuteScriptWhenScriptsDisabled);
  }
}

void InspectorPageAgent::DomContentLoadedEventFired(LocalFrame* frame) {
  double timestamp = base::TimeTicks::Now().since_origin().InSecondsF();
  if (frame == inspected_frames_->Root())
    GetFrontend()->domContentEventFired(timestamp);
  DocumentLoader* loader = frame->Loader().GetDocumentLoader();
  LifecycleEvent(frame, loader, "DOMContentLoaded", timestamp);
}

void InspectorPageAgent::LoadEventFired(LocalFrame* frame) {
  double timestamp = base::TimeTicks::Now().since_origin().InSecondsF();
  if (frame == inspected_frames_->Root())
    GetFrontend()->loadEventFired(timestamp);
  DocumentLoader* loader = frame->Loader().GetDocumentLoader();
  LifecycleEvent(frame, loader, "load", timestamp);
}

void InspectorPageAgent::WillCommitLoad(LocalFrame*, DocumentLoader* loader) {
  if (loader->GetFrame() == inspected_frames_->Root()) {
    script_to_evaluate_on_load_once_ = pending_script_to_evaluate_on_load_once_;
    pending_script_to_evaluate_on_load_once_ = String();
  }
  GetFrontend()->frameNavigated(BuildObjectForFrame(loader->GetFrame()));
}

void InspectorPageAgent::FrameAttachedToParent(LocalFrame* frame) {
  Frame* parent_frame = frame->Tree().Parent();
  std::unique_ptr<SourceLocation> location =
      SourceLocation::CaptureWithFullStackTrace();
  GetFrontend()->frameAttached(
      IdentifiersFactory::FrameId(frame),
      IdentifiersFactory::FrameId(parent_frame),
      location ? location->BuildInspectorObject() : nullptr);
  // Some network events referencing this frame will be reported from the
  // browser, so make sure to deliver FrameAttached without buffering,
  // so it gets to the front-end first.
  GetFrontend()->flush();
}

void InspectorPageAgent::FrameDetachedFromParent(LocalFrame* frame) {
  GetFrontend()->frameDetached(IdentifiersFactory::FrameId(frame));
}

bool InspectorPageAgent::ScreencastEnabled() {
  return enabled_.Get() && screencast_enabled_.Get();
}

void InspectorPageAgent::FrameStartedLoading(LocalFrame* frame) {
  GetFrontend()->frameStartedLoading(IdentifiersFactory::FrameId(frame));
  GetFrontend()->flush();
}

void InspectorPageAgent::FrameStoppedLoading(LocalFrame* frame) {
  GetFrontend()->frameStoppedLoading(IdentifiersFactory::FrameId(frame));
  GetFrontend()->flush();
}

void InspectorPageAgent::FrameRequestedNavigation(Frame* target_frame,
                                                  const KURL& url,
                                                  ClientNavigationReason reason,
                                                  NavigationPolicy policy) {
  GetFrontend()->frameRequestedNavigation(
      IdentifiersFactory::FrameId(target_frame),
      ClientNavigationReasonToProtocol(reason), url.GetString(),
      NavigationPolicyToProtocol(policy));
  GetFrontend()->flush();
}

void InspectorPageAgent::FrameScheduledNavigation(
    LocalFrame* frame,
    const KURL& url,
    base::TimeDelta delay,
    ClientNavigationReason reason) {
  GetFrontend()->frameScheduledNavigation(
      IdentifiersFactory::FrameId(frame), delay.InSecondsF(),
      ClientNavigationReasonToProtocol(reason), url.GetString());
  GetFrontend()->flush();
}

void InspectorPageAgent::FrameClearedScheduledNavigation(LocalFrame* frame) {
  GetFrontend()->frameClearedScheduledNavigation(
      IdentifiersFactory::FrameId(frame));
  GetFrontend()->flush();
}

void InspectorPageAgent::WillRunJavaScriptDialog() {
  GetFrontend()->flush();
}

void InspectorPageAgent::DidRunJavaScriptDialog() {
  GetFrontend()->flush();
}

void InspectorPageAgent::DidResizeMainFrame() {
  if (!inspected_frames_->Root()->IsMainFrame())
    return;
#if !defined(OS_ANDROID)
  PageLayoutInvalidated(true);
#endif
  GetFrontend()->frameResized();
}

void InspectorPageAgent::DidChangeViewport() {
  PageLayoutInvalidated(false);
}

void InspectorPageAgent::LifecycleEvent(LocalFrame* frame,
                                        DocumentLoader* loader,
                                        const char* name,
                                        double timestamp) {
  if (!loader || !lifecycle_events_enabled_.Get())
    return;
  GetFrontend()->lifecycleEvent(IdentifiersFactory::FrameId(frame),
                                IdentifiersFactory::LoaderId(loader), name,
                                timestamp);
  GetFrontend()->flush();
}

void InspectorPageAgent::PaintTiming(Document* document,
                                     const char* name,
                                     double timestamp) {
  LocalFrame* frame = document->GetFrame();
  DocumentLoader* loader = frame->Loader().GetDocumentLoader();
  LifecycleEvent(frame, loader, name, timestamp);
}

void InspectorPageAgent::Will(const probe::UpdateLayout&) {}

void InspectorPageAgent::Did(const probe::UpdateLayout&) {
  PageLayoutInvalidated(false);
}

void InspectorPageAgent::Will(const probe::RecalculateStyle&) {}

void InspectorPageAgent::Did(const probe::RecalculateStyle&) {
  PageLayoutInvalidated(false);
}

void InspectorPageAgent::PageLayoutInvalidated(bool resized) {
  if (enabled_.Get() && client_)
    client_->PageLayoutInvalidated(resized);
}

void InspectorPageAgent::WindowOpen(const KURL& url,
                                    const AtomicString& window_name,
                                    const WebWindowFeatures& window_features,
                                    bool user_gesture) {
  GetFrontend()->windowOpen(url.IsEmpty() ? BlankURL() : url, window_name,
                            GetEnabledWindowFeatures(window_features),
                            user_gesture);
  GetFrontend()->flush();
}

namespace {
protocol::Page::SecureContextType CreateProtocolSecureContextType(
    SecureContextModeExplanation explanation) {
  switch (explanation) {
    case SecureContextModeExplanation::kSecure:
      return protocol::Page::SecureContextTypeEnum::Secure;
    case SecureContextModeExplanation::kInsecureAncestor:
      return protocol::Page::SecureContextTypeEnum::InsecureAncestor;
    case SecureContextModeExplanation::kInsecureScheme:
      return protocol::Page::SecureContextTypeEnum::InsecureScheme;
    case SecureContextModeExplanation::kSecureLocalhost:
      return protocol::Page::SecureContextTypeEnum::SecureLocalhost;
  }
}
protocol::Page::CrossOriginIsolatedContextType
CreateProtocolCrossOriginIsolatedContextType(ExecutionContext* context) {
  if (context->CrossOriginIsolatedCapability()) {
    return protocol::Page::CrossOriginIsolatedContextTypeEnum::Isolated;
  } else if (context->IsFeatureEnabled(
                 mojom::blink::FeaturePolicyFeature::kCrossOriginIsolated)) {
    return protocol::Page::CrossOriginIsolatedContextTypeEnum::NotIsolated;
  }
  return protocol::Page::CrossOriginIsolatedContextTypeEnum::
      NotIsolatedFeatureDisabled;
}
}  // namespace

std::unique_ptr<protocol::Page::Frame> InspectorPageAgent::BuildObjectForFrame(
    LocalFrame* frame) {
  DocumentLoader* loader = frame->Loader().GetDocumentLoader();
  std::unique_ptr<protocol::Page::Frame> frame_object =
      protocol::Page::Frame::create()
          .setId(IdentifiersFactory::FrameId(frame))
          .setLoaderId(IdentifiersFactory::LoaderId(loader))
          .setUrl(UrlWithoutFragment(loader->Url()).GetString())
          .setDomainAndRegistry(blink::network_utils::GetDomainAndRegistry(
              loader->Url().Host(),
              blink::network_utils::PrivateRegistryFilter::
                  kIncludePrivateRegistries))
          .setMimeType(frame->Loader().GetDocumentLoader()->MimeType())
          .setSecurityOrigin(
              SecurityOrigin::Create(loader->Url())->ToRawString())
          .setSecureContextType(CreateProtocolSecureContextType(
              frame->DomWindow()
                  ->GetSecurityContext()
                  .GetSecureContextModeExplanation()))
          .setCrossOriginIsolatedContextType(
              CreateProtocolCrossOriginIsolatedContextType(frame->DomWindow()))
          .build();
  if (loader->Url().HasFragmentIdentifier())
    frame_object->setUrlFragment("#" + loader->Url().FragmentIdentifier());
  Frame* parent_frame = frame->Tree().Parent();
  if (parent_frame) {
    frame_object->setParentId(IdentifiersFactory::FrameId(parent_frame));
    AtomicString name = frame->Tree().GetName();
    if (name.IsEmpty() && frame->DeprecatedLocalOwner()) {
      name =
          frame->DeprecatedLocalOwner()->FastGetAttribute(html_names::kIdAttr);
    }
    frame_object->setName(name);
  }
  if (loader && !loader->UnreachableURL().IsEmpty())
    frame_object->setUnreachableUrl(loader->UnreachableURL().GetString());
  if (frame->IsAdRoot()) {
    frame_object->setAdFrameType(protocol::Page::AdFrameTypeEnum::Root);
  } else if (frame->IsAdSubframe()) {
    frame_object->setAdFrameType(protocol::Page::AdFrameTypeEnum::Child);
  } else {
    frame_object->setAdFrameType(protocol::Page::AdFrameTypeEnum::None);
  }
  return frame_object;
}

std::unique_ptr<protocol::Page::FrameTree>
InspectorPageAgent::BuildObjectForFrameTree(LocalFrame* frame) {
  std::unique_ptr<protocol::Page::FrameTree> result =
      protocol::Page::FrameTree::create()
          .setFrame(BuildObjectForFrame(frame))
          .build();

  std::unique_ptr<protocol::Array<protocol::Page::FrameTree>> children_array;
  for (Frame* child = frame->Tree().FirstChild(); child;
       child = child->Tree().NextSibling()) {
    auto* child_local_frame = DynamicTo<LocalFrame>(child);
    if (!child_local_frame)
      continue;
    if (!children_array) {
      children_array =
          std::make_unique<protocol::Array<protocol::Page::FrameTree>>();
    }
    children_array->emplace_back(BuildObjectForFrameTree(child_local_frame));
  }
  result->setChildFrames(std::move(children_array));
  return result;
}

std::unique_ptr<protocol::Page::FrameResourceTree>
InspectorPageAgent::BuildObjectForResourceTree(LocalFrame* frame) {
  std::unique_ptr<protocol::Page::Frame> frame_object =
      BuildObjectForFrame(frame);
  auto subresources =
      std::make_unique<protocol::Array<protocol::Page::FrameResource>>();

  HeapVector<Member<Resource>> all_resources =
      CachedResourcesForFrame(frame, true);
  for (Resource* cached_resource : all_resources) {
    std::unique_ptr<protocol::Page::FrameResource> resource_object =
        protocol::Page::FrameResource::create()
            .setUrl(UrlWithoutFragment(cached_resource->Url()).GetString())
            .setType(CachedResourceTypeJson(*cached_resource))
            .setMimeType(cached_resource->GetResponse().MimeType())
            .setContentSize(cached_resource->GetResponse().DecodedBodyLength())
            .build();
    base::Optional<base::Time> last_modified =
        cached_resource->GetResponse().LastModified();
    if (last_modified)
      resource_object->setLastModified(last_modified.value().ToDoubleT());
    if (cached_resource->WasCanceled())
      resource_object->setCanceled(true);
    else if (cached_resource->GetStatus() == ResourceStatus::kLoadError)
      resource_object->setFailed(true);
    subresources->emplace_back(std::move(resource_object));
  }

  HeapVector<Member<Document>> all_imports =
      InspectorPageAgent::ImportsForFrame(frame);
  for (Document* import : all_imports) {
    std::unique_ptr<protocol::Page::FrameResource> resource_object =
        protocol::Page::FrameResource::create()
            .setUrl(UrlWithoutFragment(import->Url()).GetString())
            .setType(ResourceTypeJson(InspectorPageAgent::kDocumentResource))
            .setMimeType(import->SuggestedMIMEType())
            .build();
    subresources->emplace_back(std::move(resource_object));
  }

  std::unique_ptr<protocol::Page::FrameResourceTree> result =
      protocol::Page::FrameResourceTree::create()
          .setFrame(std::move(frame_object))
          .setResources(std::move(subresources))
          .build();

  std::unique_ptr<protocol::Array<protocol::Page::FrameResourceTree>>
      children_array;
  for (Frame* child = frame->Tree().FirstChild(); child;
       child = child->Tree().NextSibling()) {
    auto* child_local_frame = DynamicTo<LocalFrame>(child);
    if (!child_local_frame)
      continue;
    if (!children_array) {
      children_array = std::make_unique<
          protocol::Array<protocol::Page::FrameResourceTree>>();
    }
    children_array->emplace_back(BuildObjectForResourceTree(child_local_frame));
  }
  result->setChildFrames(std::move(children_array));
  return result;
}

Response InspectorPageAgent::startScreencast(Maybe<String> format,
                                             Maybe<int> quality,
                                             Maybe<int> max_width,
                                             Maybe<int> max_height,
                                             Maybe<int> every_nth_frame) {
  screencast_enabled_.Set(true);
  return Response::Success();
}

Response InspectorPageAgent::stopScreencast() {
  screencast_enabled_.Set(false);
  return Response::Success();
}

Response InspectorPageAgent::getLayoutMetrics(
    std::unique_ptr<protocol::Page::LayoutViewport>* out_layout_viewport,
    std::unique_ptr<protocol::Page::VisualViewport>* out_visual_viewport,
    std::unique_ptr<protocol::DOM::Rect>* out_content_size) {
  LocalFrame* main_frame = inspected_frames_->Root();
  VisualViewport& visual_viewport = main_frame->GetPage()->GetVisualViewport();

  main_frame->GetDocument()->UpdateStyleAndLayout(
      DocumentUpdateReason::kInspector);

  IntRect visible_contents =
      main_frame->View()->LayoutViewport()->VisibleContentRect();
  *out_layout_viewport = protocol::Page::LayoutViewport::create()
                             .setPageX(visible_contents.X())
                             .setPageY(visible_contents.Y())
                             .setClientWidth(visible_contents.Width())
                             .setClientHeight(visible_contents.Height())
                             .build();

  LocalFrameView* frame_view = main_frame->View();
  ScrollOffset page_offset = frame_view->GetScrollableArea()->GetScrollOffset();
  // page_zoom is either CSS-to-DP or CSS-to-DIP depending on
  // enable-use-zoom-for-dsf flag.
  float page_zoom = main_frame->PageZoomFactor();
  // page_zoom_factor is CSS to DIP (device independent pixels).
  float page_zoom_factor =
      page_zoom /
      main_frame->GetPage()->GetChromeClient().WindowToViewportScalar(
          main_frame, 1);
  FloatRect visible_rect = visual_viewport.VisibleRect();
  float scale = visual_viewport.Scale();

  IntSize content_size = frame_view->GetScrollableArea()->ContentsSize();
  *out_content_size = protocol::DOM::Rect::create()
                          .setX(0)
                          .setY(0)
                          .setWidth(content_size.Width())
                          .setHeight(content_size.Height())
                          .build();

  *out_visual_viewport = protocol::Page::VisualViewport::create()
                             .setOffsetX(AdjustForAbsoluteZoom::AdjustScroll(
                                 visible_rect.X(), page_zoom))
                             .setOffsetY(AdjustForAbsoluteZoom::AdjustScroll(
                                 visible_rect.Y(), page_zoom))
                             .setPageX(AdjustForAbsoluteZoom::AdjustScroll(
                                 page_offset.Width(), page_zoom))
                             .setPageY(AdjustForAbsoluteZoom::AdjustScroll(
                                 page_offset.Height(), page_zoom))
                             .setClientWidth(visible_rect.Width())
                             .setClientHeight(visible_rect.Height())
                             .setScale(scale)
                             .setZoom(page_zoom_factor)
                             .build();
  return Response::Success();
}

protocol::Response InspectorPageAgent::createIsolatedWorld(
    const String& frame_id,
    Maybe<String> world_name,
    Maybe<bool> grant_universal_access,
    int* execution_context_id) {
  LocalFrame* frame =
      IdentifiersFactory::FrameById(inspected_frames_, frame_id);
  if (!frame)
    return Response::ServerError("No frame for given id found");

  scoped_refptr<DOMWrapperWorld> world = EnsureDOMWrapperWorld(
      frame, world_name.fromMaybe(""), grant_universal_access.fromMaybe(false));
  if (!world)
    return Response::ServerError("Could not create isolated world");

  LocalWindowProxy* isolated_world_window_proxy =
      frame->DomWindow()->GetScriptController().WindowProxy(*world);
  v8::HandleScope handle_scope(V8PerIsolateData::MainThreadIsolate());
  *execution_context_id = v8_inspector::V8ContextInfo::executionContextId(
      isolated_world_window_proxy->ContextIfInitialized());
  return Response::Success();
}

Response InspectorPageAgent::setFontFamilies(
    std::unique_ptr<protocol::Page::FontFamilies> font_families) {
  LocalFrame* frame = inspected_frames_->Root();
  auto* settings = frame->GetSettings();
  if (settings) {
    auto& family_settings = settings->GetGenericFontFamilySettings();
    if (font_families->hasStandard()) {
      standard_font_family_.Set(font_families->getStandard(String()));
      family_settings.UpdateStandard(AtomicString(standard_font_family_.Get()));
    }
    if (font_families->hasFixed()) {
      fixed_font_family_.Set(font_families->getFixed(String()));
      family_settings.UpdateFixed(AtomicString(fixed_font_family_.Get()));
    }
    if (font_families->hasSerif()) {
      serif_font_family_.Set(font_families->getSerif(String()));
      family_settings.UpdateSerif(AtomicString(serif_font_family_.Get()));
    }
    if (font_families->hasSansSerif()) {
      sans_serif_font_family_.Set(font_families->getSansSerif(String()));
      family_settings.UpdateSansSerif(
          AtomicString(sans_serif_font_family_.Get()));
    }
    if (font_families->hasCursive()) {
      cursive_font_family_.Set(font_families->getCursive(String()));
      family_settings.UpdateCursive(AtomicString(cursive_font_family_.Get()));
    }
    if (font_families->hasFantasy()) {
      fantasy_font_family_.Set(font_families->getFantasy(String()));
      family_settings.UpdateFantasy(AtomicString(fantasy_font_family_.Get()));
    }
    if (font_families->hasPictograph()) {
      pictograph_font_family_.Set(font_families->getPictograph(String()));
      family_settings.UpdatePictograph(
          AtomicString(pictograph_font_family_.Get()));
    }
    settings->NotifyGenericFontFamilyChange();
  }

  return Response::Success();
}

Response InspectorPageAgent::setFontSizes(
    std::unique_ptr<protocol::Page::FontSizes> font_sizes) {
  LocalFrame* frame = inspected_frames_->Root();
  auto* settings = frame->GetSettings();
  if (settings) {
    if (font_sizes->hasStandard()) {
      standard_font_size_.Set(font_sizes->getStandard(0));
      settings->SetDefaultFontSize(standard_font_size_.Get());
    }
    if (font_sizes->hasFixed()) {
      fixed_font_size_.Set(font_sizes->getFixed(0));
      settings->SetDefaultFixedFontSize(fixed_font_size_.Get());
    }
  }

  return Response::Success();
}

void InspectorPageAgent::ConsumeCompilationCache(
    const ScriptSourceCode& source,
    v8::ScriptCompiler::CachedData** cached_data) {
  if (source.SourceLocationType() != ScriptSourceLocationType::kExternalFile)
    return;
  if (source.Url().IsEmpty())
    return;
  auto it = compilation_cache_.find(source.Url().GetString());
  if (it == compilation_cache_.end())
    return;
  const protocol::Binary& data = it->value;
  *cached_data = new v8::ScriptCompiler::CachedData(
      data.data(), data.size(), v8::ScriptCompiler::CachedData::BufferNotOwned);
}

void InspectorPageAgent::ProduceCompilationCache(const ScriptSourceCode& source,
                                                 v8::Local<v8::Script> script) {
  if (!produce_compilation_cache_.Get())
    return;
  KURL url = source.Url();
  if (source.Streamer())
    return;
  if (source.SourceLocationType() != ScriptSourceLocationType::kExternalFile)
    return;
  if (url.IsEmpty())
    return;
  String url_string = url.GetString();
  auto it = compilation_cache_.find(url_string);
  if (it != compilation_cache_.end())
    return;
  static const int kMinimalCodeLength = 1024;
  if (source.Source().length() < kMinimalCodeLength)
    return;
  std::unique_ptr<v8::ScriptCompiler::CachedData> cached_data(
      v8::ScriptCompiler::CreateCodeCache(script->GetUnboundScript()));
  if (cached_data) {
    // CachedData produced by CreateCodeCache always owns its buffer.
    CHECK_EQ(cached_data->buffer_policy,
             v8::ScriptCompiler::CachedData::BufferOwned);
    GetFrontend()->compilationCacheProduced(
        url_string, protocol::Binary::fromCachedData(std::move(cached_data)));
  }
}

void InspectorPageAgent::FileChooserOpened(LocalFrame* frame,
                                           HTMLInputElement* element,
                                           bool* intercepted) {
  *intercepted |= intercept_file_chooser_;
  if (!intercept_file_chooser_)
    return;
  bool multiple = element->Multiple();
  GetFrontend()->fileChooserOpened(
      IdentifiersFactory::FrameId(frame), DOMNodeIds::IdForNode(element),
      multiple ? protocol::Page::FileChooserOpened::ModeEnum::SelectMultiple
               : protocol::Page::FileChooserOpened::ModeEnum::SelectSingle);
}

Response InspectorPageAgent::setProduceCompilationCache(bool enabled) {
  produce_compilation_cache_.Set(enabled);
  return Response::Success();
}

Response InspectorPageAgent::addCompilationCache(const String& url,
                                                 const protocol::Binary& data) {
  compilation_cache_.Set(url, data);
  return Response::Success();
}

Response InspectorPageAgent::clearCompilationCache() {
  compilation_cache_.clear();
  return Response::Success();
}

Response InspectorPageAgent::waitForDebugger() {
  client_->WaitForDebugger();
  return Response::Success();
}

Response InspectorPageAgent::setInterceptFileChooserDialog(bool enabled) {
  intercept_file_chooser_ = enabled;
  return Response::Success();
}

Response InspectorPageAgent::generateTestReport(const String& message,
                                                Maybe<String> group) {
  LocalDOMWindow* window = inspected_frames_->Root()->DomWindow();

  // Construct the test report.
  TestReportBody* body = MakeGarbageCollected<TestReportBody>(message);
  Report* report = MakeGarbageCollected<Report>(
      "test", window->document()->Url().GetString(), body);

  // Send the test report to any ReportingObservers.
  ReportingContext::From(window)->QueueReport(report);

  return Response::Success();
}

void InspectorPageAgent::Trace(Visitor* visitor) const {
  visitor->Trace(inspected_frames_);
  visitor->Trace(inspector_resource_content_loader_);
  visitor->Trace(isolated_worlds_);
  InspectorBaseAgent::Trace(visitor);
}

}  // namespace blink
