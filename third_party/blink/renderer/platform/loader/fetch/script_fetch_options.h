// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_SCRIPT_FETCH_OPTIONS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_SCRIPT_FETCH_OPTIONS_H_

#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/platform/cross_origin_attribute_value.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/integrity_metadata.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/weborigin/referrer_policy.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class KURL;
class SecurityOrigin;

// ScriptFetchOptions corresponds to the spec concept "script fetch options".
// https://html.spec.whatwg.org/multipage/webappapis.html#script-fetch-options
class PLATFORM_EXPORT ScriptFetchOptions final {
 public:
  // https://html.spec.whatwg.org/multipage/webappapis.html#default-classic-script-fetch-options
  // "The default classic script fetch options are a script fetch options whose
  // cryptographic nonce is the empty string, integrity metadata is the empty
  // string, parser metadata is "not-parser-inserted", and credentials mode
  // is "omit"." [spec text]
  // TODO(domfarolino): Update this to use probably "include" or "same-origin"
  // credentials mode, once spec decision is made at
  // https://github.com/whatwg/html/pull/3656.
  ScriptFetchOptions()
      : parser_state_(ParserDisposition::kNotParserInserted),
        credentials_mode_(network::mojom::FetchCredentialsMode::kOmit),
        referrer_policy_(kReferrerPolicyDefault) {}

  ScriptFetchOptions(const String& nonce,
                     const IntegrityMetadataSet& integrity_metadata,
                     const String& integrity_attribute,
                     ParserDisposition parser_state,
                     network::mojom::FetchCredentialsMode credentials_mode,
                     ReferrerPolicy referrer_policy)
      : nonce_(nonce),
        integrity_metadata_(integrity_metadata),
        integrity_attribute_(integrity_attribute),
        parser_state_(parser_state),
        credentials_mode_(credentials_mode),
        referrer_policy_(referrer_policy) {}
  ~ScriptFetchOptions() = default;

  const String& Nonce() const { return nonce_; }
  const IntegrityMetadataSet& GetIntegrityMetadata() const {
    return integrity_metadata_;
  }
  const String& GetIntegrityAttributeValue() const {
    return integrity_attribute_;
  }
  const ParserDisposition& ParserState() const { return parser_state_; }
  network::mojom::FetchCredentialsMode CredentialsMode() const {
    return credentials_mode_;
  }
  ReferrerPolicy GetReferrerPolicy() const { return referrer_policy_; }

  // https://html.spec.whatwg.org/multipage/webappapis.html#fetch-a-classic-script
  // Steps 1 and 3.
  FetchParameters CreateFetchParameters(const KURL&,
                                        const SecurityOrigin*,
                                        CrossOriginAttributeValue,
                                        const WTF::TextEncoding&,
                                        FetchParameters::DeferOption) const;

 private:
  // https://html.spec.whatwg.org/multipage/webappapis.html#concept-script-fetch-options-nonce
  const String nonce_;

  // https://html.spec.whatwg.org/multipage/webappapis.html#concept-script-fetch-options-integrity
  const IntegrityMetadataSet integrity_metadata_;
  const String integrity_attribute_;

  // https://html.spec.whatwg.org/multipage/webappapis.html#concept-script-fetch-options-parser
  const ParserDisposition parser_state_;

  // https://html.spec.whatwg.org/multipage/webappapis.html#concept-script-fetch-options-credentials
  const network::mojom::FetchCredentialsMode credentials_mode_;

  // https://html.spec.whatwg.org/multipage/webappapis.html#concept-script-fetch-options-referrer-policy
  const ReferrerPolicy referrer_policy_;
};

}  // namespace blink

#endif
