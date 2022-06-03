// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/base/file_path_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<mojo_base::mojom::FilePathDataView, base::FilePath>::Read(
    mojo_base::mojom::FilePathDataView data,
    base::FilePath* out) {
  base::FilePath::StringPieceType path_view;
#if defined(OS_WIN)
  ArrayDataView<uint16_t> view;
  data.GetPathDataView(&view);
  path_view = {reinterpret_cast<const wchar_t*>(view.data()), view.size()};
#else
  if (!data.ReadPath(&path_view)) {
    return false;
  }
#endif
  *out = base::FilePath(path_view);
  return true;
}

}  // namespace mojo
