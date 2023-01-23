// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/webui/mojo_facade.h"

#import <stdint.h>

#import <limits>
#import <utility>
#import <vector>

#import <Foundation/Foundation.h>

#import "base/base64.h"
#import "base/functional/bind.h"
#import "base/ios/block_types.h"
#import "base/json/json_reader.h"
#import "base/json/json_writer.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/values.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frame_util.h"
#import "ios/web/public/thread/web_thread.h"
#import "ios/web/public/web_state.h"
#import "mojo/public/cpp/bindings/generic_pending_receiver.h"

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
  auto value_with_error = base::JSONReader::ReadAndReturnValueWithError(
      mojo_message_as_json, base::JSON_PARSE_RFC);
  CHECK(value_with_error.has_value());
  CHECK(value_with_error->is_dict());

  base::Value::Dict& dict = value_with_error->GetDict();
  const std::string* name = dict.FindString("name");
  CHECK(name);

  base::Value::Dict* args = dict.FindDict("args");
  CHECK(args);

  return {*name, std::move(*args)};
}

void MojoFacade::HandleMojoBindInterface(base::Value::Dict args) {
  const std::string* interface_name = args.FindString("interfaceName");
  CHECK(interface_name);

  absl::optional<int> raw_handle = args.FindInt("requestHandle");
  CHECK(raw_handle.has_value());

  mojo::ScopedMessagePipeHandle handle(
      static_cast<mojo::MessagePipeHandle>(*raw_handle));
  web_state_->GetInterfaceBinderForMainFrame()->BindInterface(
      mojo::GenericPendingReceiver(*interface_name, std::move(handle)));
}

void MojoFacade::HandleMojoHandleClose(base::Value::Dict args) {
  absl::optional<int> handle = args.FindInt("handle");
  CHECK(handle.has_value());

  mojo::Handle(*handle).Close();
}

base::Value MojoFacade::HandleMojoCreateMessagePipe(base::Value::Dict args) {
  mojo::ScopedMessagePipeHandle handle0, handle1;
  MojoResult mojo_result = mojo::CreateMessagePipe(nullptr, &handle0, &handle1);
  base::Value::Dict result;
  result.Set("result", static_cast<int>(mojo_result));
  if (mojo_result == MOJO_RESULT_OK) {
    result.Set("handle0", static_cast<int>(handle0.release().value()));
    result.Set("handle1", static_cast<int>(handle1.release().value()));
  }
  return base::Value(std::move(result));
}

base::Value MojoFacade::HandleMojoHandleWriteMessage(base::Value::Dict args) {
  absl::optional<int> handle = args.FindInt("handle");
  CHECK(handle.has_value());

  const base::Value::List* handles_list = args.FindList("handles");
  CHECK(handles_list);

  const std::string* buffer = args.FindString("buffer");
  CHECK(buffer);

  int flags = MOJO_WRITE_MESSAGE_FLAG_NONE;

  std::vector<MojoHandle> handles(handles_list->size());
  for (size_t i = 0; i < handles_list->size(); i++) {
    int one_handle = (*handles_list)[i].GetInt();
    handles[i] = one_handle;
  }
  absl::optional<std::vector<uint8_t>> bytes = base::Base64Decode(*buffer);
  if (!bytes) {
    return base::Value(static_cast<int>(MOJO_RESULT_INVALID_ARGUMENT));
  }

  mojo::MessagePipeHandle message_pipe(static_cast<MojoHandle>(*handle));
  MojoResult result =
      mojo::WriteMessageRaw(message_pipe, bytes->data(), bytes->size(),
                            handles.data(), handles.size(), flags);

  return base::Value(static_cast<int>(result));
}

base::Value MojoFacade::HandleMojoHandleReadMessage(base::Value::Dict args) {
  base::Value* handle_as_value = args.Find("handle");
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

  base::Value::Dict result;
  if (mojo_result == MOJO_RESULT_OK) {
    base::Value::List handles_list;
    for (uint32_t i = 0; i < handles.size(); i++) {
      handles_list.Append(static_cast<int>(handles[i].release().value()));
    }
    result.Set("handles", std::move(handles_list));

    base::Value::List buffer;
    for (uint32_t i = 0; i < bytes.size(); i++) {
      buffer.Append(bytes[i]);
    }
    result.Set("buffer", std::move(buffer));
  }
  result.Set("result", static_cast<int>(mojo_result));

  return base::Value(std::move(result));
}

base::Value MojoFacade::HandleMojoHandleWatch(base::Value::Dict args) {
  absl::optional<int> handle = args.FindInt("handle");
  CHECK(handle.has_value());
  absl::optional<int> signals = args.FindInt("signals");
  CHECK(signals.has_value());
  absl::optional<int> callback_id = args.FindInt("callbackId");
  CHECK(callback_id.has_value());

  mojo::SimpleWatcher::ReadyCallback callback = base::BindRepeating(
      ^(int inner_callback_id, MojoResult result) {
        NSString* script = [NSString
            stringWithFormat:
                @"Mojo.internal.watchCallbacksHolder.callCallback(%d, %d)",
                inner_callback_id, result];
        web::WebFrame* main_frame = web::GetMainFrame(web_state_);
        if (main_frame) {
          main_frame->ExecuteJavaScript(base::SysNSStringToUTF16(script));
        }
      },
      *callback_id);
  auto watcher = std::make_unique<mojo::SimpleWatcher>(
      FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::AUTOMATIC);
  watcher->Watch(static_cast<mojo::Handle>(*handle), *signals, callback);
  watchers_.insert(std::make_pair(++last_watch_id_, std::move(watcher)));
  return base::Value(last_watch_id_);
}

void MojoFacade::HandleMojoWatcherCancel(base::Value::Dict args) {
  absl::optional<int> watch_id = args.FindInt("watchId");
  CHECK(watch_id.has_value());
  watchers_.erase(*watch_id);
}

}  // namespace web
