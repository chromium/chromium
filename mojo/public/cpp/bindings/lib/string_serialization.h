// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_STRING_SERIALIZATION_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_STRING_SERIALIZATION_H_

#include <stddef.h>
#include <string.h>

#include "mojo/public/cpp/bindings/lib/array_internal.h"
#include "mojo/public/cpp/bindings/lib/message_fragment.h"
#include "mojo/public/cpp/bindings/lib/serialization_forward.h"
#include "mojo/public/cpp/bindings/lib/serialization_util.h"
#include "mojo/public/cpp/bindings/string_data_view.h"
#include "mojo/public/cpp/bindings/string_traits.h"

namespace mojo {
namespace internal {

template <typename MaybeConstUserType>
struct Serializer<StringDataView, MaybeConstUserType> {
  using UserType = typename std::remove_const<MaybeConstUserType>::type;
  using Traits = StringTraits<UserType>;

  static void Serialize(MaybeConstUserType& input,
                        MessageFragment<String_Data>& fragment) {
    if (CallIsNullIfExists<Traits>(input))
      return;

    auto r = Traits::GetUTF8(input);
    fragment.AllocateArrayData(r.size());
    memcpy(fragment->storage(), r.data(), r.size());
  }

  static bool Deserialize(String_Data* input,
                          UserType* output,
                          Message* message) {
    if (!input)
      return CallSetToNullIfExists<Traits>(output);
    return Traits::Read(StringDataView(input, message), output);
  }
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_STRING_SERIALIZATION_H_
