// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_MESSAGING_NATIVE_MESSAGE_HOST_H_
#define EXTENSIONS_BROWSER_API_MESSAGING_NATIVE_MESSAGE_HOST_H_

#include <memory>
#include <string>

#include "base/task/single_thread_task_runner.h"
#include "ui/gfx/native_widget_types.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {

// An interface for receiving messages from MessageService (Chrome) using the
// Native Messaging API.  A NativeMessageHost object hosts a native component,
// which can run in the browser-process or in a separate process (See
// NativeMessageProcessHost).
class NativeMessageHost {
 public:
  static const char kFailedToStartError[];
  static const char kInvalidNameError[];
  static const char kNativeHostExited[];
  static const char kNotFoundError[];
  static const char kForbiddenError[];
  static const char kHostInputOutputError[];

  // Callback interface for receiving messages from the native host.
  class Client {
   public:
    virtual ~Client() {}

    // Called on the UI thread.
    virtual void PostMessageFromNativeHost(const std::string& message) = 0;
    virtual void CloseChannel(const std::string& error_message) = 0;
  };

  // Creates the NativeMessageHost based on the |native_host_name|.
  static std::unique_ptr<NativeMessageHost> Create(
      content::BrowserContext* browser_context,
      gfx::NativeView native_view,
      const std::string& source_extension_id,
      const std::string& native_host_name,
      bool allow_user_level,
      std::string* error);

  virtual ~NativeMessageHost() {}

  // Called when a message is received from MessageService (Chrome).
  virtual void OnMessage(const std::string& message) = 0;

  // Sets the client to start receiving messages from the native host.
  virtual void Start(Client* client) = 0;

  // Returns the task runner that the host runs on. The Client should only
  // invoke OnMessage() on this task runner.
  virtual scoped_refptr<base::SingleThreadTaskRunner> task_runner() const = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_MESSAGING_NATIVE_MESSAGE_HOST_H_
