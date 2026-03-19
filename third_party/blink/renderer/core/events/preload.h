// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EVENTS_PRELOAD_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EVENTS_PRELOAD_H_

#include "services/network/public/mojom/fetch_api.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_cross_origin_mode.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// https://github.com/WICG/speculative_load_measurement
class Preload final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  Preload(const KURL& url,
          const String& as,
          network::mojom::CredentialsMode credentials_mode,
          network::mojom::RequestMode request_mode,
          bool used);

  String url() const { return url_.GetString(); }
  const String& as() const { return as_; }
  // Returns the crossorigin mode for the IDL interface.
  // kNone = no crossorigin attribute,
  // kAnonymous = "anonymous" or no attribute value (same-origin),
  // kUseCredentials = "use-credentials".
  V8CrossOriginMode crossorigin() const;
  bool used() const { return used_; }

  void Trace(Visitor*) const override;

 private:
  const KURL url_;
  const String as_;
  const network::mojom::CredentialsMode credentials_mode_;
  const network::mojom::RequestMode request_mode_;
  const bool used_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EVENTS_PRELOAD_H_
