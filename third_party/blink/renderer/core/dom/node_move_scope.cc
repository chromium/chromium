// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/node_move_scope.h"

namespace blink {

NodeMoveScopeItem* NodeMoveScope::current_item_ = nullptr;
Document* NodeMoveScope::document_ = nullptr;

}  // namespace blink
