// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/common/test/test_component_crash_observer.h"

#include <lib/sys/cpp/component_context.h>
#include <zircon/status.h>
#include <zircon/syscalls/object.h>

#include <string>
#include <vector>

#include "base/fuchsia/process_context.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace test {
namespace {

std::string DescribeStopped(const fuchsia::component::StoppedPayload& stopped) {
  std::string status_str = stopped.has_status()
                               ? zx_status_get_string(stopped.status())
                               : "<missing>";
  std::string exit_code_str = stopped.has_exit_code()
                                  ? base::NumberToString(stopped.exit_code())
                                  : "<missing>";
  return base::StrCat({"status=", status_str, ", exit_code=", exit_code_str});
}

// Returns true if the component termination is normal/clean (e.g. clean exit or
// killed as part of normal teardown).
bool IsNormalTermination(const fuchsia::component::StoppedPayload& stopped) {
  if (!stopped.has_status()) {
    return false;
  }

  zx_status_t status = stopped.status();

  // Components killed via zx_task_kill() during normal realm or component
  // teardown report ZX_TASK_RETCODE_SYSCALL_KILL (-1024).
  if (status == static_cast<zx_status_t>(ZX_TASK_RETCODE_SYSCALL_KILL)) {
    return true;
  }

  if (stopped.has_exit_code()) {
    int64_t exit_code = stopped.exit_code();
    if (exit_code == ZX_TASK_RETCODE_SYSCALL_KILL) {
      return true;
    }
    if (exit_code != 0) {
      return false;
    }
  }

  return (status == ZX_OK || status == ZX_ERR_PEER_CLOSED);
}

}  // namespace

TestComponentCrashObserver::TestComponentCrashObserver() {
  Connect();
}

TestComponentCrashObserver::~TestComponentCrashObserver() = default;

void TestComponentCrashObserver::Connect() {
  auto* context = base::ComponentContextForProcess();
  if (!context) {
    return;
  }

  zx_status_t status = context->svc()->Connect(event_stream_.NewRequest());
  if (status != ZX_OK) {
    LOG(WARNING) << "Failed to connect to fuchsia.component.EventStream: "
                 << zx_status_get_string(status);
    return;
  }

  event_stream_.set_error_handler([](zx_status_t status) {
    LOG(WARNING) << "EventStream channel closed: "
                 << zx_status_get_string(status);
  });

  event_stream_->WaitForReady([this]() { ListenNext(); });
}

void TestComponentCrashObserver::ListenNext() {
  if (!event_stream_) {
    return;
  }

  event_stream_->GetNext([this](std::vector<fuchsia::component::Event> events) {
    OnEvents(std::move(events));
  });
}

void TestComponentCrashObserver::OnEvents(
    std::vector<fuchsia::component::Event> events) {
  for (const auto& event : events) {
    if (!event.has_header() ||
        event.header().event_type() != fuchsia::component::EventType::STOPPED) {
      continue;
    }
    if (!event.has_payload() || !event.payload().is_stopped()) {
      continue;
    }

    std::string moniker =
        event.header().has_moniker() ? event.header().moniker() : "<unknown>";
    const auto& stopped = event.payload().stopped();

    EXPECT_TRUE(IsNormalTermination(stopped))
        << "Component '" << moniker
        << "' terminated abnormally: " << DescribeStopped(stopped);
  }

  ListenNext();
}

void TestComponentCrashObserver::VerifyNoCrashes() {
  base::RunLoop().RunUntilIdle();
}

}  // namespace test
