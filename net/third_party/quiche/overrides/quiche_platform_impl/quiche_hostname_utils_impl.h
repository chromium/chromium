// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_HOSTNAME_UTILS_IMPL_H_
#define NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_HOSTNAME_UTILS_IMPL_H_

#include "net/third_party/quiche/src/quiche/common/platform/api/quiche_export.h"
#include "third_party/abseil-cpp/absl/strings/string_view.h"

namespace quiche {

class QUICHE_EXPORT_PRIVATE QuicheHostnameUtilsImpl {
 public:
  QuicheHostnameUtilsImpl(const QuicheHostnameUtilsImpl&) = delete;
  QuicheHostnameUtilsImpl& operator=(const QuicheHostnameUtilsImpl&) = delete;

  // Returns true if the sni is valid, false otherwise.
  //  (1) disallow IP addresses;
  //  (2) check that the hostname contains valid characters only; and
  //  (3) contains at least one dot.
  static bool IsValidSNI(absl::string_view sni);

  // Convert hostname to lowercase and remove the trailing '.'.
  static std::string NormalizeHostname(absl::string_view hostname);
};

}  // namespace quiche

#endif  // NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_HOSTNAME_UTILS_IMPL_H_
