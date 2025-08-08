// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/ml/chrome_ml_api.h"

#if defined(_WIN32)
#include <Windows.h>

const PlatformFile kInvalidPlatformFile = INVALID_HANDLE_VALUE;
#endif
