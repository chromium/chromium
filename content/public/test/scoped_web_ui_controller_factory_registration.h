// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_SCOPED_WEB_UI_CONTROLLER_FACTORY_REGISTRATION_H_
#define CONTENT_PUBLIC_TEST_SCOPED_WEB_UI_CONTROLLER_FACTORY_REGISTRATION_H_

#include "base/memory/raw_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class WebUIControllerFactory;

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

// A class used in tests to ensure that registered WebUIControllerFactory
// instances are unregistered. This should be enforced on unit-test suites
// with tests that register WebUIControllerFactory instances, to prevent those
// tests from causing flakiness in later tests run in the same process. This is
// not needed for browser tests, which are each run in their own process.
class CheckForLeakedWebUIControllerFactoryRegistrations
    : public testing::EmptyTestEventListener {
 public:
  void OnTestStart(const testing::TestInfo& test_info) override;
  void OnTestEnd(const testing::TestInfo& test_info) override;

 private:
  int initial_num_registered_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_SCOPED_WEB_UI_CONTROLLER_FACTORY_REGISTRATION_H_
