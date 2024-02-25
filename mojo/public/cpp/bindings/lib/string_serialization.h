// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_STRING_SERIALIZATION_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_STRING_SERIALIZATION_H_

#include <stddef.h>
#include <string.h>

#include <string_view>

#include "base/strings/string_util.h"
#include "mojo/public/cpp/bindings/lib/array_internal.h"
#include "mojo/public/cpp/bindings/lib/message_fragment.h"
#include "mojo/public/cpp/bindings/lib/serialization_forward.h"
#include "mojo/public/cpp/bindings/lib/serialization_util.h"
#include "mojo/public/cpp/bindings/lib/validation_errors.h"
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
    if (r.size() > 0)
      memcpy(fragment->storage(), r.data(), r.size());
  }

  static bool Deserialize(String_Data* input,
                          UserType* output,
                          Message* message) {
    if (!input)
      return CallSetToNullIfExists<Traits>(output);
    bool ok = Traits::Read(StringDataView(input, message), output);
    if (ok && !base::IsStringUTF8(
                  std::string_view(input->storage(), input->size()))) {
      RecordInvalidStringDeserialization();
    }
    return ok;
  }
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_STRING_SERIALIZATION_H_
