// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/common/test/test_realm_root.h"

#include <fuchsia/component/decl/cpp/fidl.h>
#include <lib/async/default.h>
#include <lib/sys/cpp/component_context.h>
#include <zircon/status.h>
#include <zircon/syscalls/object.h>

#include <cstdint>
#include <iomanip>
#include <iostream>
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

const char* LogSeverityToString(int32_t severity) {
  if (severity <=
      static_cast<int32_t>(fuchsia_logger::LogLevelFilter::kTrace)) {
    return "TRACE";
  }
  if (severity <=
      static_cast<int32_t>(fuchsia_logger::LogLevelFilter::kDebug)) {
    return "DEBUG";
  }
  if (severity <= static_cast<int32_t>(fuchsia_logger::LogLevelFilter::kInfo)) {
    return "INFO";
  }
  if (severity <= static_cast<int32_t>(fuchsia_logger::LogLevelFilter::kWarn)) {
    return "WARN";
  }
  if (severity <=
      static_cast<int32_t>(fuchsia_logger::LogLevelFilter::kError)) {
    return "ERROR";
  }
  return "FATAL";
}

}  // namespace

TestRealmRoot::TestRealmRoot(::component_testing::RealmBuilder realm_builder) {
  constexpr char kIsolatedArchivistName[] = "isolated_archivist";
  constexpr char kArchivistUrl[] =
      "fuchsia-pkg://fuchsia.com/archivist-for-embedding#meta/"
      "archivist-for-embedding.cm";

  realm_builder.AddChild(kIsolatedArchivistName, kArchivistUrl);

  realm_builder.AddRoute(::component_testing::Route{
      .capabilities = {::component_testing::Protocol{"fuchsia.logger.Log"}},
      .source = ::component_testing::ChildRef{kIsolatedArchivistName},
      .targets = {::component_testing::ParentRef{}}});

  auto realm_decl = realm_builder.GetRealmDecl();

  // Declare a diagnostics dictionary capability on the realm root.
  fuchsia::component::decl::Dictionary diagnostics_dict;
  diagnostics_dict.set_name("diagnostics");
  fuchsia::component::decl::Capability dict_cap;
  dict_cap.set_dictionary(std::move(diagnostics_dict));
  if (!realm_decl.has_capabilities()) {
    realm_decl.set_capabilities({});
  }
  realm_decl.mutable_capabilities()->push_back(std::move(dict_cap));

  // Route LogSink and InspectSink from isolated_archivist into the diagnostics
  // dictionary.
  auto make_protocol_offer_to_dict = [&](const char* protocol_name) {
    fuchsia::component::decl::OfferProtocol offer;
    offer.set_source(fuchsia::component::decl::Ref::WithChild(
        fuchsia::component::decl::ChildRef{.name = kIsolatedArchivistName}));
    offer.set_source_name(protocol_name);
    offer.set_target(fuchsia::component::decl::Ref::WithCapability(
        fuchsia::component::decl::CapabilityRef{.name = "diagnostics"}));
    offer.set_target_name(protocol_name);
    offer.set_dependency_type(fuchsia::component::decl::DependencyType::STRONG);
    offer.set_availability(fuchsia::component::decl::Availability::REQUIRED);
    fuchsia::component::decl::Offer offer_wrapper;
    offer_wrapper.set_protocol(std::move(offer));
    return offer_wrapper;
  };

  if (!realm_decl.has_offers()) {
    realm_decl.set_offers({});
  }
  realm_decl.mutable_offers()->push_back(
      make_protocol_offer_to_dict("fuchsia.logger.LogSink"));
  realm_decl.mutable_offers()->push_back(
      make_protocol_offer_to_dict("fuchsia.inspect.InspectSink"));

  // Offer the capability_requested event stream from parent to
  // isolated_archivist.
  fuchsia::component::decl::OfferEventStream offer_event_stream;
  offer_event_stream.set_source(fuchsia::component::decl::Ref::WithParent({}));
  offer_event_stream.set_source_name("capability_requested");
  offer_event_stream.set_target(fuchsia::component::decl::Ref::WithChild(
      fuchsia::component::decl::ChildRef{.name = kIsolatedArchivistName}));
  offer_event_stream.set_target_name("capability_requested");
  offer_event_stream.set_availability(
      fuchsia::component::decl::Availability::REQUIRED);

  fuchsia::component::decl::Offer offer_es_wrapper;
  offer_es_wrapper.set_event_stream(std::move(offer_event_stream));
  realm_decl.mutable_offers()->push_back(std::move(offer_es_wrapper));

  // Rewrite existing offers from parent:
  // - "diagnostics" dictionary offers from parent -> self
  // - "fuchsia.logger.LogSink" and "fuchsia.inspect.InspectSink" protocol
  //   offers from parent -> isolated_archivist
  for (auto& offer : *realm_decl.mutable_offers()) {
    if (offer.is_dictionary() && offer.dictionary().has_source() &&
        offer.dictionary().source().is_parent() &&
        offer.dictionary().has_source_name() &&
        offer.dictionary().source_name() == "diagnostics") {
      offer.dictionary().set_source(
          fuchsia::component::decl::Ref::WithSelf({}));
    } else if (offer.is_protocol() && offer.protocol().has_source() &&
               offer.protocol().source().is_parent() &&
               offer.protocol().has_source_name() &&
               (offer.protocol().source_name() == "fuchsia.logger.LogSink" ||
                offer.protocol().source_name() ==
                    "fuchsia.inspect.InspectSink")) {
      offer.protocol().set_source(fuchsia::component::decl::Ref::WithChild(
          fuchsia::component::decl::ChildRef{.name = kIsolatedArchivistName}));
    }
  }

  realm_builder.ReplaceRealmDecl(std::move(realm_decl));

  const std::string child_name =
      base::StrCat({"auto-", base::NumberToString(base::RandUint64())});
  realm_builder.SetRealmName(child_name);
  realm_prefix_ = base::StrCat({"realm_builder:", child_name, "/"});

  Connect();
  realm_root_ = realm_builder.Build();

  auto log_client_end = realm_root_->component().Connect<fuchsia_logger::Log>();
  ZX_CHECK(log_client_end.is_ok(), log_client_end.status_value())
      << "Failed to connect to fuchsia.logger.Log";
  log_client_.Bind(std::move(log_client_end.value()),
                   async_get_default_dispatcher());

  log_listener_.set_on_log_message(base::BindRepeating(
      &TestRealmRoot::PrintLogMessage, base::Unretained(this)));

  auto listener_endpoints =
      fidl::CreateEndpoints<fuchsia_logger::LogListenerSafe>();
  ZX_CHECK(listener_endpoints.is_ok(), listener_endpoints.status_value())
      << "Failed to create LogListenerSafe endpoints";
  log_listener_binding_.emplace(
      async_get_default_dispatcher(), std::move(listener_endpoints->server),
      &log_listener_, [](fidl::UnbindInfo info) {
        if (info.status() != ZX_ERR_PEER_CLOSED) {
          ZX_DLOG(WARNING, info.status()) << "LogListenerSafe unbound";
        }
      });

  auto listen_result = log_client_->ListenSafe(
      {{.log_listener = std::move(listener_endpoints->client)}});
  if (listen_result.is_error()) {
    ZX_DLOG(ERROR, listen_result.error_value().status()) << "ListenSafe failed";
  }
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

  log_listener_binding_.reset();
  if (log_client_.is_valid()) {
    log_client_ = {};
  }

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

void TestRealmRoot::PrintLogMessage(const fuchsia_logger::LogMessage& message) {
  std::string tags_str;
  if (!message.tags().empty()) {
    tags_str =
        base::StrCat({"[", base::JoinString(message.tags(), ", "), "] "});
  }
  const double time_sec = static_cast<double>(message.time().get()) / 1e9;
  std::cerr << "[" << std::fixed << std::setprecision(3) << time_sec << "]["
            << message.pid() << "][" << message.tid() << "]" << tags_str
            << LogSeverityToString(message.severity()) << ": " << message.msg()
            << "\n"
            << std::flush;
}

}  // namespace test
