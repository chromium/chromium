// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_WEB_UI_EXTENSION_H_
#define CONTENT_RENDERER_WEB_UI_EXTENSION_H_

#include <string>

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

  static void Install(blink::WebLocalFrame* frame);

 private:
  static void Send(gin::Arguments* args);
  static std::string GetVariableValue(const std::string& name);
};

}  // namespace content

#endif  // CONTENT_RENDERER_WEB_UI_EXTENSION_H_
