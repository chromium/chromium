// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Please see inteface_ppb_public_stable for the documentation on the format of
// this file.

// no-include-guard-because-multiply-included
// NOLINT(build/header_guard)

#include "build/build_config.h"
#include "ppapi/thunk/interfaces_preamble.h"

// Map the old dev console interface to the stable one (which is the same) to
// keep Flash, etc. working.
PROXIED_IFACE("PPB_Console(Dev);0.1", PPB_Console_1_0)
PROXIED_IFACE(PPB_CURSOR_CONTROL_DEV_INTERFACE_0_4, PPB_CursorControl_Dev_0_4)
PROXIED_IFACE(PPB_FILECHOOSER_DEV_INTERFACE_0_5, PPB_FileChooser_Dev_0_5)
PROXIED_IFACE(PPB_FILECHOOSER_DEV_INTERFACE_0_6, PPB_FileChooser_Dev_0_6)
PROXIED_IFACE(PPB_IME_INPUT_EVENT_DEV_INTERFACE_0_2, PPB_IMEInputEvent_Dev_0_2)
PROXIED_IFACE(PPB_MEMORY_DEV_INTERFACE_0_1, PPB_Memory_Dev_0_1)
PROXIED_IFACE(PPB_PRINTING_DEV_INTERFACE_0_7, PPB_Printing_Dev_0_7)
PROXIED_IFACE(PPB_TEXTINPUT_DEV_INTERFACE_0_2, PPB_TextInput_Dev_0_2)
PROXIED_IFACE(PPB_VIEW_DEV_INTERFACE_0_1, PPB_View_Dev_0_1)

#if !BUILDFLAG(IS_NACL)
PROXIED_API(PPB_Buffer)
PROXIED_API(PPB_VideoDecoder)

PROXIED_IFACE(PPB_AUDIO_INPUT_DEV_INTERFACE_0_3, PPB_AudioInput_Dev_0_3)
PROXIED_IFACE(PPB_AUDIO_INPUT_DEV_INTERFACE_0_4, PPB_AudioInput_Dev_0_4)
PROXIED_IFACE(PPB_AUDIO_OUTPUT_DEV_INTERFACE_0_1, PPB_AudioOutput_Dev_0_1)
PROXIED_IFACE(PPB_BUFFER_DEV_INTERFACE_0_4, PPB_Buffer_Dev_0_4)
PROXIED_IFACE(PPB_CHAR_SET_DEV_INTERFACE_0_4, PPB_CharSet_Dev_0_4)
PROXIED_IFACE(PPB_CRYPTO_DEV_INTERFACE_0_1, PPB_Crypto_Dev_0_1)
PROXIED_IFACE(PPB_DEVICEREF_DEV_INTERFACE_0_1, PPB_DeviceRef_Dev_0_1)
PROXIED_IFACE(PPB_GLES_CHROMIUM_TEXTURE_MAPPING_DEV_INTERFACE_0_1,
              PPB_GLESChromiumTextureMapping_Dev_0_1)
PROXIED_IFACE(PPB_IME_INPUT_EVENT_DEV_INTERFACE_0_1, PPB_IMEInputEvent_Dev_0_1)
PROXIED_IFACE(PPB_TEXTINPUT_DEV_INTERFACE_0_1, PPB_TextInput_Dev_0_1)
PROXIED_IFACE(PPB_URLUTIL_DEV_INTERFACE_0_6, PPB_URLUtil_Dev_0_6)
PROXIED_IFACE(PPB_URLUTIL_DEV_INTERFACE_0_7, PPB_URLUtil_Dev_0_7)
PROXIED_IFACE(PPB_VIDEOCAPTURE_DEV_INTERFACE_0_3, PPB_VideoCapture_Dev_0_3)
PROXIED_IFACE(PPB_VIDEODECODER_DEV_INTERFACE_0_16, PPB_VideoDecoder_Dev_0_16)
#endif  // !BUILDFLAG(IS_NACL)

#include "ppapi/thunk/interfaces_postamble.h"
