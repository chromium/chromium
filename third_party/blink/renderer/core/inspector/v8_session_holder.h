// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_V8_SESSION_HOLDER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_V8_SESSION_HOLDER_H_

#include <memory>

#include "third_party/blink/renderer/core/core_export.h"

namespace v8_inspector {
class V8InspectorSession;
}

namespace blink {

class CORE_EXPORT V8SessionHolder {
 public:
  V8SessionHolder() = default;
  explicit V8SessionHolder(
      std::shared_ptr<v8_inspector::V8InspectorSession> session)
      : v8_session_(std::move(session)) {}

  v8_inspector::V8InspectorSession* operator->() const {
    return v8_session_.get();
  }
  v8_inspector::V8InspectorSession* get() const { return v8_session_.get(); }
  explicit operator bool() const { return !!v8_session_; }
  void reset() { v8_session_.reset(); }

 private:
  std::shared_ptr<v8_inspector::V8InspectorSession> v8_session_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_V8_SESSION_HOLDER_H_
