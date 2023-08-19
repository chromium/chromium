// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_SCOPED_WEB_UI_CONTROLLER_FACTORY_REGISTRATION_H_
#define CONTENT_PUBLIC_TEST_SCOPED_WEB_UI_CONTROLLER_FACTORY_REGISTRATION_H_

#include "base/memory/raw_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

class WebUIControllerFactory;
class WebUIConfig;

// A class to manage the registration of WebUIControllerFactory instances in
// tests. Registers the given |factory| on construction and unregisters it
// on destruction. If a |factory_to_replace| is provided, it is unregistered on
// construction and re-registered on destruction. Both factories must remain
// alive throughout the lifetime of this object.
class ScopedWebUIControllerFactoryRegistration {
 public:
  // |factory| and |factory_to_replace| must both outlive this object.
  explicit ScopedWebUIControllerFactoryRegistration(
      content::WebUIControllerFactory* factory,
      content::WebUIControllerFactory* factory_to_replace = nullptr);
  ~ScopedWebUIControllerFactoryRegistration();

 private:
  raw_ptr<content::WebUIControllerFactory> factory_;
  raw_ptr<content::WebUIControllerFactory> factory_to_replace_;
};

// A class to manage the registration of WebUIConfig instances in tests.
// Registers the given |webui_config| on construction and unregisters it
// on destruction. This should be used in unit tests where multiple tests can
// run in the same process and is not needed for browser tests, which are each
// run in their own process.
class ScopedWebUIConfigRegistration {
 public:
  // Registers `webui_config` and un-registers on destruction. If there is a
  // WebUIConfig with the same origin as `webui_config`, unregisters it. The
  // unregistered WebUIConfig will be re-registered on destruction.
  explicit ScopedWebUIConfigRegistration(
      std::unique_ptr<WebUIConfig> webui_config);

  // Removes the WebUIConfig with `webui_url`. The removed WebUIConfig is
  // re-registered on destruction.
  explicit ScopedWebUIConfigRegistration(const GURL& webui_url);

  ~ScopedWebUIConfigRegistration();

 private:
  const GURL webui_config_url_;
  std::unique_ptr<WebUIConfig> replaced_webui_config_;
};

// A class used in tests to ensure that registered WebUIControllerFactory and
// WebUIConfig instances are unregistered. This should be enforced on unit-test
// suites with tests that register WebUIControllerFactory and WebUIConfig
// instances, to prevent those tests from causing flakiness in later tests run
// in the same process. This is not needed for browser tests, which are each run
// in their own process.
class CheckForLeakedWebUIRegistrations
    : public testing::EmptyTestEventListener {
 public:
  void OnTestStart(const testing::TestInfo& test_info) override;
  void OnTestEnd(const testing::TestInfo& test_info) override;

 private:
  size_t initial_size_of_webui_config_map_;
  int initial_num_factories_registered_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_SCOPED_WEB_UI_CONTROLLER_FACTORY_REGISTRATION_H_
