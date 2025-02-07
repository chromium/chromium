// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Header file that includes libfuzzer_macro.h from libprotobuf-mutator. Useful
// for inclusion in fuzz targets that can't include headers from third_party/.

#ifndef TESTING_LIBFUZZER_PROTO_LPM_INTERFACE_H_
#define TESTING_LIBFUZZER_PROTO_LPM_INTERFACE_H_

#include "third_party/libprotobuf-mutator/src/src/libfuzzer/libfuzzer_macro.h"  // IWYU pragma: export

// Silence logging from the protobuf library.
protobuf_mutator::protobuf::LogSilencer log_silencer;

#endif  // TESTING_LIBFUZZER_PROTO_LPM_INTERFACE_H_
