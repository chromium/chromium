// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ACCESSIBILITY_FEATURES_TEXT_DECODER_H_
#define SERVICES_ACCESSIBILITY_FEATURES_TEXT_DECODER_H_

#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "gin/wrappable.h"
#include "services/accessibility/features/registered_wrappable.h"

namespace gin {
class Arguments;
}

namespace ax {

// Provides TextDecoder object to the Accessibility Service's V8 Javascript.
// This class is a parallel to blink::TextDecoder, which does the same for
// any blink renderer.
// Note that this only supports UTF-8 decoding.
class TextDecoder : public gin::Wrappable<TextDecoder>,
                    public RegisteredWrappable {
 public:
  static gin::WrapperInfo kWrapperInfo;

  static gin::Handle<TextDecoder> Create(v8::Local<v8::Context> context);

  ~TextDecoder() override = default;
  TextDecoder(const TextDecoder&) = delete;
  TextDecoder& operator=(const TextDecoder&) = delete;

  // gin::Wrappable:
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;

  //
  // Methods exposed to Javascript.
  // Note: gin::Wrappable's bound methods need to be public.
  //

  // Decodes a Javascript string from a Buffer.
  // See third_party/blink/renderer/modules/encoding/text_decoder.idl.
  void Decode(gin::Arguments* arguments);

  //
  // End of methods exposed to Javascript.
  //

 private:
  explicit TextDecoder(v8::Local<v8::Context> context);
};

}  // namespace ax

#endif  // SERVICES_ACCESSIBILITY_FEATURES_TEXT_DECODER_H_
