// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/mojo/mojo.h"

#include <string>
#include <utility>

#include "mojo/public/cpp/system/message_pipe.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_mojo_create_data_pipe_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_mojo_create_data_pipe_result.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_mojo_create_message_pipe_result.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_mojo_create_shared_buffer_result.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_mojo_scope.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/mojo/mojo_handle.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"

namespace blink {

// static
MojoCreateMessagePipeResult* Mojo::createMessagePipe() {
  MojoCreateMessagePipeResult* result_dict =
      MojoCreateMessagePipeResult::Create();
  MojoCreateMessagePipeOptions options = {0};
  options.struct_size = sizeof(::MojoCreateMessagePipeOptions);
  options.flags = MOJO_CREATE_MESSAGE_PIPE_FLAG_NONE;

  mojo::ScopedMessagePipeHandle handle0, handle1;
  MojoResult result = mojo::CreateMessagePipe(&options, &handle0, &handle1);

  result_dict->setResult(result);
  if (result == MOJO_RESULT_OK) {
    result_dict->setHandle0(MakeGarbageCollected<MojoHandle>(
        mojo::ScopedHandle::From(std::move(handle0))));
    result_dict->setHandle1(MakeGarbageCollected<MojoHandle>(
        mojo::ScopedHandle::From(std::move(handle1))));
  }
  return result_dict;
}

// static
MojoCreateDataPipeResult* Mojo::createDataPipe(
    const MojoCreateDataPipeOptions* options_dict) {
  MojoCreateDataPipeResult* result_dict = MojoCreateDataPipeResult::Create();

  // NOTE: CreateDataPipe below validates options, but its inputs are unsigned.
  // The inputs here may be negative, hence this additional validation.
  if (!options_dict->hasElementNumBytes() ||
      !options_dict->hasCapacityNumBytes() ||
      options_dict->capacityNumBytes() < 1 ||
      options_dict->elementNumBytes() < 1) {
    result_dict->setResult(MOJO_RESULT_INVALID_ARGUMENT);
    return result_dict;
  }

  ::MojoCreateDataPipeOptions options = {0};
  options.struct_size = sizeof(options);
  options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
  options.element_num_bytes = options_dict->elementNumBytes();
  options.capacity_num_bytes = options_dict->capacityNumBytes();

  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  MojoResult result = mojo::CreateDataPipe(&options, producer, consumer);
  result_dict->setResult(result);
  if (result == MOJO_RESULT_OK) {
    result_dict->setProducer(MakeGarbageCollected<MojoHandle>(
        mojo::ScopedHandle::From(std::move(producer))));
    result_dict->setConsumer(MakeGarbageCollected<MojoHandle>(
        mojo::ScopedHandle::From(std::move(consumer))));
  }
  return result_dict;
}

// static
MojoCreateSharedBufferResult* Mojo::createSharedBuffer(unsigned num_bytes) {
  MojoCreateSharedBufferResult* result_dict =
      MojoCreateSharedBufferResult::Create();
  MojoCreateSharedBufferOptions* options = nullptr;
  mojo::Handle handle;
  MojoResult result =
      MojoCreateSharedBuffer(num_bytes, options, handle.mutable_value());

  result_dict->setResult(result);
  if (result == MOJO_RESULT_OK) {
    result_dict->setHandle(
        MakeGarbageCollected<MojoHandle>(mojo::MakeScopedHandle(handle)));
  }
  return result_dict;
}

// static
void Mojo::bindInterface(ScriptState* script_state,
                         const String& interface_name,
                         MojoHandle* request_handle,
                         const V8MojoScope& scope,
                         ExceptionState& exception_state) {
  std::string name = interface_name.Utf8();
  auto handle =
      mojo::ScopedMessagePipeHandle::From(request_handle->TakeHandle());

  auto* context = ExecutionContext::From(script_state);

  // If MojoJS broker is enabled, it must be used to handle bindInterface
  // calls.
  if (context->use_mojo_js_interface_broker()) {
    if (scope == V8MojoScope::Enum::kContext) {
      context->GetMojoJSInterfaceBroker().GetInterface(name, std::move(handle));
    } else {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kNotAllowedError,
          String::FromUTF8("MojoJS interface broker is specified, can't use "
                           "scopes other than 'context'"));
    }
    return;
  }

  if (scope == V8MojoScope::Enum::kProcess) {
    Platform::Current()->GetBrowserInterfaceBroker()->GetInterface(
        mojo::GenericPendingReceiver(name, std::move(handle)));
    return;
  }

  context->GetBrowserInterfaceBroker().GetInterface(name, std::move(handle));
}

}  // namespace blink
