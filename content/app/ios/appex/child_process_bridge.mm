// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include <pthread.h>
#include <xpc/xpc.h>

#include "base/apple/bundle_locations.h"
#include "base/apple/mach_port_rendezvous.h"
#include "base/check_op.h"
#include "base/logging.h"

// Leaked variables for now.
static size_t g_argc = 0;
static const char** g_argv = nullptr;
static pthread_t g_main_thread;

#define IOS_INIT_EXPORT __attribute__((visibility("default")))

// The embedder must implement this.
extern "C" int ChildProcessMain(int argc, const char** argv);

extern "C" IOS_INIT_EXPORT void ChildProcessInit() {
  // Up two levels: chrome.app/Extensions/chrome_content_process.appex
  NSBundle* bundle = [NSBundle bundleWithURL:[[[NSBundle mainBundle].bundleURL
                                               URLByDeletingLastPathComponent]
                                              URLByDeletingLastPathComponent]];
  base::apple::SetOverrideFrameworkBundle(bundle);
}

void* RunMain(void* data) {
  ChildProcessMain((int)g_argc, g_argv);
  return nullptr;
}

extern "C" IOS_INIT_EXPORT void ChildProcessHandleNewConnection(
                                 xpc_connection_t connection) {
  xpc_connection_set_event_handler(connection, ^(xpc_object_t msg) {
    xpc_type_t msg_type = xpc_get_type(msg);
    CHECK_EQ(msg_type, XPC_TYPE_DICTIONARY);
    xpc_object_t args_array = xpc_dictionary_get_array(msg, "args");
    g_argc = xpc_array_get_count(args_array);
    g_argv = new const char*[g_argc];
    for (size_t i = 0; i < g_argc; ++i) {
      g_argv[i] = strdup(xpc_array_get_string(args_array, i));
    }

    mach_port_t port = xpc_dictionary_copy_mach_send(msg, "port");
    base::apple::ScopedMachSendRight server_port(port);
    bool res =
        base::MachPortRendezvousClientIOS::Initialize(std::move(server_port));
    CHECK(res) << "MachPortRendezvousClient failed";
    // TODO(dtapuska): For now we create our own main thread, figure out if we
    // can use the ExtensionMain (thread 0) as the main thread but calling
    // CFRunLoopRunInMode seems to crash it so we can't enter a nested event
    // loop with some objects on the stack.
    pthread_create(&g_main_thread, NULL, RunMain, NULL);
  });
  xpc_connection_activate(connection);
}
