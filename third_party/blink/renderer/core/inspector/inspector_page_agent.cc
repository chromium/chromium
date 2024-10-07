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
#include <optional>
#include <utility>

#include "base/containers/span.h"
#include "base/numerics/safe_conversions.h"
#include "build/build_config.h"
#include "third_party/blink/public/common/frame/frame_ad_evidence.h"
#include "third_party/blink/public/common/origin_trials/trial_token.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "third_party/blink/public/mojom/ad_tagging/ad_evidence.mojom-blink.h"
#include "third_party/blink/public/mojom/loader/same_document_navigation_type.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/local_window_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/script_evaluation_result.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_timing.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/frame/ad_tracker.h"
#include "third_party/blink/renderer/core/frame/frame.h"
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
#include "third_party/blink/renderer/core/html/parser/text_resource_decoder.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/core/inspector/inspected_frames.h"
#include "third_party/blink/renderer/core/inspector/inspector_css_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_resource_content_loader.h"
#include "third_party/blink/renderer/core/inspector/v8_inspector_string.h"
#include "third_party/blink/renderer/core/layout/adjust_for_absolute_zoom.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/core/loader/idleness_detector.h"
#include "third_party/blink/renderer/core/loader/resource/css_style_sheet_resource.h"
#include "third_party/blink/renderer/core/loader/resource/script_resource.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trial_context.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/permissions_policy/permissions_policy_devtools_support.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/script/classic_script.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/script_regexp.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/text_resource_decoder_options.h"
#include "third_party/blink/renderer/platform/network/mime/mime_type_registry.h"
#include "third_party/blink/renderer/platform/network/network_utils.h"
#include "third_party/blink/renderer/platform/text/locale_to_script_mapping.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/text/base64.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/inspector_protocol/crdtp/protocol_core.h"
#include "ui/display/screen_info.h"
#include "v8/include/v8-inspector.h"

namespace blink {

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
    case ClientNavigationReason::kInitialFrameNavigation:
      return ReasonEnum::InitialFrameNavigation;
    case ClientNavigationReason::kMetaTagRefresh:
      return ReasonEnum::MetaTagRefresh;
    case ClientNavigationReason::kPageBlock:
      return ReasonEnum::PageBlockInterstitial;
    case ClientNavigationReason::kReload:
      return ReasonEnum::Reload;
    case ClientNavigationReason::kNone:
      return ReasonEnum::Other;
  }
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
    case kNavigationPolicyPictureInPicture:
      return DispositionEnum::NewWindow;
    case kNavigationPolicyLinkPreview:
      NOTREACHED();
  }
  return DispositionEnum::CurrentTab;
}

String FrameDetachTypeToProtocol(FrameDetachType type) {
  namespace ReasonEnum = protocol::Page::FrameDetached::ReasonEnum;
  switch (type) {
    case FrameDetachType::kRemove:
      return ReasonEnum::Remove;
    case FrameDetachType::kSwap:
      return ReasonEnum::Swap;
  }
}

