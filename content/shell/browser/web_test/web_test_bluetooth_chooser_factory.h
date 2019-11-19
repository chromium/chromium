// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_WEB_TEST_WEB_TEST_BLUETOOTH_CHOOSER_FACTORY_H_
#define CONTENT_SHELL_BROWSER_WEB_TEST_WEB_TEST_BLUETOOTH_CHOOSER_FACTORY_H_

#include <memory>
#include <set>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "content/public/browser/bluetooth_chooser.h"

namespace content {

class RenderFrameHost;

class WebTestBluetoothChooserFactory {
 public:
  WebTestBluetoothChooserFactory();
  ~WebTestBluetoothChooserFactory();

  std::unique_ptr<BluetoothChooser> RunBluetoothChooser(
      RenderFrameHost* frame,
      const BluetoothChooser::EventHandler& event_handler);

  std::vector<std::string> GetAndResetEvents();

  void SendEvent(BluetoothChooser::Event event, const std::string& device_id);

 private:
  class Chooser;

  std::vector<std::string> events_;

  // Contains the set of live choosers, in order to send them events.
  std::set<Chooser*> choosers_;

  base::WeakPtrFactory<WebTestBluetoothChooserFactory> weak_this_{this};
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_WEB_TEST_WEB_TEST_BLUETOOTH_CHOOSER_FACTORY_H_
