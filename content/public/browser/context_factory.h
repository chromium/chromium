// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_CONTEXT_FACTORY_H_
#define CONTENT_PUBLIC_BROWSER_CONTEXT_FACTORY_H_

#include "content/common/content_export.h"

namespace ui {
class ContextFactory;
}

namespace content {

// Returns the singleton ContextFactory used by content. The return value is
// owned by content.
CONTENT_EXPORT ui::ContextFactory* GetContextFactory();

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_CONTEXT_FACTORY_H_
