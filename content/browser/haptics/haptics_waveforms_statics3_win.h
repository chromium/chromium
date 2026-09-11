// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_HAPTICS_HAPTICS_WAVEFORMS_STATICS3_WIN_H_
#define CONTENT_BROWSER_HAPTICS_HAPTICS_WAVEFORMS_STATICS3_WIN_H_

#include <inspectable.h>

namespace content {

// The Windows SDK bundled with the Chromium toolchain predates
// IKnownSimpleHapticsControllerWaveformsStatics3 (UniversalApiContract 19.0),
// which exposes the Collide/Align/Step waveforms. Declare it here so the
// production backend and its unit-test fakes share one IID and vtable layout,
// matching the SDK. Below the 24H2 floor the QueryInterface for it fails and
// callers fall back to a device-supported waveform.
//
// TODO(crbug.com/531787872): Remove this local declaration once Chromium's
// bundled Windows SDK reaches UniversalApiContract 19.0 and exposes
// IKnownSimpleHapticsControllerWaveformsStatics3 directly.
MIDL_INTERFACE("ae480ce4-4ab6-5b2f-ad0b-cb52f37d45fb")
IKnownSimpleHapticsControllerWaveformsStatics3 : public IInspectable {
 public:
  virtual HRESULT STDMETHODCALLTYPE get_Collide(UINT16 * value) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_Align(UINT16 * value) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_Step(UINT16 * value) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_Grow(UINT16 * value) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_HAPTICS_HAPTICS_WAVEFORMS_STATICS3_WIN_H_
