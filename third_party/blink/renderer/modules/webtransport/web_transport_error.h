// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBTRANSPORT_WEB_TRANSPORT_ERROR_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBTRANSPORT_WEB_TRANSPORT_ERROR_H_

#include <stdint.h>

#include "base/types/pass_key.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "v8/include/v8.h"

namespace blink {

class WebTransportErrorInit;

// https://w3c.github.io/webtransport/#web-transport-error-interface
class MODULES_EXPORT WebTransportError : public DOMException {
  DEFINE_WRAPPERTYPEINFO();

 public:
  using PassKey = base::PassKey<WebTransportError>;

  enum class Source { kStream, kSession };

  // Constructor exposed to script. Called by the V8 bindings.
  static WebTransportError* Create(const WebTransportErrorInit*);

  // For creating a WebTransportError from C++. Typically this will be
  // immediately passed to ScriptPromiseResolver::Reject.
  static v8::Local<v8::Value> Create(v8::Isolate*,
                                     absl::optional<uint8_t> stream_error_code,
                                     String message,
                                     Source);

  // Use one of the Create() methods instead. This constructor has to be public
  // so that it can be used with MakeGarbageCollected<> inside the Create
  // methods.
  WebTransportError(PassKey,
                    absl::optional<uint8_t> stream_error_code,
                    String message,
                    Source);
  ~WebTransportError() override;

  absl::optional<uint8_t> streamErrorCode() const { return stream_error_code_; }

  String source() const;

 private:
  const absl::optional<uint8_t> stream_error_code_;
  const Source source_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBTRANSPORT_WEB_TRANSPORT_ERROR_H_
