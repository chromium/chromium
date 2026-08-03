// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/webui/mojo_facade.h"

#import <Foundation/Foundation.h>
#import <stdint.h>

#import <tuple>
#import <utility>
#import <vector>

#import "base/base64.h"
#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/ios/block_types.h"
#import "base/json/json_writer.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/values.h"
#import "ios/web/js_messaging/web_frame_internal.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/thread/web_thread.h"
#import "ios/web/public/web_state.h"
#import "ios/web/webui/web_ui_metrics.h"
#import "mojo/public/cpp/bindings/generic_pending_receiver.h"

namespace web {

// Retrieves a numeric value as an integer even if it's stored as a double
// (common for JSON values parsed from JavaScript/TypeScript numbers).
std::optional<int> FindIntOrDoubleAsInt(const base::Value& value) {
  if (std::optional<int> i = value.GetIfInt()) {
    return *i;
  }
  if (std::optional<double> d = value.GetIfDouble()) {
    return static_cast<int>(*d);
  }
  return std::nullopt;
}

std::optional<int> FindIntOrDoubleAsInt(const base::DictValue& args,
                                        std::string_view key) {
  if (const base::Value* val = args.Find(key)) {
    return FindIntOrDoubleAsInt(*val);
  }
  return std::nullopt;
}

MojoFacade::MojoFacade(WebState* web_state) : web_state_(web_state) {
  DCHECK_CURRENTLY_ON(WebThread::UI);
  DCHECK(web_state_);
  web_state_->AddObserver(this);
  web_state_->GetPageWorldWebFramesManager()->AddObserver(this);

  WebFrame* main_frame =
      web_state_->GetPageWorldWebFramesManager()->GetMainWebFrame();
  if (main_frame) {
    main_frame_ = main_frame->AsWeakPtr();
    if (!web_state_->IsLoading() && web_state_->GetInterfaceBinderForMainFrame()
                                        ->HasRegisteredInterfaces()) {
      AwaitNextMessage();
    }
  }
}

MojoFacade::~MojoFacade() {
  DCHECK_CURRENTLY_ON(WebThread::UI);
  if (web_state_) {
    web_state_->GetPageWorldWebFramesManager()->RemoveObserver(this);
    web_state_->RemoveObserver(this);
  }
}

web::WebFrame* MojoFacade::GetMainWebFrame() const {
  return main_frame_.get();
}

std::string MojoFacade::GetMainFrameId() const {
  WebFrame* main_frame = GetMainWebFrame();
  if (main_frame) {
    return main_frame->GetFrameId();
  }
  return std::string();
}

void MojoFacade::AwaitNextMessage() {
  DCHECK_CURRENTLY_ON(WebThread::UI);

  if (is_awaiting_message_) {
    return;
  }

  WebFrame* main_frame = GetMainWebFrame();
  if (!main_frame) {
    return;
  }

  is_awaiting_message_ = true;
  std::string web_frame_id = main_frame->GetFrameId();

  auto callback = base::BindOnce(&MojoFacade::OnAwaitNextMessageCompleted,
                                 weak_ptr_factory_.GetWeakPtr(), web_frame_id);

  std::u16string fetch_next_message =
      u"return await Mojo.internal.fetchNextMessageFromJS();";

  main_frame->ExecuteAsyncJavaScript(fetch_next_message, base::DictValue(),
                                     std::move(callback));
}

void MojoFacade::OnAwaitNextMessageCompleted(const std::string& web_frame_id,
                                             const base::Value* value,
                                             NSError* error) {
  DCHECK_CURRENTLY_ON(WebThread::UI);

  is_awaiting_message_ = false;

  bool is_same_frame =
      !web_state_->IsBeingDestroyed() && GetMainFrameId() == web_frame_id;

  if (error || !is_same_frame || !value) {
    if (error && is_same_frame) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&MojoFacade::AwaitNextMessage,
                                    weak_ptr_factory_.GetWeakPtr()));
    }
    return;
  }

  const base::DictValue* dict = value->GetIfDict();
  if (dict) {
    std::optional<int> message_id = FindIntOrDoubleAsInt(*dict, "message_id");
    const base::DictValue* message = dict->FindDict("message");

    if (message_id && message) {
      WebFrame* main_frame = GetMainWebFrame();
      if (main_frame) {
        base::WeakPtr<WebFrame> weak_frame = main_frame->AsWeakPtr();
        HandleMojoMessage(
            *message_id, message,
            base::BindOnce(^(int msg_id, std::string response) {
              WebFrame* frame = weak_frame.get();
              if (!frame) {
                return;
              }
              NSString* response_str = @"null";
              if (!response.empty()) {
                response_str = base::SysUTF8ToNSString(response);
              }
              NSString* script = [NSString
                  stringWithFormat:@"Mojo.internal.messageReceived(%d, %@)",
                                   msg_id, response_str];
              frame->ExecuteAsyncJavaScript(base::SysNSStringToUTF16(script),
                                            base::DictValue(),
                                            base::DoNothing());
            }));
      }
    }
  }
  AwaitNextMessage();
}

