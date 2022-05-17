// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quiche/overrides/quiche_platform_impl/quic_flags_impl.h"

#define DEFINE_QUIC_PROTOCOL_FLAG_SINGLE_VALUE(type, flag, value, doc) \
  type FLAGS_##flag = value;

#define DEFINE_QUIC_PROTOCOL_FLAG_TWO_VALUES(type, flag, internal_value, \
                                             external_value, doc)        \
  type FLAGS_##flag = external_value;

// Preprocessor macros can only have one definition.
// Select the right macro based on the number of arguments.
#define GET_6TH_ARG(arg1, arg2, arg3, arg4, arg5, arg6, ...) arg6
#define QUIC_PROTOCOL_FLAG_MACRO_CHOOSER(...)                    \
  GET_6TH_ARG(__VA_ARGS__, DEFINE_QUIC_PROTOCOL_FLAG_TWO_VALUES, \
              DEFINE_QUIC_PROTOCOL_FLAG_SINGLE_VALUE)
#define QUIC_PROTOCOL_FLAG(...) \
  QUIC_PROTOCOL_FLAG_MACRO_CHOOSER(__VA_ARGS__)(__VA_ARGS__)

#include "net/third_party/quiche/src/quiche/quic/core/quic_protocol_flags_list.h"

#undef QUIC_PROTOCOL_FLAG
#undef QUIC_PROTOCOL_FLAG_MACRO_CHOOSER
#undef GET_6TH_ARG
#undef DEFINE_QUIC_PROTOCOL_FLAG_TWO_VALUES
#undef DEFINE_QUIC_PROTOCOL_FLAG_SINGLE_VALUE
