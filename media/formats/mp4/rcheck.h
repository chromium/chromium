// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_MP4_RCHECK_H_
#define MEDIA_FORMATS_MP4_RCHECK_H_

#include "base/logging.h"
#include "media/base/media_log.h"

// Evaluate |condition| once. If the result is false, log |msg| to |media_log|,
// and (early) return false from the containing function.
#define RCHECK_MEDIA_LOGGED(condition, media_log, msg)                 \
  do {                                                                 \
    if (!(condition)) {                                                \
      DLOG(ERROR) << "Failure while parsing MP4: " #condition;         \
      MEDIA_LOG(ERROR, media_log) << "Failure parsing MP4: " << (msg); \
      return false;                                                    \
    }                                                                  \
  } while (0)

// Evaluate |condition| once. If the result is false, (early) return false from
// the containing function.
// TODO(wolenetz,chcunningham): Where appropriate, replace usage of this macro
// in favor of RCHECK_MEDIA_LOGGED. See https://crbug.com/487410.
#define RCHECK(condition)                                      \
  do {                                                         \
    if (!(condition)) {                                        \
      DLOG(ERROR) << "Failure while parsing MP4: " #condition; \
      return false;                                            \
    }                                                          \
  } while (0)

#endif  // MEDIA_FORMATS_MP4_RCHECK_H_