Resource* CachedResource(LocalFrame* frame,
                         const KURL& url,
                         InspectorResourceContentLoader* loader) {
  Document* document = frame->GetDocument();
  if (!document)
    return nullptr;
  Resource* cached_resource = document->Fetcher()->CachedResource(url);
  if (!cached_resource) {
    cached_resource = MemoryCache::Get()->ResourceForURL(
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
  if (!window_features.is_popup) {
    feature_strings->emplace_back("menubar");
    feature_strings->emplace_back("toolbar");
    feature_strings->emplace_back("status");
    feature_strings->emplace_back("scrollbars");
  }
  if (window_features.resizable)
    feature_strings->emplace_back("resizable");
  if (window_features.noopener)
    feature_strings->emplace_back("noopener");
  if (window_features.explicit_opener) {
    feature_strings->emplace_back("opener");
  }
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
         type == ResourceType::kScript || type == ResourceType::kRaw;
}

static std::unique_ptr<TextResourceDecoder> CreateResourceTextDecoder(
    const String& mime_type,
    const String& text_encoding_name) {
  if (!text_encoding_name.empty()) {
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
                                   base::span<const uint8_t> buffer,
                                   String* result,
                                   bool* base64_encoded) {
  if (!text_content.IsNull()) {
    *result = text_content;
    *base64_encoded = false;
  } else if (buffer.data()) {
    *result = Base64Encode(buffer);
    *base64_encoded = true;
  } else {
    *result = "";
    *base64_encoded = false;
  }
}

static void MaybeEncodeTextContent(const String& text_content,
                                   scoped_refptr<const SharedBuffer> buffer,
                                   String* result,
                                   bool* base64_encoded) {
  if (!buffer) {
    const base::span<const uint8_t> empty;
    return MaybeEncodeTextContent(text_content, empty, result, base64_encoded);
  }

  const SegmentedBuffer::DeprecatedFlatData flat_buffer(buffer.get());
  return MaybeEncodeTextContent(text_content, base::as_byte_span(flat_buffer),
                                result, base64_encoded);
}

// static
KURL InspectorPageAgent::UrlWithoutFragment(const KURL& url) {
  KURL result = url;
  result.RemoveFragmentIdentifier();
  return result;
}

// static
bool InspectorPageAgent::SegmentedBufferContent(
    const SegmentedBuffer* buffer,
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

  const SegmentedBuffer::DeprecatedFlatData flat_buffer(buffer);
  const auto byte_buffer = base::as_byte_span(flat_buffer);
  if (decoder) {
    text_content = decoder->Decode(byte_buffer);
    text_content = text_content + decoder->Flush();
  } else if (encoding.IsValid()) {
    text_content = encoding.Decode(byte_buffer);
  }

  MaybeEncodeTextContent(text_content, byte_buffer, result, base64_encoded);
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

    const SegmentedBuffer::DeprecatedFlatData flat_buffer(buffer.get());
    *result = Base64Encode(base::as_byte_span(flat_buffer));
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
          To<CSSStyleSheetResource>(cached_resource)
              ->SheetText(nullptr, CSSStyleSheetResource::MIMETypeCheck::kLax),
          cached_resource->ResourceBuffer(), result, base64_encoded);
      return true;
    case blink::ResourceType::kScript:
      MaybeEncodeTextContent(
          To<ScriptResource>(cached_resource)->TextForInspector(),
          cached_resource->ResourceBuffer(), result, base64_encoded);
      return true;
    default:
      String text_encoding_name =
          cached_resource->GetResponse().TextEncodingName();
      if (text_encoding_name.empty() &&
          cached_resource->GetType() != blink::ResourceType::kRaw)
        text_encoding_name = "WinLatin1";
      return InspectorPageAgent::SegmentedBufferContent(
          cached_resource->ResourceBuffer().get(),
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
    case kPingResource:
      return protocol::Network::ResourceTypeEnum::Ping;
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
    default:
      break;
  }
  return InspectorPageAgent::kOtherResource;
}

String InspectorPageAgent::CachedResourceTypeJson(
    const Resource& cached_resource) {
  return ResourceTypeJson(ToResourceType(cached_resource.GetType()));
}

InspectorPageAgent::PageReloadScriptInjection::PageReloadScriptInjection(
    InspectorAgentState& agent_state)
    : pending_script_to_evaluate_on_load_once_(&agent_state,
                                               /*default_value=*/{}),
      target_url_for_pending_script_(&agent_state,
                                     /*default_value=*/{}) {}

void InspectorPageAgent::PageReloadScriptInjection::clear() {
  script_to_evaluate_on_load_once_ = {};
  pending_script_to_evaluate_on_load_once_.Set({});
  target_url_for_pending_script_.Set({});
}

void InspectorPageAgent::PageReloadScriptInjection::SetPending(
    String script,
    const KURL& target_url) {
  pending_script_to_evaluate_on_load_once_.Set(script);
  target_url_for_pending_script_.Set(target_url.GetString().GetString());
}

void InspectorPageAgent::PageReloadScriptInjection::PromoteToLoadOnce() {
  script_to_evaluate_on_load_once_ =
      pending_script_to_evaluate_on_load_once_.Get();
  target_url_for_active_script_ = target_url_for_pending_script_.Get();
  pending_script_to_evaluate_on_load_once_.Set({});
  target_url_for_pending_script_.Set({});
}

String InspectorPageAgent::PageReloadScriptInjection::GetScriptForInjection(
    const KURL& target_url) {
  if (target_url_for_active_script_ == target_url.GetString()) {
    return script_to_evaluate_on_load_once_;
  }
  return {};
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
      intercept_file_chooser_(&agent_state_, false),
      enabled_(&agent_state_, /*default_value=*/false),
      screencast_enabled_(&agent_state_, /*default_value=*/false),
      lifecycle_events_enabled_(&agent_state_, /*default_value=*/false),
      bypass_csp_enabled_(&agent_state_, /*default_value=*/false),
      scripts_to_evaluate_on_load_(&agent_state_,
                                   /*default_value=*/String()),
      worlds_to_evaluate_on_load_(&agent_state_,
                                  /*default_value=*/String()),
      include_command_line_api_for_scripts_to_evaluate_on_load_(
          &agent_state_,
          /*default_value=*/false),
      standard_font_size_(&agent_state_, /*default_value=*/0),
      fixed_font_size_(&agent_state_, /*default_value=*/0),
      script_font_families_cbor_(&agent_state_, std::vector<uint8_t>()),
      script_injection_on_load_(agent_state_) {}

void InspectorPageAgent::Restore() {
  if (enabled_.Get())
    enable();
  if (bypass_csp_enabled_.Get())
    setBypassCSP(true);
  LocalFrame* frame = inspected_frames_->Root();
  auto* settings = frame->GetSettings();
  if (settings) {
    // Re-apply generic fonts overrides.
    if (!script_font_families_cbor_.Get().empty()) {
      protocol::Array<protocol::Page::ScriptFontFamilies> script_font_families;
      crdtp::DeserializerState state(script_font_families_cbor_.Get());
      bool result = crdtp::ProtocolTypeTraits<
          protocol::Array<protocol::Page::ScriptFontFamilies>>::
          Deserialize(&state, &script_font_families);
      CHECK(result);
      auto& family_settings = settings->GetGenericFontFamilySettings();
      setFontFamilies(family_settings, script_font_families);
      settings->NotifyGenericFontFamilyChange();
    }
    // Re-apply default font size overrides.
    if (standard_font_size_.Get() != 0)
      settings->SetDefaultFontSize(standard_font_size_.Get());
    if (fixed_font_size_.Get() != 0)
      settings->SetDefaultFixedFontSize(fixed_font_size_.Get());
  }
}

protocol::Response InspectorPageAgent::enable() {
  enabled_.Set(true);
  instrumenting_agents_->AddInspectorPageAgent(this);
  return protocol::Response::Success();
}

protocol::Response InspectorPageAgent::disable() {
  agent_state_.ClearAllFields();
  pending_isolated_worlds_.clear();
  script_injection_on_load_.clear();
  instrumenting_agents_->RemoveInspectorPageAgent(this);
  inspector_resource_content_loader_->Cancel(
      resource_content_loader_client_id_);
  requested_compilation_cache_.clear();
  compilation_cache_.clear();
  ad_script_identifiers_.clear();
  stopScreencast();

  return protocol::Response::Success();
}

protocol::Response InspectorPageAgent::addScriptToEvaluateOnNewDocument(
    const String& source,
    Maybe<String> world_name,
    Maybe<bool> include_command_line_api,
    Maybe<bool> runImmediately,
    String* identifier) {
  Vector<WTF::String> keys = scripts_to_evaluate_on_load_.Keys();
  auto result = std::max_element(
      keys.begin(), keys.end(), [](const WTF::String& a, const WTF::String& b) {
        return Decimal::FromString(a) < Decimal::FromString(b);
      });
  if (result == keys.end()) {
    *identifier = String::Number(1);
  } else {
    *identifier = String::Number(Decimal::FromString(*result).ToDouble() + 1);
  }

  scripts_to_evaluate_on_load_.Set(*identifier, source);
  worlds_to_evaluate_on_load_.Set(*identifier, world_name.value_or(""));
  include_command_line_api_for_scripts_to_evaluate_on_load_.Set(
      *identifier, include_command_line_api.value_or(false));

  if (client_->IsPausedForNewWindow() || runImmediately.value_or(false)) {
    // client_->IsPausedForNewWindow(): When opening a new popup,
    // Page.addScriptToEvaluateOnNewDocument could be called after
    // Runtime.enable that forces main context creation. In this case, we would
    // not normally evaluate the script, but we should.
    for (LocalFrame* frame : *inspected_frames_) {
      EvaluateScriptOnNewDocument(*frame, *identifier);
    }
  }

  return protocol::Response::Success();
}

protocol::Response InspectorPageAgent::removeScriptToEvaluateOnNewDocument(
    const String& identifier) {
  if (scripts_to_evaluate_on_load_.Get(identifier).IsNull())
    return protocol::Response::ServerError("Script not found");
  scripts_to_evaluate_on_load_.Clear(identifier);
  worlds_to_evaluate_on_load_.Clear(identifier);
  include_command_line_api_for_scripts_to_evaluate_on_load_.Clear(identifier);
  return protocol::Response::Success();
}

protocol::Response InspectorPageAgent::addScriptToEvaluateOnLoad(
    const String& source,
    String* identifier) {
  return addScriptToEvaluateOnNewDocument(source, Maybe<String>(""),
                                          Maybe<bool>(false),
                                          Maybe<bool>(false), identifier);
}

protocol::Response InspectorPageAgent::removeScriptToEvaluateOnLoad(
    const String& identifier) {
  return removeScriptToEvaluateOnNewDocument(identifier);
}

protocol::Response InspectorPageAgent::setLifecycleEventsEnabled(bool enabled) {
  lifecycle_events_enabled_.Set(enabled);
  if (!enabled)
    return protocol::Response::Success();

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

  return protocol::Response::Success();
}

protocol::Response InspectorPageAgent::setAdBlockingEnabled(bool enable) {
  return protocol::Response::Success();
}

protocol::Response InspectorPageAgent::reload(
    Maybe<bool> optional_bypass_cache,
    Maybe<String> optional_script_to_evaluate_on_load,
    Maybe<String> loader_id) {
  if (loader_id.has_value() && inspected_frames_->Root()
                                       ->Loader()
                                       .GetDocumentLoader()
                                       ->GetDevToolsNavigationToken()
                                       .ToString() != loader_id->Ascii()) {
    return protocol::Response::InvalidParams("Document already navigated");
  }
  script_injection_on_load_.SetPending(
      optional_script_to_evaluate_on_load.value_or(""),
      inspected_frames_->Root()->Loader().GetDocumentLoader()->Url());
  v8_session_->setSkipAllPauses(true);
  v8_session_->resume(true /* terminate on resume */);
  return protocol::Response::Success();
}

protocol::Response InspectorPageAgent::stopLoading() {
  return protocol::Response::Success();
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
static HeapVector<Member<Resource>> CachedResourcesForFrame(LocalFrame* frame,
                                                            bool skip_xhrs) {
  HeapVector<Member<Resource>> result;
  CachedResourcesForDocument(frame->GetDocument(), result, skip_xhrs);
  return result;
}

protocol::Response InspectorPageAgent::getResourceTree(
    std::unique_ptr<protocol::Page::FrameResourceTree>* object) {
  *object = BuildObjectForResourceTree(inspected_frames_->Root());
  return protocol::Response::Success();
}

protocol::Response InspectorPageAgent::getFrameTree(
    std::unique_ptr<protocol::Page::FrameTree>* object) {
  *object = BuildObjectForFrameTree(inspected_frames_->Root());
  return protocol::Response::Success();
}

void InspectorPageAgent::GetResourceContentAfterResourcesContentLoaded(
    const String& frame_id,
    const String& url,
    std::unique_ptr<GetResourceContentCallback> callback) {
  LocalFrame* frame =
      IdentifiersFactory::FrameById(inspected_frames_, frame_id);
  if (!frame) {
    callback->sendFailure(
        protocol::Response::ServerError("No frame for given id found"));
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
        protocol::Response::ServerError("No resource with given URL found"));
  }
}

void InspectorPageAgent::getResourceContent(
    const String& frame_id,
    const String& url,
    std::unique_ptr<GetResourceContentCallback> callback) {
  if (!enabled_.Get()) {
    callback->sendFailure(
        protocol::Response::ServerError("Agent is not enabled."));
    return;
  }
  inspector_resource_content_loader_->EnsureResourcesContentLoaded(
      resource_content_loader_client_id_,
      WTF::BindOnce(
          &InspectorPageAgent::GetResourceContentAfterResourcesContentLoaded,
          WrapPersistent(this), frame_id, url, std::move(callback)));
}

protocol::Response InspectorPageAgent::getAdScriptId(
    const String& frame_id,
    Maybe<protocol::Page::AdScriptId>* ad_script_id) {
  if (ad_script_identifiers_.Contains(frame_id)) {
    AdScriptIdentifier* ad_script_identifier =
        ad_script_identifiers_.at(frame_id);
    *ad_script_id =
        protocol::Page::AdScriptId::create()
            .setScriptId(String::Number(ad_script_identifier->id))
            .setDebuggerId(ToCoreString(
                ad_script_identifier->context_id.toString()->string()))
            .build();
  }

  return protocol::Response::Success();
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
    callback->sendFailure(
        protocol::Response::ServerError("No frame for given id found"));
    return;
  }
  String content;
  bool base64_encoded;
  if (!InspectorPageAgent::CachedResourceContent(
          CachedResource(frame, KURL(url), inspector_resource_content_loader_),
          &content, &base64_encoded)) {
    callback->sendFailure(
        protocol::Response::ServerError("No resource with given URL found"));
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
    callback->sendFailure(
        protocol::Response::ServerError("Agent is not enabled."));
    return;
  }
  inspector_resource_content_loader_->EnsureResourcesContentLoaded(
      resource_content_loader_client_id_,
      WTF::BindOnce(
          &InspectorPageAgent::SearchContentAfterResourcesContentLoaded,
          WrapPersistent(this), frame_id, url, query,
          optional_case_sensitive.value_or(false),
          optional_is_regex.value_or(false), std::move(callback)));
}

protocol::Response InspectorPageAgent::setBypassCSP(bool enabled) {
  LocalFrame* frame = inspected_frames_->Root();
  frame->GetSettings()->SetBypassCSP(enabled);
  bypass_csp_enabled_.Set(enabled);
  return protocol::Response::Success();
}

namespace {

std::unique_ptr<protocol::Page::PermissionsPolicyBlockLocator>
CreatePermissionsPolicyBlockLocator(
    const blink::PermissionsPolicyBlockLocator& locator) {
  protocol::Page::PermissionsPolicyBlockReason reason;
  switch (locator.reason) {
    case blink::PermissionsPolicyBlockReason::kHeader:
      reason = protocol::Page::PermissionsPolicyBlockReasonEnum::Header;
      break;
    case blink::PermissionsPolicyBlockReason::kIframeAttribute:
      reason =
          protocol::Page::PermissionsPolicyBlockReasonEnum::IframeAttribute;
      break;
    case blink::PermissionsPolicyBlockReason::kInFencedFrameTree:
      reason =
          protocol::Page::PermissionsPolicyBlockReasonEnum::InFencedFrameTree;
      break;
    case blink::PermissionsPolicyBlockReason::kInIsolatedApp:
      reason = protocol::Page::PermissionsPolicyBlockReasonEnum::InIsolatedApp;
      break;
  }

  return protocol::Page::PermissionsPolicyBlockLocator::create()
      .setFrameId(locator.frame_id)
      .setBlockReason(reason)
      .build();
}
}  // namespace

protocol::Response InspectorPageAgent::getPermissionsPolicyState(
    const String& frame_id,
    std::unique_ptr<
        protocol::Array<protocol::Page::PermissionsPolicyFeatureState>>*
        states) {
  LocalFrame* frame =
      IdentifiersFactory::FrameById(inspected_frames_, frame_id);

  if (!frame) {
    return protocol::Response::ServerError(
        "No frame for given id found in this target");
  }

  const blink::PermissionsPolicy* permissions_policy =
      frame->GetSecurityContext()->GetPermissionsPolicy();

  if (!permissions_policy)
    return protocol::Response::ServerError("Frame not ready");

  auto feature_states = std::make_unique<
      protocol::Array<protocol::Page::PermissionsPolicyFeatureState>>();

  bool is_isolated_context =
      frame->DomWindow() && frame->DomWindow()->IsIsolatedContext();
  for (const auto& entry :
       blink::GetDefaultFeatureNameMap(is_isolated_context)) {
    const String& feature_name = entry.key;
    const mojom::blink::PermissionsPolicyFeature feature = entry.value;

    if (blink::DisabledByOriginTrial(feature_name, frame->DomWindow()))
      continue;

    std::optional<blink::PermissionsPolicyBlockLocator> locator =
        blink::TracePermissionsPolicyBlockSource(frame, feature);

    std::unique_ptr<protocol::Page::PermissionsPolicyFeatureState>
        feature_state =
            protocol::Page::PermissionsPolicyFeatureState::create()
                .setFeature(blink::PermissionsPolicyFeatureToProtocol(
                    feature, frame->DomWindow()))
                .setAllowed(!locator.has_value())
                .build();

    if (locator.has_value())
      feature_state->setLocator(CreatePermissionsPolicyBlockLocator(*locator));

    feature_states->push_back(std::move(feature_state));
  }

  *states = std::move(feature_states);
  return protocol::Response::Success();
}

protocol::Response InspectorPageAgent::setDocumentContent(
    const String& frame_id,
    const String& html) {
  LocalFrame* frame =
      IdentifiersFactory::FrameById(inspected_frames_, frame_id);
  if (!frame)
    return protocol::Response::ServerError("No frame for given id found");

  Document* document = frame->GetDocument();
  if (!document) {
    return protocol::Response::ServerError(
        "No Document instance to set HTML for");
  }
  document->SetContent(html);
  return protocol::Response::Success();
}

namespace {
const char* NavigationTypeToProtocolString(
    mojom::blink::SameDocumentNavigationType navigation_type) {
  switch (navigation_type) {
    case mojom::blink::SameDocumentNavigationType::kFragment:
      return protocol::Page::NavigatedWithinDocument::NavigationTypeEnum::
          Fragment;
    case mojom::blink::SameDocumentNavigationType::kHistoryApi:
      return protocol::Page::NavigatedWithinDocument::NavigationTypeEnum::
          HistoryApi;
    case mojom::blink::SameDocumentNavigationType::kNavigationApiIntercept:
    case mojom::blink::SameDocumentNavigationType::
        kPrerenderNoVarySearchActivation:
      return protocol::Page::NavigatedWithinDocument::NavigationTypeEnum::Other;
  }
}
}  // namespace

void InspectorPageAgent::DidNavigateWithinDocument(
    LocalFrame* frame,
    mojom::blink::SameDocumentNavigationType navigation_type) {
  Document* document = frame->GetDocument();
  if (document) {
    return GetFrontend()->navigatedWithinDocument(
        IdentifiersFactory::FrameId(frame), document->Url(),
        NavigationTypeToProtocolString(navigation_type));
  }
}

DOMWrapperWorld* InspectorPageAgent::EnsureDOMWrapperWorld(
    LocalFrame* frame,
    const String& world_name,
    bool grant_universal_access) {
  if (!isolated_worlds_.Contains(frame))
    isolated_worlds_.Set(frame, MakeGarbageCollected<FrameIsolatedWorlds>());
  FrameIsolatedWorlds& frame_worlds = *isolated_worlds_.find(frame)->value;

  auto world_it = frame_worlds.find(world_name);
  if (world_it != frame_worlds.end())
    return world_it->value;
  LocalDOMWindow* window = frame->DomWindow();
  DOMWrapperWorld* world =
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

void InspectorPageAgent::DidCreateMainWorldContext(LocalFrame* frame) {
  if (!GetFrontend())
    return;

  for (auto& request : pending_isolated_worlds_.Take(frame)) {
    CreateIsolatedWorldImpl(*frame, request.world_name,
                            request.grant_universal_access,
                            std::move(request.callback));
  }
  Vector<WTF::String> keys = scripts_to_evaluate_on_load_.Keys();
  std::sort(keys.begin(), keys.end(),
            [](const WTF::String& a, const WTF::String& b) {
              return Decimal::FromString(a) < Decimal::FromString(b);
            });

  for (const WTF::String& key : keys) {
    EvaluateScriptOnNewDocument(*frame, key);
  }

  String script = script_injection_on_load_.GetScriptForInjection(
      frame->Loader().GetDocumentLoader()->Url());
  if (script.empty()) {
    return;
  }
  ScriptState* script_state = ToScriptStateForMainWorld(frame);
  if (!script_state || !v8_session_) {
    return;
  }

  v8_session_->evaluate(script_state->GetContext(),
                        ToV8InspectorStringView(script));
}

void InspectorPageAgent::EvaluateScriptOnNewDocument(
    LocalFrame& frame,
    const String& script_identifier) {
  auto* window = frame.DomWindow();
  v8::HandleScope handle_scope(window->GetIsolate());

  ScriptState* script_state = nullptr;
  const String world_name = worlds_to_evaluate_on_load_.Get(script_identifier);
  if (world_name.empty()) {
    script_state = ToScriptStateForMainWorld(window->GetFrame());
  } else if (DOMWrapperWorld* world = EnsureDOMWrapperWorld(
                 &frame, world_name, true /* grant_universal_access */)) {
    script_state =
        ToScriptState(window->GetFrame(),
                      *DOMWrapperWorld::EnsureIsolatedWorld(
                          ToIsolate(window->GetFrame()), world->GetWorldId()));
  }
  if (!script_state || !v8_session_) {
    return;
  }

  v8_session_->evaluate(
      script_state->GetContext(),
      ToV8InspectorStringView(
          scripts_to_evaluate_on_load_.Get(script_identifier)),
      include_command_line_api_for_scripts_to_evaluate_on_load_.Get(
          script_identifier));
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
    script_injection_on_load_.PromoteToLoadOnce();
  }
  GetFrontend()->frameNavigated(BuildObjectForFrame(loader->GetFrame()),
                                protocol::Page::NavigationTypeEnum::Navigation);
  GetFrontend()->flush();
}

void InspectorPageAgent::DidRestoreFromBackForwardCache(LocalFrame* frame) {
  GetFrontend()->frameNavigated(
      BuildObjectForFrame(frame),
      protocol::Page::NavigationTypeEnum::BackForwardCacheRestore);
}

void InspectorPageAgent::DidOpenDocument(LocalFrame* frame,
                                         DocumentLoader* loader) {
  GetFrontend()->documentOpened(BuildObjectForFrame(loader->GetFrame()));
  LifecycleEvent(frame, loader, "init",
                 base::TimeTicks::Now().since_origin().InSecondsF());
}

void InspectorPageAgent::FrameAttachedToParent(
    LocalFrame* frame,
    const std::optional<AdScriptIdentifier>& ad_script_on_stack) {
  // TODO(crbug.com/1217041): If an ad script on the stack caused this frame to
  // be tagged as an ad, send the script's ID to the frontend.
  Frame* parent_frame = frame->Tree().Parent();
  std::unique_ptr<SourceLocation> location =
      SourceLocation::CaptureWithFullStackTrace();
  if (ad_script_on_stack.has_value()) {
    ad_script_identifiers_.Set(
        IdentifiersFactory::FrameId(frame),
        std::make_unique<AdScriptIdentifier>(ad_script_on_stack.value()));
  }
  GetFrontend()->frameAttached(
      IdentifiersFactory::FrameId(frame),
      IdentifiersFactory::FrameId(parent_frame),
      location ? location->BuildInspectorObject() : nullptr);
  // Some network events referencing this frame will be reported from the
  // browser, so make sure to deliver FrameAttached without buffering,
  // so it gets to the front-end first.
  GetFrontend()->flush();
}

void InspectorPageAgent::FrameDetachedFromParent(LocalFrame* frame,
                                                 FrameDetachType type) {
  // If the frame is swapped, we still maintain the ad script id for it.
  if (type == FrameDetachType::kRemove)
    ad_script_identifiers_.erase(IdentifiersFactory::FrameId(frame));

  GetFrontend()->frameDetached(IdentifiersFactory::FrameId(frame),
                               FrameDetachTypeToProtocol(type));
}

void InspectorPageAgent::FrameSubtreeWillBeDetached(Frame* frame) {
  GetFrontend()->frameSubtreeWillBeDetached(IdentifiersFactory::FrameId(frame));
  GetFrontend()->flush();
}

bool InspectorPageAgent::ScreencastEnabled() {
  return enabled_.Get() && screencast_enabled_.Get();
}

void InspectorPageAgent::FrameStoppedLoading(LocalFrame* frame) {
  // The actual event is reported by the browser, but let's make sure
  // earlier events from the commit make their way to client first.
  GetFrontend()->flush();
}

void InspectorPageAgent::FrameRequestedNavigation(Frame* target_frame,
                                                  const KURL& url,
                                                  ClientNavigationReason reason,
                                                  NavigationPolicy policy) {
  // TODO(b:303396822): Support Link Preview
  if (policy == kNavigationPolicyLinkPreview) {
    return;
  }

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
  if (!inspected_frames_->Root()->IsOutermostMainFrame())
    return;
#if !BUILDFLAG(IS_ANDROID)
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
  } else if (context->IsFeatureEnabled(mojom::blink::PermissionsPolicyFeature::
                                           kCrossOriginIsolated)) {
    return protocol::Page::CrossOriginIsolatedContextTypeEnum::NotIsolated;
  }
  return protocol::Page::CrossOriginIsolatedContextTypeEnum::
      NotIsolatedFeatureDisabled;
}
std::unique_ptr<std::vector<protocol::Page::GatedAPIFeatures>>
CreateGatedAPIFeaturesArray(LocalDOMWindow* window) {
  auto features =
      std::make_unique<std::vector<protocol::Page::GatedAPIFeatures>>();
  // SABs are available if at least one of the following is true:
  //  - features::kWebAssemblyThreads enabled
  //  - features::kSharedArrayBuffer enabled
  //  - agent has the cross-origin isolated bit (but not necessarily the
  //    capability)
  if (RuntimeEnabledFeatures::SharedArrayBufferEnabled(window) ||
      Agent::IsCrossOriginIsolated()) {
    features->push_back(
        protocol::Page::GatedAPIFeaturesEnum::SharedArrayBuffers);
  }
  if (window->SharedArrayBufferTransferAllowed()) {
    features->push_back(protocol::Page::GatedAPIFeaturesEnum::
                            SharedArrayBuffersTransferAllowed);
  }
  // TODO(chromium:1139899): Report availablility of performance.measureMemory()
  // and performance.profile() once they are gated/available, respectively.
  return features;
}

protocol::Page::OriginTrialTokenStatus CreateOriginTrialTokenStatus(
    blink::OriginTrialTokenStatus status) {
  switch (status) {
    case blink::OriginTrialTokenStatus::kSuccess:
      return protocol::Page::OriginTrialTokenStatusEnum::Success;
    case blink::OriginTrialTokenStatus::kNotSupported:
      return protocol::Page::OriginTrialTokenStatusEnum::NotSupported;
    case blink::OriginTrialTokenStatus::kInsecure:
      return protocol::Page::OriginTrialTokenStatusEnum::Insecure;
    case blink::OriginTrialTokenStatus::kExpired:
      return protocol::Page::OriginTrialTokenStatusEnum::Expired;
    case blink::OriginTrialTokenStatus::kWrongOrigin:
      return protocol::Page::OriginTrialTokenStatusEnum::WrongOrigin;
    case blink::OriginTrialTokenStatus::kInvalidSignature:
      return protocol::Page::OriginTrialTokenStatusEnum::InvalidSignature;
    case blink::OriginTrialTokenStatus::kMalformed:
      return protocol::Page::OriginTrialTokenStatusEnum::Malformed;
    case blink::OriginTrialTokenStatus::kWrongVersion:
      return protocol::Page::OriginTrialTokenStatusEnum::WrongVersion;
    case blink::OriginTrialTokenStatus::kFeatureDisabled:
      return protocol::Page::OriginTrialTokenStatusEnum::FeatureDisabled;
    case blink::OriginTrialTokenStatus::kTokenDisabled:
      return protocol::Page::OriginTrialTokenStatusEnum::TokenDisabled;
    case blink::OriginTrialTokenStatus::kFeatureDisabledForUser:
      return protocol::Page::OriginTrialTokenStatusEnum::FeatureDisabledForUser;
    case blink::OriginTrialTokenStatus::kUnknownTrial:
      return protocol::Page::OriginTrialTokenStatusEnum::UnknownTrial;
  }
}

protocol::Page::OriginTrialStatus CreateOriginTrialStatus(
    blink::OriginTrialStatus status) {
  switch (status) {
    case blink::OriginTrialStatus::kEnabled:
      return protocol::Page::OriginTrialStatusEnum::Enabled;
    case blink::OriginTrialStatus::kValidTokenNotProvided:
      return protocol::Page::OriginTrialStatusEnum::ValidTokenNotProvided;
    case blink::OriginTrialStatus::kOSNotSupported:
      return protocol::Page::OriginTrialStatusEnum::OSNotSupported;
    case blink::OriginTrialStatus::kTrialNotAllowed:
      return protocol::Page::OriginTrialStatusEnum::TrialNotAllowed;
  }
}

protocol::Page::OriginTrialUsageRestriction CreateOriginTrialUsageRestriction(
    blink::TrialToken::UsageRestriction blink_restriction) {
  switch (blink_restriction) {
    case blink::TrialToken::UsageRestriction::kNone:
      return protocol::Page::OriginTrialUsageRestrictionEnum::None;
    case blink::TrialToken::UsageRestriction::kSubset:
      return protocol::Page::OriginTrialUsageRestrictionEnum::Subset;
  }
}

std::unique_ptr<protocol::Page::OriginTrialToken> CreateOriginTrialToken(
    const blink::TrialToken& blink_trial_token) {
  return protocol::Page::OriginTrialToken::create()
      .setOrigin(SecurityOrigin::CreateFromUrlOrigin(blink_trial_token.origin())
                     ->ToRawString())
      .setIsThirdParty(blink_trial_token.is_third_party())
      .setMatchSubDomains(blink_trial_token.match_subdomains())
      .setExpiryTime(blink_trial_token.expiry_time().InSecondsFSinceUnixEpoch())
      .setTrialName(blink_trial_token.feature_name().c_str())
      .setUsageRestriction(CreateOriginTrialUsageRestriction(
          blink_trial_token.usage_restriction()))
      .build();
}

std::unique_ptr<protocol::Page::OriginTrialTokenWithStatus>
CreateOriginTrialTokenWithStatus(
    const blink::OriginTrialTokenResult& blink_token_result) {
  auto result =
      protocol::Page::OriginTrialTokenWithStatus::create()
          .setRawTokenText(blink_token_result.raw_token)
          .setStatus(CreateOriginTrialTokenStatus(blink_token_result.status))
          .build();

  if (blink_token_result.parsed_token.has_value()) {
    result->setParsedToken(
        CreateOriginTrialToken(*blink_token_result.parsed_token));
  }
  return result;
}

std::unique_ptr<protocol::Page::OriginTrial> CreateOriginTrial(
    const blink::OriginTrialResult& blink_trial_result) {
  auto tokens_with_status = std::make_unique<
      protocol::Array<protocol::Page::OriginTrialTokenWithStatus>>();

  for (const auto& blink_token_result : blink_trial_result.token_results) {
    tokens_with_status->push_back(
        CreateOriginTrialTokenWithStatus(blink_token_result));
  }

  return protocol::Page::OriginTrial::create()
      .setTrialName(blink_trial_result.trial_name)
      .setStatus(CreateOriginTrialStatus(blink_trial_result.status))
      .setTokensWithStatus(std::move(tokens_with_status))
      .build();
}

std::unique_ptr<protocol::Array<protocol::Page::OriginTrial>>
CreateOriginTrials(LocalDOMWindow* window) {
  auto trials =
      std::make_unique<protocol::Array<protocol::Page::OriginTrial>>();
  // Note: `blink::OriginTrialContext` is initialized when
  // `blink::ExecutionContext` is created. `GetOriginTrialContext()` should
  // not return nullptr.
  const blink::OriginTrialContext* context = window->GetOriginTrialContext();
  DCHECK(context);
  for (const auto& entry : context->GetOriginTrialResultsForDevtools()) {
    trials->push_back(CreateOriginTrial(entry.value));
  }
  return trials;
}

protocol::Page::AdFrameType BuildAdFrameType(LocalFrame* frame) {
  if (frame->IsAdRoot())
    return protocol::Page::AdFrameTypeEnum::Root;
  if (frame->IsAdFrame())
    return protocol::Page::AdFrameTypeEnum::Child;
  return protocol::Page::AdFrameTypeEnum::None;
}

std::unique_ptr<protocol::Page::AdFrameStatus> BuildAdFrameStatus(
    LocalFrame* frame) {
  if (!frame->AdEvidence() || !frame->AdEvidence()->is_complete()) {
    return protocol::Page::AdFrameStatus::create()
        .setAdFrameType(protocol::Page::AdFrameTypeEnum::None)
        .build();
  }
  const FrameAdEvidence& evidence = *frame->AdEvidence();
  auto explanations =
      std::make_unique<protocol::Array<protocol::Page::AdFrameExplanation>>();
  if (evidence.parent_is_ad()) {
    explanations->push_back(protocol::Page::AdFrameExplanationEnum::ParentIsAd);
  }
  if (evidence.created_by_ad_script() ==
      mojom::blink::FrameCreationStackEvidence::kCreatedByAdScript) {
    explanations->push_back(
        protocol::Page::AdFrameExplanationEnum::CreatedByAdScript);
  }
  if (evidence.most_restrictive_filter_list_result() ==
      mojom::blink::FilterListResult::kMatchedBlockingRule) {
    explanations->push_back(
        protocol::Page::AdFrameExplanationEnum::MatchedBlockingRule);
  }
  return protocol::Page::AdFrameStatus::create()
      .setAdFrameType(BuildAdFrameType(frame))
      .setExplanations(std::move(explanations))
      .build();
}

}  // namespace

