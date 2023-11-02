// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/etw_trace_controller.h"

#include <stdint.h>
#include <memory>

#include "base/check_op.h"
#include "base/logging.h"
#include "base/sequence_checker.h"
#include "remoting/base/logging.h"

namespace remoting {

namespace {

constexpr wchar_t kSessionName[] = L"chrome_remote_desktop_host_logger";
constexpr uint8_t kDefaultTracingLevel = 4;
constexpr uint32_t kDefaultTracingFlags = 0;
constexpr size_t kDefaultBufferSizeKb = 16;

}  // namespace

EtwTraceController* EtwTraceController::instance_ = nullptr;

EtwTraceController::EtwTraceController() = default;

EtwTraceController::~EtwTraceController() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Stop();
}

const wchar_t* EtwTraceController::GetActiveSessionName() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return (instance_ == this) ? kSessionName : nullptr;
}

bool EtwTraceController::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(instance_, nullptr);
  instance_ = this;

  // The shared Chromium event tracing class registers itself as a 'classic'
  // provider which only supports one session so stop any existing sessions now.
  // More info on ETW provider registration:
  // https://docs.microsoft.com/en-us/windows/win32/etw/about-event-tracing#mof-classic-providers
  base::win::EtwTraceProperties ignore;
  HRESULT hr = base::win::EtwTraceController::Stop(kSessionName, &ignore);
  LOG_IF(ERROR,
         FAILED(hr) && hr != HRESULT_FROM_WIN32(ERROR_WMI_INSTANCE_NOT_FOUND))
      << "Failed to stop previous trace session: 0x" << std::hex << hr;

  hr = controller_.StartRealtimeSession(kSessionName, kDefaultBufferSizeKb);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to start ETW realtime session: 0x" << std::hex << hr;
    return false;
  }

  hr = controller_.EnableProvider(kRemotingHostLogProviderGuid,
                                  kDefaultTracingLevel, kDefaultTracingFlags);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to configure ETW provider: 0x" << std::hex << hr;
    controller_.Stop(nullptr);
    return false;
  }

  return true;
}

void EtwTraceController::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (instance_ != this) {
    DCHECK_EQ(instance_, nullptr);
    return;
  }

  // The following are all best effort, so try to stop tracing and clean up but
  // don't skip any steps if a previous one failed.
  HRESULT hr = controller_.DisableProvider(kRemotingHostLogProviderGuid);
  LOG_IF(ERROR, FAILED(hr))
      << "Failed to disable ETW provider: 0x" << std::hex << hr;

  hr = controller_.Flush(nullptr);
  LOG_IF(ERROR, FAILED(hr)) << "Failed to flush events: 0x" << std::hex << hr;

  hr = controller_.Stop(nullptr);
  LOG_IF(ERROR, FAILED(hr))
      << "Failed to stop ETW trace session: 0x" << std::hex << hr;

  instance_ = nullptr;
}

}  // namespace remoting
