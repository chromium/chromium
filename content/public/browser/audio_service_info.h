// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_AUDIO_SERVICE_INFO_H_
#define CONTENT_PUBLIC_BROWSER_AUDIO_SERVICE_INFO_H_

#include "base/process/process_handle.h"
#include "content/common/content_export.h"

namespace content {

// Returns the process id of the audio service utility process or
// base::kNullProcessId if audio service is not running in an utility process.
// Must be called on UI thread.
CONTENT_EXPORT base::ProcessId GetProcessIdForAudioService();

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_AUDIO_SERVICE_INFO_H_
