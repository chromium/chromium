// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/common/test/test_component_crash_observer.h"

#include <lib/sys/cpp/component_context.h>
#include <zircon/status.h>
#include <zircon/syscalls/object.h>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace test {
namespace {

// Returns true if `moniker` refers to a dynamic component or a descendant of
// one. Dynamic components are created within collections, so their top-level
// moniker segment contains a collection prefix separated by a colon
// (e.g. "collection:child_name").
bool IsDynamicComponent(std::string_view moniker) {
  return moniker.substr(0, moniker.find('/')).find(':') !=
         std::string_view::npos;
}

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

TestComponentCrashObserver::~TestComponentCrashObserver() {
  VerifyNoCrashes();
}

void TestComponentCrashObserver::Connect() {
  auto* const context = base::ComponentContextForProcess();
  CHECK(context);

  const zx_status_t status =
      context->svc()->Connect(event_stream_.NewRequest());
  ZX_CHECK(status == ZX_OK, status)
      << "Failed to connect to fuchsia.component.EventStream";

  event_stream_.set_error_handler([](zx_status_t status) {
    ZX_LOG(FATAL, status) << "EventStream channel closed unexpectedly";
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

void TestComponentCrashObserver::ExpectAbnormalTermination(
    std::string moniker) {
  CHECK(IsDynamicComponent(moniker));
  expected_abnormal_terminations_.insert(std::move(moniker));
}

void TestComponentCrashObserver::OnEvents(
    std::vector<fuchsia::component::Event> events) {
  for (const auto& event : events) {
    CHECK(event.has_header());
    CHECK(event.header().has_moniker());
    CHECK(event.header().has_event_type());

    const std::string& moniker = event.header().moniker();
    const auto event_type = event.header().event_type();

    if (!IsDynamicComponent(moniker)) {
      if (event_type == fuchsia::component::EventType::STOPPED) {
        CHECK(event.has_payload() && event.payload().is_stopped());
        const auto& stopped = event.payload().stopped();
        CHECK(IsNormalTermination(stopped))
            << "Static component '" << moniker
            << "' terminated unexpectedly: " << DescribeStopped(stopped);
      }
      continue;
    }

    if (event_type == fuchsia::component::EventType::STARTED) {
      running_components_.insert(moniker);
      continue;
    }

    if (event_type == fuchsia::component::EventType::STOPPED) {
      if (running_components_.erase(moniker) == 0) {
        ADD_FAILURE() << "Observed stop event for unexpected component '"
                      << moniker << "'";
      }
      CHECK(event.has_payload() && event.payload().is_stopped());
      const auto& stopped = event.payload().stopped();
      const bool expect_normal_termination =
          expected_abnormal_terminations_.erase(moniker) == 0;
      const bool terminated_normally = IsNormalTermination(stopped);
      EXPECT_EQ(terminated_normally, expect_normal_termination)
          << "Component '" << moniker
          << "' terminated unexpectedly: " << DescribeStopped(stopped);
      continue;
    }

    LOG(FATAL) << "Observed unexpected event type ("
               << static_cast<int>(event_type) << ") for component '" << moniker
               << "'";
  }

  ListenNext();
}

void TestComponentCrashObserver::VerifyNoCrashes() {
  DCHECK(base::test::ScopedRunLoopTimeout::ExistsForCurrentThread());
  base::test::ScopedRunLoopTimeout timeout(
      FROM_HERE, std::nullopt, base::BindLambdaForTesting([this]() {
        return base::StrCat({"Still waiting for component(s) to stop:\n  ",
                             base::JoinString(running_components_, "\n  ")});
      }));

  EXPECT_TRUE(
      base::test::RunUntil([this]() { return running_components_.empty(); }));

  EXPECT_THAT(expected_abnormal_terminations_, ::testing::IsEmpty())
      << "Abnormal termination expected for non-existent component(s)";
}

}  // namespace test
