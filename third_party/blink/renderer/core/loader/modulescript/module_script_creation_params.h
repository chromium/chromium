// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_MODULESCRIPT_MODULE_SCRIPT_CREATION_PARAMS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_MODULESCRIPT_MODULE_SCRIPT_CREATION_PARAMS_H_

#include "base/optional.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/platform/bindings/parkable_string.h"
#include "third_party/blink/renderer/platform/cross_thread_copier.h"
#include "third_party/blink/renderer/platform/loader/fetch/access_control_status.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// ModuleScriptCreationParams contains parameters for creating ModuleScript.
class ModuleScriptCreationParams {
 public:
  ModuleScriptCreationParams(
      const KURL& response_url,
      const ParkableString& source_text,
      network::mojom::FetchCredentialsMode fetch_credentials_mode,
      AccessControlStatus access_control_status)
      : response_url_(response_url),
        is_isolated_(false),
        source_text_(source_text),
        isolated_source_text_(),
        fetch_credentials_mode_(fetch_credentials_mode),
        access_control_status_(access_control_status) {}

  ~ModuleScriptCreationParams() = default;

  ModuleScriptCreationParams IsolatedCopy() const {
    String isolated_source_text =
        isolated_source_text_ ? isolated_source_text_.IsolatedCopy()
                              : GetSourceText().ToString().IsolatedCopy();
    return ModuleScriptCreationParams(
        GetResponseUrl().Copy(), isolated_source_text,
        GetFetchCredentialsMode(), GetAccessControlStatus());
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
  network::mojom::FetchCredentialsMode GetFetchCredentialsMode() const {
    return fetch_credentials_mode_;
  }
  AccessControlStatus GetAccessControlStatus() const {
    return access_control_status_;
  }

  bool IsSafeToSendToAnotherThread() const {
    return response_url_.IsSafeToSendToAnotherThread() && is_isolated_;
  }

 private:
  // Creates an isolated copy.
  ModuleScriptCreationParams(
      const KURL& response_url,
      const String& isolated_source_text,
      network::mojom::FetchCredentialsMode fetch_credentials_mode,
      AccessControlStatus access_control_status)
      : response_url_(response_url),
        is_isolated_(true),
        source_text_(),
        isolated_source_text_(isolated_source_text),
        fetch_credentials_mode_(fetch_credentials_mode),
        access_control_status_(access_control_status) {}

  const KURL response_url_;

  // Mutable because an isolated copy can become bound to a thread when
  // calling GetSourceText().
  mutable bool is_isolated_;
  mutable ParkableString source_text_;
  mutable String isolated_source_text_;

  const network::mojom::FetchCredentialsMode fetch_credentials_mode_;
  const AccessControlStatus access_control_status_;
};

// Creates a deep copy because |response_url_| and |source_text_| are not
// cross-thread-transfer-safe.
template <>
struct CrossThreadCopier<ModuleScriptCreationParams> {
  static ModuleScriptCreationParams Copy(
      const ModuleScriptCreationParams& params) {
    return params.IsolatedCopy();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_MODULESCRIPT_MODULE_SCRIPT_CREATION_PARAMS_H_
