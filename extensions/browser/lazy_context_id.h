// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_LAZY_CONTEXT_ID_H_
#define EXTENSIONS_BROWSER_LAZY_CONTEXT_ID_H_

#include <tuple>

#include "base/memory/raw_ptr.h"
#include "extensions/common/extension_id.h"

namespace content {
class BrowserContext;
}

namespace extensions {
class Extension;
class LazyContextTaskQueue;

class LazyContextId {
 public:
  static LazyContextId ForBackgroundPage(content::BrowserContext* context,
                                         const ExtensionId& extension_id) {
    return LazyContextId(Type::kBackgroundPage, context, extension_id);
  }

  static LazyContextId ForServiceWorker(content::BrowserContext* context,
                                        const ExtensionId& extension_id) {
    return LazyContextId(Type::kServiceWorker, context, extension_id);
  }

  static LazyContextId ForExtension(content::BrowserContext* context,
                                    const Extension* extension) {
    return LazyContextId(context, extension);
  }

  // Copy and move constructors.
  LazyContextId(const LazyContextId& other) = default;
  LazyContextId(LazyContextId&& other) = default;

  LazyContextId& operator=(const LazyContextId&) noexcept = default;
  LazyContextId& operator=(LazyContextId&&) noexcept = default;

  bool IsForBackgroundPage() const { return type_ == Type::kBackgroundPage; }
  bool IsForServiceWorker() const { return type_ == Type::kServiceWorker; }

  content::BrowserContext* browser_context() const { return context_; }

  const ExtensionId& extension_id() const { return extension_id_; }

  LazyContextTaskQueue* GetTaskQueue() const;

 private:
  enum class Type {
    kNone,
    kBackgroundPage,
    kServiceWorker,
  };

  friend bool operator<(const LazyContextId& lhs, const LazyContextId& rhs);
  friend bool operator==(const LazyContextId& lhs, const LazyContextId& rhs);

  // An event page or service worker based on the type.
  LazyContextId(Type type,
                content::BrowserContext* context,
                const ExtensionId& extension_id);

  // The type is derived from the extension.
  LazyContextId(content::BrowserContext* context, const Extension* extension);

  Type type_;
  raw_ptr<content::BrowserContext, DanglingUntriaged> context_;
  ExtensionId extension_id_;
};

inline bool operator<(const LazyContextId& lhs, const LazyContextId& rhs) {
  return std::tie(lhs.type_, lhs.context_, lhs.extension_id_) <
         std::tie(rhs.type_, rhs.context_, rhs.extension_id_);
}

inline bool operator==(const LazyContextId& lhs, const LazyContextId& rhs) {
  return std::tie(lhs.type_, lhs.context_, lhs.extension_id_) ==
         std::tie(rhs.type_, rhs.context_, rhs.extension_id_);
}

inline bool operator!=(const LazyContextId& lhs, const LazyContextId& rhs) {
  return !(lhs == rhs);
}

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_LAZY_CONTEXT_ID_H_
