// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/libfuzzer/fuzzers/command_buffer_lpm_fuzzer/webgpu_support.h"
#include "testing/libfuzzer/fuzzers/command_buffer_lpm_fuzzer/cmd_buf_lpm_fuzz.h"

namespace gpu::cmdbuf::fuzzing {

DawnWireSerializerFuzzer::DawnWireSerializerFuzzer() = default;
DawnWireSerializerFuzzer::~DawnWireSerializerFuzzer() = default;

size_t DawnWireSerializerFuzzer::GetMaximumAllocationSize() const {
  // Some fuzzer bots have a 2GB allocation limit. Pick a value reasonably
  // below that.
  return 1024 * 1024 * 1024;
}

void* DawnWireSerializerFuzzer::GetCmdSpace(size_t size) {
  if (size > buf.size()) {
    buf.resize(size);
  }
  return buf.data();
}

bool DawnWireSerializerFuzzer::Flush() {
  return true;
}

void CmdBufFuzz::WebGPURequestAdapter() {
  DVLOG(3) << "Requesting WebGPU adapter...";
  wgpu::RequestAdapterOptions ra_options = {};
  ra_options.forceFallbackAdapter = false;
  bool done = false;
  webgpu_instance_.RequestAdapter(
      &ra_options, wgpu::CallbackMode::AllowSpontaneous,
      [&](wgpu::RequestAdapterStatus status, wgpu::Adapter adapter,
          const char* message) {
        CHECK_EQ(status, wgpu::RequestAdapterStatus::Success);
        CHECK_NE(adapter.Get(), nullptr);
        webgpu_adapter_ = std::move(adapter);
        DVLOG(3) << "Adapter acquired";
        done = true;
      });
  webgpu_impl_->FlushCommands();
  while (!done) {
    RunPendingTasks();
    base::PlatformThread::Sleep(kTinyTimeout);
  }
}

void CmdBufFuzz::WebGPURequestDevice() {
  DVLOG(3) << "Requesting WebGPU device...";
  bool done = false;
  wgpu::DeviceDescriptor device_desc = {};
  DCHECK(webgpu_adapter_);
  webgpu_adapter_.RequestDevice(&device_desc,
                                wgpu::CallbackMode::AllowSpontaneous,
                                [&](wgpu::RequestDeviceStatus status,
                                    wgpu::Device device, const char* message) {
                                  DVLOG(3) << "Attempting to acquire device";
                                  webgpu_device_ = std::move(device);
                                  DVLOG(3) << "Device acquired";
                                  done = true;
                                });
  webgpu()->FlushCommands();
  while (!done) {
    RunPendingTasks();
    base::PlatformThread::Sleep(kTinyTimeout);
  }

  webgpu_device_.SetDeviceLostCallback(
      [](WGPUDeviceLostReason reason, const char* message, void*) {
        if (message) {
          DVLOG(3) << "***** Device lost: " << message << " *****";
        }
        if (reason == WGPUDeviceLostReason_Destroyed) {
          return;
        }
        LOG(FATAL) << "Unexpected device lost (" << reason << "): " << message;
      },
      nullptr);
}

void CmdBufFuzz::WebGPUDestroyDevice() {
  DVLOG(3) << "Destroying device";
  webgpu_device_.Destroy();
  webgpu()->FlushCommands();
  WaitForCompletion(webgpu_instance_, webgpu_device_);
  DVLOG(3) << "Device destroyed? (see DeviceLostCallback log)";
}

void CmdBufFuzz::WebGPUCreateBuffer() {
  DVLOG(3) << "Creating WebGPU buffer";
  wgpu::BufferDescriptor buffer_desc;
  buffer_desc.size = 4;
  buffer_desc.usage = wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst;
  wgpu::Buffer buff = webgpu_device_.CreateBuffer(&buffer_desc);
  webgpu()->FlushCommands();
  WaitForCompletion(webgpu_instance_, webgpu_device_);
  wgpu_buffers_.push_back(std::move(buff));
  DVLOG(3) << "Created WebGPU buffer";
}

void CmdBufFuzz::WebGPUDestroyBuffer() {
  DVLOG(3) << "Destroying WebGPU buffer";
  for (wgpu::Buffer buff : wgpu_buffers_) {
    buff.Destroy();
  }
  wgpu_buffers_.clear();
  DVLOG(3) << "Destroyed WebGPU buffer";
  return;
}

void CmdBufFuzz::WebGPUReset() {
  // Let in-flight work finish.
  PollUntilIdle();
  // TODO: release & destroy buffer(s), device(s) & adapter(s).
}

}  // namespace gpu::cmdbuf::fuzzing
