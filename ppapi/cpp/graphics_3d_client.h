// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_GRAPHICS_3D_CLIENT_H_
#define PPAPI_CPP_GRAPHICS_3D_CLIENT_H_

#include "ppapi/c/pp_stdint.h"
#include "ppapi/cpp/instance_handle.h"

/// @file
/// This file defines the API for callbacks related to 3D.

namespace pp {

class Instance;

// This class provides a C++ interface for callbacks related to 3D. You
// would normally use multiple inheritance to derive from this class in your
// instance.
class Graphics3DClient {
 public:
  ///
  /// A constructor for creating a Graphics3DClient.
  ///
  /// @param[in] instance The instance that will own the new
  /// <code>Graphics3DClient</code>.
  explicit Graphics3DClient(Instance* instance);

  /// Destructor.
  virtual ~Graphics3DClient();

  /// Graphics3DContextLost() is a notification that the context was lost for
  /// the 3D devices.
  virtual void Graphics3DContextLost() = 0;

 private:
  InstanceHandle associated_instance_;
};

}  // namespace pp

#endif  // PPAPI_CPP_GRAPHICS_3D_CLIENT_H_
