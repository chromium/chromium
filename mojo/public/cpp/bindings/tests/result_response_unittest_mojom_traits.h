// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_TESTS_RESULT_RESPONSE_UNITTEST_MOJOM_TRAITS_H_
#define MOJO_PUBLIC_CPP_BINDINGS_TESTS_RESULT_RESPONSE_UNITTEST_MOJOM_TRAITS_H_

#include <string>

#include "mojo/public/cpp/bindings/string_data_view.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "mojo/public/cpp/bindings/tests/result_response.test-mojom-shared.h"

namespace mojo {
namespace test {

struct MappedResultValue {
  int magic_value;
};

class MappedResultError {
 public:
  MappedResultError() = default;
  virtual ~MappedResultError() = default;
  MappedResultError(const MappedResultError&) = default;
  MappedResultError& operator=(const MappedResultError&) = default;

  bool is_game_over_;
  std::string reason_;
};

}  //  namespace test

template <>
struct StructTraits<test::mojom::ResultValueDataView, test::MappedResultValue> {
  static int value(const test::MappedResultValue& in) { return in.magic_value; }

  static bool Read(test::mojom::ResultValueDataView in,
                   test::MappedResultValue* out) {
    out->magic_value = in.value();
    return true;
  }
};

template <>
struct StructTraits<test::mojom::ResultErrorDataView, test::MappedResultError> {
  static bool is_catastrophic(const test::MappedResultError& in) {
    return in.is_game_over_;
  }

  static std::string_view msg(const test::MappedResultError& in) {
    return in.reason_;
  }

  static bool Read(test::mojom::ResultErrorDataView in,
                   test::MappedResultError* out) {
    out->is_game_over_ = in.is_catastrophic();

    mojo::StringDataView s_view;
    in.GetMsgDataView(&s_view);
    out->reason_ = s_view.value();
    return true;
  }
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_TESTS_RESULT_RESPONSE_UNITTEST_MOJOM_TRAITS_H_
