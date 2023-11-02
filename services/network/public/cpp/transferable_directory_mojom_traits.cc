// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/transferable_directory_mojom_traits.h"

#include "mojo/public/cpp/base/file_path_mojom_traits.h"

namespace mojo {

network::mojom::TransferableDirectoryDataView::Tag UnionTraits<
    network::mojom::TransferableDirectoryDataView,
    network::TransferableDirectory>::GetTag(network::TransferableDirectory&
                                                input) {
  if (input.HasValidHandle()) {
    return network::mojom::TransferableDirectoryDataView::Tag::kHandleForIpc;
  } else {
    return network::mojom::TransferableDirectoryDataView::Tag::kPath;
  }
}

base::FilePath UnionTraits<network::mojom::TransferableDirectoryDataView,
                           network::TransferableDirectory>::
    path(const network::TransferableDirectory& value) {
  return value.path();
}

mojo::PlatformHandle UnionTraits<network::mojom::TransferableDirectoryDataView,
                                 network::TransferableDirectory>::
    handle_for_ipc(network::TransferableDirectory& value) {
  return value.TakeHandle();
}

bool UnionTraits<network::mojom::TransferableDirectoryDataView,
                 network::TransferableDirectory>::
    Read(network::mojom::TransferableDirectoryDataView in,
         network::TransferableDirectory* out) {
  switch (in.tag()) {
    case network::mojom::TransferableDirectoryDataView::Tag::kHandleForIpc:
      *out = network::TransferableDirectory(in.TakeHandleForIpc());
      return true;

    case network::mojom::TransferableDirectoryDataView::Tag::kPath: {
      base::FilePath path;
      if (!in.ReadPath(&path)) {
        return false;
      }
      *out = network::TransferableDirectory(path);
      return true;
    }
  }
}

}  // namespace mojo
