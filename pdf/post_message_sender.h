// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_POST_MESSAGE_SENDER_H_
#define PDF_POST_MESSAGE_SENDER_H_

#include "base/memory/raw_ptr.h"
#include "v8/include/v8-forward.h"

namespace base {
class Value;
}  // namespace base

namespace blink {
class WebPluginContainer;
}  // namespace blink

namespace chrome_pdf {

class V8ValueConverter;

// Manages messages sent from the plugin to its embedder.
class PostMessageSender final {
 public:
  explicit PostMessageSender(V8ValueConverter* v8_value_converter);
  PostMessageSender(const PostMessageSender&) = delete;
  PostMessageSender& operator=(const PostMessageSender&) = delete;
  ~PostMessageSender();

  // Enqueues a "message" event carrying `message` to the plugin embedder.
  // Nothing is enqueued if `container_` is null.
  void Post(base::Value message);

  // Sets the plugin container that enqueues the messages. This method should be
  // called by the owning plugin whenever its container is set or unset to
  // mirror the initialized lifetime of the plugin.
  void set_container(blink::WebPluginContainer* container) {
    container_ = container;
  }

 private:
  const raw_ptr<V8ValueConverter> v8_value_converter_;

  const raw_ptr<v8::Isolate> isolate_;

  raw_ptr<blink::WebPluginContainer> container_;
};

}  // namespace chrome_pdf

#endif  // PDF_POST_MESSAGE_SENDER_H_
