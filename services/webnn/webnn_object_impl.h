// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_WEBNN_OBJECT_IMPL_H_
#define SERVICES_WEBNN_WEBNN_OBJECT_IMPL_H_

#include "base/component_export.h"
#include "base/unguessable_token.h"

namespace webnn {

class COMPONENT_EXPORT(WEBNN_SERVICE) WebNNObjectImpl {
 public:
  explicit WebNNObjectImpl(const base::UnguessableToken& handle);
  virtual ~WebNNObjectImpl();

  WebNNObjectImpl(const WebNNObjectImpl&) = delete;
  WebNNObjectImpl& operator=(const WebNNObjectImpl&) = delete;

  const base::UnguessableToken& handle() const { return handle_; }

  // Defines a "transparent" comparator so that unique_ptr keys to
  // WebNNObjectImpl instances can be compared against tokens for lookup in
  // associative containers like base::flat_set.
  template <typename WebNNObjectImplType>
  struct Comparator {
    using is_transparent = base::UnguessableToken;

    template <class Deleter = std::default_delete<WebNNObjectImplType>>
    bool operator()(
        const std::unique_ptr<WebNNObjectImplType, Deleter>& lhs,
        const std::unique_ptr<WebNNObjectImplType, Deleter>& rhs) const {
      return lhs->handle() < rhs->handle();
    }

    template <class Deleter = std::default_delete<WebNNObjectImplType>>
    bool operator()(
        const base::UnguessableToken& lhs,
        const std::unique_ptr<WebNNObjectImplType, Deleter>& rhs) const {
      return lhs < rhs->handle();
    }

    template <class Deleter = std::default_delete<WebNNObjectImplType>>
    bool operator()(const std::unique_ptr<WebNNObjectImplType, Deleter>& lhs,
                    const base::UnguessableToken& rhs) const {
      return lhs->handle() < rhs;
    }
  };

 private:
  const base::UnguessableToken handle_;
};

}  // namespace webnn

#endif  // SERVICES_WEBNN_WEBNN_OBJECT_IMPL_H_
