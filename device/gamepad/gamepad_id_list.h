// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_GAMEPAD_ID_LIST_H_
#define DEVICE_GAMEPAD_GAMEPAD_ID_LIST_H_

#include <stddef.h>
#include <stdint.h>

#include <string_view>
#include <tuple>
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

// Enumeration of gamepads recognized by data fetchers. GamepadId values are
// composed of the USB or Bluetooth vendor ID in the high 16 bits and product ID
// in the low 16 bits.
//
// Enum labels used here should match the GamepadVendorProduct enum defined in
// enums.xml. New labels can be generated using the gamepad_vendor_product_id
// tool.
enum class GamepadId : uint32_t {
  // ID value representing an unknown gamepad or non-gamepad.
  kUnknownGamepad = 0,
  // Fake IDs for devices which report as 0x0000 0x0000
  kPowerALicPro = 0x0000ff00,
  // ID values for supported devices.
  k8BitDoProduct3106 = 0x2dc83106,
  kAcerProduct1304 = 0x05021304,
  kAcerProduct1305 = 0x05021305,
  kAcerProduct1316 = 0x05021316,
  kAcerProduct1317 = 0x05021317,
  kAmazonProduct041a = 0x1949041a,
  kAsusTekProduct4500 = 0x0b054500,
  kBdaProduct6271 = 0x20d66271,
  kBdaProduct89e5 = 0x20d689e5,
  kBroadcomProduct8502 = 0x0a5c8502,
  kDjiProduct1020 = 0x2ca31020,
  kDragonRiseProduct0006 = 0x00790006,
  kDragonRiseProduct0011 = 0x00790011,
  kElecomProduct200f = 0x056e200f,
  kElecomProduct2010 = 0x056e2010,
  kGoogleProduct2c40 = 0x18d12c40,
  kGoogleProduct9400 = 0x18d19400,
  kGreenAsiaProduct0003 = 0x0e8f0003,
  kHoriProduct00c1 = 0x0f0d00c1,
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
  kMicrosoftProduct0b00 = 0x045e0b00,
  kMicrosoftProduct0b05 = 0x045e0b05,
  kMicrosoftProduct0b06 = 0x045e0b06,
  kMicrosoftProduct0b0a = 0x045e0b0a,
  kMicrosoftProduct0b0c = 0x045e0b0c,
  kMicrosoftProduct0b12 = 0x045e0b12,
  kMicrosoftProduct0b13 = 0x045e0b13,
  kMicrosoftProduct0b20 = 0x045e0b20,
  kMicrosoftProduct0b21 = 0x045e0b21,
  kMicrosoftProduct0b22 = 0x045e0b22,
  kNintendoProduct2006 = 0x057e2006,
  kNintendoProduct2007 = 0x057e2007,
  kNintendoProduct2009 = 0x057e2009,
  kNintendoProduct200e = 0x057e200e,
  kNvidiaProduct7210 = 0x09557210,
  kNvidiaProduct7214 = 0x09557214,
  kOnLiveProduct1008 = 0x23781008,
  kOnLiveProduct100a = 0x2378100a,
  kOuyaProduct0001 = 0x28360001,
  kPadixProduct2060 = 0x05832060,
  kPdpProduct0003 = 0x0e6f0003,
  kPlayComProduct0005 = 0x0b430005,
  kPrototypeVendorProduct0667 = 0x66660667,
  kPrototypeVendorProduct9401 = 0x66669401,
  kRazer1532Product0900 = 0x15320900,
  kSamsungElectronicsProducta000 = 0x04e8a000,
  kScufProduct7725 = 0x2e957725,
  kSonyProduct0268 = 0x054c0268,
  kSonyProduct05c4 = 0x054c05c4,
  kSonyProduct09cc = 0x054c09cc,
  kSonyProduct0ba0 = 0x054c0ba0,
  kSonyProduct0ce6 = 0x054c0ce6,
  kSonyProduct0df2 = 0x054c0df2,
  kSteelSeriesBtProduct1419 = 0x01111419,
  kSteelSeriesBtProduct1431 = 0x01111431,
  kSteelSeriesBtProduct1434 = 0x01111434,
  kSteelSeriesProduct1412 = 0x10381412,
  kSteelSeriesProduct1418 = 0x10381418,
  kSteelSeriesProduct1420 = 0x10381420,
  kSteelSeriesProduct1430 = 0x10381430,
  kSteelSeriesProduct1431 = 0x10381431,
};

class DEVICE_GAMEPAD_EXPORT GamepadIdList {
 public:
  // Returns a singleton instance of the GamepadId list.
  static GamepadIdList& Get();

  GamepadIdList(const GamepadIdList& entry) = delete;
  GamepadIdList& operator=(const GamepadIdList& entry) = delete;

  // Returns a GamepadId value suitable for identifying a specific model of
  // gamepad. If the gamepad is not contained in the list of known gamepads,
  // returns kUnknownGamepad.
  GamepadId GetGamepadId(std::string_view product_name,
                         uint16_t vendor_id,
                         uint16_t product_id) const;

  std::pair<uint16_t, uint16_t> GetDeviceIdsFromGamepadId(
      GamepadId gamepad_id) const;

  // Return the XInput flavor (Xbox, Xbox 360, or Xbox One) for the device with
  // the specified |vendor_id| and |product_id|, or kXInputTypeNone if the
  // device is not XInput or the XInput flavor is unknown.
  XInputType GetXInputType(uint16_t vendor_id, uint16_t product_id) const;

  // Returns true if the gamepad device identified by |gamepad_id| has haptic
  // actuators on its triggers. Returns false otherwise.
  bool HasTriggerRumbleSupport(GamepadId gamepad_id) const;

  // Returns the internal list of gamepad info for testing purposes.
  std::vector<std::tuple<uint16_t, uint16_t, XInputType>>
  GetGamepadListForTesting() const;

 private:
  friend base::LazyInstanceTraitsBase<GamepadIdList>;
  GamepadIdList() = default;
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_GAMEPAD_ID_LIST_H_