void MojoFacade::HandleMojoMessage(
    int message_id,
    const base::DictValue* message,
    base::OnceCallback<void(int, std::string)> completion) {
  DCHECK_CURRENTLY_ON(WebThread::UI);

  const std::string* name = message->FindString("name");
  CHECK(name);
  const base::DictValue* args = message->FindDict("args");
  CHECK(args);

  base::Value result;
  WebUIMojoActions action_outcome = WebUIMojoActions::kSuccess;

  if (*name == "Mojo.bindInterface") {
    // HandleMojoBindInterface does not return a value.
    HandleMojoBindInterface(*args);
  } else if (*name == "MojoHandle.close") {
    // HandleMojoHandleClose does not return a value.
    HandleMojoHandleClose(*args);
  } else if (*name == "Mojo.createMessagePipe") {
    result = HandleMojoCreateMessagePipe(*args);
    int create_pipe_result = *result.GetDict().FindInt("result");
    if (create_pipe_result != MOJO_RESULT_OK) {
      action_outcome = WebUIMojoActions::kFailure;
    }
  } else if (*name == "MojoHandle.writeMessage") {
    result = HandleMojoHandleWriteMessage(*args);
    if (result.GetInt() != MOJO_RESULT_OK) {
      action_outcome = WebUIMojoActions::kFailure;
    }
  } else if (*name == "MojoHandle.watch") {
    result = HandleMojoHandleWatch(*args);
    // HandleMojoHandleWatch returns a base::Value wrapping the watch_id
    // integer. A watch_id of 0 indicates that creating the watcher failed.
    if (result.GetInt() == 0) {
      action_outcome = WebUIMojoActions::kFailure;
    }
  } else if (*name == "MojoWatcher.cancel") {
    // HandleMojoWatcherCancel does not return a value.
    HandleMojoWatcherCancel(*args);
  } else {
    action_outcome = WebUIMojoActions::kFailure;
  }

  std::string action_name = "Unknown";
  if (name) {
    action_name = *name;
  }
  RecordWebUIMojoActionOutcome(action_name, action_outcome);

  if (completion.is_null()) {
    return;
  }

  if (result.is_none()) {
    std::move(completion).Run(message_id, std::string());
    return;
  }

  std::string json_result = base::WriteJson(result).value_or("");
  std::move(completion).Run(message_id, json_result);
}

void MojoFacade::HandleMojoBindInterface(const base::DictValue& args) {
  const std::string* interface_name = args.FindString("interfaceName");
  CHECK(interface_name);

  std::optional<int> pipe_id = FindIntOrDoubleAsInt(args, "requestHandle");
  CHECK(pipe_id.has_value());

  mojo::ScopedMessagePipeHandle pipe = TakePipeFromId(*pipe_id);
  CHECK(pipe.is_valid());
  web_state_->GetInterfaceBinderForMainFrame()->BindInterface(
      mojo::GenericPendingReceiver(*interface_name, std::move(pipe)));
}

