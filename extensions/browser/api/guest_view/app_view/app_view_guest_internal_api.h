// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_GUEST_VIEW_APP_VIEW_APP_VIEW_GUEST_INTERNAL_API_H_
#define EXTENSIONS_BROWSER_API_GUEST_VIEW_APP_VIEW_APP_VIEW_GUEST_INTERNAL_API_H_

#include "extensions/browser/extension_function.h"

namespace extensions {

class AppViewGuestInternalAttachFrameFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("appViewGuestInternal.attachFrame",
                             APPVIEWINTERNAL_ATTACHFRAME)
  AppViewGuestInternalAttachFrameFunction();

  AppViewGuestInternalAttachFrameFunction(
      const AppViewGuestInternalAttachFrameFunction&) = delete;
  AppViewGuestInternalAttachFrameFunction& operator=(
      const AppViewGuestInternalAttachFrameFunction&) = delete;

 protected:
  ~AppViewGuestInternalAttachFrameFunction() override {}
  ResponseAction Run() final;
};

class AppViewGuestInternalDenyRequestFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("appViewGuestInternal.denyRequest",
                             APPVIEWINTERNAL_DENYREQUEST)
  AppViewGuestInternalDenyRequestFunction();

  AppViewGuestInternalDenyRequestFunction(
      const AppViewGuestInternalDenyRequestFunction&) = delete;
  AppViewGuestInternalDenyRequestFunction& operator=(
      const AppViewGuestInternalDenyRequestFunction&) = delete;

 protected:
  ~AppViewGuestInternalDenyRequestFunction() override {}
  ResponseAction Run() final;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_GUEST_VIEW_APP_VIEW_APP_VIEW_GUEST_INTERNAL_API_H_
