// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_POST_MESSAGE_RECEIVER_H_
#define PDF_POST_MESSAGE_RECEIVER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "gin/interceptor.h"
#include "gin/public/wrapper_info.h"
#include "gin/wrappable.h"
#include "v8/include/v8.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace gin {
class ObjectTemplateBuilder;
}  // namespace gin

namespace chrome_pdf {

class V8ValueConverter;

// Implements the `postMessage()` API exposed to the plugin embedder. The
// received messages are converted and forwarded to the `Client`.
// `PostMessageReceiver`'s lifetime is managed by the V8 garbage collector,
// meaning it can outlive the `Client`. Messages are dropped if the `Client` is
// destroyed.
class PostMessageReceiver final : public gin::Wrappable<PostMessageReceiver>,
                                  public gin::NamedPropertyInterceptor {
 public:
  // The interface for a plugin client that handles messages from its embedder.
  class Client {
   public:
    // Handles converted messages from the embedder.
    virtual void OnMessage(const base::Value::Dict& message) = 0;

   protected:
    Client() = default;
    ~Client() = default;
  };

  static gin::WrapperInfo kWrapperInfo;

  // Creates a scriptable object with an implemented `postMessage()` method.
  // Messages are posted asynchronously to `client` using `client_task_runner`.
  static v8::Local<v8::Object> Create(
      v8::Isolate* isolate,
      base::WeakPtr<V8ValueConverter> v8_value_converter,
      base::WeakPtr<Client> client,
      scoped_refptr<base::SequencedTaskRunner> client_task_runner);

  PostMessageReceiver(const PostMessageReceiver&) = delete;
  PostMessageReceiver& operator=(const PostMessageReceiver&) = delete;

 protected:
  ~PostMessageReceiver() override;

 private:
  PostMessageReceiver(
      v8::Isolate* isolate,
      base::WeakPtr<V8ValueConverter> v8_value_converter,
      base::WeakPtr<Client> client,
      scoped_refptr<base::SequencedTaskRunner> client_task_runner);

  // gin::Wrappable:
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;
  const char* GetTypeName() override;

  // gin::NamedPropertyInterceptor:
  v8::Local<v8::Value> GetNamedProperty(v8::Isolate* isolate,
                                        const std::string& property) override;
  std::vector<std::string> EnumerateNamedProperties(
      v8::Isolate* isolate) override;

  // Lazily creates and retrieves `function_template_`.
  v8::Local<v8::FunctionTemplate> GetFunctionTemplate();

  // Implements the `postMessage()` method called by the embedder.
  void PostMessage(v8::Local<v8::Value> message);

  base::WeakPtr<V8ValueConverter> v8_value_converter_;

  v8::Persistent<v8::FunctionTemplate> function_template_;

  raw_ptr<v8::Isolate> isolate_;

  base::WeakPtr<Client> client_;

  scoped_refptr<base::SequencedTaskRunner> client_task_runner_;

  base::WeakPtrFactory<PostMessageReceiver> weak_factory_{this};
};

}  // namespace chrome_pdf

#endif  // PDF_POST_MESSAGE_RECEIVER_H_
