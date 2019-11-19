// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_COMMON_WEB_ENGINE_CONTENT_CLIENT_H_
#define FUCHSIA_ENGINE_COMMON_WEB_ENGINE_CONTENT_CLIENT_H_

#include "base/macros.h"
#include "content/public/common/content_client.h"

class WebEngineContentClient : public content::ContentClient {
 public:
  // URL scheme used to access content directories.
  static const char kFuchsiaContentDirectoryScheme[];

  WebEngineContentClient();
  ~WebEngineContentClient() override;

  // content::ContentClient implementation.
  base::string16 GetLocalizedString(int message_id) override;
  base::StringPiece GetDataResource(int resource_id,
                                    ui::ScaleFactor scale_factor) override;
  base::RefCountedMemory* GetDataResourceBytes(int resource_id) override;
  gfx::Image& GetNativeImageNamed(int resource_id) override;
  blink::OriginTrialPolicy* GetOriginTrialPolicy() override;
  void AddAdditionalSchemes(Schemes* schemes) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebEngineContentClient);
};

#endif  // FUCHSIA_ENGINE_COMMON_WEB_ENGINE_CONTENT_CLIENT_H_
