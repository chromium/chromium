// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_JAVA_GIN_JAVA_FUNCTION_INVOCATION_HELPER_H_
#define CONTENT_RENDERER_JAVA_GIN_JAVA_FUNCTION_INVOCATION_HELPER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "content/renderer/java/gin_java_bridge_dispatcher.h"
#include "gin/arguments.h"
#include "gin/handle.h"

namespace content {

class GinJavaBridgeValueConverter;

class GinJavaFunctionInvocationHelper {
 public:
  GinJavaFunctionInvocationHelper(
      const std::string& method_name,
      const base::WeakPtr<GinJavaBridgeDispatcher>& dispatcher);

  GinJavaFunctionInvocationHelper(const GinJavaFunctionInvocationHelper&) =
      delete;
  GinJavaFunctionInvocationHelper& operator=(
      const GinJavaFunctionInvocationHelper&) = delete;

  ~GinJavaFunctionInvocationHelper();

  v8::Local<v8::Value> Invoke(gin::Arguments* args);

 private:
  std::string method_name_;
  base::WeakPtr<GinJavaBridgeDispatcher> dispatcher_;
  std::unique_ptr<GinJavaBridgeValueConverter> converter_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_JAVA_GIN_JAVA_FUNCTION_INVOCATION_HELPER_H_
