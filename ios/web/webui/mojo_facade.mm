// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/webui/mojo_facade.h"

#include <stdint.h>

#include <limits>
#include <utility>
#include <vector>

#import <Foundation/Foundation.h>

#include "base/bind.h"
#import "base/ios/block_types.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "base/values.h"
#include "ios/web/public/thread/web_thread.h"
#import "ios/web/public/web_state.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"
#include "mojo/public/cpp/system/core.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

MojoFacade::MojoFacade(WebState* web_state) : web_state_(web_state) {
  DCHECK_CURRENTLY_ON(WebThread::UI);
  DCHECK(web_state_);
}

MojoFacade::~MojoFacade() {
  DCHECK_CURRENTLY_ON(WebThread::UI);
}

std::string MojoFacade::HandleMojoMessage(
    const std::string& mojo_message_as_json) {
  DCHECK_CURRENTLY_ON(WebThread::UI);
  MessageNameAndArguments name_and_args =
      GetMessageNameAndArguments(mojo_message_as_json);

  base::Value result;
  if (name_and_args.name == "Mojo.bindInterface") {
    // HandleMojoBindInterface does not return a value.
    HandleMojoBindInterface(std::move(name_and_args.args));
  } else if (name_and_args.name == "MojoHandle.close") {
    // HandleMojoHandleClose does not return a value.
    HandleMojoHandleClose(std::move(name_and_args.args));
  } else if (name_and_args.name == "Mojo.createMessagePipe") {
    result = HandleMojoCreateMessagePipe(std::move(name_and_args.args));
  } else if (name_and_args.name == "MojoHandle.writeMessage") {
    result = HandleMojoHandleWriteMessage(std::move(name_and_args.args));
  } else if (name_and_args.name == "MojoHandle.readMessage") {
    result = HandleMojoHandleReadMessage(std::move(name_and_args.args));
  } else if (name_and_args.name == "MojoHandle.watch") {
    result = HandleMojoHandleWatch(std::move(name_and_args.args));
  } else if (name_and_args.name == "MojoWatcher.cancel") {
    // HandleMojoWatcherCancel does not return a value.
    HandleMojoWatcherCancel(std::move(name_and_args.args));
  }

  if (result.is_none()) {
    return std::string();
  }

  std::string json_result;
  base::JSONWriter::Write(result, &json_result);
  return json_result;
}

MojoFacade::MessageNameAndArguments MojoFacade::GetMessageNameAndArguments(
    const std::string& mojo_message_as_json) {
  base::JSONReader::ValueWithError value_with_error =
      base::JSONReader::ReadAndReturnValueWithError(mojo_message_as_json,
                                                    base::JSON_PARSE_RFC);
  CHECK(value_with_error.value);
  CHECK(value_with_error.value->is_dict());
  CHECK_EQ(value_with_error.error_code, base::JSONReader::JSON_NO_ERROR);

  const std::string* name = value_with_error.value->FindStringKey("name");
  CHECK(name);

  base::Value* args = value_with_error.value->FindKeyOfType(
      "args", base::Value::Type::DICTIONARY);
  CHECK(args);

  return {*name, std::move(*args)};
}

void MojoFacade::HandleMojoBindInterface(base::Value args) {
  const std::string* interface_name = args.FindStringKey("interfaceName");
  CHECK(interface_name);

  base::Optional<int> raw_handle = args.FindIntKey("requestHandle");
  CHECK(raw_handle.has_value());

  mojo::ScopedMessagePipeHandle handle(
      static_cast<mojo::MessagePipeHandle>(*raw_handle));
  web_state_->GetInterfaceBinderForMainFrame()->BindInterface(
      mojo::GenericPendingReceiver(*interface_name, std::move(handle)));
}

void MojoFacade::HandleMojoHandleClose(base::Value args) {
  base::Optional<int> handle = args.FindIntKey("handle");
  CHECK(handle.has_value());

  mojo::Handle(*handle).Close();
}

base::Value MojoFacade::HandleMojoCreateMessagePipe(base::Value args) {
  mojo::ScopedMessagePipeHandle handle0, handle1;
  MojoResult mojo_result = mojo::CreateMessagePipe(nullptr, &handle0, &handle1);
  base::Value result(base::Value::Type::DICTIONARY);
  result.SetKey("result", base::Value(static_cast<int>(mojo_result)));
  if (mojo_result == MOJO_RESULT_OK) {
    result.SetKey("handle0",
                  base::Value(static_cast<int>(handle0.release().value())));
    result.SetKey("handle1",
                  base::Value(static_cast<int>(handle1.release().value())));
  }
  return result;
}

