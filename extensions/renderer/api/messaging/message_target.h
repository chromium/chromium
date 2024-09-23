// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_API_MESSAGING_MESSAGE_TARGET_H_
#define EXTENSIONS_RENDERER_API_MESSAGING_MESSAGE_TARGET_H_

#include <optional>
#include <string>

#include "extensions/common/extension_id.h"

namespace extensions {

// Represents a target of an extension message. This could be:
// 1. An Extension. Either an extension page is messaging another extension
//    page (e.g., popup messaging background page), a content script is
//    messaging an extension page, or a separate extension or web page is
//    messaging an extension page. Any calls to chrome.runtime.connect() or
//    chrome.runtime.sendMessage() target extensions.
// 2. A tab (i.e., a content script running in a web page). Any calls to
//    chrome.tabs.connect() or chrome.tabs.sendMessage() target tabs.
// 3. A native application. An extension is using native messaging to
//    communicate with a third-party application. Any calls to
//    chrome.runtime.connectNative() or chrome.runtime.sendNativeMessage()
//    target native applications.
struct MessageTarget {
  enum Type {
    TAB,
    EXTENSION,
    NATIVE_APP,
  };

  static MessageTarget ForTab(int tab_id, int frame_id);
  static MessageTarget ForTab(int tab_id,
                              int frame_id,
                              const std::string& document_id);
  static MessageTarget ForExtension(const ExtensionId& extension_id);
  static MessageTarget ForNativeApp(const std::string& native_app_name);

  MessageTarget(MessageTarget&& other);
  MessageTarget(const MessageTarget& other);
  ~MessageTarget();

  Type type;
  // Only valid for Type::EXTENSION.
  std::optional<ExtensionId> extension_id;
  // Only valid for Type::NATIVE_APP.
  std::optional<std::string> native_application_name;
  // Only valid for Type::TAB.
  std::optional<int> tab_id;
  std::optional<int> frame_id;
  std::optional<std::string> document_id;

  bool operator==(const MessageTarget& other) const;

 private:
  MessageTarget(Type type);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_API_MESSAGING_MESSAGE_TARGET_H_
