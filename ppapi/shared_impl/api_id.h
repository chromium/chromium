// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_API_ID_H_
#define PPAPI_SHARED_IMPL_API_ID_H_

namespace ppapi {

// These numbers must be all small integers. They are used in a lookup table
// to route messages to the appropriate message handler.
enum ApiID {
  // Zero is reserved for control messages.
  API_ID_NONE = 0,
  API_ID_PPB_AUDIO = 1,
  API_ID_PPB_BUFFER,
  API_ID_PPB_CORE,
  API_ID_PPB_GRAPHICS_3D,
  API_ID_PPB_IMAGE_DATA,
  API_ID_PPB_INSTANCE,
  API_ID_PPB_INSTANCE_PRIVATE,
  API_ID_PPB_TESTING,
  API_ID_PPB_VAR_ARRAY_BUFFER,
  API_ID_PPB_VAR_DEPRECATED,
  API_ID_PPB_VIDEO_CAPTURE_DEV,
  API_ID_PPB_VIDEO_DECODER_DEV,
  API_ID_PPB_X509_CERTIFICATE_PRIVATE,
  API_ID_PPP_CLASS,
  API_ID_PPP_GRAPHICS_3D,
  API_ID_PPP_INPUT_EVENT,
  API_ID_PPP_INSTANCE,
  API_ID_PPP_INSTANCE_PRIVATE,
  API_ID_PPP_MESSAGING,
  API_ID_PPP_MOUSE_LOCK,
  API_ID_PPP_PRINTING,
  API_ID_PPP_TEXT_INPUT,
  API_ID_PPP_VIDEO_DECODER_DEV,
  API_ID_RESOURCE_CREATION,

  // Must be last to indicate the number of interface IDs.
  API_ID_COUNT
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_API_ID_H_
