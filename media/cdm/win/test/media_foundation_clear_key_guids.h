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

#define MEDIA_FOUNDATION_CLEARKEY_GUID_CLEARKEY_PROTECTION_SYSTEM_ID_STRING \
  L"{E4E94971-696A-447E-96E4-93FDF3A57A7A}"

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
