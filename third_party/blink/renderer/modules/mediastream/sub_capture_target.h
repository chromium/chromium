// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_SUB_CAPTURE_TARGET_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_SUB_CAPTURE_TARGET_H_

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

class ScriptState;
enum class SubCaptureTargetType;

// Mutual non-Web-exposed parent class for various Web-exposed tokens
// which use the same minting logic under the hood.
class MODULES_EXPORT SubCaptureTarget : public ScriptWrappable {
 public:
  SubCaptureTargetType GetType() const { return type_; }

  // The ID is a UUID. SubCaptureTarget wraps it and abstracts it away for JS,
  // but internally, the implementation is based on this implementation detail.
  const String& GetId() const { return id_; }

 protected:
  static ScriptPromise fromElement(ScriptState* script_state,
                                   Element* element,
                                   ExceptionState& exception_state,
                                   SubCaptureTargetType type);

  SubCaptureTarget(SubCaptureTargetType type, String id);

 private:
  const SubCaptureTargetType type_;

  // TODO(crbug.com/1332628): Wrap the base::Token instead of wrapping its
  // string representation.
  const String id_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_SUB_CAPTURE_TARGET_H_
