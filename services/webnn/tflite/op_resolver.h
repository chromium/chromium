// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_TFLITE_OP_RESOLVER_H_
#define SERVICES_WEBNN_TFLITE_OP_RESOLVER_H_

#include "services/webnn/public/mojom/webnn_context_provider.mojom-forward.h"
#include "third_party/tflite/src/tensorflow/lite/mutable_op_resolver.h"

namespace webnn::tflite {

// This class maintains all the currently supported TFLite
// operations for the Chromium build of TFLite and registers them for use.
class OpResolver : public ::tflite::MutableOpResolver {
 public:
  explicit OpResolver(const mojom::CreateContextOptions& options);
};

}  // namespace webnn::tflite

#endif  // SERVICES_WEBNN_TFLITE_OP_RESOLVER_H_
