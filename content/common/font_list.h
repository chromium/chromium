// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_FONT_LIST_H_
#define CONTENT_COMMON_FONT_LIST_H_

#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "content/common/content_export.h"

namespace content {

// GetFontList_SlowBlocking() must only be called from the SequencedTaskRunner
// returned by this function because it is non-threadsafe on Linux for versions
// of Pango predating 2013.
CONTENT_EXPORT scoped_refptr<base::SequencedTaskRunner> GetFontListTaskRunner();

// Retrieves the fonts available on the current platform and returns them.
// The caller will own the returned pointer. Each entry will be a list of
// two strings, the first being the font family, and the second being the
// localized name.
//
// Can only be called from the SequencedTaskRunner returned by
// GetFontListTaskRunner(). Most callers will want to use the GetFontListAsync
// function in content/browser/font_list_async.h which does an asynchronous
// call.
CONTENT_EXPORT base::Value::List GetFontList_SlowBlocking();

}  // namespace content

#endif  // CONTENT_COMMON_FONT_LIST_H_
