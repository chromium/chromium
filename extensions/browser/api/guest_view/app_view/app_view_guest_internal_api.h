// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_GUEST_VIEW_APP_VIEW_APP_VIEW_GUEST_INTERNAL_API_H_
#define EXTENSIONS_BROWSER_API_GUEST_VIEW_APP_VIEW_APP_VIEW_GUEST_INTERNAL_API_H_

#include "base/macros.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

class AppViewGuestInternalAttachFrameFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("appViewGuestInternal.attachFrame",
                             APPVIEWINTERNAL_ATTACHFRAME)
  AppViewGuestInternalAttachFrameFunction();

 protected:
  ~AppViewGuestInternalAttachFrameFunction() override {}
  ResponseAction Run() final;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppViewGuestInternalAttachFrameFunction);
};

class AppViewGuestInternalDenyRequestFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("appViewGuestInternal.denyRequest",
                             APPVIEWINTERNAL_DENYREQUEST)
  AppViewGuestInternalDenyRequestFunction();

 protected:
  ~AppViewGuestInternalDenyRequestFunction() override {}
  ResponseAction Run() final;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppViewGuestInternalDenyRequestFunction);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_GUEST_VIEW_APP_VIEW_APP_VIEW_GUEST_INTERNAL_API_H_
