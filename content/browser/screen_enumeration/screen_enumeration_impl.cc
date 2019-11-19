// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/screen_enumeration/screen_enumeration_impl.h"

#include <memory>

#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace content {

// static
void ScreenEnumerationImpl::Create(
    mojo::PendingReceiver<blink::mojom::ScreenEnumeration> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<ScreenEnumerationImpl>(),
                              std::move(receiver));
}

ScreenEnumerationImpl::ScreenEnumerationImpl() = default;
ScreenEnumerationImpl::~ScreenEnumerationImpl() = default;

void ScreenEnumerationImpl::GetDisplays(GetDisplaysCallback callback) {
  display::Screen* screen = display::Screen::GetScreen();
  const std::vector<display::Display> displays = screen->GetAllDisplays();
  const int64_t primary_id = screen->GetPrimaryDisplay().id();
  // TODO(msw): Return no data and |false| if a permission check fails.
  std::move(callback).Run(std::move(displays), primary_id, true);
}

}  // namespace content