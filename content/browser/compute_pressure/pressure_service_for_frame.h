// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPUTE_PRESSURE_PRESSURE_SERVICE_FOR_FRAME_H_
#define CONTENT_BROWSER_COMPUTE_PRESSURE_PRESSURE_SERVICE_FOR_FRAME_H_

#include "base/functional/callback.h"
#include "content/browser/compute_pressure/pressure_service_base.h"
#include "content/common/content_export.h"
#include "content/public/browser/document_user_data.h"

namespace content {

class RenderFrameHost;

class CONTENT_EXPORT PressureServiceForFrame
    : public PressureServiceBase,
      public DocumentUserData<PressureServiceForFrame> {
 public:
  ~PressureServiceForFrame() override;

  PressureServiceForFrame(const PressureServiceForFrame&) = delete;
  PressureServiceForFrame& operator=(const PressureServiceForFrame&) = delete;

  // PressureServiceBase override.
  bool CanCallAddClient() const override;
  bool ShouldDeliverUpdate() const override;
  std::optional<base::UnguessableToken> GetTokenFor(
      device::mojom::PressureSource) const override;

 private:
  explicit PressureServiceForFrame(RenderFrameHost* render_frame_host);

  friend DocumentUserData;
  DOCUMENT_USER_DATA_KEY_DECL();
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPUTE_PRESSURE_PRESSURE_SERVICE_FOR_FRAME_H_
