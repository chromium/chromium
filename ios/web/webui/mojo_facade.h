// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEBUI_MOJO_FACADE_H_
#define IOS_WEB_WEBUI_MOJO_FACADE_H_

#include <map>
#include <memory>
#include <string>
#include <unordered_map>

#include "base/functional/callback.h"
#import "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"

namespace web {

class WebState;

// Facade class for Mojo. All inputs and outputs are optimized for communication
// with WebUI pages and hence use JSON format. Must be created used and
// destroyed on UI thread.
class MojoFacade {
 public:
  // Constructs MojoFacade. The calling code must retain ownership of
  // `web_state`, which cannot be null.
  explicit MojoFacade(WebState* web_state);
  ~MojoFacade();

  // Handles Mojo message received from WebUI page. Returns a valid JSON string
  // on success or empty string if supplied JSON does not have required
  // structure. Every message must have "name" and "args" keys, where "name" is
  // a string representing the name of Mojo message and "args" is a dictionary
  // with arguments specific for each message name.
  // Supported message names with their handler methods in parenthesis:
  //   Mojo.bindInterface (HandleMojoBindInterface)
  //   MojoHandle.close (HandleMojoHandleClose)
  //   Mojo.createMessagePipe (HandleMojoCreateMessagePipe)
  //   MojoHandle.writeMessage (HandleMojoHandleWriteMessage)
  //   MojoHandle.readMessage (HandleMojoHandleReadMessage)
  //   MojoHandle.watch (HandleMojoHandleWatch)
  //   MojoWatcher.cancel (HandleMojoWatcherCancel)
  std::string HandleMojoMessage(const std::string& mojo_message_as_json);

 private:
  // Value returned by GetMessageNameAndArguments.
  struct MessageNameAndArguments {
    std::string name;
    base::Value::Dict args;
  };

  // Extracts message name and arguments from the given JSON string obtained
  // from WebUI page. This method either succeeds or crashes the app (this
  // matches other platforms where Mojo API is strict on malformed input).
  MessageNameAndArguments GetMessageNameAndArguments(
      const std::string& mojo_message_as_json);

  // Connects to specified Mojo interface. `args` is a dictionary with the
  // following keys:
  //   - "interfaceName" (a string representing an interface name);
  //   - "requestHandle" (a number representing MojoHandle of the interface
  //     request).
  void HandleMojoBindInterface(base::Value::Dict args);

  // Closes the given handle. `args` is a dictionary which must contain "handle"
  // key, which is a number representing a MojoHandle.
  void HandleMojoHandleClose(base::Value::Dict args);

  // Creates a Mojo message pipe. `args` is unused.
  // Returns a dictionary with the following keys:
  //   - "result" (a number representing MojoResult);
  //   - "handle0" and "handle1" (the numbers representing two endpoints of the
  //     message pipe).
  base::Value HandleMojoCreateMessagePipe(base::Value::Dict args);

  // Writes a message to the message pipe endpoint given by handle. `args` is a
  // dictionary which must contain the following keys:
  //   - "handle" (a number representing MojoHandle, the endpoint to write to);
  //   - "buffer" (a base-64 string representing the message data; may be
  //   empty);
  //   - "handles" (an array representing any handles to attach; handles are
  //     transferred and will no longer be valid; may be empty);
  // Returns MojoResult as a number.
  base::Value HandleMojoHandleWriteMessage(base::Value::Dict args);

  // Reads a message from the message pipe endpoint given by handle. `args` is
  // a dictionary which must contain the keys "handle" (a number representing
  // MojoHandle, the endpoint to read from).
  // Returns a dictionary with the following keys:
  //   - "result" (a number representing MojoResult);
  //   - "buffer" (an array representing message data; non-empty only on
  //     success);
  //   - "handles" (an array representing MojoHandles received, if any);
  base::Value HandleMojoHandleReadMessage(base::Value::Dict args);

  // Begins watching a handle for signals to be satisfied or unsatisfiable.
  // `args` is a dictionary which must contain the following keys:
  //   - "handle" (a number representing a MojoHandle);
  //   - "signals" (a number representing MojoHandleSignals to watch);
  //   - "callbackId" (a number representing the id which should be passed to
  //     Mojo.internal.signalWatch call).
  // Returns watch id as a number.
  base::Value HandleMojoHandleWatch(base::Value::Dict args);

  // Cancels a handle watch initiated by "MojoHandle.watch". `args` is a
  // dictionary which must contain "watchId" key (a number representing id
  // returned from "MojoHandle.watch").
  void HandleMojoWatcherCancel(base::Value::Dict args);

  // Assigns a new unique integer ID to the given message pipe handle and
  // returns that ID. The ID can be used by JS to reference this pipe.
  int AllocatePipeId(mojo::ScopedMessagePipeHandle pipe);

  // SimpleWatcher callback which notifies us when a handle's watched signals
  // are raised. `callback_id` identifies the JS-side callback registered for
  // this watcher, and `watch_id` identifies the JS-side MojoWatcher responsible
  // for the event. This ultimately invokes the JS-side callback and then
  // re-arms the watcher once the JS has run.
  void OnWatcherCallback(int callback_id, int watch_id, MojoResult result);

  // Calls ArmOrNotify() for matching watcher.
  void ArmOnNotifyWatcher(int watch_id);

  // Returns the pipe handle associated with `id` in JS, or an invalid handle if
  // no such association exists.
  mojo::MessagePipeHandle GetPipeFromId(int id);

  // Returns the pipe handle associated with `id` in JS, and removes that
  // association, effectively invalidating `id` and taking ownership of the
  // pipe. Returns an invalid pipe if `id` had no associated pipe.
  mojo::ScopedMessagePipeHandle TakePipeFromId(int id);

  // Runs JavaScript on WebUI page.
  raw_ptr<WebState> web_state_ = nullptr;

  // The next available integer ID to assign a Mojo pipe for use in JS.
  int next_pipe_id_ = 1;

  // A mapping of integer handles used by JS, to actual pipe handles used with
  // Mojo APIs.
  std::unordered_map<int, mojo::ScopedMessagePipeHandle> pipes_;

  // Id of the last created watch.
  int last_watch_id_ = 0;

  // Currently active watches created through this facade.
  std::map<int, std::unique_ptr<mojo::SimpleWatcher>> watchers_;

  base::WeakPtrFactory<MojoFacade> weak_ptr_factory_{this};
};

}  // web

#endif  // IOS_WEB_WEBUI_MOJO_FACADE_H_
