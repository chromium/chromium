// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_SCOPED_WEB_UI_CONTROLLER_FACTORY_REGISTRATION_H_
#define CONTENT_PUBLIC_TEST_SCOPED_WEB_UI_CONTROLLER_FACTORY_REGISTRATION_H_

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
  content::WebUIControllerFactory* factory_;
  content::WebUIControllerFactory* factory_to_replace_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_SCOPED_WEB_UI_CONTROLLER_FACTORY_REGISTRATION_H_