std::unique_ptr<protocol::Page::Frame> InspectorPageAgent::BuildObjectForFrame(
    LocalFrame* frame) {
  DocumentLoader* loader = frame->Loader().GetDocumentLoader();
  // There are some rare cases where no DocumentLoader is set. We use an empty
  // Url and MimeType in those cases. See e.g. https://crbug.com/1270184.
  const KURL url = loader ? loader->Url() : KURL();
  const String mime_type = loader ? loader->MimeType() : String();
  std::unique_ptr<protocol::Page::Frame> frame_object =
      protocol::Page::Frame::create()
          .setId(IdentifiersFactory::FrameId(frame))
          .setLoaderId(IdentifiersFactory::LoaderId(loader))
          .setUrl(UrlWithoutFragment(url).GetString())
          .setDomainAndRegistry(blink::network_utils::GetDomainAndRegistry(
              url.Host(), blink::network_utils::PrivateRegistryFilter::
                              kIncludePrivateRegistries))
          .setMimeType(mime_type)
          .setSecurityOrigin(SecurityOrigin::Create(url)->ToRawString())
          .setSecureContextType(CreateProtocolSecureContextType(
              frame->DomWindow()
                  ->GetSecurityContext()
                  .GetSecureContextModeExplanation()))
          .setCrossOriginIsolatedContextType(
              CreateProtocolCrossOriginIsolatedContextType(frame->DomWindow()))
          .setGatedAPIFeatures(CreateGatedAPIFeaturesArray(frame->DomWindow()))
          .build();
  if (url.HasFragmentIdentifier())
    frame_object->setUrlFragment("#" + url.FragmentIdentifier());
  Frame* parent_frame = frame->Tree().Parent();
  if (parent_frame) {
    frame_object->setParentId(IdentifiersFactory::FrameId(parent_frame));
    AtomicString name = frame->Tree().GetName();
    if (name.empty() && frame->DeprecatedLocalOwner()) {
      name =
          frame->DeprecatedLocalOwner()->FastGetAttribute(html_names::kIdAttr);
    }
    frame_object->setName(name);
  }
  if (loader && !loader->UnreachableURL().IsEmpty())
    frame_object->setUnreachableUrl(loader->UnreachableURL().GetString());
  frame_object->setAdFrameStatus(BuildAdFrameStatus(frame));
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
    std::optional<base::Time> last_modified =
        cached_resource->GetResponse().LastModified();
    if (last_modified) {
      resource_object->setLastModified(
          last_modified.value().InSecondsFSinceUnixEpoch());
    }
    if (cached_resource->WasCanceled())
      resource_object->setCanceled(true);
    else if (cached_resource->GetStatus() == ResourceStatus::kLoadError)
      resource_object->setFailed(true);
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

protocol::Response InspectorPageAgent::startScreencast(
    Maybe<String> format,
    Maybe<int> quality,
    Maybe<int> max_width,
    Maybe<int> max_height,
    Maybe<int> every_nth_frame) {
  screencast_enabled_.Set(true);
  return protocol::Response::Success();
}

protocol::Response InspectorPageAgent::stopScreencast() {
  screencast_enabled_.Set(false);
  return protocol::Response::Success();
}

protocol::Response InspectorPageAgent::getLayoutMetrics(
    std::unique_ptr<protocol::Page::LayoutViewport>* out_layout_viewport,
    std::unique_ptr<protocol::Page::VisualViewport>* out_visual_viewport,
    std::unique_ptr<protocol::DOM::Rect>* out_content_size,
    std::unique_ptr<protocol::Page::LayoutViewport>* out_css_layout_viewport,
    std::unique_ptr<protocol::Page::VisualViewport>* out_css_visual_viewport,
    std::unique_ptr<protocol::DOM::Rect>* out_css_content_size) {
  LocalFrame* main_frame = inspected_frames_->Root();
  VisualViewport& visual_viewport = main_frame->GetPage()->GetVisualViewport();

  main_frame->GetDocument()->UpdateStyleAndLayout(
      DocumentUpdateReason::kInspector);

  gfx::Rect visible_contents =
      main_frame->View()->LayoutViewport()->VisibleContentRect();
  *out_layout_viewport = protocol::Page::LayoutViewport::create()
                             .setPageX(visible_contents.x())
                             .setPageY(visible_contents.y())
                             .setClientWidth(visible_contents.width())
                             .setClientHeight(visible_contents.height())
                             .build();

  // LayoutZoomFactor takes CSS pixels to device/physical pixels. It includes
  // both browser ctrl+/- zoom as well as the device scale factor for screen
  // density. Note: we don't account for pinch-zoom, even though it scales a
  // CSS pixel, since "device pixels" coming from Blink are also unscaled by
  // pinch-zoom.
  float css_to_physical = main_frame->LayoutZoomFactor();
  float physical_to_css = 1.f / css_to_physical;

  // `visible_contents` is in physical pixels. Normlisation is needed to
  // convert it to CSS pixels. Details: https://crbug.com/1181313
  gfx::Rect css_visible_contents =
      gfx::ScaleToEnclosedRect(visible_contents, physical_to_css);

  *out_css_layout_viewport = protocol::Page::LayoutViewport::create()
                                 .setPageX(css_visible_contents.x())
                                 .setPageY(css_visible_contents.y())
                                 .setClientWidth(css_visible_contents.width())
                                 .setClientHeight(css_visible_contents.height())
                                 .build();

  LocalFrameView* frame_view = main_frame->View();

  gfx::Size content_size = frame_view->GetScrollableArea()->ContentsSize();
  *out_content_size = protocol::DOM::Rect::create()
                          .setX(0)
                          .setY(0)
                          .setWidth(content_size.width())
                          .setHeight(content_size.height())
                          .build();

  // `content_size` is in physical pixels. Normlisation is needed to convert it
  // to CSS pixels. Details: https://crbug.com/1181313
  gfx::Size css_content_size =
      gfx::ScaleToFlooredSize(content_size, physical_to_css);
  *out_css_content_size = protocol::DOM::Rect::create()
                              .setX(0.0)
                              .setY(0.0)
                              .setWidth(css_content_size.width())
                              .setHeight(css_content_size.height())
                              .build();

  // page_zoom_factor transforms CSS pixels into DIPs (device independent
  // pixels).  This is the zoom factor coming only from browser ctrl+/-
  // zooming.
  float page_zoom_factor =
      css_to_physical /
      main_frame->GetPage()->GetChromeClient().WindowToViewportScalar(
          main_frame, 1.f);
  gfx::RectF visible_rect = visual_viewport.VisibleRect();
  float scale = visual_viewport.Scale();
  ScrollOffset page_offset = frame_view->GetScrollableArea()->GetScrollOffset();
  *out_visual_viewport = protocol::Page::VisualViewport::create()
                             .setOffsetX(visible_rect.x() * physical_to_css)
                             .setOffsetY(visible_rect.y() * physical_to_css)
                             .setPageX(page_offset.x() * physical_to_css)
                             .setPageY(page_offset.y() * physical_to_css)
                             .setClientWidth(visible_rect.width())
                             .setClientHeight(visible_rect.height())
                             .setScale(scale)
                             .setZoom(page_zoom_factor)
                             .build();

  *out_css_visual_viewport =
      protocol::Page::VisualViewport::create()
          .setOffsetX(visible_rect.x() * physical_to_css)
          .setOffsetY(visible_rect.y() * physical_to_css)
          .setPageX(page_offset.x() * physical_to_css)
          .setPageY(page_offset.y() * physical_to_css)
          .setClientWidth(visible_rect.width() * physical_to_css)
          .setClientHeight(visible_rect.height() * physical_to_css)
          .setScale(scale)
          .setZoom(page_zoom_factor)
          .build();
  return protocol::Response::Success();
}

void InspectorPageAgent::createIsolatedWorld(
    const String& frame_id,
    Maybe<String> world_name,
    Maybe<bool> grant_universal_access,
    std::unique_ptr<CreateIsolatedWorldCallback> callback) {
  LocalFrame* frame =
      IdentifiersFactory::FrameById(inspected_frames_, frame_id);
  if (!frame) {
    callback->sendFailure(
        protocol::Response::InvalidParams("No frame for given id found"));
    return;
  }
  if (frame->IsProvisional()) {
    // If we're not enabled, we won't have DidClearWindowObject, so the below
    // won't work!
    if (!enabled_.Get()) {
      callback->sendFailure(
          protocol::Response::ServerError("Agent needs to be enabled first"));
      return;
    }
    pending_isolated_worlds_.insert(frame, Vector<IsolatedWorldRequest>())
        .stored_value->value.push_back(IsolatedWorldRequest(
            world_name.value_or(""), grant_universal_access.value_or(false),
            std::move(callback)));
    return;
  }
  CreateIsolatedWorldImpl(*frame, world_name.value_or(""),
                          grant_universal_access.value_or(false),
                          std::move(callback));
}

void InspectorPageAgent::CreateIsolatedWorldImpl(
    LocalFrame& frame,
    String world_name,
    bool grant_universal_access,
    std::unique_ptr<CreateIsolatedWorldCallback> callback) {
  DCHECK(!frame.IsProvisional());
  DOMWrapperWorld* world =
      EnsureDOMWrapperWorld(&frame, world_name, grant_universal_access);
  if (!world) {
    callback->sendFailure(
        protocol::Response::ServerError("Could not create isolated world"));
    return;
  }

  LocalWindowProxy* isolated_world_window_proxy =
      frame.DomWindow()->GetScriptController().WindowProxy(*world);
  v8::HandleScope handle_scope(frame.DomWindow()->GetIsolate());

  callback->sendSuccess(v8_inspector::V8ContextInfo::executionContextId(
      isolated_world_window_proxy->ContextIfInitialized()));
}

protocol::Response InspectorPageAgent::setFontFamilies(
    GenericFontFamilySettings& family_settings,
    const protocol::Array<protocol::Page::ScriptFontFamilies>&
        script_font_families) {
  for (const auto& entry : script_font_families) {
    UScriptCode script = ScriptNameToCode(entry->getScript());
    if (script == USCRIPT_INVALID_CODE) {
      return protocol::Response::InvalidParams("Invalid script name: " +
                                               entry->getScript().Utf8());
    }
    auto* font_families = entry->getFontFamilies();
    if (font_families->hasStandard()) {
      family_settings.UpdateStandard(
          AtomicString(font_families->getStandard(String())), script);
    }
    if (font_families->hasFixed()) {
      family_settings.UpdateFixed(
          AtomicString(font_families->getFixed(String())), script);
    }
    if (font_families->hasSerif()) {
      family_settings.UpdateSerif(
          AtomicString(font_families->getSerif(String())), script);
    }
    if (font_families->hasSansSerif()) {
      family_settings.UpdateSansSerif(
          AtomicString(font_families->getSansSerif(String())), script);
    }
    if (font_families->hasCursive()) {
      family_settings.UpdateCursive(
          AtomicString(font_families->getCursive(String())), script);
    }
    if (font_families->hasFantasy()) {
      family_settings.UpdateFantasy(
          AtomicString(font_families->getFantasy(String())), script);
    }
    if (font_families->hasMath()) {
      family_settings.UpdateMath(AtomicString(font_families->getMath(String())),
                                 script);
    }
  }
  return protocol::Response::Success();
}

protocol::Response InspectorPageAgent::setFontFamilies(
    std::unique_ptr<protocol::Page::FontFamilies> font_families,
    Maybe<protocol::Array<protocol::Page::ScriptFontFamilies>> for_scripts) {
  LocalFrame* frame = inspected_frames_->Root();
  auto* settings = frame->GetSettings();
  if (!settings) {
    return protocol::Response::ServerError("No settings");
  }

  if (!script_font_families_cbor_.Get().empty()) {
    return protocol::Response::ServerError(
        "Font families can only be set once");
  }

  if (!for_scripts.has_value()) {
    for_scripts =
        std::make_unique<protocol::Array<protocol::Page::ScriptFontFamilies>>();
  }
  auto& script_fonts = for_scripts.value();
  script_fonts.push_back(protocol::Page::ScriptFontFamilies::create()
                             .setScript(blink::web_pref::kCommonScript)
                             .setFontFamilies(std::move(font_families))
                             .build());

  auto response =
      setFontFamilies(settings->GetGenericFontFamilySettings(), script_fonts);
  if (response.IsError())
    return response;
  std::vector<uint8_t> serialized;
  crdtp::ProtocolTypeTraits<protocol::Array<
      protocol::Page::ScriptFontFamilies>>::Serialize(script_fonts,
                                                      &serialized);
  script_font_families_cbor_.Set(serialized);
  settings->NotifyGenericFontFamilyChange();
  return protocol::Response::Success();
}

protocol::Response InspectorPageAgent::setFontSizes(
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

  return protocol::Response::Success();
}

void InspectorPageAgent::ApplyCompilationModeOverride(
    const ClassicScript& classic_script,
    v8::ScriptCompiler::CachedData** cached_data,
    v8::ScriptCompiler::CompileOptions* compile_options) {
  if (classic_script.SourceLocationType() !=
      ScriptSourceLocationType::kExternalFile)
    return;
  if (classic_script.SourceUrl().IsEmpty())
    return;
  auto it = compilation_cache_.find(classic_script.SourceUrl().GetString());
  if (it == compilation_cache_.end()) {
    auto requested = requested_compilation_cache_.find(
        classic_script.SourceUrl().GetString());
    if (requested != requested_compilation_cache_.end() && requested->value)
      *compile_options = v8::ScriptCompiler::kEagerCompile;
    return;
  }
  const protocol::Binary& data = it->value;
  *cached_data = new v8::ScriptCompiler::CachedData(
      data.data(), base::checked_cast<int>(data.size()),
      v8::ScriptCompiler::CachedData::BufferNotOwned);
}

void InspectorPageAgent::DidProduceCompilationCache(
    const ClassicScript& classic_script,
    v8::Local<v8::Script> script) {
  KURL url = classic_script.SourceUrl();
  if (url.IsEmpty())
    return;
  String url_string = url.GetString();
  auto requested = requested_compilation_cache_.find(url_string);
  if (requested == requested_compilation_cache_.end())
    return;
  requested_compilation_cache_.erase(requested);
  if (classic_script.SourceLocationType() !=
      ScriptSourceLocationType::kExternalFile)
    return;
  // TODO(caseq): should we rather issue updates if compiled code differs?
  if (compilation_cache_.Contains(url_string))
    return;
  static const int kMinimalCodeLength = 1024;
  if (classic_script.SourceText().length() < kMinimalCodeLength)
    return;
  std::unique_ptr<v8::ScriptCompiler::CachedData> cached_data(
      v8::ScriptCompiler::CreateCodeCache(script->GetUnboundScript()));
  if (cached_data) {
    CHECK_EQ(cached_data->buffer_policy,
             v8::ScriptCompiler::CachedData::BufferOwned);
    auto data = protocol::Binary::fromCachedData(std::move(cached_data));
    // This also prevents the notification from being re-issued.
    compilation_cache_.Set(url_string, data);
    // CachedData produced by CreateCodeCache always owns its buffer.
    GetFrontend()->compilationCacheProduced(url_string, data);
  }
}

void InspectorPageAgent::FileChooserOpened(LocalFrame* frame,
                                           HTMLInputElement* element,
                                           bool multiple,
                                           bool* intercepted) {
  *intercepted |= intercept_file_chooser_.Get();
  if (!intercept_file_chooser_.Get())
    return;
  GetFrontend()->fileChooserOpened(
      IdentifiersFactory::FrameId(frame),
      multiple ? protocol::Page::FileChooserOpened::ModeEnum::SelectMultiple
               : protocol::Page::FileChooserOpened::ModeEnum::SelectSingle,
      element ? Maybe<int>(element->GetDomNodeId()) : Maybe<int>());
}

protocol::Response InspectorPageAgent::produceCompilationCache(
    std::unique_ptr<protocol::Array<protocol::Page::CompilationCacheParams>>
        scripts) {
  if (!enabled_.Get())
    return protocol::Response::ServerError("Agent needs to be enabled first");
  for (const auto& script : *scripts) {
    requested_compilation_cache_.Set(script->getUrl(), script->getEager(false));
  }
  return protocol::Response::Success();
}

protocol::Response InspectorPageAgent::addCompilationCache(
    const String& url,
    const protocol::Binary& data) {
  // TODO(caseq): this is temporary undocumented behavior, remove after m91.
  if (!data.size()) {
    requested_compilation_cache_.Set(url, true);
  } else {
    compilation_cache_.Set(url, data);
  }
  return protocol::Response::Success();
}

protocol::Response InspectorPageAgent::clearCompilationCache() {
  compilation_cache_.clear();
  return protocol::Response::Success();
}

protocol::Response InspectorPageAgent::waitForDebugger() {
  client_->WaitForDebugger();
  return protocol::Response::Success();
}

protocol::Response InspectorPageAgent::setInterceptFileChooserDialog(
    bool enabled) {
  intercept_file_chooser_.Set(enabled);
  return protocol::Response::Success();
}

protocol::Response InspectorPageAgent::generateTestReport(const String& message,
                                                          Maybe<String> group) {
  LocalDOMWindow* window = inspected_frames_->Root()->DomWindow();

  // Construct the test report.
  TestReportBody* body = MakeGarbageCollected<TestReportBody>(message);
  Report* report = MakeGarbageCollected<Report>(
      "test", window->document()->Url().GetString(), body);

  // Send the test report to any ReportingObservers.
  ReportingContext::From(window)->QueueReport(report);

  return protocol::Response::Success();
}

void InspectorPageAgent::Trace(Visitor* visitor) const {
  visitor->Trace(inspected_frames_);
  visitor->Trace(pending_isolated_worlds_);
  visitor->Trace(inspector_resource_content_loader_);
  visitor->Trace(isolated_worlds_);
  InspectorBaseAgent::Trace(visitor);
}

void InspectorPageAgent::Dispose() {
  InspectorBaseAgent::Dispose();
  v8_session_ = nullptr;
}

protocol::Response InspectorPageAgent::getOriginTrials(
    const String& frame_id,
    std::unique_ptr<protocol::Array<protocol::Page::OriginTrial>>*
        originTrials) {
  LocalFrame* frame =
      IdentifiersFactory::FrameById(inspected_frames_, frame_id);

  if (!frame)
    return protocol::Response::InvalidParams("Invalid frame id");

  *originTrials = CreateOriginTrials(frame->DomWindow());

  return protocol::Response::Success();
}

}  // namespace blink
