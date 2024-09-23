// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The avrt namespace encapsulates the details needed to support MMCSS.
//
// The Multimedia Class Scheduler service (MMCSS) enables multimedia
// applications to ensure that their time-sensitive processing receives
// prioritized access to CPU resources. This service enables multimedia
// applications to utilize as much of the CPU as possible without denying
// CPU resources to lower-priority applications.
// MMCSS requires Windows Vista or higher and that the Avrt DLL is loaded.
//
// TODO(henrika): refactor and merge into existing thread implementation
// for Windows to ensure that MMCSS can be enabled for all threads.
//
#ifndef MEDIA_AUDIO_WIN_AVRT_WRAPPER_WIN_H_
#define MEDIA_AUDIO_WIN_AVRT_WRAPPER_WIN_H_

#include <windows.h>

#include <avrt.h>

namespace avrt {

// Loads the Avrt.dll which is available on Windows Vista and later.
bool Initialize();

// Function wrappers for the underlying MMCSS functions.
bool AvRevertMmThreadCharacteristics(HANDLE avrt_handle);
HANDLE AvSetMmThreadCharacteristics(const wchar_t* task_name,
                                    DWORD* task_index);
bool AvSetMmThreadPriority(HANDLE avrt_handle, AVRT_PRIORITY priority);

}  // namespace avrt

#endif  // MEDIA_AUDIO_WIN_AVRT_WRAPPER_WIN_H_

