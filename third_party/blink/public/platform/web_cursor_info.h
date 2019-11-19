/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_CURSOR_INFO_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_CURSOR_INFO_H_

#include "third_party/blink/public/platform/web_point.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/cursor/types/cursor_types.h"

#ifdef WIN32
typedef struct HICON__* HICON;
typedef HICON HCURSOR;
#endif

namespace blink {

class Cursor;

struct WebCursorInfo {
  ui::CursorType type;
  WebPoint hot_spot;
  float image_scale_factor;
  SkBitmap custom_image;

#ifdef WIN32
  // On Windows, kTypeCustom may alternatively reference an externally
  // defined HCURSOR. If |type| is kTypeCustom and |external_handle| is non-
  // null, then |custom_image| should be ignored. The WebCursorInfo is not
  // responsible for managing the lifetime of this cursor handle.
  HCURSOR external_handle;
#endif

  explicit WebCursorInfo(ui::CursorType type = ui::CursorType::kPointer)
      : type(type), image_scale_factor(1) {
#ifdef WIN32
    external_handle = 0;
#endif
  }

#if INSIDE_BLINK
  BLINK_PLATFORM_EXPORT explicit WebCursorInfo(const Cursor&);
#endif
};

}  // namespace blink

#endif
