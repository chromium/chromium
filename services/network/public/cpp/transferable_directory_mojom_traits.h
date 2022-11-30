// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_TRANSFERABLE_DIRECTORY_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_TRANSFERABLE_DIRECTORY_MOJOM_TRAITS_H_

#include "mojo/public/cpp/bindings/union_traits.h"
#include "net/http/http_auth_preferences.h"
#include "services/network/public/cpp/transferable_directory.h"
#include "services/network/public/mojom/transferable_directory.mojom-shared.h"

namespace mojo {

template <>
struct UnionTraits<network::mojom::TransferableDirectoryDataView,
                   network::TransferableDirectory> {
  static network::mojom::TransferableDirectoryDataView::Tag GetTag(
      network::TransferableDirectory& input);
  static base::FilePath path(const network::TransferableDirectory& value);
  static mojo::PlatformHandle handle_for_ipc(
      network::TransferableDirectory& value);
  static bool Read(network::mojom::TransferableDirectoryDataView in,
                   network::TransferableDirectory* out);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_TRANSFERABLE_DIRECTORY_MOJOM_TRAITS_H_
