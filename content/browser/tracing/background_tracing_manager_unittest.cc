// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/values.h"
#include "build/build_config.h"
#include "content/browser/tracing/background_tracing_config_impl.h"
#include "content/browser/tracing/background_tracing_manager_impl.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/network_change_notifier.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

class MockNetworkChangeNotifier : public net::NetworkChangeNotifier {
 public:
  ConnectionType GetCurrentConnectionType() const override { return type_; }
  void set_type(ConnectionType type) { type_ = type; }

 private:
  ConnectionType type_;
};

}  // namespace

TEST(BackgroundTracingManagerTest, HasTraceToUpload) {
  BrowserTaskEnvironment task_environment;
  std::unique_ptr<content::BackgroundTracingManager>
      background_tracing_manager =
          content::BackgroundTracingManager::CreateInstance();

  base::Value::Dict dict;
  dict.Set("mode", "REACTIVE_TRACING_MODE");
  dict.Set("category", "BENCHMARK_STARTUP");

  base::Value::List rules_list;
  {
    base::Value::Dict rules_dict;
    rules_dict.Set("rule", "MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED");
    rules_dict.Set("trigger_name", "reactive_test");
    rules_list.Append(std::move(rules_dict));
  }
  dict.Set("configs", std::move(rules_list));
  dict.Set("upload_limit_kb", 2);
  dict.Set("upload_limit_network_kb", 1);

  std::unique_ptr<BackgroundTracingConfig> config(
      BackgroundTracingConfigImpl::FromDict(std::move(dict)));
  EXPECT_TRUE(config);

  EXPECT_TRUE(background_tracing_manager->SetActiveScenario(
      std::move(config), BackgroundTracingManager::ANONYMIZE_DATA));

  auto trace = std::make_unique<std::string>(1500, 'a');

  background_tracing_manager->SetTraceToUploadForTesting(std::move(trace));

  MockNetworkChangeNotifier notifier;
  notifier.set_type(net::NetworkChangeNotifier::CONNECTION_2G);
#if BUILDFLAG(IS_ANDROID)
  EXPECT_FALSE(background_tracing_manager->HasTraceToUpload());
#endif

  notifier.set_type(net::NetworkChangeNotifier::CONNECTION_WIFI);
  EXPECT_TRUE(background_tracing_manager->HasTraceToUpload());
}

}  // namespace content
