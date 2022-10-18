// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/text/copy_lchars_from_uchar_source.h"

namespace WTF {

bool g_enable_sse_path_for_copy_lchars = false;

}  // namespace WTF
