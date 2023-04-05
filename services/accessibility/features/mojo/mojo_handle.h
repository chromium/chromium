
// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ACCESSIBILITY_FEATURES_MOJO_MOJO_HANDLE_H_
#define SERVICES_ACCESSIBILITY_FEATURES_MOJO_MOJO_HANDLE_H_

#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "gin/wrappable.h"
#include "mojo/public/cpp/system/handle.h"
#include "services/accessibility/features/registered_wrappable.h"
#include "v8/include/v8-isolate.h"

namespace gin {
class Arguments;
}

namespace ax {

// Provides a MojoHandle object to the Accessibility Service's V8 Javascript.
// This class is parallel to blink::MojoHandle, which does the same for any
// blink renderer.
class MojoHandle : public gin::Wrappable<MojoHandle>,
                   public RegisteredWrappable {
 public:
  static gin::WrapperInfo kWrapperInfo;

  static gin::Handle<MojoHandle> Create(v8::Local<v8::Context> context,
                                        mojo::ScopedHandle handle);
  ~MojoHandle() override;
  MojoHandle(const MojoHandle&) = delete;
  MojoHandle& operator=(const MojoHandle&) = delete;

  // gin::Wrappable:
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;

  //
  // Methods exposed to Javascript.
  // Note: gin::Wrappable's bound methods need to be public.
  //

  // Calls a callback any time a pipe handle becomes (e.g.) readable; returns a
  // MojoWatcher.
  // See third_party/blink/renderer/core/mojo/mojo_handle.idl.
  void Watch(gin::Arguments* arguments);

  // Closes the handle.
  // See third_party/blink/renderer/core/mojo/mojo_handle.idl.
  void Close(gin::Arguments* arguments);

  // Reads the next available message from a pipe (as a raw list of bytes and
  // handles).
  // See third_party/blink/renderer/core/mojo/mojo_handle.idl.
  void ReadMessage(gin::Arguments* arguments);

  // Writes a raw list of bytes and handles into a pipe.
  // See third_party/blink/renderer/core/mojo/mojo_handle.idl.
  void WriteMessage(gin::Arguments* arguments);

  //
  // End of methods exposed to Javascript.
  //

  // Used by mojo::BindInterface:
  mojo::ScopedHandle TakeHandle();

 private:
  MojoHandle(v8::Local<v8::Context> context, mojo::ScopedHandle handle);

  mojo::ScopedHandle handle_;
};

}  // namespace ax

#endif  // SERVICES_ACCESSIBILITY_FEATURES_MOJO_MOJO_HANDLE_H_
