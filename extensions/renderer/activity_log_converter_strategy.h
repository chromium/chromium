// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_ACTIVITY_LOG_CONVERTER_STRATEGY_H_
#define EXTENSIONS_RENDERER_ACTIVITY_LOG_CONVERTER_STRATEGY_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "content/public/renderer/v8_value_converter.h"
#include "v8/include/v8.h"

namespace extensions {

// This class is used by activity logger and extends behavior of
// V8ValueConverter. It overwrites conversion of V8 arrays and objects.  When
// converting arrays and objects, we must not invoke any JS code which may
// result in triggering activity logger. In such case, the log entries will be
// generated due to V8 object conversion rather than extension activity.
class ActivityLogConverterStrategy
    : public content::V8ValueConverter::Strategy {
 public:
  ActivityLogConverterStrategy();
  ~ActivityLogConverterStrategy() override;

  // content::V8ValueConverter::Strategy implementation.
  bool FromV8Object(v8::Local<v8::Object> value,
                    std::unique_ptr<base::Value>* out,
                    v8::Isolate* isolate) override;
  bool FromV8Array(v8::Local<v8::Array> value,
                   std::unique_ptr<base::Value>* out,
                   v8::Isolate* isolate) override;

 private:
  bool FromV8Internal(v8::Local<v8::Object> value,
                      std::unique_ptr<base::Value>* out,
                      v8::Isolate* isolate) const;

  DISALLOW_COPY_AND_ASSIGN(ActivityLogConverterStrategy);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_ACTIVITY_LOG_CONVERTER_STRATEGY_H_
