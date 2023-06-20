// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_GUEST_VIEW_GUEST_VIEW_INTERNAL_API_H_
#define EXTENSIONS_BROWSER_API_GUEST_VIEW_GUEST_VIEW_INTERNAL_API_H_

#include "extensions/browser/extension_function.h"

namespace guest_view {
class GuestViewBase;
}  //  namespace guest_view

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
  ~GuestViewInternalCreateGuestFunction() override;

  // ExtensionFunction:
  ResponseAction Run() final;

 private:
  void CreateGuestCallback(guest_view::GuestViewBase* guest);
};

class GuestViewInternalDestroyUnattachedGuestFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("guestViewInternal.destroyUnattachedGuest",
                             GUESTVIEWINTERNAL_DESTROYUNATTACHEDGUEST)
  GuestViewInternalDestroyUnattachedGuestFunction();

  GuestViewInternalDestroyUnattachedGuestFunction(
      const GuestViewInternalDestroyUnattachedGuestFunction&) = delete;
  GuestViewInternalDestroyUnattachedGuestFunction& operator=(
      const GuestViewInternalDestroyUnattachedGuestFunction&) = delete;

 protected:
  ~GuestViewInternalDestroyUnattachedGuestFunction() override;

  // ExtensionFunction:
  ResponseAction Run() final;
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
