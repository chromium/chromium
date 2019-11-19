// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/tests/enum_headers_unittest.test-mojom.h"

// This file contains compile-time assertions about the mojo C++ bindings
// generator to ensure it does not transitively include unnecessary headers.
// It just checks a few common ones - the list doesn't have to be exhaustive.
//
// If this file won't compile, check module.h.tmpl and related .tmpl files.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_ENUM_TRAITS_H_
#error Mojo header guards changed, tests below are invalid.
#endif

#ifdef MOJO_PUBLIC_CPP_BINDINGS_INTERFACE_PTR_H_
#error interface_ptr.h should not be included by the generated header \
    for a mojom containing only an enum.
#endif

#ifdef MOJO_PUBLIC_CPP_BINDINGS_ASSOCIATED_INTERFACE_PTR_H_
#error associated_interface_ptr.h should not be included by the generated \
    header for a mojom containing only an enum.
#endif

#ifdef MOJO_PUBLIC_CPP_BINDINGS_INTERFACE_REQUEST_H_
#error interface_request.h should not be included by the generated header \
    for a mojom containing only an enum.
#endif

#ifdef MOJO_PUBLIC_CPP_BINDINGS_LIB_NATIVE_ENUM_SERIALIZATION_H_
#error native_enum_serialization.h should not be included by the generated \
    header for a mojom that does not use native enums.
#endif