void MojoFacade::HandleMojoHandleClose(const base::DictValue& args) {
  std::optional<int> pipe_id = FindIntOrDoubleAsInt(args, "handle");
  CHECK(pipe_id.has_value());

  // Will close once out of scope.
  mojo::ScopedMessagePipeHandle pipe = TakePipeFromId(*pipe_id);
}

base::Value MojoFacade::HandleMojoCreateMessagePipe(
    const base::DictValue& args) {
  std::optional<int> pipe_0_id = FindIntOrDoubleAsInt(args, "handle0Id");
  std::optional<int> pipe_1_id = FindIntOrDoubleAsInt(args, "handle1Id");

  mojo::MessagePipe pipe;

  base::DictValue result;
  result.Set("result", static_cast<int>(MOJO_RESULT_OK));
  result.Set("handle0", AllocatePipeId(std::move(pipe.handle0), pipe_0_id));
  result.Set("handle1", AllocatePipeId(std::move(pipe.handle1), pipe_1_id));
  return base::Value(std::move(result));
}

base::Value MojoFacade::HandleMojoHandleWriteMessage(
    const base::DictValue& args) {
  std::optional<int> pipe_id = FindIntOrDoubleAsInt(args, "handle");
  CHECK(pipe_id.has_value());
  mojo::MessagePipeHandle pipe = GetPipeFromId(*pipe_id);
  CHECK(pipe.is_valid());

  const base::ListValue* handles_list = args.FindList("handles");
  CHECK(handles_list);

  const std::string* buffer = args.FindString("buffer");
  CHECK(buffer);

  int flags = MOJO_WRITE_MESSAGE_FLAG_NONE;

  std::vector<mojo::ScopedMessagePipeHandle> handles;
  handles.reserve(handles_list->size());
  for (const base::Value& item : *handles_list) {
    std::optional<int> handle_id = FindIntOrDoubleAsInt(item);
    handles.push_back(TakePipeFromId(*handle_id));
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

base::Value MojoFacade::ReadMessageFromPipe(int pipe_id) {
  mojo::MessagePipeHandle pipe = GetPipeFromId(pipe_id);
  if (!pipe.is_valid()) {
    base::DictValue result;
    result.Set("result", static_cast<int>(MOJO_RESULT_INVALID_ARGUMENT));
    return base::Value(std::move(result));
  }

  int flags = MOJO_READ_MESSAGE_FLAG_NONE;

  std::vector<uint8_t> bytes;
  std::vector<mojo::ScopedHandle> handles;
  MojoResult mojo_result = mojo::ReadMessageRaw(pipe, &bytes, &handles, flags);

  base::DictValue result;
  if (mojo_result == MOJO_RESULT_OK) {
    base::ListValue handles_list;
    for (uint32_t i = 0; i < handles.size(); i++) {
      handles_list.Append(AllocatePipeId(mojo::ScopedMessagePipeHandle(
          mojo::MessagePipeHandle(handles[i].release().value()))));
    }
    result.Set("handles", std::move(handles_list));

    base::ListValue buffer;
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
                                   int pipe_id,
                                   MojoResult result) {
  WebFrame* main_frame = GetMainWebFrame();
  if (!main_frame) {
    return;
  }

  NSString* script;
  if (result == MOJO_RESULT_OK) {
    base::Value read_result = ReadMessageFromPipe(pipe_id);
    std::string json_result = base::WriteJson(read_result).value_or("");
    script = [NSString
        stringWithFormat:
            @"Mojo.internal.fetchNextMessageFromNative(%d, %@); "
            @"Mojo.internal.watchCallbacksHolder.callCallback(%d, %d);",
            pipe_id, base::SysUTF8ToNSString(json_result), callback_id, result];
  } else {
    script = [NSString
        stringWithFormat:
            @"Mojo.internal.watchCallbacksHolder.callCallback(%d, %d);",
            callback_id, result];
  }

  auto callback = base::BindOnce(
      [](base::WeakPtr<MojoFacade> facade, int watch_id, MojoResult mojo_res,
         const base::Value*, NSError*) {
        if (facade && mojo_res == MOJO_RESULT_OK) {
          facade->ArmOnNotifyWatcher(watch_id);
        }
      },
      weak_ptr_factory_.GetWeakPtr(), watch_id, result);

  main_frame->ExecuteAsyncJavaScript(base::SysNSStringToUTF16(script),
                                     base::DictValue(), std::move(callback));
}

base::Value MojoFacade::HandleMojoHandleWatch(const base::DictValue& args) {
  std::optional<int> pipe_id = FindIntOrDoubleAsInt(args, "handle");
  CHECK(pipe_id.has_value());
  std::optional<int> signals = FindIntOrDoubleAsInt(args, "signals");
  CHECK(signals.has_value());
  std::optional<int> callback_id = FindIntOrDoubleAsInt(args, "callbackId");
  CHECK(callback_id.has_value());
  const int watch_id = ++last_watch_id_;

  // Note: base::Unretained() is safe because `this` owns all the watchers.
  auto callback = base::BindRepeating(&MojoFacade::OnWatcherCallback,
                                      base::Unretained(this), *callback_id,
                                      watch_id, *pipe_id);

  auto watcher = std::make_unique<mojo::SimpleWatcher>(
      FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL);

  mojo::MessagePipeHandle pipe = GetPipeFromId(*pipe_id);
  watcher->Watch(pipe, *signals, callback);
  watcher->ArmOrNotify();
  watchers_.insert(std::make_pair(watch_id, std::move(watcher)));
  return base::Value(watch_id);
}

void MojoFacade::HandleMojoWatcherCancel(const base::DictValue& args) {
  std::optional<int> watch_id = FindIntOrDoubleAsInt(args, "watchId");
  CHECK(watch_id.has_value());
  watchers_.erase(*watch_id);
}

int MojoFacade::AllocatePipeId(mojo::ScopedMessagePipeHandle pipe,
                               std::optional<int> custom_id) {
  int pipe_id = custom_id.value_or(next_pipe_id_--);
  pipes_[pipe_id] = std::move(pipe);
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

void MojoFacade::WebFrameBecameAvailable(WebFramesManager* web_frames_manager,
                                         WebFrame* web_frame) {
  DCHECK_CURRENTLY_ON(WebThread::UI);
  if (web_frame && web_frame->IsMainFrame()) {
    main_frame_ = web_frame->AsWeakPtr();
  }
}

void MojoFacade::WebFrameBecameUnavailable(WebFramesManager* web_frames_manager,
                                           const std::string& frame_id) {
  DCHECK_CURRENTLY_ON(WebThread::UI);
  if (main_frame_ && main_frame_->GetFrameId() == frame_id) {
    main_frame_.reset();
    pipes_.clear();
    watchers_.clear();
    is_awaiting_message_ = false;
    weak_ptr_factory_.InvalidateWeakPtrs();
  }
}

void MojoFacade::PageLoaded(WebState* web_state,
                            PageLoadCompletionStatus load_completion_status) {
  DCHECK_CURRENTLY_ON(WebThread::UI);
  DCHECK_EQ(web_state_, web_state);
  if (load_completion_status == PageLoadCompletionStatus::SUCCESS &&
      GetMainWebFrame() &&
      web_state_->GetInterfaceBinderForMainFrame()->HasRegisteredInterfaces()) {
    pipes_.clear();
    watchers_.clear();
    last_watch_id_ = 0;
    is_awaiting_message_ = false;
    weak_ptr_factory_.InvalidateWeakPtrs();
    AwaitNextMessage();
  }
}

void MojoFacade::WebStateDestroyed(WebState* web_state) {
  DCHECK_CURRENTLY_ON(WebThread::UI);
  DCHECK_EQ(web_state_, web_state);

  main_frame_.reset();
  pipes_.clear();
  watchers_.clear();
  is_awaiting_message_ = false;
  weak_ptr_factory_.InvalidateWeakPtrs();
  web_state_->GetPageWorldWebFramesManager()->RemoveObserver(this);
  web_state_->RemoveObserver(this);
  web_state_ = nullptr;
}
}  // namespace web
