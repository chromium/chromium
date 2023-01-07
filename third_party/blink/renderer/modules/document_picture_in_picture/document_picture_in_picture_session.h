// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_DOCUMENT_PICTURE_IN_PICTURE_DOCUMENT_PICTURE_IN_PICTURE_SESSION_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_DOCUMENT_PICTURE_IN_PICTURE_DOCUMENT_PICTURE_IN_PICTURE_SESSION_H_

#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class DocumentPictureInPictureSession : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit DocumentPictureInPictureSession(LocalDOMWindow* window);

  LocalDOMWindow* window() const { return window_.Get(); }

  void Trace(Visitor*) const override;

 private:
  Member<LocalDOMWindow> window_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_DOCUMENT_PICTURE_IN_PICTURE_DOCUMENT_PICTURE_IN_PICTURE_SESSION_H_
