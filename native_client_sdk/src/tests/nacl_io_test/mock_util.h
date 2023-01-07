// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TESTS_NACL_IO_TEST_MOCK_UTIL_H_
#define TESTS_NACL_IO_TEST_MOCK_UTIL_H_

#include <gmock/gmock.h>
#include <ppapi/c/pp_completion_callback.h>
#include <ppapi/c/pp_var.h>

ACTION_TEMPLATE(CallCallback,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_1_VALUE_PARAMS(result)) {
  PP_CompletionCallback callback = std::tr1::get<k>(args);
  if (callback.func) {
    (*callback.func)(callback.user_data, result);
  }
}

MATCHER_P(IsEqualToVar, var, "") {
  if (arg.type != var.type)
    return false;

  switch (arg.type) {
    case PP_VARTYPE_BOOL:
      return arg.value.as_bool == var.value.as_bool;

    case PP_VARTYPE_INT32:
      return arg.value.as_int == var.value.as_int;

    case PP_VARTYPE_DOUBLE:
      return arg.value.as_double == var.value.as_double;

    case PP_VARTYPE_STRING:
      return arg.value.as_id == var.value.as_id;

    case PP_VARTYPE_UNDEFINED:
    case PP_VARTYPE_NULL:
      return true;

    case PP_VARTYPE_ARRAY:
    case PP_VARTYPE_ARRAY_BUFFER:
    case PP_VARTYPE_DICTIONARY:
    case PP_VARTYPE_OBJECT:
    default:
      // Not supported.
      return false;
  }
}

#endif  // TESTS_NACL_IO_TEST_MOCK_UTIL_H_
