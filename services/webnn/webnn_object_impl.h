// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_WEBNN_OBJECT_IMPL_H_
#define SERVICES_WEBNN_WEBNN_OBJECT_IMPL_H_

#include "base/component_export.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace webnn {

namespace internal {
// Supported WebNN token types. The list can be expanded as needed.
// Adding a new type must be explicitly instantiated in the cpp.
template <typename T, typename... U>
concept IsAnyOf = (std::same_as<T, U> || ...);
template <typename T>
concept IsSupportedTokenType =
    IsAnyOf<T, blink::WebNNContextToken, blink::WebNNTensorToken>;
}  // namespace internal

template <typename WebNNTokenType>
  requires internal::IsSupportedTokenType<WebNNTokenType>
class COMPONENT_EXPORT(WEBNN_SERVICE) WebNNObjectImpl {
 public:
  WebNNObjectImpl() = default;
  virtual ~WebNNObjectImpl() = default;

  WebNNObjectImpl(const WebNNObjectImpl&) = delete;
  WebNNObjectImpl& operator=(const WebNNObjectImpl&) = delete;

  const WebNNTokenType& handle() const { return handle_; }

  // Defines a "transparent" comparator so that unique_ptr keys to
  // WebNNObjectImpl instances can be compared against tokens for lookup in
  // associative containers like base::flat_set.
  template <typename WebNNObjectImplType>
  struct Comparator {
    using is_transparent = WebNNTokenType;

    template <class Deleter = std::default_delete<WebNNObjectImplType>>
    bool operator()(
        const std::unique_ptr<WebNNObjectImplType, Deleter>& lhs,
        const std::unique_ptr<WebNNObjectImplType, Deleter>& rhs) const {
      return lhs->handle() < rhs->handle();
    }

    template <class Deleter = std::default_delete<WebNNObjectImplType>>
    bool operator()(
        const WebNNTokenType& lhs,
        const std::unique_ptr<WebNNObjectImplType, Deleter>& rhs) const {
      return lhs < rhs->handle();
    }

    template <class Deleter = std::default_delete<WebNNObjectImplType>>
    bool operator()(const std::unique_ptr<WebNNObjectImplType, Deleter>& lhs,
                    const WebNNTokenType& rhs) const {
      return lhs->handle() < rhs;
    }
  };

 private:
  const WebNNTokenType handle_;
};

}  // namespace webnn

#endif  // SERVICES_WEBNN_WEBNN_OBJECT_IMPL_H_
