// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "mojo/public/cpp/base/file_path_mojom_traits.h"

#include "build/build_config.h"

namespace mojo {

// static
bool StructTraits<mojo_base::mojom::FilePathDataView, base::FilePath>::Read(
    mojo_base::mojom::FilePathDataView data,
    base::FilePath* out) {
  base::FilePath::StringPieceType path_view;
#if BUILDFLAG(IS_WIN)
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

// static
#if BUILDFLAG(IS_WIN)
base::span<const uint16_t>
StructTraits<mojo_base::mojom::RelativeFilePathDataView, base::FilePath>::path(
    const base::FilePath& path) {
  CHECK(!path.IsAbsolute());
  CHECK(!path.ReferencesParent());
  return base::make_span(reinterpret_cast<const uint16_t*>(path.value().data()),
                         path.value().size());
}
#else
// static
const base::FilePath::StringType&
StructTraits<mojo_base::mojom::RelativeFilePathDataView, base::FilePath>::path(
    const base::FilePath& path) {
  CHECK(!path.IsAbsolute());
  CHECK(!path.ReferencesParent());
  return path.value();
}
#endif

// static
bool StructTraits<mojo_base::mojom::RelativeFilePathDataView, base::FilePath>::
    Read(mojo_base::mojom::RelativeFilePathDataView data, base::FilePath* out) {
  base::FilePath::StringPieceType path_view;
#if BUILDFLAG(IS_WIN)
  ArrayDataView<uint16_t> view;
  data.GetPathDataView(&view);
  path_view = {reinterpret_cast<const wchar_t*>(view.data()), view.size()};
#else
  if (!data.ReadPath(&path_view)) {
    return false;
  }
#endif
  *out = base::FilePath(path_view);

  if (out->IsAbsolute()) {
    return false;
  }
  if (out->ReferencesParent()) {
    return false;
  }
  return true;
}

}  // namespace mojo
