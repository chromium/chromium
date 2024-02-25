// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/mojom/status_mojom_traits.h"

#include "media/mojo/mojom/media_types.mojom.h"
#include "mojo/public/cpp/base/values_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<
    media::mojom::StatusDataDataView,
    media::internal::StatusData>::Read(media::mojom::StatusDataDataView data,
                                       media::internal::StatusData* output) {
  output->code = data.code();
  output->packed_root_cause = data.packed_root_cause();

  if (!data.ReadGroup(&output->group))
    return false;

  if (!data.ReadMessage(&output->message))
    return false;

  if (!data.ReadFrames(&output->frames))
    return false;

  if (!data.ReadData(&output->data))
    return false;

  std::optional<media::internal::StatusData> cause;
  if (!data.ReadCause(&cause))
    return false;

  if (cause.has_value()) {
    output->cause =
        std::make_unique<media::internal::StatusData>(std::move(*cause));
  }

  return true;
}

}  // namespace mojo
