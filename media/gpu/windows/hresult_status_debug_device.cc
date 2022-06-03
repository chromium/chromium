// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/hresult_status_debug_device.h"
#include "media/base/media_serializers.h"
#include "media/base/win/hresult_status_helper.h"

namespace media {

// Only define this method in debug mode.
#if !defined(NDEBUG)

Status AddDebugMessages(Status error, ComD3D11Device device) {
  // MSDN says that this needs to be casted twice, then GetMessage should
  // be called with a malloc.
  Microsoft::WRL::ComPtr<ID3D11Debug> debug_layer;
  if (!SUCCEEDED(device.As(&debug_layer)))
    return error;

  Microsoft::WRL::ComPtr<ID3D11InfoQueue> message_layer;
  if (!SUCCEEDED(debug_layer.As(&message_layer)))
    return error;

  uint64_t messages_count = message_layer->GetNumStoredMessages();
  if (messages_count == 0)
    return error;

  std::vector<std::string> messages(messages_count, "");
  for (uint64_t i = 0; i < messages_count; i++) {
    SIZE_T size;
    message_layer->GetMessage(i, nullptr, &size);
    D3D11_MESSAGE* message = reinterpret_cast<D3D11_MESSAGE*>(malloc(size));
    if (!message)  // probably OOM - so just stop trying to get more.
      return error;
    message_layer->GetMessage(i, message, &size);
    messages.emplace_back(message->pDescription);
    free(message);
  }

  return std::move(error).WithData("debug_info", messages);
}

#endif  // !defined(NDEBUG)

Status D3D11HresultToStatus(HRESULT hresult,
                            ComD3D11Device device,
                            const char* message,
                            const base::Location& location) {
  if (SUCCEEDED(hresult))
    return OkStatus();
#if !defined(NDEBUG)
  return AddDebugMessages(
      HresultToStatus(hresult, message, StatusCode::kWindowsD3D11Error,
                      location),
      device);
#else   // !defined(NDEBUG)
  return HresultToStatus(hresult, message, StatusCode::kWindowsD3D11Error,
                         location);
#endif  // !defined(NDEBUG)
}

}  // namespace media
