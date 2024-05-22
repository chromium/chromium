// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/link_header.h"

#include <string_view>

#include "base/strings/string_util.h"
#include "components/link_header_util/link_header_util.h"
#include "third_party/blink/public/common/web_package/signed_exchange_consts.h"
#include "third_party/blink/renderer/platform/wtf/text/parsing_utilities.h"

namespace blink {

// Verify that the parameter is a link-extension which according to spec doesn't
// have to have a value.
static bool IsExtensionParameter(LinkHeader::LinkParameterName name) {
  return name >= LinkHeader::kLinkParameterUnknown;
}

static LinkHeader::LinkParameterName ParameterNameFromString(
    std::string_view name) {
  if (base::EqualsCaseInsensitiveASCII(name, "rel"))
    return LinkHeader::kLinkParameterRel;
  if (base::EqualsCaseInsensitiveASCII(name, "anchor"))
    return LinkHeader::kLinkParameterAnchor;
  if (base::EqualsCaseInsensitiveASCII(name, "crossorigin"))
    return LinkHeader::kLinkParameterCrossOrigin;
  if (base::EqualsCaseInsensitiveASCII(name, "title"))
    return LinkHeader::kLinkParameterTitle;
  if (base::EqualsCaseInsensitiveASCII(name, "media"))
    return LinkHeader::kLinkParameterMedia;
  if (base::EqualsCaseInsensitiveASCII(name, "type"))
    return LinkHeader::kLinkParameterType;
  if (base::EqualsCaseInsensitiveASCII(name, "rev"))
    return LinkHeader::kLinkParameterRev;
  if (base::EqualsCaseInsensitiveASCII(name, "referrerpolicy"))
    return LinkHeader::kLinkParameterReferrerPolicy;
  if (base::EqualsCaseInsensitiveASCII(name, "hreflang"))
    return LinkHeader::kLinkParameterHreflang;
  if (base::EqualsCaseInsensitiveASCII(name, "as"))
    return LinkHeader::kLinkParameterAs;
  if (base::EqualsCaseInsensitiveASCII(name, "nonce"))
    return LinkHeader::kLinkParameterNonce;
  if (base::EqualsCaseInsensitiveASCII(name, "integrity"))
    return LinkHeader::kLinkParameterIntegrity;
  if (base::EqualsCaseInsensitiveASCII(name, "imagesrcset"))
    return LinkHeader::kLinkParameterImageSrcset;
  if (base::EqualsCaseInsensitiveASCII(name, "imagesizes"))
    return LinkHeader::kLinkParameterImageSizes;
  if (base::EqualsCaseInsensitiveASCII(name, "anchor"))
    return LinkHeader::kLinkParameterAnchor;

  // "header-integrity" and "variants" and "variant-key" are used only for
  // SignedExchangeSubresourcePrefetch.
  if (base::EqualsCaseInsensitiveASCII(name, "header-integrity"))
    return LinkHeader::kLinkParameterHeaderIntegrity;
  if (base::EqualsCaseInsensitiveASCII(name, kSignedExchangeVariantsHeader))
    return LinkHeader::kLinkParameterVariants;
  if (base::EqualsCaseInsensitiveASCII(name, kSignedExchangeVariantKeyHeader))
    return LinkHeader::kLinkParameterVariantKey;

  if (base::EqualsCaseInsensitiveASCII(name, "blocking")) {
    return LinkHeader::kLinkParameterBlocking;
  }

  if (base::EqualsCaseInsensitiveASCII(name, "fetchpriority")) {
    return LinkHeader::kLinkParameterFetchPriority;
  }

  return LinkHeader::kLinkParameterUnknown;
}

void LinkHeader::SetValue(LinkParameterName name, const String& value) {
  if (name == kLinkParameterRel && !rel_) {
    rel_ = value.DeprecatedLower();
  } else if (name == kLinkParameterAnchor) {
    anchor_ = value;
  } else if (name == kLinkParameterCrossOrigin) {
    cross_origin_ = value;
  } else if (name == kLinkParameterAs) {
    as_ = value.DeprecatedLower();
  } else if (name == kLinkParameterType) {
    mime_type_ = value.DeprecatedLower();
  } else if (name == kLinkParameterMedia) {
    media_ = value.DeprecatedLower();
  } else if (name == kLinkParameterNonce) {
    nonce_ = value;
  } else if (name == kLinkParameterIntegrity) {
    integrity_ = value;
  } else if (name == kLinkParameterImageSrcset) {
    image_srcset_ = value;
  } else if (name == kLinkParameterImageSizes) {
    image_sizes_ = value;
  } else if (name == kLinkParameterHeaderIntegrity) {
    header_integrity_ = value;
  } else if (name == kLinkParameterVariants) {
    variants_ = value;
  } else if (name == kLinkParameterVariantKey) {
    variant_key_ = value;
  } else if (name == kLinkParameterBlocking) {
    blocking_ = value;
  } else if (name == kLinkParameterReferrerPolicy) {
    referrer_policy_ = value;
  } else if (name == kLinkParameterFetchPriority) {
    fetch_priority_ = value;
  }
}

template <typename Iterator>
LinkHeader::LinkHeader(Iterator begin, Iterator end) : is_valid_(true) {
  std::string url;
  std::unordered_map<std::string, std::optional<std::string>> params;
  is_valid_ = link_header_util::ParseLinkHeaderValue(begin, end, &url, &params);
  if (!is_valid_)
    return;

  url_ = String(&url[0], url.length());
  for (const auto& param : params) {
    LinkParameterName name = ParameterNameFromString(param.first);
    if (!IsExtensionParameter(name) && !param.second)
      is_valid_ = false;
    std::string value = param.second.value_or("");
    SetValue(name, String(&value[0], value.length()));
  }
  // According to Section 5.2 of RFC 5988, "anchor" parameters in Link headers
  // must be either respected, or the entire header must be ignored:
  // https://tools.ietf.org/html/rfc5988#section-5.2
  // Blink uses "anchor" parameters only for SignedExchangeSubresourcePrefetch
  // and the rel is "alternate".
  if (anchor_.has_value() && rel_ != "alternate")
    is_valid_ = false;
}

LinkHeaderSet::LinkHeaderSet(const String& header) {
  if (header.IsNull())
    return;

  DCHECK(header.Is8Bit()) << "Headers should always be 8 bit";
  std::string header_string(reinterpret_cast<const char*>(header.Characters8()),
                            header.length());
  for (const auto& value : link_header_util::SplitLinkHeader(header_string))
    header_set_.push_back(LinkHeader(value.first, value.second));
}

}  // namespace blink
