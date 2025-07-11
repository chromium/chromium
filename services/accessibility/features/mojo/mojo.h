// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ACCESSIBILITY_FEATURES_MOJO_MOJO_H_
#define SERVICES_ACCESSIBILITY_FEATURES_MOJO_MOJO_H_

#include "gin/object_template_builder.h"
#include "gin/wrappable.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-local-handle.h"

namespace gin {
class Arguments;
}

namespace ax {

// Provides Mojo object to the Accessibility Service's V8 Javascript.
// This class is a parallel to blink::Mojo, which does the same for
// any blink renderer.
class Mojo final : public gin::Wrappable<Mojo> {
 public:
  static constexpr gin::WrapperInfo kWrapperInfo = {
      {gin::kEmbedderNativeGin}, gin::kMojo};

  static v8::Local<v8::Object> Create(v8::Isolate* isolate);

  Mojo();
  ~Mojo() override;
  Mojo(const Mojo&) = delete;
  Mojo& operator=(const Mojo&) = delete;

  // gin::Wrappable:
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;
  const gin::WrapperInfo* wrapper_info() const override;

  //
  // Methods exposed to Javascript.
  // Note: gin::Wrappable's bound methods need to be public.
  //

  // Returns two MojoHandles, one for each end of a new pipe.
  // See third_party/blink/renderer/core/mojo/mojo.idl.
  void CreateMessagePipe(gin::Arguments* arguments);

  // Passes a pipe handle to the AccessibilityService from V8 for someone
  // to bind it.
  // See third_party/blink/renderer/core/mojo/mojo.idl.
  void BindInterface(gin::Arguments* arguments);

  //
  // End of methods exposed to Javascript.
  //
};

}  // namespace ax

#endif  // SERVICES_ACCESSIBILITY_FEATURES_MOJO_MOJO_H_
