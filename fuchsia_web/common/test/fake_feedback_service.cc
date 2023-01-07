// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/common/test/fake_feedback_service.h"

#include <lib/fidl/cpp/interface_request.h>
#include <lib/sys/component/cpp/testing/realm_builder.h>
#include <lib/sys/cpp/outgoing_directory.h>

#include <utility>

#include "fuchsia_web/common/test/test_realm_support.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::component_testing::ChildRef;
using ::component_testing::Protocol;
using ::component_testing::Route;

namespace test {

FakeFeedbackService::FakeFeedbackService(
    ::component_testing::RealmBuilder& realm_builder,
    std::string_view child_name) {
  static constexpr char kFeedbackService[] = "fake_feedback";
  realm_builder.AddLocalChild(kFeedbackService, this);
  realm_builder.AddRoute(Route{
      .capabilities =
          {Protocol{fuchsia::feedback::ComponentDataRegister::Name_},
           Protocol{fuchsia::feedback::CrashReportingProductRegister::Name_}},
      .source = ChildRef{kFeedbackService},
      .targets = {ChildRef{child_name}}});
}

FakeFeedbackService::~FakeFeedbackService() = default;

void FakeFeedbackService::Start(
    std::unique_ptr<::component_testing::LocalComponentHandles> mock_handles) {
  handles_ = std::move(mock_handles);
  ASSERT_EQ(handles_->outgoing()->AddPublicService(
                component_data_register_bindings_.GetHandler(this)),
            ZX_OK);
  ASSERT_EQ(handles_->outgoing()->AddPublicService(
                crash_reporting_product_register_bindings_.GetHandler(this)),
            ZX_OK);
}

void FakeFeedbackService::Upsert(::fuchsia::feedback::ComponentData data,
                                 UpsertCallback callback) {
  callback();
}

void FakeFeedbackService::Upsert(
    ::std::string component_url,
    ::fuchsia::feedback::CrashReportingProduct product) {}

void FakeFeedbackService::UpsertWithAck(
    ::std::string component_url,
    ::fuchsia::feedback::CrashReportingProduct product,
    UpsertWithAckCallback callback) {
  callback();
}

}  // namespace test
