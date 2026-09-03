// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/common/test/test_realm_root.h"

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
#include "base/rand_util.h"
#include "base/run_loop.h"
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

TestRealmRoot::TestRealmRoot(::component_testing::RealmBuilder realm_builder) {
  const std::string child_name =
      base::StrCat({"auto-", base::NumberToString(base::RandUint64())});
  realm_builder.SetRealmName(child_name);
  realm_prefix_ = base::StrCat({"realm_builder:", child_name, "/"});

  Connect();
  realm_root_ = realm_builder.Build();
}

TestRealmRoot::~TestRealmRoot() {
  Teardown();
}

void TestRealmRoot::Connect() {
  auto* const context = base::ComponentContextForProcess();
  CHECK(context);

  // Connect synchronously and block on WaitForReady() to ensure that the
  // EventStream subscription has been processed by Component Manager before
  // any components in the realm are started.
  fuchsia::component::EventStreamSyncPtr sync_event_stream;
  const zx_status_t status =
      context->svc()->Connect(sync_event_stream.NewRequest());
  ZX_CHECK(status == ZX_OK, status)
      << "Failed to connect to fuchsia.component.EventStream";

  const zx_status_t wait_status = sync_event_stream->WaitForReady();
  ZX_CHECK(wait_status == ZX_OK, wait_status)
      << "Failed to wait for EventStream to be ready";

  event_stream_.Bind(sync_event_stream.Unbind());
  event_stream_.set_error_handler([](zx_status_t status) {
    ZX_LOG(FATAL, status) << "EventStream channel closed unexpectedly";
  });

  ListenNext();
}

void TestRealmRoot::ListenNext() {
  if (!event_stream_) {
    return;
  }

  event_stream_->GetNext([this](std::vector<fuchsia::component::Event> events) {
    OnEvents(std::move(events));
  });
}

void TestRealmRoot::ExpectAbnormalTermination(
    std::string_view relative_moniker) {
  expected_abnormal_terminations_.insert(
      base::StrCat({realm_prefix_, relative_moniker}));
}

void TestRealmRoot::OnEvents(std::vector<fuchsia::component::Event> events) {
  for (const auto& event : events) {
    CHECK(event.has_header());
    CHECK(event.header().has_moniker());
    CHECK(event.header().has_event_type());

    const std::string& moniker = event.header().moniker();
    const auto event_type = event.header().event_type();

    // Ignore events that do not belong to components inside this realm.
    if (realm_prefix_.empty() || !base::StartsWith(moniker, realm_prefix_)) {
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

void TestRealmRoot::Teardown() {
  if (!realm_root_.has_value()) {
    return;
  }

  base::RunLoop teardown_loop;
  realm_root_->Teardown(
      [quit = teardown_loop.QuitClosure()](auto result) { quit.Run(); });
  teardown_loop.Run();
  realm_root_.reset();

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
