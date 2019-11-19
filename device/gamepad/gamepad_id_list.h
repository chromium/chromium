// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_GAMEPAD_ID_LIST_H_
#define DEVICE_GAMEPAD_GAMEPAD_ID_LIST_H_

#include <stddef.h>
#include <stdint.h>

#include <unordered_set>
#include <vector>

#include "base/lazy_instance.h"
#include "device/gamepad/gamepad_export.h"

namespace device {

enum XInputType {
  // Not an XInput device, or unknown.
  kXInputTypeNone,
  // Microsoft Xbox compatible.
  kXInputTypeXbox,
  // Microsoft Xbox 360 compatible.
  kXInputTypeXbox360,
  // Microsoft Xbox One compatible.
  kXInputTypeXboxOne,
};

enum class GamepadId : uint32_t {
  // ID value representing an unknown gamepad or non-gamepad.
  kUnknownGamepad = 0,

  // ID values for supported devices.
  kAsusTekProduct4500 = 0x0b054500,
  kDragonRiseProduct0006 = 0x00790006,
  kDragonRiseProduct0011 = 0x00790011,
  kGoogleProduct2c40 = 0x18d12c40,
  kGoogleProduct9400 = 0x18d19400,
  kGreenAsiaProduct0003 = 0x0e8f0003,
  kLakeviewResearchProduct0005 = 0x09250005,
  kLakeviewResearchProduct8866 = 0x09258866,
  kLogitechProductc216 = 0x046dc216,
  kLogitechProductc218 = 0x046dc218,
  kLogitechProductc219 = 0x046dc219,
  kLogitechProductc21d = 0x046dc21d,
  kLogitechProductc21e = 0x046dc21e,
  kLogitechProductc21f = 0x046dc21f,
  kMacAllyProduct0060 = 0x22220060,
  kMacAllyProduct4010 = 0x22224010,
  kMicrosoftProduct028e = 0x045e028e,
  kMicrosoftProduct028f = 0x045e028f,
  kMicrosoftProduct0291 = 0x045e0291,
  kMicrosoftProduct02a1 = 0x045e02a1,
  kMicrosoftProduct02d1 = 0x045e02d1,
  kMicrosoftProduct02dd = 0x045e02dd,
  kMicrosoftProduct02e0 = 0x045e02e0,
  kMicrosoftProduct02e3 = 0x045e02e3,
  kMicrosoftProduct02ea = 0x045e02ea,
  kMicrosoftProduct02fd = 0x045e02fd,
  kMicrosoftProduct0719 = 0x045e0719,
  kMicrosoftProduct0b0a = 0x045e0b0a,
  kMicrosoftProduct0b0c = 0x045e0b0c,
  kNintendoProduct2006 = 0x057e2006,
  kNintendoProduct2007 = 0x057e2007,
  kNintendoProduct2009 = 0x057e2009,
  kNintendoProduct200e = 0x057e200e,
  kNvidiaProduct7210 = 0x09557210,
  kNvidiaProduct7214 = 0x09557214,
  kPadixProduct2060 = 0x05832060,
  kPdpProduct0003 = 0x0e6f0003,
  kPlayComProduct0005 = 0x0b430005,
  kPrototypeVendorProduct0667 = 0x66660667,
  kPrototypeVendorProduct9401 = 0x66669401,
  kRazer1532Product0900 = 0x15320900,
  kSamsungElectronicsProducta000 = 0x04e8a000,
  kSonyProduct0268 = 0x054c0268,
  kSonyProduct05c4 = 0x054c05c4,
  kSonyProduct09cc = 0x054c09cc,
  kSonyProduct0ba0 = 0x054c0ba0,
  kSteelSeriesBtProduct1419 = 0x01111419,
  kSteelSeriesProduct1412 = 0x10381412,
  kSteelSeriesProduct1418 = 0x10381418,
  kSteelSeriesProduct1420 = 0x10381420,
  kVendor20d6Product6271 = 0x20d66271,
  kVendor20d6Product89e5 = 0x20d689e5,
  kVendor2378Product1008 = 0x23781008,
  kVendor2378Product100a = 0x2378100a,
  kVendor2836Product0001 = 0x28360001,
  kVendor2e95Product7725 = 0x2e957725,
};

class DEVICE_GAMEPAD_EXPORT GamepadIdList {
 public:
  // Returns a singleton instance of the GamepadId list.
  static GamepadIdList& Get();

  // Returns a GamepadId value suitable for identifying a specific model of
  // gamepad. If the gamepad is not contained in the list of known gamepads,
  // returns kUnknownGamepad.
  GamepadId GetGamepadId(uint16_t vendor_id, uint16_t product_id) const;

  // Return the XInput flavor (Xbox, Xbox 360, or Xbox One) for the device with
  // the specified |vendor_id| and |product_id|, or kXInputTypeNone if the
  // device is not XInput or the XInput flavor is unknown.
  XInputType GetXInputType(uint16_t vendor_id, uint16_t product_id) const;

  // Returns the internal list of gamepad info for testing purposes.
  std::vector<std::tuple<uint16_t, uint16_t, XInputType>>
  GetGamepadListForTesting() const;

 private:
  friend base::LazyInstanceTraitsBase<GamepadIdList>;
  GamepadIdList();

  DISALLOW_COPY_AND_ASSIGN(GamepadIdList);
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_GAMEPAD_ID_LIST_H_
