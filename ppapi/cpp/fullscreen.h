// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_FULLSCREEN_H_
#define PPAPI_CPP_FULLSCREEN_H_

#include "ppapi/cpp/instance_handle.h"

/// @file
/// This file defines the API for handling transitions of a module instance to
/// and from fullscreen mode.

namespace pp {

class Size;

/// The Fullscreen class allowing you to check and toggle fullscreen mode.
class Fullscreen {
 public:
  /// A constructor for creating a <code>Fullscreen</code>.
  ///
  /// @param[in] instance The instance with which this resource will be
  /// associated.
  explicit Fullscreen(const InstanceHandle& instance);

  /// Destructor.
  virtual ~Fullscreen();

  /// IsFullscreen() checks whether the module instance is currently in
  /// fullscreen mode.
  ///
  /// @return <code>true</code> if the module instance is in fullscreen mode,
  /// <code>false</code> if the module instance is not in fullscreen mode.
  bool IsFullscreen();

  /// SetFullscreen() switches the module instance to and from fullscreen
  /// mode.
  ///
  /// The transition to and from fullscreen mode is asynchronous. During the
  /// transition, IsFullscreen() will return the previous value and
  /// no 2D or 3D device can be bound. The transition ends at DidChangeView()
  /// when IsFullscreen() returns the new value. You might receive other
  /// DidChangeView() calls while in transition.
  ///
  /// The transition to fullscreen mode can only occur while the browser is
  /// processing a user gesture, even if <code>true</code> is returned.
  ///
  /// @param[in] fullscreen <code>true</code> to enter fullscreen mode, or
  /// <code>false</code> to exit fullscreen mode.
  ///
  /// @return <code>true</code> on success or <code>false</code> on
  /// failure.
  bool SetFullscreen(bool fullscreen);

  /// GetScreenSize() gets the size of the screen in pixels. The module instance
  /// will be resized to this size when SetFullscreen() is called to enter
  /// fullscreen mode.
  ///
  /// @param[out] size The size of the entire screen in pixels.
  ///
  /// @return <code>true</code> on success or <code>false</code> on
  /// failure.
  bool GetScreenSize(Size* size);

 private:
  InstanceHandle instance_;
};

}  // namespace pp

#endif  // PPAPI_CPP_FULLSCREEN_H_
