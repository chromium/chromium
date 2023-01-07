// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_provider_wrapper.h"

#include "base/observer_list.h"

namespace blink {

WebGraphicsContext3DProviderWrapper::~WebGraphicsContext3DProviderWrapper() {
  for (auto& observer : observers_)
    observer.OnContextDestroyed();
}

void WebGraphicsContext3DProviderWrapper::AddObserver(
    DestructionObserver* obs) {
  observers_.AddObserver(obs);
}

void WebGraphicsContext3DProviderWrapper::RemoveObserver(
    DestructionObserver* obs) {
  observers_.RemoveObserver(obs);
}

}  // namespace blink
