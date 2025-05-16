// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/app/ios/appex/child_process_bridge.h"

#include <pthread.h>
#include <xpc/xpc.h>

#include "base/apple/bundle_locations.h"
#include "base/apple/mach_port_rendezvous_ios.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/system/sys_info.h"
#include "content/app/ios/appex/child_process_sandbox.h"
#include "gpu/ipc/common/ios/be_layer_hierarchy_transport.h"
#include "sandbox/policy/switches.h"

class GPUProcessTransport;

// Leaked variables for now.
static size_t g_argc = 0;
static const char** g_argv = nullptr;
static pthread_t g_main_thread;
static id<ChildProcessExtension> g_swift_process;
static xpc_connection_t g_connection;
static std::unique_ptr<GPUProcessTransport> g_gpu_transport;

#define IOS_INIT_EXPORT __attribute__((visibility("default")))

// The embedder must implement this.
extern "C" int ChildProcessMain(int argc, const char** argv);

class GPUProcessTransport : public gpu::BELayerHierarchyTransport {
 public:
  GPUProcessTransport() { gpu::BELayerHierarchyTransport::SetInstance(this); }
  ~GPUProcessTransport() override {
    gpu::BELayerHierarchyTransport::SetInstance(nullptr);
  }

  void ForwardBELayerHierarchyToBrowser(
      gpu::SurfaceHandle surface_handle,
      xpc_object_t ipc_representation) override {
    xpc_object_t message = xpc_dictionary_create(nil, nil, 0);
    xpc_dictionary_set_string(message, "message", "layerHandle");
    xpc_dictionary_set_value(message, "layer", ipc_representation);
    xpc_dictionary_set_uint64(message, "handle", surface_handle);
    xpc_connection_send_message(g_connection, message);
  }
};

extern "C" IOS_INIT_EXPORT void GpuProcessInit() {
  g_gpu_transport = std::make_unique<GPUProcessTransport>();
}

extern "C" IOS_INIT_EXPORT void ChildProcessInit(
    id<ChildProcessExtension> process) {
  // Up two levels: chrome.app/Extensions/chrome_content_process.appex
  NSBundle* bundle = [NSBundle bundleWithURL:[[[NSBundle mainBundle].bundleURL
                                               URLByDeletingLastPathComponent]
                                              URLByDeletingLastPathComponent]];
  base::apple::SetOverrideFrameworkBundle(bundle);
  g_swift_process = process;
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

    // Setup stdout/stderr.
    int fd = xpc_dictionary_dup_fd(msg, "stdout");
    if (fd != -1) {
      dup2(fd, STDOUT_FILENO);
      close(fd);
    }
    fd = xpc_dictionary_dup_fd(msg, "stderr");
    if (fd != -1) {
      dup2(fd, STDERR_FILENO);
      close(fd);
    }

    // See child_process_launcher_helper_ios.mm for discussion of this
    // bookmark data.
    size_t tmp_dir_length = 0;
    const void* tmp_dir =
        xpc_dictionary_get_data(msg, "tmp_dir", &tmp_dir_length);
    CHECK(tmp_dir);
    NSData* bookmark_temp_dir = [NSData dataWithBytes:tmp_dir
                                               length:tmp_dir_length];
    BOOL bookmarkIsStale = NO;
    NSError* error = nil;
    NSURL* tmp_dir_url =
        [NSURL URLByResolvingBookmarkData:bookmark_temp_dir
                                  options:NSURLBookmarkResolutionWithoutUI
                            relativeToURL:nil
                      bookmarkDataIsStale:&bookmarkIsStale
                                    error:&error];
    CHECK(tmp_dir_url);
    std::string file_path = base::SysNSStringToUTF8(tmp_dir_url.path) + "/";
    setenv("TMPDIR", file_path.c_str(), 1);

    base::FilePath assigned_path;
    CHECK(base::GetTempDir(&assigned_path));
    CHECK(assigned_path.value() == file_path);

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
  g_connection = connection;
}

namespace content {

void ChildProcessEnterSandbox() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          sandbox::policy::switches::kNoSandbox)) {
    return;
  }

  base::SysInfo::IsLowEndDevice();

  // Request the local time before entering the sandbox since that causes a
  // crash after the sandbox is entered.
  base::Time::Now().LocalMidnight();

  [g_swift_process applySandbox];
}

}  // namespace content
