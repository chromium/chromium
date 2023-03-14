// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_WIN_TEST_MEDIA_FOUNDATION_CLEAR_KEY_GUIDS_H_
#define MEDIA_CDM_WIN_TEST_MEDIA_FOUNDATION_CLEAR_KEY_GUIDS_H_

#include <initguid.h>

// Note about GUID: When a 16 byte array is being represented by a BYTE*, it is
// assumed to be in big endian. When a 16 byte array is being represented by a
// GUID, it is assumed to be in little endian.

namespace media {

// Media Foundation Clear Key protection system ID
// {E4E94971-696A-447E-96E4-93FDF3A57A7A}
DEFINE_GUID(MEDIA_FOUNDATION_CLEARKEY_GUID_CLEARKEY_PROTECTION_SYSTEM_ID,
            0xe4e94971,
            0x696a,
            0x447e,
            0x96,
            0xe4,
            0x93,
            0xfd,
            0xf3,
            0xa5,
            0x7a,
            0x7a);

// PlayReady media protection system ID to create an in-process PMP server.
// {F4637010-03C3-42CD-B932-B48ADF3A6A54}
DEFINE_GUID(PLAYREADY_GUID_MEDIA_PROTECTION_SYSTEM_ID,
            0xf4637010,
            0x03c3,
            0x42cd,
            0xb9,
            0x32,
            0xb4,
            0x8a,
            0xdf,
            0x3a,
            0x6a,
            0x54);

#define PLAYREADY_GUID_MEDIA_PROTECTION_SYSTEM_ID_STRING \
  L"{F4637010-03C3-42CD-B932-B48ADF3A6A54}"

// Media Foundation Clear Key content enabler type
// {C262FD73-2F13-41C2-94E7-4CAF087AE1D1}
DEFINE_GUID(MEDIA_FOUNDATION_CLEARKEY_GUID_CONTENT_ENABLER_TYPE,
            0xc262fd73,
            0x2f13,
            0x41c2,
            0x94,
            0xe7,
            0x4c,
            0xaf,
            0x8,
            0x7a,
            0xe1,
            0xd1);

}  // namespace media

#endif  // MEDIA_CDM_WIN_TEST_MEDIA_FOUNDATION_CLEAR_KEY_GUIDS_H_