base::Value MojoFacade::HandleMojoHandleWriteMessage(base::Value args) {
  base::Optional<int> handle = args.FindIntKey("handle");
  CHECK(handle.has_value());

  const base::Value* handles_list =
      args.FindKeyOfType("handles", base::Value::Type::LIST);
  CHECK(handles_list);

  const base::Value* buffer =
      args.FindKeyOfType("buffer", base::Value::Type::DICTIONARY);
  CHECK(buffer);

  int flags = MOJO_WRITE_MESSAGE_FLAG_NONE;

  const auto& handles_list_storage = handles_list->GetList();
  std::vector<MojoHandle> handles(handles_list_storage.size());
  for (size_t i = 0; i < handles_list_storage.size(); i++) {
    int one_handle = handles_list_storage[i].GetInt();
    handles[i] = one_handle;
  }

  std::vector<uint8_t> bytes(buffer->DictSize());
  for (const auto& item : buffer->DictItems()) {
    size_t index = std::numeric_limits<size_t>::max();
    CHECK(base::StringToSizeT(item.first, &index));
    CHECK(index < bytes.size());
    int one_byte = item.second.GetInt();
    bytes[index] = one_byte;
  }

  mojo::MessagePipeHandle message_pipe(static_cast<MojoHandle>(*handle));
  MojoResult result =
      mojo::WriteMessageRaw(message_pipe, bytes.data(), bytes.size(),
                            handles.data(), handles.size(), flags);

  return base::Value(static_cast<int>(result));
}

base::Value MojoFacade::HandleMojoHandleReadMessage(base::Value args) {
  base::Value* handle_as_value = args.FindKey("handle");
  CHECK(handle_as_value);
  int handle_as_int = 0;
  if (handle_as_value->is_int()) {
    handle_as_int = handle_as_value->GetInt();
  }

  int flags = MOJO_READ_MESSAGE_FLAG_NONE;

  std::vector<uint8_t> bytes;
  std::vector<mojo::ScopedHandle> handles;
  mojo::MessagePipeHandle handle(static_cast<MojoHandle>(handle_as_int));
  MojoResult mojo_result =
      mojo::ReadMessageRaw(handle, &bytes, &handles, flags);

  base::Value result(base::Value::Type::DICTIONARY);
  if (mojo_result == MOJO_RESULT_OK) {
    base::Value handles_list(base::Value::Type::LIST);
    base::Value::ListStorage& handles_list_storage = handles_list.GetList();
    for (uint32_t i = 0; i < handles.size(); i++) {
      handles_list_storage.emplace_back(
          static_cast<int>(handles[i].release().value()));
    }
    result.SetKey("handles", std::move(handles_list));

    base::Value buffer(base::Value::Type::LIST);
    base::Value::ListStorage& buffer_storage = buffer.GetList();
    for (uint32_t i = 0; i < bytes.size(); i++) {
      buffer_storage.emplace_back(bytes[i]);
    }
    result.SetKey("buffer", std::move(buffer));
  }
  result.SetKey("result", base::Value(static_cast<int>(mojo_result)));

  return result;
}

base::Value MojoFacade::HandleMojoHandleWatch(base::Value args) {
  base::Optional<int> handle = args.FindIntKey("handle");
  CHECK(handle.has_value());
  base::Optional<int> signals = args.FindIntKey("signals");
  CHECK(signals.has_value());
  base::Optional<int> callback_id = args.FindIntKey("callbackId");
  CHECK(callback_id.has_value());

  mojo::SimpleWatcher::ReadyCallback callback = base::BindRepeating(
      ^(int callback_id, MojoResult result) {
        NSString* script = [NSString
            stringWithFormat:
                @"Mojo.internal.watchCallbacksHolder.callCallback(%d, %d)",
                callback_id, result];
        web_state_->ExecuteJavaScript(base::SysNSStringToUTF16(script));
      },
      *callback_id);
  auto watcher = std::make_unique<mojo::SimpleWatcher>(
      FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::AUTOMATIC);
  watcher->Watch(static_cast<mojo::Handle>(*handle), *signals, callback);
  watchers_.insert(std::make_pair(++last_watch_id_, std::move(watcher)));
  return base::Value(last_watch_id_);
}

void MojoFacade::HandleMojoWatcherCancel(base::Value args) {
  base::Optional<int> watch_id = args.FindIntKey("watchId");
  CHECK(watch_id.has_value());
  watchers_.erase(*watch_id);
}

}  // namespace web
