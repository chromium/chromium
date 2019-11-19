/*
 * Copyright (C) 2007, 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/css/css_font_face_src_value.h"

#include "base/feature_list.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/core/css/css_markup.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/loader/resource/font_resource.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/fonts/font_custom_platform_data.h"
#include "third_party/blink/renderer/platform/loader/fetch/cross_origin_attribute_value.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_type_names.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

bool CSSFontFaceSrcValue::IsSupportedFormat() const {
  // Normally we would just check the format, but in order to avoid conflicts
  // with the old WinIE style of font-face, we will also check to see if the URL
  // ends with .eot.  If so, we'll go ahead and assume that we shouldn't load
  // it.
  if (format_.IsEmpty()) {
    return absolute_resource_.StartsWithIgnoringASCIICase("data:") ||
           !absolute_resource_.EndsWithIgnoringASCIICase(".eot");
  }

  return FontCustomPlatformData::SupportsFormat(format_);
}

String CSSFontFaceSrcValue::CustomCSSText() const {
  StringBuilder result;
  if (IsLocal()) {
    result.Append("local(");
    result.Append(SerializeString(absolute_resource_));
    result.Append(')');
  } else {
    result.Append(SerializeURI(specified_resource_));
  }
  if (!format_.IsEmpty()) {
    result.Append(" format(");
    result.Append(SerializeString(format_));
    result.Append(')');
  }
  return result.ToString();
}

bool CSSFontFaceSrcValue::HasFailedOrCanceledSubresources() const {
  return fetched_ && fetched_->GetResource()->LoadFailedOrCanceled();
}

FontResource& CSSFontFaceSrcValue::Fetch(ExecutionContext* context,
                                         FontResourceClient* client) const {
  if (!fetched_) {
    ResourceRequest resource_request(absolute_resource_);
    resource_request.SetReferrerPolicy(
        ReferrerPolicyResolveDefault(referrer_.referrer_policy));
    resource_request.SetReferrerString(referrer_.referrer);
    ResourceLoaderOptions options;
    options.initiator_info.name = fetch_initiator_type_names::kCSS;
    FetchParameters params(resource_request, options);
    if (base::FeatureList::IsEnabled(
            features::kWebFontsCacheAwareTimeoutAdaption)) {
      params.SetCacheAwareLoadingEnabled(kIsCacheAwareLoadingEnabled);
    }
    params.SetContentSecurityCheck(should_check_content_security_policy_);
    params.SetFromOriginDirtyStyleSheet(origin_clean_ != OriginClean::kTrue);
    const SecurityOrigin* security_origin = context->GetSecurityOrigin();

    // Local fonts are accessible from file: URLs even when
    // allowFileAccessFromFileURLs is false.
    if (!params.Url().IsLocalFile()) {
      params.SetCrossOriginAccessControl(security_origin,
                                         kCrossOriginAttributeAnonymous);
    }
    // For Workers, Fetcher is lazily loaded, so we must ensure it's available
    // here.
    if (auto* scope = DynamicTo<WorkerGlobalScope>(context)) {
      scope->EnsureFetcher();
    }
    fetched_ = MakeGarbageCollected<FontResourceHelper>(
        FontResource::Fetch(params, context->Fetcher(), client),
        context->GetTaskRunner(TaskType::kInternalLoading).get());
  } else {
    // FIXME: CSSFontFaceSrcValue::Fetch is invoked when @font-face rule
    // is processed by StyleResolver / StyleEngine.
    RestoreCachedResourceIfNeeded(context);
    if (client) {
      client->SetResource(
          fetched_->GetResource(),
          context->GetTaskRunner(TaskType::kInternalLoading).get());
    }
  }
  return *ToFontResource(fetched_->GetResource());
}

void CSSFontFaceSrcValue::RestoreCachedResourceIfNeeded(
    ExecutionContext* context) const {
  DCHECK(fetched_);
  DCHECK(context);
  DCHECK(context->Fetcher());

  const String resource_url = context->CompleteURL(absolute_resource_);
  DCHECK_EQ(should_check_content_security_policy_,
            fetched_->GetResource()->Options().content_security_policy_option);
  context->Fetcher()->EmulateLoadStartedForInspector(
      fetched_->GetResource(), KURL(resource_url),
      mojom::RequestContextType::FONT, fetch_initiator_type_names::kCSS);
}

bool CSSFontFaceSrcValue::Equals(const CSSFontFaceSrcValue& other) const {
  return is_local_ == other.is_local_ && format_ == other.format_ &&
         specified_resource_ == other.specified_resource_ &&
         absolute_resource_ == other.absolute_resource_;
}

}  // namespace blink
