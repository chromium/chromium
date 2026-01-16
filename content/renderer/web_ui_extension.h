// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_WEB_UI_EXTENSION_H_
#define CONTENT_RENDERER_WEB_UI_EXTENSION_H_

#include <string>

#include "content/public/common/bindings_policy.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-function.h"
#include "v8/include/v8-object.h"

namespace blink {
class WebLocalFrame;
}

namespace gin {
class Arguments;
}

namespace content {

class WebUIExtension {
 public:
  WebUIExtension() = delete;
  WebUIExtension(const WebUIExtension&) = delete;
  WebUIExtension& operator=(const WebUIExtension&) = delete;

  static void Install(blink::WebLocalFrame* frame, BindingsPolicySet bindings);

 private:
  static void InstallDefaultWebUIExtension(v8::Isolate* isolate,
                                           v8::Local<v8::Context> context,
                                           v8::Local<v8::Object> chrome);
  static void Send(gin::Arguments* args);
  static std::string GetVariableValue(const std::string& name);
};

}  // namespace content

#endif  // CONTENT_RENDERER_WEB_UI_EXTENSION_H_
