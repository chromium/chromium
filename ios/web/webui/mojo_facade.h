// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEBUI_MOJO_FACADE_H_
#define IOS_WEB_WEBUI_MOJO_FACADE_H_

#include <map>
#include <memory>
#include <string>

#include "base/callback.h"
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
  // |web_state|, which cannot be null.
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
    base::Value args;
  };

  // Extracts message name and arguments from the given JSON string obtained
  // from WebUI page. This method either succeeds or crashes the app (this
  // matches other platforms where Mojo API is strict on malformed input).
  MessageNameAndArguments GetMessageNameAndArguments(
      const std::string& mojo_message_as_json);

  // Connects to specified Mojo interface. |args| is a dictionary with the
  // following keys:
  //   - "interfaceName" (a string representing an interface name);
  //   - "requestHandle" (a number representing MojoHandle of the interface
  //     request).
  void HandleMojoBindInterface(base::Value args);

  // Closes the given handle. |args| is a dictionary which must contain "handle"
  // key, which is a number representing a MojoHandle.
  void HandleMojoHandleClose(base::Value args);

  // Creates a Mojo message pipe. |args| is unused.
  // Returns a dictionary with the following keys:
  //   - "result" (a number representing MojoResult);
  //   - "handle0" and "handle1" (the numbers representing two endpoints of the
  //     message pipe).
  base::Value HandleMojoCreateMessagePipe(base::Value args);

  // Writes a message to the message pipe endpoint given by handle. |args| is a
  // dictionary which must contain the following keys:
  //   - "handle" (a number representing MojoHandle, the endpoint to write to);
  //   - "buffer" (a dictionary representing the message data; may be empty);
  //   - "handles" (an array representing any handles to attach; handles are
  //     transferred and will no longer be valid; may be empty);
  // Returns MojoResult as a number.
  base::Value HandleMojoHandleWriteMessage(base::Value args);

  // Reads a message from the message pipe endpoint given by handle. |args| is
  // a dictionary which must contain the keys "handle" (a number representing
  // MojoHandle, the endpoint to read from).
  // Returns a dictionary with the following keys:
  //   - "result" (a number representing MojoResult);
  //   - "buffer" (an array representing message data; non-empty only on
  //     success);
  //   - "handles" (an array representing MojoHandles received, if any);
  base::Value HandleMojoHandleReadMessage(base::Value args);

  // Begins watching a handle for signals to be satisfied or unsatisfiable.
  // |args| is a dictionary which must contain the following keys:
  //   - "handle" (a number representing a MojoHandle);
  //   - "signals" (a number representing MojoHandleSignals to watch);
  //   - "callbackId" (a number representing the id which should be passed to
  //     Mojo.internal.signalWatch call).
  // Returns watch id as a number.
  base::Value HandleMojoHandleWatch(base::Value args);

  // Cancels a handle watch initiated by "MojoHandle.watch". |args| is a
  // dictionary which must contain "watchId" key (a number representing id
  // returned from "MojoHandle.watch").
  void HandleMojoWatcherCancel(base::Value args);

  // Runs JavaScript on WebUI page.
  WebState* web_state_ = nil;
  //  __weak id<CRWJSInjectionEvaluator> script_evaluator_ = nil;
  // Id of the last created watch.
  int last_watch_id_ = 0;
  // Currently active watches created through this facade.
  std::map<int, std::unique_ptr<mojo::SimpleWatcher>> watchers_;
};

}  // web

#endif  // IOS_WEB_WEBUI_MOJO_FACADE_H_
