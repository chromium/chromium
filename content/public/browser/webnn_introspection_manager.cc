// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/webnn_introspection_manager.h"

#include "content/browser/webnn/webnn_introspection_manager_impl.h"

namespace content {

// static
WebNNIntrospectionManager* WebNNIntrospectionManager::GetInstance() {
  return WebNNIntrospectionManagerImpl::GetInstance();
}

}  // namespace content
