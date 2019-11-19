/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2010 Google Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_HTML_PRELOAD_SCANNER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_HTML_PRELOAD_SCANNER_H_

#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/media_values_cached.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/parser/compact_html_token.h"
#include "third_party/blink/renderer/core/html/parser/css_preload_scanner.h"
#include "third_party/blink/renderer/core/html/parser/html_token.h"
#include "third_party/blink/renderer/core/html/parser/preload_request.h"
#include "third_party/blink/renderer/core/page/viewport_description.h"
#include "third_party/blink/renderer/platform/text/segmented_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

typedef wtf_size_t TokenPreloadScannerCheckpoint;

class HTMLParserOptions;
class HTMLTokenizer;
class SegmentedString;

struct CORE_EXPORT CachedDocumentParameters {
  USING_FAST_MALLOC(CachedDocumentParameters);

 public:
  explicit CachedDocumentParameters(Document*);
  CachedDocumentParameters() = default;

  bool do_html_preload_scanning;
  Length default_viewport_min_width;
  bool viewport_meta_zero_values_quirk;
  bool viewport_meta_enabled;
  network::mojom::ReferrerPolicy referrer_policy;
  SubresourceIntegrity::IntegrityFeatures integrity_features;
  bool lazyload_policy_enforced;
  LocalFrame::LazyLoadImageSetting lazy_load_image_setting;
};

class TokenPreloadScanner {
  USING_FAST_MALLOC(TokenPreloadScanner);

 public:
  enum class ScannerType { kMainDocument, kInsertion };

  TokenPreloadScanner(const KURL& document_url,
                      std::unique_ptr<CachedDocumentParameters>,
                      const MediaValuesCached::MediaValuesCachedData&,
                      const ScannerType,
                      bool priority_hints_origin_trial_enabled);
  ~TokenPreloadScanner();

  void Scan(const HTMLToken&,
            const SegmentedString&,
            PreloadRequestStream& requests,
            base::Optional<ViewportDescription>*,
            bool* is_csp_meta_tag);
  void Scan(const CompactHTMLToken&,
            const SegmentedString&,
            PreloadRequestStream& requests,
            base::Optional<ViewportDescription>*,
            bool* is_csp_meta_tag);

  void SetPredictedBaseElementURL(const KURL& url) {
    predicted_base_element_url_ = url;
  }

  // A TokenPreloadScannerCheckpoint is valid until the next call to rewindTo,
  // at which point all outstanding checkpoints are invalidated.
  TokenPreloadScannerCheckpoint CreateCheckpoint();
  void RewindTo(TokenPreloadScannerCheckpoint);

 private:
  class StartTagScanner;

  template <typename Token>
  inline void ScanCommon(const Token&,
                         const SegmentedString&,
                         PreloadRequestStream& requests,
                         base::Optional<ViewportDescription>*,
                         bool* is_csp_meta_tag);

  template <typename Token>
  void UpdatePredictedBaseURL(const Token&);

  struct Checkpoint {
    Checkpoint(const KURL& predicted_base_element_url,
               bool in_style,
               bool in_script,
               size_t template_count)
        : predicted_base_element_url(predicted_base_element_url),
          in_style(in_style),
          in_script(in_script),
          template_count(template_count) {}

    KURL predicted_base_element_url;
    bool in_style;
    bool in_script;
    size_t template_count;
  };

  struct PictureData {
    PictureData() : source_size(0.0), source_size_set(false), picked(false) {}
    String source_url;
    float source_size;
    bool source_size_set;
    bool picked;
  };

  CSSPreloadScanner css_scanner_;
  const KURL document_url_;
  KURL predicted_base_element_url_;
  bool in_style_;
  bool in_picture_;
  bool in_script_;
  PictureData picture_data_;
  size_t template_count_;
  std::unique_ptr<CachedDocumentParameters> document_parameters_;
  Persistent<MediaValuesCached> media_values_;
  ClientHintsPreferences client_hints_preferences_;
  ScannerType scanner_type_;
  // TODO(domfarolino): Remove this once Priority Hints is no longer in Origin
  // Trial (see https://crbug.com/821464). This member exists because
  // HTMLPreloadScanner has no access to an ExecutionContext*, and therefore
  // cannot determine an Origin Trial's status, so we accept this information in
  // the constructor and set this flag accordingly.
  bool priority_hints_origin_trial_enabled_;

  bool did_rewind_ = false;

  Vector<Checkpoint> checkpoints_;

  DISALLOW_COPY_AND_ASSIGN(TokenPreloadScanner);
};

class CORE_EXPORT HTMLPreloadScanner {
  USING_FAST_MALLOC(HTMLPreloadScanner);

 public:
  HTMLPreloadScanner(const HTMLParserOptions&,
                     const KURL& document_url,
                     std::unique_ptr<CachedDocumentParameters>,
                     const MediaValuesCached::MediaValuesCachedData&,
                     const TokenPreloadScanner::ScannerType);
  ~HTMLPreloadScanner();

  void AppendToEnd(const SegmentedString&);
  PreloadRequestStream Scan(const KURL& document_base_element_url,
                            base::Optional<ViewportDescription>*,
                            bool& has_csp_meta_tag);

 private:
  TokenPreloadScanner scanner_;
  SegmentedString source_;
  HTMLToken token_;
  std::unique_ptr<HTMLTokenizer> tokenizer_;

  DISALLOW_COPY_AND_ASSIGN(HTMLPreloadScanner);
};

}  // namespace blink

#endif
