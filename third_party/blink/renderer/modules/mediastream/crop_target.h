// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_CROP_TARGET_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_CROP_TARGET_H_

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

class ScriptState;

class MODULES_EXPORT CropTarget final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static ScriptPromise fromElement(ScriptState* script_state,
                                   Element* element,
                                   ExceptionState& exception_state);

  // Not Web-exposed.
  explicit CropTarget(String crop_id);

  // The crop-ID is a UUID. CropTarget wraps it and abstracts it away for JS,
  // but internally, the implementation is based on this implementation detail.
  const String& GetCropId() const { return crop_id_; }

 private:
  // TODO(crbug.com/1332628): Wrap the base::Token instead of wrapping its
  // string representation.
  const String crop_id_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_CROP_TARGET_H_
