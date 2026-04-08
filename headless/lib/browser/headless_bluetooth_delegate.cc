// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_bluetooth_delegate.h"

#include "content/public/browser/render_frame_host.h"

namespace headless {

using ::content::BluetoothChooser;
using ::content::RenderFrameHost;

HeadlessBluetoothDelegate::HeadlessBluetoothDelegate() = default;
HeadlessBluetoothDelegate::~HeadlessBluetoothDelegate() = default;

std::unique_ptr<BluetoothChooser>
HeadlessBluetoothDelegate::RunBluetoothChooser(
    RenderFrameHost* frame,
    const BluetoothChooser::EventHandler& event_handler) {
  return std::make_unique<BluetoothChooser>();
}

}  // namespace headless
