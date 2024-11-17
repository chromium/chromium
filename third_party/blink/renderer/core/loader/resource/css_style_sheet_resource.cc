/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2002 Waldo Bastian (bastian@kde.org)
    Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
    Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.

    This class provides all functionality needed for loading images, style
    sheets and html pages from the web. It has a memory cache for these objects.
*/

#include "third_party/blink/renderer/core/loader/resource/css_style_sheet_resource.h"

#include "base/metrics/histogram_functions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/mojom/loader/request_context_frame_type.mojom-blink.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/platform/heap/cross_thread_persistent.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/loader/fetch/response_body_loader.h"
#include "third_party/blink/renderer/platform/loader/fetch/text_resource_decoder_options.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/network/mime/mime_type_registry.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/worker_pool.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_mojo.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_std.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"

namespace blink {
CSSStyleSheetResource* CSSStyleSheetResource::Fetch(FetchParameters& params,
                                                    ResourceFetcher* fetcher,
                                                    ResourceClient* client) {
  params.SetRequestContext(mojom::blink::RequestContextType::STYLE);
  params.SetRequestDestination(network::mojom::RequestDestination::kStyle);
  auto* resource = To<CSSStyleSheetResource>(
      fetcher->RequestResource(params, CSSStyleSheetResourceFactory(), client));
  return resource;
}

CSSStyleSheetResource* CSSStyleSheetResource::CreateForTest(
    const KURL& url,
    const WTF::TextEncoding& encoding) {
  ResourceRequest request(url);
  request.SetCredentialsMode(network::mojom::CredentialsMode::kOmit);
  ResourceLoaderOptions options(nullptr /* world */);
  TextResourceDecoderOptions decoder_options(
      TextResourceDecoderOptions::kCSSContent, encoding);
  return MakeGarbageCollected<CSSStyleSheetResource>(request, options,
                                                     decoder_options);
}

CSSStyleSheetResource::CSSStyleSheetResource(
    const ResourceRequest& resource_request,
    const ResourceLoaderOptions& options,
    const TextResourceDecoderOptions& decoder_options)
    : TextResource(resource_request,
                   ResourceType::kCSSStyleSheet,
                   options,
                   decoder_options) {}

CSSStyleSheetResource::~CSSStyleSheetResource() = default;

void CSSStyleSheetResource::SetParsedStyleSheetCache(
    StyleSheetContents* new_sheet) {
  if (parsed_style_sheet_cache_)
    parsed_style_sheet_cache_->ClearReferencedFromResource();
  parsed_style_sheet_cache_ = new_sheet;
  if (parsed_style_sheet_cache_)
    parsed_style_sheet_cache_->SetReferencedFromResource(this);

  // Updates the decoded size to take parsed stylesheet cache into account.
  UpdateDecodedSize();
}

void CSSStyleSheetResource::Trace(Visitor* visitor) const {
  visitor->Trace(parsed_style_sheet_cache_);
  TextResource::Trace(visitor);
}

void CSSStyleSheetResource::OnMemoryDump(
    WebMemoryDumpLevelOfDetail level_of_detail,
    WebProcessMemoryDump* memory_dump) const {
  Resource::OnMemoryDump(level_of_detail, memory_dump);
  const String name = GetMemoryDumpName() + "/style_sheets";
  auto* dump = memory_dump->CreateMemoryAllocatorDump(name);
  dump->AddScalar("size", "bytes", decoded_sheet_text_.CharactersSizeInBytes());
  memory_dump->AddSuballocation(
      dump->Guid(), String(WTF::Partitions::kAllocatedObjectPoolName));
}

network::mojom::ReferrerPolicy CSSStyleSheetResource::GetReferrerPolicy()
    const {
  network::mojom::ReferrerPolicy referrer_policy =
      network::mojom::ReferrerPolicy::kDefault;
  String referrer_policy_header =
      GetResponse().HttpHeaderField(http_names::kReferrerPolicy);
  if (!referrer_policy_header.IsNull()) {
    SecurityPolicy::ReferrerPolicyFromHeaderValue(
        referrer_policy_header, kDoNotSupportReferrerPolicyLegacyKeywords,
        &referrer_policy);
  }
  return referrer_policy;
}

const String CSSStyleSheetResource::SheetText(
    const CSSParserContext* parser_context,
    MIMETypeCheck mime_type_check) const {
  if (!CanUseSheet(parser_context, mime_type_check))
    return String();

  // Use cached decoded sheet text when available
  if (!decoded_sheet_text_.IsNull()) {
    // We should have the decoded sheet text cached when the resource is fully
    // loaded.
    DCHECK_EQ(GetStatus(), ResourceStatus::kCached);

    return decoded_sheet_text_;
  }

  if (!Data() || Data()->empty())
    return String();

  return DecodedText();
}

void CSSStyleSheetResource::NotifyFinished() {
  // Decode the data to find out the encoding and cache the decoded sheet text.
  if (Data()) {
    SetDecodedSheetText(DecodedText());
  }

  Resource::NotifyFinished();
  // Clear raw bytes as now we have the full decoded sheet text.
  // We wait for all LinkStyle::setCSSStyleSheet to run (at least once)
  // as SubresourceIntegrity checks require raw bytes.
  // Note that LinkStyle::setCSSStyleSheet can be called from didAddClient too,
  // but is safe as we should have a cached ResourceIntegrityDisposition.
  ClearData();
}

void CSSStyleSheetResource::DestroyDecodedDataIfPossible() {
  if (!parsed_style_sheet_cache_)
    return;

  SetParsedStyleSheetCache(nullptr);
}

void CSSStyleSheetResource::DestroyDecodedDataForFailedRevalidation() {
  SetDecodedSheetText(String());
  DestroyDecodedDataIfPossible();
}

bool CSSStyleSheetResource::CanUseSheet(const CSSParserContext* parser_context,
                                        MIMETypeCheck mime_type_check) const {
  if (ErrorOccurred())
    return false;

  // For `file:` URLs, we may need to be a little more strict than the below.
  // Though we'll likely change this in the future, for the moment we're going
  // to enforce a file-extension requirement on stylesheets loaded from `file:`
  // URLs and see how far it gets us.
  KURL sheet_url = GetResponse().CurrentRequestUrl();
  if (sheet_url.IsLocalFile()) {
    if (parser_context) {
      parser_context->Count(WebFeature::kLocalCSSFile);
    }
    // Grab |sheet_url|'s filename's extension (if present), and check whether
    // or not it maps to a `text/css` MIME type:
    String extension;
    String last_path_component = sheet_url.LastPathComponent().ToString();
    int last_dot = last_path_component.ReverseFind('.');
    if (last_dot != -1) {
      extension = last_path_component.Substring(last_dot + 1);
    }
    if (!EqualIgnoringASCIICase(
            MIMETypeRegistry::GetMIMETypeForExtension(extension), "text/css")) {
      if (parser_context) {
        parser_context->CountDeprecation(
            WebFeature::kLocalCSSFileExtensionRejected);
      }
      return false;
    }
  }

  // This check exactly matches Firefox. Note that we grab the Content-Type
  // header directly because we want to see what the value is BEFORE content
  // sniffing. Firefox does this by setting a "type hint" on the channel. This
  // implementation should be observationally equivalent.
  //
  // This code defaults to allowing the stylesheet for non-HTTP protocols so
  // folks can use standards mode for local HTML documents.
  if (mime_type_check == MIMETypeCheck::kLax)
    return true;
  AtomicString content_type = HttpContentType();
  return content_type.empty() ||
         EqualIgnoringASCIICase(content_type, "text/css") ||
         EqualIgnoringASCIICase(content_type,
                                "application/x-unknown-content-type");
}

StyleSheetContents* CSSStyleSheetResource::CreateParsedStyleSheetFromCache(
    const CSSParserContext* context) {
  if (!parsed_style_sheet_cache_) {
    return nullptr;
  }
  if (parsed_style_sheet_cache_->HasFailedOrCanceledSubresources()) {
    SetParsedStyleSheetCache(nullptr);
    return nullptr;
  }

  DCHECK(parsed_style_sheet_cache_->IsCacheableForResource());
  DCHECK(parsed_style_sheet_cache_->IsReferencedFromResource());

  // Contexts must be identical so we know we would get the same exact result if
  // we parsed again.
  if (*parsed_style_sheet_cache_->ParserContext() != *context) {
    return nullptr;
  }

  // StyleSheetContents with @media queries are shared between different
  // documents, in the same rendering process, which may evaluate these media
  // queries differently. For instance, two documents rendered in different tabs
  // or iframes with different sizes. In that case, an active stylesheet update
  // in one document may clear the cached RuleSet in StyleSheetContents, that
  // would otherwise be a valid cache for the other document.
  //
  // This should not be problematic as the case of continuously modifying,
  // adding, or removing stylesheets, while at the same time have different
  // media query evaluations in the different documents should be quite rare.

  DCHECK(!parsed_style_sheet_cache_->IsLoading());
  return parsed_style_sheet_cache_.Get();
}

void CSSStyleSheetResource::SaveParsedStyleSheet(StyleSheetContents* sheet) {
  DCHECK(sheet);
  DCHECK(sheet->IsCacheableForResource());

  if (!MemoryCache::Get()->Contains(this)) {
    // This stylesheet resource did conflict with another resource and was not
    // added to the cache.
    SetParsedStyleSheetCache(nullptr);
    return;
  }
  SetParsedStyleSheetCache(sheet);
}

void CSSStyleSheetResource::SetDecodedSheetText(
    const String& decoded_sheet_text) {
  decoded_sheet_text_ = decoded_sheet_text;
  UpdateDecodedSize();
}

void CSSStyleSheetResource::UpdateDecodedSize() {
  size_t decoded_size = decoded_sheet_text_.CharactersSizeInBytes();
  if (parsed_style_sheet_cache_)
    decoded_size += parsed_style_sheet_cache_->EstimatedSizeInBytes();
  SetDecodedSize(decoded_size);
}

}  // namespace blink
