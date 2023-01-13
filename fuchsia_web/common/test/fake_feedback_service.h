// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_COMMON_TEST_FAKE_FEEDBACK_SERVICE_H_
#define FUCHSIA_WEB_COMMON_TEST_FAKE_FEEDBACK_SERVICE_H_

#include <fuchsia/feedback/cpp/fidl.h>
#include <lib/fidl/cpp/binding_set.h>
#include <lib/sys/component/cpp/testing/realm_builder_types.h>

#include <string_view>

namespace component_testing {
class RealmBuilder;
}

namespace test {

// An implementation of ComponentDataRegister and CrashReportingProductRegister
// for use in tests that utilize RealmBuilder. All updates are silently accepted
// but ignored.
class FakeFeedbackService
    : public ::fuchsia::feedback::ComponentDataRegister,
      public ::fuchsia::feedback::CrashReportingProductRegister,
      public ::component_testing::LocalComponentImpl {
 public:
  FakeFeedbackService();
  FakeFeedbackService(const FakeFeedbackService&) = delete;
  FakeFeedbackService& operator=(const FakeFeedbackService&) = delete;
  ~FakeFeedbackService() override;

  // Registers a LocalComponentFactory function for the FakeFeedbackService with
  // RealmBuilder and plumbs its protocols to the peer component identified
  // by the given child_name. Note, each constructed instance of
  // FakeFeedbackService supports one RealmBuilder instance.
  static void RouteToChild(::component_testing::RealmBuilder& realm_builder,
                           std::string_view child_name);

  // ::component_testing::LocalComponentImpl:
  void OnStart() override;

  // ::fuchsia::feedback::ComponentDataRegister:
  void Upsert(::fuchsia::feedback::ComponentData data,
              UpsertCallback callback) override;

  // ::fuchsia::feedback::CrashReportingProductRegister:
  void Upsert(::std::string component_url,
              ::fuchsia::feedback::CrashReportingProduct product) override;
  void UpsertWithAck(::std::string component_url,
                     ::fuchsia::feedback::CrashReportingProduct product,
                     UpsertWithAckCallback callback) override;

 private:
  fidl::BindingSet<::fuchsia::feedback::ComponentDataRegister>
      component_data_register_bindings_;
  fidl::BindingSet<::fuchsia::feedback::CrashReportingProductRegister>
      crash_reporting_product_register_bindings_;
};

}  // namespace test

#endif  // FUCHSIA_WEB_COMMON_TEST_FAKE_FEEDBACK_SERVICE_H_
