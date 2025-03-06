// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ACCESSIBILITY_FEATURES_TEXT_ENCODER_H_
#define SERVICES_ACCESSIBILITY_FEATURES_TEXT_ENCODER_H_

#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "gin/wrappable.h"
#include "services/accessibility/features/registered_wrappable.h"

namespace gin {
class Arguments;
}

namespace ax {

// Provides TextEncoder object to the Accessibility Service's V8 Javascript.
// This class is a parallel to blink::TextEncoder, which does the same for
// any blink renderer.
// Note that this only supports UTF-8 encoding.
class TextEncoder : public gin::Wrappable<TextEncoder>,
                    public RegisteredWrappable {
 public:
  static gin::WrapperInfo kWrapperInfo;

  static gin::Handle<TextEncoder> Create(v8::Local<v8::Context> context);

  ~TextEncoder() override = default;
  TextEncoder(const TextEncoder&) = delete;
  TextEncoder& operator=(const TextEncoder&) = delete;

  // gin::Wrappable:
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;

  //
  // Methods exposed to Javascript.
  // Note: gin::Wrappable's bound methods need to be public.
  //

  // Encodes a Javascript string into a v8::Uint8Array.
  // See third_party/blink/renderer/modules/encoding/text_encoder.idl.
  void Encode(gin::Arguments* arguments);

  //
  // End of methods exposed to Javascript.
  //

 private:
  explicit TextEncoder(v8::Local<v8::Context> context);
};

}  // namespace ax

#endif  // SERVICES_ACCESSIBILITY_FEATURES_TEXT_ENCODER_H_
