// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_GENERIC_PENDING_RECEIVER_H_
#define MOJO_PUBLIC_CPP_BINDINGS_GENERIC_PENDING_RECEIVER_H_

#include <optional>
#include <string>
#include <string_view>

#include "base/component_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/runtime_features.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value_forward.h"

namespace mojo {

// GenericPendingReceiver encapsulates a pairing of a receiving pipe endpoint
// with the name of the mojom interface assumed by the corresponding remote
// endpoint.
//
// This is used by mojom C++ bindings to represent
// |mojo_base.mojom.GenericPendingReceiver|, and it serves as a semi-safe
// wrapper for transporting arbitrary interface receivers in a generic object.
class COMPONENT_EXPORT(MOJO_CPP_BINDINGS) GenericPendingReceiver {
 public:
  GenericPendingReceiver();
  GenericPendingReceiver(std::string_view interface_name,
                         mojo::ScopedMessagePipeHandle receiving_pipe);

  template <typename Interface>
  GenericPendingReceiver(mojo::PendingReceiver<Interface> receiver)
      : GenericPendingReceiver(Interface::Name_, receiver.PassPipe()) {}

  GenericPendingReceiver(GenericPendingReceiver&&);

  GenericPendingReceiver(const GenericPendingReceiver&) = delete;
  GenericPendingReceiver& operator=(const GenericPendingReceiver&) = delete;

  ~GenericPendingReceiver();

  GenericPendingReceiver& operator=(GenericPendingReceiver&&);

  bool is_valid() const { return pipe_.is_valid(); }
  explicit operator bool() const { return is_valid(); }

  void reset();

  const std::optional<std::string>& interface_name() const {
    return interface_name_;
  }

  mojo::MessagePipeHandle pipe() const { return pipe_.get(); }

  // Takes ownership of the receiving pipe, invalidating this
  // GenericPendingReceiver.
  mojo::ScopedMessagePipeHandle PassPipe();

  // Takes ownership of the pipe, strongly typed as an |Interface| receiver, if
  // and only if that interface's name matches the stored interface name. May
  // return an empty pending receiver for RuntimeFeature disabled Interfaces.
  template <typename Interface>
  mojo::PendingReceiver<Interface> As() {
    if (!internal::GetRuntimeFeature_ExpectEnabled<Interface>()) {
      return mojo::PendingReceiver<Interface>();
    }
    return mojo::PendingReceiver<Interface>(PassPipeIfNameIs(Interface::Name_));
  }

  void WriteIntoTrace(perfetto::TracedValue ctx) const;

 private:
  mojo::ScopedMessagePipeHandle PassPipeIfNameIs(const char* interface_name);

  std::optional<std::string> interface_name_;
  mojo::ScopedMessagePipeHandle pipe_;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_GENERIC_PENDING_RECEIVER_H_
