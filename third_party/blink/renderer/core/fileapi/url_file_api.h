// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FILEAPI_URL_FILE_API_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FILEAPI_URL_FILE_API_H_

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class ExceptionState;
class ExecutionContext;
class ScriptState;
class Blob;

class URLFileAPI {
  STATIC_ONLY(URLFileAPI);

 public:
  static String createObjectURL(ScriptState*, Blob*, ExceptionState&);
  static void revokeObjectURL(ScriptState*, const String&);
  static void revokeObjectURL(ExecutionContext*, const String&);
};

}  // namespace blink

#endif
