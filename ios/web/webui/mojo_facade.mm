// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/webui/mojo_facade.h"

#import <Foundation/Foundation.h>
#import <stdint.h>

#import <limits>
#import <tuple>
#import <utility>
#import <vector>

#import "base/base64.h"
#import "base/functional/bind.h"
#import "base/ios/block_types.h"
#import "base/json/json_reader.h"
#import "base/json/json_writer.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/values.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/thread/web_thread.h"
#import "ios/web/public/web_state.h"
#import "mojo/public/cpp/bindings/generic_pending_receiver.h"

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

  std::optional<int> pipe_id = args.FindInt("requestHandle");
  CHECK(pipe_id.has_value());

  mojo::ScopedMessagePipeHandle pipe = TakePipeFromId(*pipe_id);
  CHECK(pipe.is_valid());
  web_state_->GetInterfaceBinderForMainFrame()->BindInterface(
      mojo::GenericPendingReceiver(*interface_name, std::move(pipe)));
}

void MojoFacade::HandleMojoHandleClose(base::Value::Dict args) {
  std::optional<int> pipe_id = args.FindInt("handle");
  CHECK(pipe_id.has_value());

  // Will close once out of scope.
  mojo::ScopedMessagePipeHandle pipe = TakePipeFromId(*pipe_id);
}

base::Value MojoFacade::HandleMojoCreateMessagePipe(base::Value::Dict args) {
  mojo::MessagePipe pipe;
  base::Value::Dict result;
  result.Set("result", static_cast<int>(MOJO_RESULT_OK));
  result.Set("handle0", AllocatePipeId(std::move(pipe.handle0)));
  result.Set("handle1", AllocatePipeId(std::move(pipe.handle1)));
  return base::Value(std::move(result));
}

base::Value MojoFacade::HandleMojoHandleWriteMessage(base::Value::Dict args) {
  std::optional<int> pipe_id = args.FindInt("handle");
  CHECK(pipe_id.has_value());
  mojo::MessagePipeHandle pipe = GetPipeFromId(*pipe_id);
  CHECK(pipe.is_valid());

  const base::Value::List* handles_list = args.FindList("handles");
  CHECK(handles_list);

  const std::string* buffer = args.FindString("buffer");
  CHECK(buffer);

  int flags = MOJO_WRITE_MESSAGE_FLAG_NONE;

  std::vector<mojo::ScopedMessagePipeHandle> handles(handles_list->size());
  for (size_t i = 0; i < handles_list->size(); i++) {
    int one_handle = (*handles_list)[i].GetInt();
    handles[i] = TakePipeFromId(one_handle);
  }
  std::optional<std::vector<uint8_t>> bytes = base::Base64Decode(*buffer);
  if (!bytes) {
    return base::Value(static_cast<int>(MOJO_RESULT_INVALID_ARGUMENT));
  }

  MojoResult result = mojo::WriteMessageRaw(
      pipe, bytes->data(), bytes->size(),
      reinterpret_cast<MojoHandle*>(handles.data()), handles.size(), flags);
  for (auto& handle : handles) {
    std::ignore = handle.release();
  }

  return base::Value(static_cast<int>(result));
}

base::Value MojoFacade::HandleMojoHandleReadMessage(base::Value::Dict args) {
  base::Value* id_as_value = args.Find("handle");
  CHECK(id_as_value);
  mojo::MessagePipeHandle pipe;
  if (id_as_value->is_int()) {
    pipe = GetPipeFromId(id_as_value->GetInt());
  }
  CHECK(pipe.is_valid());

  int flags = MOJO_READ_MESSAGE_FLAG_NONE;

  std::vector<uint8_t> bytes;
  std::vector<mojo::ScopedHandle> handles;
  MojoResult mojo_result = mojo::ReadMessageRaw(pipe, &bytes, &handles, flags);

  base::Value::Dict result;
  if (mojo_result == MOJO_RESULT_OK) {
    base::Value::List handles_list;
    for (uint32_t i = 0; i < handles.size(); i++) {
      handles_list.Append(AllocatePipeId(mojo::ScopedMessagePipeHandle(
          mojo::MessagePipeHandle(handles[i].release().value()))));
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

void MojoFacade::ArmOnNotifyWatcher(int watch_id) {
  auto watcher_it = watchers_.find(watch_id);
  if (watcher_it == watchers_.end()) {
    return;
  }
  watcher_it->second->ArmOrNotify();
}

void MojoFacade::OnWatcherCallback(int callback_id,
                                   int watch_id,
                                   MojoResult result) {
  web::WebFrame* main_frame =
      web_state_->GetPageWorldWebFramesManager()->GetMainWebFrame();
  if (!main_frame) {
    return;
  }

  NSString* script =
      [NSString stringWithFormat:
                    @"Mojo.internal.watchCallbacksHolder.callCallback(%d, %d)",
                    callback_id, result];
  auto callback = base::BindOnce(
      [](base::WeakPtr<MojoFacade> facade, int watch_id, const base::Value*,
         NSError*) {
        if (facade) {
          facade->ArmOnNotifyWatcher(watch_id);
        }
      },
      weak_ptr_factory_.GetWeakPtr(), watch_id);
  // The watcher will be rearmed in `callback` after `script` is executed.
  // `script` calls JS watcher callback which is expected to synchronously read
  // data from the handle (via readMessage). That way, the behavior matches C++
  // mojo SimpleWatcher with ArmingPolicy::AUTOMATIC.
  main_frame->ExecuteJavaScript(base::SysNSStringToUTF16(script),
                                std::move(callback));
}

base::Value MojoFacade::HandleMojoHandleWatch(base::Value::Dict args) {
  std::optional<int> pipe_id = args.FindInt("handle");
  CHECK(pipe_id.has_value());
  std::optional<int> signals = args.FindInt("signals");
  CHECK(signals.has_value());
  std::optional<int> callback_id = args.FindInt("callbackId");
  CHECK(callback_id.has_value());
  const int watch_id = ++last_watch_id_;

  // Note: base::Unretained() is safe because `this` owns all the watchers.
  auto callback =
      base::BindRepeating(&MojoFacade::OnWatcherCallback,
                          base::Unretained(this), *callback_id, watch_id);

  auto watcher = std::make_unique<mojo::SimpleWatcher>(
      FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL);

  mojo::MessagePipeHandle pipe = GetPipeFromId(*pipe_id);
  watcher->Watch(pipe, *signals, callback);
  watcher->ArmOrNotify();
  watchers_.insert(std::make_pair(watch_id, std::move(watcher)));
  return base::Value(watch_id);
}

void MojoFacade::HandleMojoWatcherCancel(base::Value::Dict args) {
  std::optional<int> watch_id = args.FindInt("watchId");
  CHECK(watch_id.has_value());
  watchers_.erase(*watch_id);
}

int MojoFacade::AllocatePipeId(mojo::ScopedMessagePipeHandle pipe) {
  const int pipe_id = next_pipe_id_++;
  pipes_.emplace(pipe_id, std::move(pipe));
  return pipe_id;
}

mojo::MessagePipeHandle MojoFacade::GetPipeFromId(int id) {
  auto it = pipes_.find(id);
  if (it == pipes_.end()) {
    return {};
  }
  return it->second.get();
}

mojo::ScopedMessagePipeHandle MojoFacade::TakePipeFromId(int id) {
  auto it = pipes_.find(id);
  if (it == pipes_.end()) {
    return {};
  }

  mojo::ScopedMessagePipeHandle pipe = std::move(it->second);
  pipes_.erase(it);
  return pipe;
}

}  // namespace web
