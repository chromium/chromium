// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_LAZY_CONTEXT_ID_H_
#define EXTENSIONS_BROWSER_LAZY_CONTEXT_ID_H_

#include <tuple>

#include "extensions/common/extension_id.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
}

namespace extensions {
class LazyContextTaskQueue;

class LazyContextId {
 public:
  enum class Type {
    kEventPage,
    kServiceWorker,
  };

  // An event page (lazy background) context.
  LazyContextId(content::BrowserContext* context,
                const ExtensionId& extension_id);

  // An extension service worker context.
  LazyContextId(content::BrowserContext* context,
                const ExtensionId& extension_id,
                const GURL& service_worker_scope);

  // Copy and move constructors.
  LazyContextId(const LazyContextId& other) = default;
  LazyContextId(LazyContextId&& other) = default;

  LazyContextId& operator=(const LazyContextId&) noexcept = default;
  LazyContextId& operator=(LazyContextId&&) noexcept = default;

  bool is_for_event_page() const { return type_ == Type::kEventPage; }
  bool is_for_service_worker() const { return type_ == Type::kServiceWorker; }

  content::BrowserContext* browser_context() const { return context_; }
  void set_browser_context(content::BrowserContext* context) {
    context_ = context;
  }

  const ExtensionId& extension_id() const { return extension_id_; }

  const GURL& service_worker_scope() const {
    DCHECK(is_for_service_worker());
    return service_worker_scope_;
  }

  LazyContextTaskQueue* GetTaskQueue() const;

  bool operator<(const LazyContextId& rhs) const {
    return std::tie(type_, context_, extension_id_, service_worker_scope_) <
           std::tie(rhs.type_, rhs.context_, rhs.extension_id_,
                    rhs.service_worker_scope_);
  }

  bool operator==(const LazyContextId& rhs) const {
    return std::tie(type_, context_, extension_id_, service_worker_scope_) ==
           std::tie(rhs.type_, rhs.context_, rhs.extension_id_,
                    rhs.service_worker_scope_);
  }

  bool operator!=(const LazyContextId& rhs) const { return !(*this == rhs); }

 private:
  Type type_;
  content::BrowserContext* context_;
  ExtensionId extension_id_;
  GURL service_worker_scope_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_LAZY_CONTEXT_ID_H_
