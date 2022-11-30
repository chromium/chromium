// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_DEV_VIEW_DEV_H_
#define PPAPI_CPP_DEV_VIEW_DEV_H_

#include "ppapi/cpp/view.h"

namespace pp {

// ViewDev is a version of View that exposes under-development APIs related to
// HiDPI
class ViewDev : public View {
 public:
  ViewDev() : View() {}
  ViewDev(const View& other) : View(other) {}

  virtual ~ViewDev() {}

  /// GetDeviceScale returns the scale factor between device pixels and DIPs
  /// (also known as logical pixels or UI pixels on some platforms). This allows
  /// the developer to render their contents at device resolution, even as
  /// coordinates / sizes are given in DIPs through the API.
  ///
  /// Note that the coordinate system for Pepper APIs is DIPs. Also note that
  /// one DIP might not equal one CSS pixel - when page scale/zoom is in effect.
  ///
  /// @return A <code>float</code> value representing the number of device
  /// pixels per DIP.
  float GetDeviceScale() const;

  /// GetCSSScale returns the scale factor between DIPs and CSS pixels. This
  /// allows proper scaling between DIPs - as sent via the Pepper API - and CSS
  /// pixel coordinates used for Web content.
  ///
  /// @return A <code>float</code> value representing the number of DIPs per CSS
  /// pixel.
  float GetCSSScale() const;
};

}  // namespace pp

#endif  // PPAPI_CPP_DEV_VIEW_DEV_H_
