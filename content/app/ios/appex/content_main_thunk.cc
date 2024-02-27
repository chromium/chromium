// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <pthread.h>
#include <xpc/xpc.h>
#include "base/mac/mach_port_rendezvous.h"

// Leaked variables for now.
static size_t g_argc = 0;
static const char** g_argv = nullptr;
static pthread_t g_main_thread;

// The embedder must implement this.
extern "C" int ContentProcessMain(int argc, const char** argv);

extern "C" void ContentProcessInit() {}

void* RunMain(void* data) {
  ContentProcessMain((int)g_argc, g_argv);
  return nullptr;
}

extern "C" void ContentProcessHandleNewConnection(xpc_connection_t connection) {
  // TODO(dtapuska): For now we create our own main thread, figure out if we can
  // use the ExtensionMain (thread 0) as the main thread but calling
  // CFRunLoopRunInMode seems to crash it so we can't enter a nested event loop
  // with some objects on the stack.
  pthread_create(&g_main_thread, NULL, RunMain, NULL);
}
