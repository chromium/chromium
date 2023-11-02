// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_MAC_SEATBELT_EXTENSION_TOKEN_H_
#define SANDBOX_MAC_SEATBELT_EXTENSION_TOKEN_H_

#include "sandbox/mac/seatbelt_export.h"

#include <memory>
#include <string>

namespace mojo {
template <typename, typename>
struct StructTraits;
}

namespace sandbox {

namespace mac {
namespace mojom {
class SeatbeltExtensionTokenDataView;
}
}  // namespace mac

class SeatbeltExtension;

// A SeatbeltExtensionToken is used to pass a sandbox extension between
// processes using IPC. A token object can be constructed by issuing an
// extension in one process, sent across IPC, and then used to create a new
// extension object on the other side.
class SEATBELT_EXPORT SeatbeltExtensionToken {
 public:
  SeatbeltExtensionToken();

  SeatbeltExtensionToken(const SeatbeltExtensionToken&) = delete;
  SeatbeltExtensionToken& operator=(const SeatbeltExtensionToken&) = delete;

  ~SeatbeltExtensionToken();

  // Token objects are move-only types.
  SeatbeltExtensionToken(SeatbeltExtensionToken&& other);
  SeatbeltExtensionToken& operator=(SeatbeltExtensionToken&&);

  const std::string& token() const { return token_; }

  // Creates a fake token for testing.
  static SeatbeltExtensionToken CreateForTesting(const std::string& fake_token);

 protected:
  friend class SeatbeltExtension;
  friend struct mojo::StructTraits<mac::mojom::SeatbeltExtensionTokenDataView,
                                   SeatbeltExtensionToken>;

  explicit SeatbeltExtensionToken(const std::string& token);

  void set_token(const std::string& token) { token_ = token; }

 private:
  std::string token_;
};

}  // namespace sandbox

#endif  // SANDBOX_MAC_SEATBELT_EXTENSION_TOKEN_H_
