// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_MOJOM_STATUS_MOJOM_TRAITS_H_
#define MEDIA_MOJO_MOJOM_STATUS_MOJOM_TRAITS_H_

#include "base/containers/span.h"
#include "base/values.h"
#include "media/base/ipc/media_param_traits.h"
#include "media/base/status.h"
#include "media/mojo/mojom/media_types.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace mojo {

template <>
struct StructTraits<media::mojom::StatusDataView, media::Status> {
  static media::StatusCode code(const media::Status& input) {
    return input.code();
  }

  static absl::optional<std::string> message(const media::Status& input) {
    if (input.is_ok())
      return absl::nullopt;
    DCHECK(input.data_);
    return input.message();
  }

  static base::span<base::Value> frames(const media::Status& input) {
    if (input.is_ok())
      return {};
    DCHECK(input.data_);
    return input.data_->frames;
  }

  static base::span<media::Status> causes(const media::Status& input) {
    if (input.is_ok())
      return {};
    DCHECK(input.data_);
    return input.data_->causes;
  }

  static absl::optional<base::Value> data(const media::Status& input) {
    if (!input.is_ok()) {
      DCHECK(input.data_);
      return input.data_->data.Clone();
    }
    return absl::nullopt;
  }

  static bool Read(media::mojom::StatusDataView data, media::Status* output);
};

}  // namespace mojo

#endif  // MEDIA_MOJO_MOJOM_STATUS_MOJOM_TRAITS_H_
