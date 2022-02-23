// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_GUEST_VIEW_GUEST_VIEW_INTERNAL_API_H_
#define EXTENSIONS_BROWSER_API_GUEST_VIEW_GUEST_VIEW_INTERNAL_API_H_

#include "extensions/browser/extension_function.h"

namespace extensions {

class GuestViewInternalCreateGuestFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("guestViewInternal.createGuest",
                             GUESTVIEWINTERNAL_CREATEGUEST)
  GuestViewInternalCreateGuestFunction();

  GuestViewInternalCreateGuestFunction(
      const GuestViewInternalCreateGuestFunction&) = delete;
  GuestViewInternalCreateGuestFunction& operator=(
      const GuestViewInternalCreateGuestFunction&) = delete;

 protected:
  ~GuestViewInternalCreateGuestFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() final;

 private:
  void CreateGuestCallback(content::WebContents* guest_web_contents);
};

class GuestViewInternalDestroyGuestFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("guestViewInternal.destroyGuest",
                             GUESTVIEWINTERNAL_DESTROYGUEST)
  GuestViewInternalDestroyGuestFunction();

  GuestViewInternalDestroyGuestFunction(
      const GuestViewInternalDestroyGuestFunction&) = delete;
  GuestViewInternalDestroyGuestFunction& operator=(
      const GuestViewInternalDestroyGuestFunction&) = delete;

 protected:
  ~GuestViewInternalDestroyGuestFunction() override;

  // ExtensionFunction:
  ResponseAction Run() final;

 private:
  void DestroyGuestCallback(content::WebContents* guest_web_contents);
};

class GuestViewInternalSetSizeFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("guestViewInternal.setSize",
                             GUESTVIEWINTERNAL_SETAUTOSIZE)

  GuestViewInternalSetSizeFunction();

  GuestViewInternalSetSizeFunction(const GuestViewInternalSetSizeFunction&) =
      delete;
  GuestViewInternalSetSizeFunction& operator=(
      const GuestViewInternalSetSizeFunction&) = delete;

 protected:
  ~GuestViewInternalSetSizeFunction() override;

  // ExtensionFunction:
  ResponseAction Run() final;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_GUEST_VIEW_GUEST_VIEW_INTERNAL_API_H_
