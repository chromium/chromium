// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_FONT_LIST_ASYNC_H_
#define CONTENT_PUBLIC_BROWSER_FONT_LIST_ASYNC_H_

#include "base/functional/callback.h"
#include "base/values.h"
#include "content/common/content_export.h"

namespace content {

// Retrieves the list of fonts on the system as a list of strings. It provides
// a non-blocking interface to GetFontList_SlowBlocking in common/.
//
// This function will run asynchronously on a background thread since getting
// the font list from the system can be slow. The callback will be executed on
// the calling sequence.
CONTENT_EXPORT void GetFontListAsync(
    base::OnceCallback<void(base::Value::List)> callback);

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_FONT_LIST_ASYNC_H_
