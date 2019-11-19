// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_MODULESCRIPT_MODULE_SCRIPT_CREATION_PARAMS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_MODULESCRIPT_MODULE_SCRIPT_CREATION_PARAMS_H_

#include "base/optional.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/platform/bindings/parkable_string.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/loader/fetch/cached_metadata_handler.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// ModuleScriptCreationParams contains parameters for creating ModuleScript.
class ModuleScriptCreationParams {
  DISALLOW_NEW();

  enum class ModuleType { kJavaScriptModule, kJSONModule };

 public:
  ModuleScriptCreationParams(
      const KURL& response_url,
      const ModuleScriptCreationParams::ModuleType module_type,
      const ParkableString& source_text,
      SingleCachedMetadataHandler* cache_handler,
      network::mojom::CredentialsMode credentials_mode)
      : response_url_(response_url),
        module_type_(module_type),
        is_isolated_(false),
        source_text_(source_text),
        isolated_source_text_(),
        cache_handler_(cache_handler),
        credentials_mode_(credentials_mode) {}

  ~ModuleScriptCreationParams() = default;

  ModuleScriptCreationParams IsolatedCopy() const {
    String isolated_source_text =
        isolated_source_text_ ? isolated_source_text_.IsolatedCopy()
                              : GetSourceText().ToString().IsolatedCopy();

    return ModuleScriptCreationParams(GetResponseUrl().Copy(), module_type_,
                                      isolated_source_text,
                                      GetFetchCredentialsMode());
  }

  ModuleScriptCreationParams::ModuleType GetModuleType() const {
    return module_type_;
  }

  const KURL& GetResponseUrl() const { return response_url_; }
  const ParkableString& GetSourceText() const {
    if (is_isolated_) {
      source_text_ = ParkableString(isolated_source_text_.ReleaseImpl());
      isolated_source_text_ = String();
      is_isolated_ = false;
    }
    return source_text_;
  }
  SingleCachedMetadataHandler* CacheHandler() const { return cache_handler_; }
  network::mojom::CredentialsMode GetFetchCredentialsMode() const {
    return credentials_mode_;
  }

  bool IsSafeToSendToAnotherThread() const {
    return response_url_.IsSafeToSendToAnotherThread() && is_isolated_;
  }

 private:
  // Creates an isolated copy.
  ModuleScriptCreationParams(
      const KURL& response_url,
      const ModuleScriptCreationParams::ModuleType& module_type,
      const String& isolated_source_text,
      network::mojom::CredentialsMode credentials_mode)
      : response_url_(response_url),
        module_type_(module_type),
        is_isolated_(true),
        source_text_(),
        isolated_source_text_(isolated_source_text),
        credentials_mode_(credentials_mode) {}

  const KURL response_url_;
  const ModuleType module_type_;

  // Mutable because an isolated copy can become bound to a thread when
  // calling GetSourceText().
  mutable bool is_isolated_;
  mutable ParkableString source_text_;
  mutable String isolated_source_text_;

  // |cache_handler_| is cleared when crossing thread boundaries.
  Persistent<SingleCachedMetadataHandler> cache_handler_;

  const network::mojom::CredentialsMode credentials_mode_;
};

}  // namespace blink

namespace WTF {

// Creates a deep copy because |response_url_| and |source_text_| are not
// cross-thread-transfer-safe.
template <>
struct CrossThreadCopier<blink::ModuleScriptCreationParams> {
  static blink::ModuleScriptCreationParams Copy(
      const blink::ModuleScriptCreationParams& params) {
    return params.IsolatedCopy();
  }
};

}  // namespace WTF

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_MODULESCRIPT_MODULE_SCRIPT_CREATION_PARAMS_H_
