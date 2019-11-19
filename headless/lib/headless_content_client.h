// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_HEADLESS_CONTENT_CLIENT_H_
#define HEADLESS_LIB_HEADLESS_CONTENT_CLIENT_H_

#include "content/public/common/content_client.h"
#include "headless/public/headless_browser.h"

namespace headless {

class HeadlessContentClient : public content::ContentClient {
 public:
  HeadlessContentClient();
  ~HeadlessContentClient() override;

  // content::ContentClient implementation:
  base::string16 GetLocalizedString(int message_id) override;
  base::StringPiece GetDataResource(int resource_id,
                                    ui::ScaleFactor scale_factor) override;
  base::RefCountedMemory* GetDataResourceBytes(int resource_id) override;
  gfx::Image& GetNativeImageNamed(int resource_id) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(HeadlessContentClient);
};

}  // namespace headless

#endif  // HEADLESS_LIB_HEADLESS_CONTENT_CLIENT_H_
