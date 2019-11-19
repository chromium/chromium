// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_TEST_GLOBALS_H_
#define PPAPI_SHARED_IMPL_TEST_GLOBALS_H_

#include <stdint.h>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "ppapi/shared_impl/callback_tracker.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/resource_tracker.h"
#include "ppapi/shared_impl/var_tracker.h"

namespace ppapi {

class TestVarTracker : public VarTracker {
 public:
  TestVarTracker() : VarTracker(THREAD_SAFE) {}
  ~TestVarTracker() override {}
  PP_Var MakeResourcePPVarFromMessage(
      PP_Instance instance,
      const IPC::Message& creation_message,
      int pending_renderer_id,
      int pending_browser_id) override {
    return PP_MakeNull();
  }
  ResourceVar* MakeResourceVar(PP_Resource pp_resource) override {
    return NULL;
  }
  ArrayBufferVar* CreateArrayBuffer(uint32_t size_in_bytes) override {
    return NULL;
  }
  ArrayBufferVar* CreateShmArrayBuffer(
      uint32_t size_in_bytes,
      base::UnsafeSharedMemoryRegion region) override {
    return NULL;
  }
  void DidDeleteInstance(PP_Instance instance) override {}
  int TrackSharedMemoryRegion(PP_Instance instance,
                              base::UnsafeSharedMemoryRegion region,
                              uint32_t size_in_bytes) override {
    return -1;
  }
  bool StopTrackingSharedMemoryRegion(int id,
                                      PP_Instance instance,
                                      base::UnsafeSharedMemoryRegion* region,
                                      uint32_t* size_in_bytes) override {
    return false;
  }
};

// Implementation of PpapiGlobals for tests that don't need either the host- or
// plugin-specific implementations.
class TestGlobals : public PpapiGlobals {
 public:
  TestGlobals();
  explicit TestGlobals(PpapiGlobals::PerThreadForTest);
  ~TestGlobals() override;

  // PpapiGlobals implementation.
  ResourceTracker* GetResourceTracker() override;
  VarTracker* GetVarTracker() override;
  CallbackTracker* GetCallbackTrackerForInstance(PP_Instance instance) override;
  thunk::PPB_Instance_API* GetInstanceAPI(PP_Instance instance) override;
  thunk::ResourceCreationAPI* GetResourceCreationAPI(
      PP_Instance instance) override;
  PP_Module GetModuleForInstance(PP_Instance instance) override;
  std::string GetCmdLine() override;
  void PreCacheFontForFlash(const void* logfontw) override;
  void LogWithSource(PP_Instance instance,
                     PP_LogLevel level,
                     const std::string& source,
                     const std::string& value) override;
  void BroadcastLogWithSource(PP_Module module,
                              PP_LogLevel level,
                              const std::string& source,
                              const std::string& value) override;
  MessageLoopShared* GetCurrentMessageLoop() override;
  base::TaskRunner* GetFileTaskRunner() override;

  // PpapiGlobals overrides:
  bool IsHostGlobals() const override;

 private:
  ResourceTracker resource_tracker_;
  TestVarTracker var_tracker_;
  scoped_refptr<CallbackTracker> callback_tracker_;

  DISALLOW_COPY_AND_ASSIGN(TestGlobals);
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_TEST_GLOBALS_H_
