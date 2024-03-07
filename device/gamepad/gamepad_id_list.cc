// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/gamepad_id_list.h"

#include <algorithm>
#include <iterator>

#include "base/containers/fixed_flat_map.h"
#include "base/containers/fixed_flat_set.h"

namespace device {

namespace {

static base::LazyInstance<GamepadIdList>::Leaky g_singleton =
    LAZY_INSTANCE_INITIALIZER;

// Information about all game input devices known to Chrome, including
// unsupported devices. Must be sorted by vendor and product ID.
//
// When recording metrics for connected gamepads, vendor and product IDs will
// only be recorded for devices that are in kGamepadInfo.
constexpr auto kGamepadInfo = base::MakeFixedFlatMap<
    std::pair<uint16_t /*vendor_id*/, uint16_t /*product_id*/>,
    XInputType>({
    {{0x0010, 0x0082}, kXInputTypeNone},
    // DragonRise Inc.
    {{0x0079, 0x0006}, kXInputTypeNone},
    {{0x0079, 0x0011}, kXInputTypeNone},
    {{0x0079, 0x1800}, kXInputTypeNone},
    {{0x0079, 0x181b}, kXInputTypeNone},
    {{0x0079, 0x1843}, kXInputTypeNone},
    {{0x0079, 0x1844}, kXInputTypeNone},
    // Steelseries ApS (Bluetooth)
    {{0x0111, 0x1417}, kXInputTypeNone},
    {{0x0111, 0x1419}, kXInputTypeNone},
    {{0x0111, 0x1420}, kXInputTypeNone},
    {{0x0111, 0x1431}, kXInputTypeNone},
    {{0x0111, 0x1434}, kXInputTypeNone},
    {{0x0113, 0xf900}, kXInputTypeNone},
    // Creative Technology, Ltd
    {{0x041e, 0x1003}, kXInputTypeNone},
    {{0x041e, 0x1050}, kXInputTypeNone},
    // Advanced Gravis Computer Tech, Ltd
    {{0x0428, 0x4001}, kXInputTypeNone},
    // Alps Electric Co., Ltd
    {{0x0433, 0x1101}, kXInputTypeNone},
    // ThrustMaster, Inc.
    {{0x044f, 0x0f00}, kXInputTypeXbox},
    {{0x044f, 0x0f03}, kXInputTypeXbox},
    {{0x044f, 0x0f07}, kXInputTypeXbox},
    {{0x044f, 0x0f10}, kXInputTypeXbox},
    {{0x044f, 0xa0a3}, kXInputTypeNone},
    {{0x044f, 0xb300}, kXInputTypeNone},
    {{0x044f, 0xb304}, kXInputTypeNone},
    {{0x044f, 0xb312}, kXInputTypeNone},
    {{0x044f, 0xb315}, kXInputTypeNone},
    {{0x044f, 0xb320}, kXInputTypeNone},
    {{0x044f, 0xb323}, kXInputTypeNone},
    {{0x044f, 0xb326}, kXInputTypeXbox},
    {{0x044f, 0xb653}, kXInputTypeNone},
    {{0x044f, 0xb677}, kXInputTypeNone},
    {{0x044f, 0xd003}, kXInputTypeNone},
    {{0x044f, 0xd008}, kXInputTypeNone},
    {{0x044f, 0xd009}, kXInputTypeNone},
    // Microsoft Corp.
    {{0x045e, 0x0026}, kXInputTypeNone},
    {{0x045e, 0x0027}, kXInputTypeNone},
    {{0x045e, 0x0202}, kXInputTypeXbox},
    {{0x045e, 0x0285}, kXInputTypeXbox},
    {{0x045e, 0x0287}, kXInputTypeXbox},
    {{0x045e, 0x0288}, kXInputTypeXbox},
    {{0x045e, 0x0289}, kXInputTypeXbox},
    {{0x045e, 0x028e}, kXInputTypeXbox360},
    {{0x045e, 0x028f}, kXInputTypeNone},
    {{0x045e, 0x0291}, kXInputTypeXbox360},
    {{0x045e, 0x02a0}, kXInputTypeXbox360},
    {{0x045e, 0x02a1}, kXInputTypeXbox360},
    {{0x045e, 0x02d1}, kXInputTypeXboxOne},
    {{0x045e, 0x02dd}, kXInputTypeXboxOne},
    {{0x045e, 0x02e0}, kXInputTypeNone},
    {{0x045e, 0x02e3}, kXInputTypeXboxOne},
    {{0x045e, 0x02e6}, kXInputTypeXbox360},
    {{0x045e, 0x02ea}, kXInputTypeXboxOne},
    {{0x045e, 0x02fd}, kXInputTypeNone},
    {{0x045e, 0x02ff}, kXInputTypeXboxOne},
    {{0x045e, 0x0719}, kXInputTypeXbox360},
    {{0x045e, 0x0b00}, kXInputTypeXboxOne},
    {{0x045e, 0x0b05}, kXInputTypeNone},
    {{0x045e, 0x0b06}, kXInputTypeXboxOne},
    {{0x045e, 0x0b0a}, kXInputTypeXboxOne},
    {{0x045e, 0x0b0c}, kXInputTypeNone},
    {{0x045e, 0x0b12}, kXInputTypeXboxOne},
    {{0x045e, 0x0b13}, kXInputTypeNone},
    {{0x045e, 0x0b20}, kXInputTypeNone},
    {{0x045e, 0x0b21}, kXInputTypeNone},
    {{0x045e, 0x0b22}, kXInputTypeNone},
    // Logitech, Inc.
    {{0x046d, 0xc208}, kXInputTypeNone},
    {{0x046d, 0xc209}, kXInputTypeNone},
    {{0x046d, 0xc211}, kXInputTypeNone},
    {{0x046d, 0xc215}, kXInputTypeNone},
    {{0x046d, 0xc216}, kXInputTypeNone},
    {{0x046d, 0xc218}, kXInputTypeNone},
    {{0x046d, 0xc219}, kXInputTypeNone},
    {{0x046d, 0xc21a}, kXInputTypeNone},
    {{0x046d, 0xc21d}, kXInputTypeXbox360},
    {{0x046d, 0xc21e}, kXInputTypeXbox360},
    {{0x046d, 0xc21f}, kXInputTypeXbox360},
    {{0x046d, 0xc242}, kXInputTypeXbox360},
    {{0x046d, 0xc24f}, kXInputTypeNone},
    {{0x046d, 0xc260}, kXInputTypeNone},
    {{0x046d, 0xc261}, kXInputTypeNone},
    {{0x046d, 0xc262}, kXInputTypeNone},
    {{0x046d, 0xc298}, kXInputTypeNone},
    {{0x046d, 0xc299}, kXInputTypeNone},
    {{0x046d, 0xc29a}, kXInputTypeNone},
    {{0x046d, 0xc29b}, kXInputTypeNone},
    {{0x046d, 0xca84}, kXInputTypeXbox},
    {{0x046d, 0xca88}, kXInputTypeXbox},
    {{0x046d, 0xca8a}, kXInputTypeXbox},
    {{0x046d, 0xcaa3}, kXInputTypeXbox360},
    // Kensington
    {{0x047d, 0x4003}, kXInputTypeNone},
    {{0x047d, 0x4005}, kXInputTypeNone},
    // Cypress Semiconductor Corp.
    {{0x04b4, 0x010a}, kXInputTypeNone},
    {{0x04b4, 0xd5d5}, kXInputTypeNone},
    // Holtek Semiconductor, Inc.
    {{0x04d9, 0x0002}, kXInputTypeNone},
    // Samsung Electronics Co., Ltd
    {{0x04e8, 0xa000}, kXInputTypeNone},
    // Siam United Hi-Tech
    {{0x0500, 0x9b28}, kXInputTypeNone},
    // Acer, Inc.
    {{0x0502, 0x1304}, kXInputTypeXbox360},
    {{0x0502, 0x1305}, kXInputTypeXbox360},
    {{0x0502, 0x1316}, kXInputTypeNone},
    {{0x0502, 0x1317}, kXInputTypeNone},
    // Belkin Components
    {{0x050d, 0x0802}, kXInputTypeNone},
    {{0x050d, 0x0803}, kXInputTypeNone},
    {{0x050d, 0x0805}, kXInputTypeNone},
    // Sony Corp.
    {{0x054c, 0x0268}, kXInputTypeNone},
    {{0x054c, 0x0306}, kXInputTypeNone},
    {{0x054c, 0x042f}, kXInputTypeNone},
    {{0x054c, 0x05c4}, kXInputTypeNone},
    {{0x054c, 0x05c5}, kXInputTypeNone},
    {{0x054c, 0x09cc}, kXInputTypeNone},
    {{0x054c, 0x0ba0}, kXInputTypeNone},
    {{0x054c, 0x0ce6}, kXInputTypeNone},
    {{0x054c, 0x0df2}, kXInputTypeNone},
    // Elecom Co., Ltd
    {{0x056e, 0x2003}, kXInputTypeNone},
    {{0x056e, 0x2004}, kXInputTypeXbox360},
    {{0x056e, 0x200f}, kXInputTypeNone},
    {{0x056e, 0x2010}, kXInputTypeNone},
    {{0x056e, 0x2013}, kXInputTypeXbox360},
    // Nintendo Co., Ltd
    {{0x057e, 0x0306}, kXInputTypeNone},
    {{0x057e, 0x0330}, kXInputTypeNone},
    {{0x057e, 0x0337}, kXInputTypeNone},
    {{0x057e, 0x2006}, kXInputTypeNone},
    {{0x057e, 0x2007}, kXInputTypeNone},
    {{0x057e, 0x2009}, kXInputTypeNone},
    {{0x057e, 0x200e}, kXInputTypeNone},
    // Padix Co., Ltd (Rockfire)
    {{0x0583, 0x2060}, kXInputTypeNone},
    {{0x0583, 0x206f}, kXInputTypeNone},
    {{0x0583, 0x3050}, kXInputTypeNone},
    {{0x0583, 0xa000}, kXInputTypeNone},
    {{0x0583, 0xa024}, kXInputTypeNone},
    {{0x0583, 0xa025}, kXInputTypeNone},
    {{0x0583, 0xa130}, kXInputTypeNone},
    {{0x0583, 0xa133}, kXInputTypeNone},
    {{0x0583, 0xb031}, kXInputTypeNone},
    // Vetronix Corp.
    {{0x05a0, 0x3232}, kXInputTypeNone},
    // Genesys Logic, Inc.
    {{0x05e3, 0x0596}, kXInputTypeNone},
    // InterAct, Inc.
    {{0x05fd, 0x1007}, kXInputTypeXbox},
    {{0x05fd, 0x107a}, kXInputTypeXbox},
    {{0x05fd, 0x3000}, kXInputTypeNone},
    // Chic Technology Corp.
    {{0x05fe, 0x0014}, kXInputTypeNone},
    {{0x05fe, 0x3030}, kXInputTypeXbox},
    {{0x05fe, 0x3031}, kXInputTypeXbox},
    // MosArt Semiconductor Corp.
    {{0x062a, 0x0020}, kXInputTypeXbox},
    {{0x062a, 0x0033}, kXInputTypeXbox},
    {{0x062a, 0x2410}, kXInputTypeNone},
    // Saitek PLC
    {{0x06a3, 0x0109}, kXInputTypeNone},
    {{0x06a3, 0x0200}, kXInputTypeXbox},
    {{0x06a3, 0x0201}, kXInputTypeXbox},
    {{0x06a3, 0x0241}, kXInputTypeNone},
    {{0x06a3, 0x040b}, kXInputTypeNone},
    {{0x06a3, 0x040c}, kXInputTypeNone},
    {{0x06a3, 0x052d}, kXInputTypeNone},
    {{0x06a3, 0x3509}, kXInputTypeNone},
    {{0x06a3, 0xf518}, kXInputTypeNone},
    {{0x06a3, 0xf51a}, kXInputTypeXbox360},
    {{0x06a3, 0xf622}, kXInputTypeNone},
    {{0x06a3, 0xf623}, kXInputTypeNone},
    {{0x06a3, 0xff0c}, kXInputTypeNone},
    // Aashima Technology B.V.
    {{0x06d6, 0x0025}, kXInputTypeNone},
    {{0x06d6, 0x0026}, kXInputTypeNone},
    // Guillemot Corp.
    {{0x06f8, 0xa300}, kXInputTypeNone},
    // Mad Catz, Inc.
    {{0x0738, 0x3250}, kXInputTypeNone},
    {{0x0738, 0x3285}, kXInputTypeNone},
    {{0x0738, 0x3384}, kXInputTypeNone},
    {{0x0738, 0x3480}, kXInputTypeNone},
    {{0x0738, 0x3481}, kXInputTypeNone},
    {{0x0738, 0x4506}, kXInputTypeXbox},
    {{0x0738, 0x4516}, kXInputTypeXbox},
    {{0x0738, 0x4520}, kXInputTypeXbox},
    {{0x0738, 0x4522}, kXInputTypeXbox},
    {{0x0738, 0x4526}, kXInputTypeXbox},
    {{0x0738, 0x4530}, kXInputTypeXbox},
    {{0x0738, 0x4536}, kXInputTypeXbox},
    {{0x0738, 0x4540}, kXInputTypeXbox},
    {{0x0738, 0x4556}, kXInputTypeXbox},
    {{0x0738, 0x4586}, kXInputTypeXbox},
    {{0x0738, 0x4588}, kXInputTypeXbox},
    {{0x0738, 0x45ff}, kXInputTypeXbox},
    {{0x0738, 0x4716}, kXInputTypeXbox360},
    {{0x0738, 0x4718}, kXInputTypeXbox360},
    {{0x0738, 0x4726}, kXInputTypeXbox360},
    {{0x0738, 0x4728}, kXInputTypeXbox360},
    {{0x0738, 0x4736}, kXInputTypeXbox360},
    {{0x0738, 0x4738}, kXInputTypeXbox360},
    {{0x0738, 0x4740}, kXInputTypeXbox360},
    {{0x0738, 0x4743}, kXInputTypeXbox},
    {{0x0738, 0x4758}, kXInputTypeXbox360},
    {{0x0738, 0x4a01}, kXInputTypeXboxOne},
    {{0x0738, 0x5266}, kXInputTypeNone},
    {{0x0738, 0x6040}, kXInputTypeXbox},
    {{0x0738, 0x8180}, kXInputTypeNone},
    {{0x0738, 0x8250}, kXInputTypeNone},
    {{0x0738, 0x8384}, kXInputTypeNone},
    {{0x0738, 0x8480}, kXInputTypeNone},
    {{0x0738, 0x8481}, kXInputTypeNone},
    {{0x0738, 0x8818}, kXInputTypeNone},
    {{0x0738, 0x8838}, kXInputTypeNone},
    {{0x0738, 0x9871}, kXInputTypeXbox360},
    {{0x0738, 0xb726}, kXInputTypeXbox360},
    {{0x0738, 0xb738}, kXInputTypeXbox360},
    {{0x0738, 0xbeef}, kXInputTypeXbox360},
    {{0x0738, 0xcb02}, kXInputTypeXbox360},
    {{0x0738, 0xcb03}, kXInputTypeXbox360},
    {{0x0738, 0xcb29}, kXInputTypeXbox360},
    {{0x0738, 0xf401}, kXInputTypeNone},
    {{0x0738, 0xf738}, kXInputTypeXbox360},
    {{0x07b5, 0x0213}, kXInputTypeNone},
    {{0x07b5, 0x0312}, kXInputTypeNone},
    {{0x07b5, 0x0314}, kXInputTypeNone},
    {{0x07b5, 0x0315}, kXInputTypeNone},
    {{0x07b5, 0x9902}, kXInputTypeNone},
    {{0x07ff, 0xffff}, kXInputTypeXbox360},
    // Personal Communication Systems, Inc.
    {{0x0810, 0x0001}, kXInputTypeNone},
    {{0x0810, 0x0003}, kXInputTypeNone},
    {{0x0810, 0x1e01}, kXInputTypeNone},
    {{0x0810, 0xe501}, kXInputTypeNone},
    // Lakeview Research
    {{0x0925, 0x0005}, kXInputTypeNone},
    {{0x0925, 0x03e8}, kXInputTypeNone},
    {{0x0925, 0x1700}, kXInputTypeNone},
    {{0x0925, 0x2801}, kXInputTypeNone},
    {{0x0925, 0x8866}, kXInputTypeNone},
    {{0x0926, 0x2526}, kXInputTypeNone},
    {{0x0926, 0x8888}, kXInputTypeNone},
    // NVIDIA Corp.
    {{0x0955, 0x7210}, kXInputTypeNone},
    {{0x0955, 0x7214}, kXInputTypeNone},
    // Broadcom Corp.
    {{0x0a5c, 0x8502}, kXInputTypeNone},
    // ASUSTek Computer, Inc.
    {{0x0b05, 0x4500}, kXInputTypeNone},
    // Play.com, Inc.
    {{0x0b43, 0x0005}, kXInputTypeNone},
    // Zeroplus
    {{0x0c12, 0x0005}, kXInputTypeXbox},
    {{0x0c12, 0x0e10}, kXInputTypeNone},
    {{0x0c12, 0x0ef6}, kXInputTypeNone},
    {{0x0c12, 0x1cf6}, kXInputTypeNone},
    {{0x0c12, 0x8801}, kXInputTypeXbox},
    {{0x0c12, 0x8802}, kXInputTypeXbox},
    {{0x0c12, 0x8809}, kXInputTypeXbox},
    {{0x0c12, 0x880a}, kXInputTypeXbox},
    {{0x0c12, 0x8810}, kXInputTypeXbox},
    {{0x0c12, 0x9902}, kXInputTypeXbox},
    // Microdia
    {{0x0c45, 0x4320}, kXInputTypeNone},
    {{0x0d2f, 0x0002}, kXInputTypeXbox},
    // Radica Games, Ltd
    {{0x0e4c, 0x1097}, kXInputTypeXbox},
    {{0x0e4c, 0x1103}, kXInputTypeXbox},
    {{0x0e4c, 0x2390}, kXInputTypeXbox},
    {{0x0e4c, 0x3510}, kXInputTypeXbox},
    // Logic3
    {{0x0e6f, 0x0003}, kXInputTypeXbox},
    {{0x0e6f, 0x0005}, kXInputTypeXbox},
    {{0x0e6f, 0x0006}, kXInputTypeXbox},
    {{0x0e6f, 0x0008}, kXInputTypeXbox},
    {{0x0e6f, 0x0105}, kXInputTypeXbox360},
    {{0x0e6f, 0x0113}, kXInputTypeXbox360},
    {{0x0e6f, 0x011e}, kXInputTypeNone},
    {{0x0e6f, 0x011f}, kXInputTypeXbox360},
    {{0x0e6f, 0x0124}, kXInputTypeNone},
    {{0x0e6f, 0x0130}, kXInputTypeNone},
    {{0x0e6f, 0x0131}, kXInputTypeXbox360},
    {{0x0e6f, 0x0133}, kXInputTypeXbox360},
    {{0x0e6f, 0x0139}, kXInputTypeXboxOne},
    {{0x0e6f, 0x013a}, kXInputTypeXboxOne},
    {{0x0e6f, 0x0146}, kXInputTypeXboxOne},
    {{0x0e6f, 0x0147}, kXInputTypeXboxOne},
    {{0x0e6f, 0x0158}, kXInputTypeNone},
    {{0x0e6f, 0x015c}, kXInputTypeXboxOne},
    {{0x0e6f, 0x0161}, kXInputTypeXboxOne},
    {{0x0e6f, 0x0162}, kXInputTypeXboxOne},
    {{0x0e6f, 0x0163}, kXInputTypeXboxOne},
    {{0x0e6f, 0x0164}, kXInputTypeXboxOne},
    {{0x0e6f, 0x0165}, kXInputTypeXboxOne},
    {{0x0e6f, 0x0201}, kXInputTypeXbox360},
    {{0x0e6f, 0x0213}, kXInputTypeXbox360},
    {{0x0e6f, 0x021f}, kXInputTypeXbox360},
    {{0x0e6f, 0x0246}, kXInputTypeXboxOne},
    {{0x0e6f, 0x02a0}, kXInputTypeNone},
    {{0x0e6f, 0x02ab}, kXInputTypeNone},
    {{0x0e6f, 0x0301}, kXInputTypeXbox360},
    {{0x0e6f, 0x0346}, kXInputTypeXboxOne},
    {{0x0e6f, 0x0401}, kXInputTypeXbox360},
    {{0x0e6f, 0x0413}, kXInputTypeXbox360},
    {{0x0e6f, 0x0501}, kXInputTypeXbox360},
    {{0x0e6f, 0xf501}, kXInputTypeNone},
    {{0x0e6f, 0xf701}, kXInputTypeNone},
    {{0x0e6f, 0xf900}, kXInputTypeXbox360},
    // GreenAsia Inc.
    {{0x0e8f, 0x0003}, kXInputTypeNone},
    {{0x0e8f, 0x0008}, kXInputTypeNone},
    {{0x0e8f, 0x0012}, kXInputTypeNone},
    {{0x0e8f, 0x0201}, kXInputTypeXbox},
    {{0x0e8f, 0x1006}, kXInputTypeNone},
    {{0x0e8f, 0x3008}, kXInputTypeXbox},
    {{0x0e8f, 0x3010}, kXInputTypeNone},
    {{0x0e8f, 0x3013}, kXInputTypeNone},
    {{0x0e8f, 0x3075}, kXInputTypeNone},
    {{0x0e8f, 0x310d}, kXInputTypeNone},
    // Hori Co., Ltd
    {{0x0f0d, 0x000a}, kXInputTypeXbox360},
    {{0x0f0d, 0x000c}, kXInputTypeXbox360},
    {{0x0f0d, 0x000d}, kXInputTypeXbox360},
    {{0x0f0d, 0x0010}, kXInputTypeNone},
    {{0x0f0d, 0x0011}, kXInputTypeNone},
    {{0x0f0d, 0x0016}, kXInputTypeXbox360},
    {{0x0f0d, 0x001b}, kXInputTypeXbox360},
    {{0x0f0d, 0x0022}, kXInputTypeNone},
    {{0x0f0d, 0x0027}, kXInputTypeNone},
    {{0x0f0d, 0x003d}, kXInputTypeNone},
    {{0x0f0d, 0x0040}, kXInputTypeNone},
    {{0x0f0d, 0x0049}, kXInputTypeNone},
    {{0x0f0d, 0x004d}, kXInputTypeNone},
    {{0x0f0d, 0x0055}, kXInputTypeNone},
    {{0x0f0d, 0x005b}, kXInputTypeNone},
    {{0x0f0d, 0x005c}, kXInputTypeNone},
    {{0x0f0d, 0x005e}, kXInputTypeNone},
    {{0x0f0d, 0x005f}, kXInputTypeNone},
    {{0x0f0d, 0x0063}, kXInputTypeXboxOne},
    {{0x0f0d, 0x0066}, kXInputTypeNone},
    {{0x0f0d, 0x0067}, kXInputTypeXboxOne},
    {{0x0f0d, 0x006a}, kXInputTypeNone},
    {{0x0f0d, 0x006b}, kXInputTypeNone},
    {{0x0f0d, 0x006e}, kXInputTypeNone},
    {{0x0f0d, 0x0070}, kXInputTypeNone},
    {{0x0f0d, 0x0078}, kXInputTypeXboxOne},
    {{0x0f0d, 0x0084}, kXInputTypeNone},
    {{0x0f0d, 0x0085}, kXInputTypeNone},
    {{0x0f0d, 0x0087}, kXInputTypeNone},
    {{0x0f0d, 0x0088}, kXInputTypeNone},
    {{0x0f0d, 0x008a}, kXInputTypeNone},
    {{0x0f0d, 0x008b}, kXInputTypeNone},
    {{0x0f0d, 0x0090}, kXInputTypeNone},
    {{0x0f0d, 0x00c1}, kXInputTypeNone},
    {{0x0f0d, 0x00ee}, kXInputTypeNone},
    // Jess Technology Co., Ltd
    {{0x0f30, 0x010b}, kXInputTypeXbox},
    {{0x0f30, 0x0110}, kXInputTypeNone},
    {{0x0f30, 0x0111}, kXInputTypeNone},
    {{0x0f30, 0x0112}, kXInputTypeNone},
    {{0x0f30, 0x0202}, kXInputTypeXbox},
    {{0x0f30, 0x0208}, kXInputTypeNone},
    {{0x0f30, 0x1012}, kXInputTypeNone},
    {{0x0f30, 0x1100}, kXInputTypeNone},
    {{0x0f30, 0x1112}, kXInputTypeNone},
    {{0x0f30, 0x1116}, kXInputTypeNone},
    {{0x0f30, 0x8888}, kXInputTypeXbox},
    // Etoms Electronics Corp.
    {{0x102c, 0xff0c}, kXInputTypeXbox},
    // SteelSeries ApS (USB)
    {{0x1038, 0x1412}, kXInputTypeNone},
    {{0x1038, 0x1418}, kXInputTypeNone},
    {{0x1038, 0x1420}, kXInputTypeNone},
    {{0x1038, 0x1430}, kXInputTypeXbox360},
    {{0x1038, 0x1431}, kXInputTypeXbox360},
    {{0x1038, 0x1434}, kXInputTypeXbox360},
    {{0x1080, 0x0009}, kXInputTypeNone},
    // Betop
    {{0x11c0, 0x5213}, kXInputTypeNone},
    {{0x11c0, 0x5506}, kXInputTypeNone},
    {{0x11c9, 0x55f0}, kXInputTypeXbox360},
    {{0x11ff, 0x3331}, kXInputTypeNone},
    {{0x11ff, 0x3341}, kXInputTypeNone},
    // Focusrite-Novation
    {{0x1235, 0xab21}, kXInputTypeNone},
    // Nyko (Honey Bee)
    {{0x124b, 0x4d01}, kXInputTypeNone},
    // Honey Bee Electronic International Ltd.
    {{0x12ab, 0x0004}, kXInputTypeXbox360},
    {{0x12ab, 0x0006}, kXInputTypeNone},
    {{0x12ab, 0x0301}, kXInputTypeXbox360},
    {{0x12ab, 0x0302}, kXInputTypeNone},
    {{0x12ab, 0x0303}, kXInputTypeXbox360},
    {{0x12ab, 0x0e6f}, kXInputTypeNone},
    {{0x12ab, 0x8809}, kXInputTypeXbox},
    // Gembird
    {{0x12bd, 0xd012}, kXInputTypeNone},
    {{0x12bd, 0xd015}, kXInputTypeNone},
    // Sino Lite Technology Corp.
    {{0x1345, 0x1000}, kXInputTypeNone},
    {{0x1345, 0x3008}, kXInputTypeNone},
    // RedOctane
    {{0x1430, 0x02a0}, kXInputTypeNone},
    {{0x1430, 0x4734}, kXInputTypeNone},
    {{0x1430, 0x4748}, kXInputTypeXbox360},
    {{0x1430, 0x474c}, kXInputTypeNone},
    {{0x1430, 0x8888}, kXInputTypeXbox},
    {{0x1430, 0xf801}, kXInputTypeXbox360},
    {{0x146b, 0x0601}, kXInputTypeXbox360},
    {{0x146b, 0x0d01}, kXInputTypeNone},
    {{0x146b, 0x5500}, kXInputTypeNone},
    // JAMER INDUSTRIES CO., LTD.
    {{0x14d8, 0x6208}, kXInputTypeNone},
    {{0x14d8, 0xcd07}, kXInputTypeNone},
    {{0x14d8, 0xcfce}, kXInputTypeNone},
    // Razer USA, Ltd
    {{0x1532, 0x0037}, kXInputTypeXbox360},
    {{0x1532, 0x0300}, kXInputTypeNone},
    {{0x1532, 0x0401}, kXInputTypeNone},
    {{0x1532, 0x0900}, kXInputTypeNone},
    {{0x1532, 0x0a00}, kXInputTypeXboxOne},
    {{0x1532, 0x0a03}, kXInputTypeXboxOne},
    {{0x1532, 0x1000}, kXInputTypeNone},
    {{0x15e4, 0x3f00}, kXInputTypeXbox360},
    {{0x15e4, 0x3f0a}, kXInputTypeXbox360},
    {{0x15e4, 0x3f10}, kXInputTypeXbox360},
    {{0x162e, 0xbeef}, kXInputTypeXbox360},
    // Razer USA, Ltd
    {{0x1689, 0x0001}, kXInputTypeNone},
    {{0x1689, 0xfd00}, kXInputTypeXbox360},
    {{0x1689, 0xfd01}, kXInputTypeXbox360},
    {{0x1689, 0xfe00}, kXInputTypeXbox360},
    // Askey Computer Corp.
    {{0x1690, 0x0001}, kXInputTypeNone},
    // Van Ooijen Technische Informatica
    {{0x16c0, 0x0487}, kXInputTypeNone},
    {{0x16c0, 0x05e1}, kXInputTypeNone},
    {{0x1781, 0x057e}, kXInputTypeNone},
    // Google Inc.
    {{0x18d1, 0x2c40}, kXInputTypeNone},
    {{0x18d1, 0x502e}, kXInputTypeNone},
    {{0x18d1, 0x9400}, kXInputTypeNone},
    // Lab126, Inc.
    {{0x1949, 0x0402}, kXInputTypeNone},
    {{0x1949, 0x041a}, kXInputTypeXbox360},
    // Gampaq Co.Ltd
    {{0x19fa, 0x0607}, kXInputTypeNone},
    // ACRUX
    {{0x1a34, 0x0203}, kXInputTypeNone},
    {{0x1a34, 0x0401}, kXInputTypeNone},
    {{0x1a34, 0x0801}, kXInputTypeNone},
    {{0x1a34, 0x0802}, kXInputTypeNone},
    {{0x1a34, 0x0836}, kXInputTypeNone},
    {{0x1a34, 0xf705}, kXInputTypeNone},
    // Harmonix Music
    {{0x1bad, 0x0002}, kXInputTypeXbox360},
    {{0x1bad, 0x0003}, kXInputTypeXbox360},
    {{0x1bad, 0x0130}, kXInputTypeXbox360},
    {{0x1bad, 0x028e}, kXInputTypeNone},
    {{0x1bad, 0x0301}, kXInputTypeNone},
    {{0x1bad, 0x5500}, kXInputTypeNone},
    {{0x1bad, 0xf016}, kXInputTypeXbox360},
    {{0x1bad, 0xf018}, kXInputTypeXbox360},
    {{0x1bad, 0xf019}, kXInputTypeXbox360},
    {{0x1bad, 0xf021}, kXInputTypeXbox360},
    {{0x1bad, 0xf023}, kXInputTypeXbox360},
    {{0x1bad, 0xf025}, kXInputTypeXbox360},
    {{0x1bad, 0xf027}, kXInputTypeXbox360},
    {{0x1bad, 0xf028}, kXInputTypeXbox360},
    {{0x1bad, 0xf02d}, kXInputTypeNone},
    {{0x1bad, 0xf02e}, kXInputTypeXbox360},
    {{0x1bad, 0xf030}, kXInputTypeXbox360},
    {{0x1bad, 0xf036}, kXInputTypeXbox360},
    {{0x1bad, 0xf038}, kXInputTypeXbox360},
    {{0x1bad, 0xf039}, kXInputTypeXbox360},
    {{0x1bad, 0xf03a}, kXInputTypeXbox360},
    {{0x1bad, 0xf03d}, kXInputTypeXbox360},
    {{0x1bad, 0xf03e}, kXInputTypeXbox360},
    {{0x1bad, 0xf03f}, kXInputTypeXbox360},
    {{0x1bad, 0xf042}, kXInputTypeXbox360},
    {{0x1bad, 0xf080}, kXInputTypeXbox360},
    {{0x1bad, 0xf0ca}, kXInputTypeNone},
    {{0x1bad, 0xf501}, kXInputTypeXbox360},
    {{0x1bad, 0xf502}, kXInputTypeXbox360},
    {{0x1bad, 0xf503}, kXInputTypeXbox360},
    {{0x1bad, 0xf504}, kXInputTypeXbox360},
    {{0x1bad, 0xf505}, kXInputTypeXbox360},
    {{0x1bad, 0xf506}, kXInputTypeXbox360},
    {{0x1bad, 0xf900}, kXInputTypeXbox360},
    {{0x1bad, 0xf901}, kXInputTypeXbox360},
    {{0x1bad, 0xf902}, kXInputTypeNone},
    {{0x1bad, 0xf903}, kXInputTypeXbox360},
    {{0x1bad, 0xf904}, kXInputTypeXbox360},
    {{0x1bad, 0xf906}, kXInputTypeXbox360},
    {{0x1bad, 0xf907}, kXInputTypeNone},
    {{0x1bad, 0xfa01}, kXInputTypeXbox360},
    {{0x1bad, 0xfd00}, kXInputTypeXbox360},
    {{0x1bad, 0xfd01}, kXInputTypeXbox360},
    // OpenMoko, Inc.
    {{0x1d50, 0x6053}, kXInputTypeNone},
    {{0x1d79, 0x0301}, kXInputTypeNone},
    {{0x1dd8, 0x000b}, kXInputTypeNone},
    {{0x1dd8, 0x000f}, kXInputTypeNone},
    {{0x1dd8, 0x0010}, kXInputTypeNone},
    // DAP Technologies
    {{0x2002, 0x9000}, kXInputTypeNone},
    {{0x20d6, 0x0dad}, kXInputTypeNone},
    {{0x20d6, 0x6271}, kXInputTypeNone},
    {{0x20d6, 0x89e5}, kXInputTypeNone},
    {{0x20d6, 0xca6d}, kXInputTypeNone},
    {{0x20e8, 0x5860}, kXInputTypeNone},
    // MacAlly
    {{0x2222, 0x0060}, kXInputTypeNone},
    {{0x2222, 0x4010}, kXInputTypeNone},
    {{0x22ba, 0x1020}, kXInputTypeNone},
    {{0x2378, 0x1008}, kXInputTypeNone},
    {{0x2378, 0x100a}, kXInputTypeNone},
    {{0x24c6, 0x5000}, kXInputTypeXbox360},
    {{0x24c6, 0x5300}, kXInputTypeXbox360},
    {{0x24c6, 0x5303}, kXInputTypeXbox360},
    {{0x24c6, 0x530a}, kXInputTypeXbox360},
    {{0x24c6, 0x531a}, kXInputTypeXbox360},
    {{0x24c6, 0x5397}, kXInputTypeXbox360},
    {{0x24c6, 0x541a}, kXInputTypeXboxOne},
    {{0x24c6, 0x542a}, kXInputTypeXboxOne},
    {{0x24c6, 0x543a}, kXInputTypeXboxOne},
    {{0x24c6, 0x5500}, kXInputTypeXbox360},
    {{0x24c6, 0x5501}, kXInputTypeXbox360},
    {{0x24c6, 0x5502}, kXInputTypeXbox360},
    {{0x24c6, 0x5503}, kXInputTypeXbox360},
    {{0x24c6, 0x5506}, kXInputTypeXbox360},
    {{0x24c6, 0x550d}, kXInputTypeXbox360},
    {{0x24c6, 0x550e}, kXInputTypeXbox360},
    {{0x24c6, 0x551a}, kXInputTypeXboxOne},
    {{0x24c6, 0x561a}, kXInputTypeXboxOne},
    {{0x24c6, 0x5b00}, kXInputTypeXbox360},
    {{0x24c6, 0x5b02}, kXInputTypeXbox360},
    {{0x24c6, 0x5b03}, kXInputTypeXbox360},
    {{0x24c6, 0x5d04}, kXInputTypeXbox360},
    {{0x24c6, 0xfafb}, kXInputTypeNone},
    {{0x24c6, 0xfafc}, kXInputTypeNone},
    {{0x24c6, 0xfafd}, kXInputTypeNone},
    {{0x24c6, 0xfafe}, kXInputTypeXbox360},
    {{0x2563, 0x0523}, kXInputTypeNone},
    {{0x25f0, 0x83c1}, kXInputTypeNone},
    {{0x25f0, 0xc121}, kXInputTypeNone},
    {{0x2717, 0x3144}, kXInputTypeNone},
    {{0x2810, 0x0009}, kXInputTypeNone},
    {{0x2836, 0x0001}, kXInputTypeNone},
    // Dracal/Raphnet technologies
    {{0x289b, 0x0003}, kXInputTypeNone},
    {{0x289b, 0x0005}, kXInputTypeNone},
    // Valve Software
    {{0x28de, 0x1002}, kXInputTypeNone},
    {{0x28de, 0x1042}, kXInputTypeNone},
    {{0x28de, 0x1052}, kXInputTypeNone},
    {{0x28de, 0x1071}, kXInputTypeNone},
    {{0x28de, 0x1101}, kXInputTypeNone},
    {{0x28de, 0x1102}, kXInputTypeNone},
    {{0x28de, 0x1105}, kXInputTypeNone},
    {{0x28de, 0x1106}, kXInputTypeNone},
    {{0x28de, 0x1142}, kXInputTypeNone},
    {{0x28de, 0x11fc}, kXInputTypeNone},
    {{0x28de, 0x11ff}, kXInputTypeXbox360},
    {{0x28de, 0x1201}, kXInputTypeNone},
    {{0x28de, 0x1202}, kXInputTypeNone},
    {{0x2c22, 0x2000}, kXInputTypeNone},
    {{0x2c22, 0x2300}, kXInputTypeNone},
    {{0x2c22, 0x2302}, kXInputTypeNone},
    // DJI
    {{0x2ca3, 0x1020}, kXInputTypeNone},
    // 8BitDo
    {{0x2dc8, 0x1003}, kXInputTypeNone},
    {{0x2dc8, 0x1080}, kXInputTypeNone},
    {{0x2dc8, 0x2830}, kXInputTypeNone},
    {{0x2dc8, 0x3000}, kXInputTypeNone},
    {{0x2dc8, 0x3001}, kXInputTypeNone},
    {{0x2dc8, 0x3106}, kXInputTypeXbox360},
    {{0x2dc8, 0x3820}, kXInputTypeNone},
    {{0x2dc8, 0x9001}, kXInputTypeNone},
    {{0x2dfa, 0x0001}, kXInputTypeNone},
    {{0x2e95, 0x7725}, kXInputTypeNone},
    {{0x3767, 0x0101}, kXInputTypeXbox},
    {{0x3820, 0x0009}, kXInputTypeNone},
    {{0x4c50, 0x5453}, kXInputTypeNone},
    {{0x5347, 0x6d61}, kXInputTypeNone},
    {{0x6469, 0x6469}, kXInputTypeNone},
    // Prototype product Vendor ID
    {{0x6666, 0x0667}, kXInputTypeNone},
    {{0x6666, 0x8804}, kXInputTypeNone},
    {{0x6666, 0x9401}, kXInputTypeNone},
    {{0x6957, 0x746f}, kXInputTypeNone},
    {{0x6978, 0x706e}, kXInputTypeNone},
    {{0x8000, 0x1002}, kXInputTypeNone},
    {{0x8888, 0x0308}, kXInputTypeNone},
    {{0xf000, 0x0003}, kXInputTypeNone},
    {{0xf000, 0x00f1}, kXInputTypeNone},
    // Hama
    {{0xf766, 0x0001}, kXInputTypeNone},
    {{0xf766, 0x0005}, kXInputTypeNone},
});

}  // namespace

// static
GamepadIdList& GamepadIdList::Get() {
  return g_singleton.Get();
}

XInputType GamepadIdList::GetXInputType(uint16_t vendor_id,
                                        uint16_t product_id) const {
  const auto find_it = kGamepadInfo.find({vendor_id, product_id});
  return find_it == kGamepadInfo.end() ? kXInputTypeNone : find_it->second;
}

GamepadId GamepadIdList::GetGamepadId(std::string_view product_name,
                                      uint16_t vendor_id,
                                      uint16_t product_id) const {
  if (kGamepadInfo.contains({vendor_id, product_id})) {
    // The ID value combines the vendor and product IDs.
    return static_cast<GamepadId>((vendor_id << 16) | product_id);
  }
  // Special cases for devices which don't report a valid vendor ID.
  if (vendor_id == 0x0 && product_id == 0x0 &&
      product_name == "Lic Pro Controller") {
    return GamepadId::kPowerALicPro;
  }
  return GamepadId::kUnknownGamepad;
}

std::pair<uint16_t, uint16_t> GamepadIdList::GetDeviceIdsFromGamepadId(
    GamepadId gamepad_id) const {
  // For most devices, the vendor/product ID pair is unique to a single gamepad
  // model. The GamepadId for these devices contains the 16-bit vendor and
  // product IDs packed into a 32-bit value. Some devices use duplicate or
  // invalid vendor and product IDs and are assigned "fake" GamepadIds that are
  // not derived from the vendor and product IDs.

  // Handle devices that have been assigned fake GamepadId values.
  if (gamepad_id == GamepadId::kPowerALicPro)
    return {0, 0};

  // Handle devices that use packed vendor/product GamepadId values.
  auto vendor_and_product = static_cast<uint32_t>(gamepad_id);
  const uint16_t vendor_id = vendor_and_product >> 16;
  const uint16_t product_id = vendor_and_product & 0xffff;
  DCHECK(kGamepadInfo.contains({vendor_id, product_id}));
  return {vendor_id, product_id};
}

bool GamepadIdList::HasTriggerRumbleSupport(GamepadId gamepad_id) const {
  static constexpr auto kTriggerRumbleGamepadIds =
      base::MakeFixedFlatSet<GamepadId>({
          // Xbox One USB
          GamepadId::kMicrosoftProduct02d1,
          // Xbox One USB 2015 Firmware
          GamepadId::kMicrosoftProduct02dd,
          // Xbox One S Bluetooth 2016 Firmware
          GamepadId::kMicrosoftProduct02fd,
          // Xbox One S Bluetooth 2021 Firmware
          GamepadId::kMicrosoftProduct0b20,
          // Xbox One S USB
          GamepadId::kMicrosoftProduct02ea,
          // Xbox One S Bluetooth
          GamepadId::kMicrosoftProduct02e0,
          // Xbox One S USB
          GamepadId::kMicrosoftProduct0b06,
          // Xbox Series X USB
          GamepadId::kMicrosoftProduct0b12,
          // Xbox Series X Bluetooth
          GamepadId::kMicrosoftProduct0b13,
          // Xbox One Elite USB
          GamepadId::kMicrosoftProduct02e3,
          // Xbox One Elite Series 2 USB
          GamepadId::kMicrosoftProduct0b00,
          // Xbox One Elite Series 2 Bluetooth
          GamepadId::kMicrosoftProduct0b05,
          // Xbox Elite Series 2 Bluetooth 2021 Firmware
          GamepadId::kMicrosoftProduct0b22,
      });

  return kTriggerRumbleGamepadIds.contains(gamepad_id);
}

std::vector<std::tuple<uint16_t, uint16_t, XInputType>>
GamepadIdList::GetGamepadListForTesting() const {
  std::vector<std::tuple<uint16_t, uint16_t, XInputType>> gamepads;
  for (const auto& entry : kGamepadInfo) {
    auto& [key, xtype] = entry;
    auto& [vendor_id, product_id] = key;
    gamepads.push_back(std::make_tuple(vendor_id, product_id, xtype));
  }
  return gamepads;
}

}  // namespace device
