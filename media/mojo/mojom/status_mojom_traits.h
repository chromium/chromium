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
struct StructTraits<media::mojom::StatusDataDataView,
                    media::internal::StatusData> {
  static media::StatusCodeType code(const media::internal::StatusData& input) {
    return input.code;
  }

  static media::StatusGroupType group(
      const media::internal::StatusData& input) {
    return input.group;
  }

  static std::string message(const media::internal::StatusData& input) {
    return input.message;
  }

  static base::span<base::Value> frames(media::internal::StatusData& input) {
    return input.frames;
  }

  static base::span<media::internal::StatusData> causes(
      media::internal::StatusData& input) {
    return input.causes;
  }

  static base::Value data(const media::internal::StatusData& input) {
    return input.data.Clone();
  }

  static bool Read(media::mojom::StatusDataDataView data,
                   media::internal::StatusData* output);
};

template <typename StatusEnum, typename DataView>
struct StructTraits<DataView, media::TypedStatus<StatusEnum>> {
  static absl::optional<media::internal::StatusData> internal(
      const media::TypedStatus<StatusEnum>& input) {
    if (input.data_)
      return *input.data_;
    return absl::nullopt;
  }

  static bool Read(DataView data, media::TypedStatus<StatusEnum>* output) {
    absl::optional<media::internal::StatusData> internal;
    if (!data.ReadInternal(&internal))
      return false;
    if (internal)
      output->data_ = internal->copy();
    return true;
  }
};

}  // namespace mojo

#endif  // MEDIA_MOJO_MOJOM_STATUS_MOJOM_TRAITS_H_
