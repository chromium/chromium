// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBDATABASE_STORAGE_LOG_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBDATABASE_STORAGE_LOG_H_

#include "base/dcheck_is_on.h"

#if DCHECK_IS_ON()
// We can see logs with |--v=N| or |--vmodule=StorageLog=N| where N is a
// verbose level.
#define STORAGE_DVLOG(verbose_level)      \
  LAZY_STREAM(VLOG_STREAM(verbose_level), \
              ((verbose_level) <= ::logging::GetVlogLevel("StorageLog.h")))
#else
#define STORAGE_DVLOG(verbose_level) EAT_STREAM_PARAMETERS
#endif

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBDATABASE_STORAGE_LOG_H_
