// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_MOJOM_STATUS_MOJOM_TRAITS_H_
#define MEDIA_MOJO_MOJOM_STATUS_MOJOM_TRAITS_H_

#include <optional>

#include "base/containers/span.h"
#include "base/values.h"
#include "media/base/decoder_status.h"
#include "media/base/encoder_status.h"
#include "media/base/ipc/media_param_traits.h"
#include "media/base/status.h"
#include "media/mojo/mojom/media_types.mojom.h"
#include "mojo/public/cpp/bindings/optional_as_pointer.h"

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

  static const std::string& message(const media::internal::StatusData& input) {
    return input.message;
  }

  static const base::Value::List& frames(
      const media::internal::StatusData& input) {
    return input.frames;
  }

  static mojo::OptionalAsPointer<const media::internal::StatusData> cause(
      const media::internal::StatusData& input) {
    return mojo::OptionalAsPointer(input.cause.get());
  }

  static const base::Value& data(const media::internal::StatusData& input) {
    return input.data;
  }

  static media::UKMPackedType packed_root_cause(
      const media::internal::StatusData& input) {
    return input.packed_root_cause;
  }

  static bool Read(media::mojom::StatusDataDataView data,
                   media::internal::StatusData* output);
};

template <typename StatusEnum, typename DataView>
struct StructTraits<DataView, media::TypedStatus<StatusEnum>> {
  static mojo::OptionalAsPointer<const media::internal::StatusData> internal(
      const media::TypedStatus<StatusEnum>& input) {
    return mojo::OptionalAsPointer(input.data_.get());
  }

  static bool Read(DataView data, media::TypedStatus<StatusEnum>* output) {
    std::optional<media::internal::StatusData> internal;
    if (!data.ReadInternal(&internal))
      return false;
    if (internal)
      output->data_ = internal->copy();
    return true;
  }
};

}  // namespace mojo

#endif  // MEDIA_MOJO_MOJOM_STATUS_MOJOM_TRAITS_H_
