// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/scoped_web_ui_controller_factory_registration.h"

#include "content/public/browser/web_ui_controller_factory.h"

namespace content {

ScopedWebUIControllerFactoryRegistration::
    ScopedWebUIControllerFactoryRegistration(
        content::WebUIControllerFactory* factory,
        content::WebUIControllerFactory* factory_to_replace)
    : factory_(factory), factory_to_replace_(factory_to_replace) {
  if (factory_to_replace_) {
    content::WebUIControllerFactory::UnregisterFactoryForTesting(
        factory_to_replace_);
  }
  content::WebUIControllerFactory::RegisterFactory(factory_);
}

ScopedWebUIControllerFactoryRegistration::
    ~ScopedWebUIControllerFactoryRegistration() {
  content::WebUIControllerFactory::UnregisterFactoryForTesting(factory_);
  // If we unregistered a registered factory, re-register it to keep global
  // state clean for future tests.
  if (factory_to_replace_)
    content::WebUIControllerFactory::RegisterFactory(factory_to_replace_);
}

void CheckForLeakedWebUIControllerFactoryRegistrations::OnTestStart(
    const testing::TestInfo& test_info) {
  initial_num_registered_ =
      content::WebUIControllerFactory::GetNumRegisteredFactoriesForTesting();
}

void CheckForLeakedWebUIControllerFactoryRegistrations::OnTestEnd(
    const testing::TestInfo& test_info) {
  EXPECT_EQ(
      initial_num_registered_,
      content::WebUIControllerFactory::GetNumRegisteredFactoriesForTesting())
      << "A WebUIControllerFactory was registered by a test but never "
         "unregistered. This can cause flakiness in later tests. Please use "
         "ScopedWebUIControllerFactoryRegistration to ensure that registered "
         "factories are unregistered.";
}

}  // namespace content
