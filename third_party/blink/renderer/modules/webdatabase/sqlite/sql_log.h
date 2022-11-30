// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBDATABASE_SQLITE_SQL_LOG_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBDATABASE_SQLITE_SQL_LOG_H_

#include "base/dcheck_is_on.h"

#if DCHECK_IS_ON()
// We can see logs with |--v=N| or |--vmodule=SQLLog=N| where N is a
// verbose level.
#define SQL_DVLOG(verbose_level)          \
  LAZY_STREAM(VLOG_STREAM(verbose_level), \
              ((verbose_level) <= ::logging::GetVlogLevel("SQLLog.h")))
#else
#define SQL_DVLOG(verbose_level) EAT_STREAM_PARAMETERS
#endif

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBDATABASE_SQLITE_SQL_LOG_H_
