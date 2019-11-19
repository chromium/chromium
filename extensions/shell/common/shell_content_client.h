// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_COMMON_SHELL_CONTENT_CLIENT_H_
#define EXTENSIONS_SHELL_COMMON_SHELL_CONTENT_CLIENT_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "content/public/common/content_client.h"
#include "url/url_util.h"

namespace extensions {

class ShellContentClient : public content::ContentClient {
 public:
  ShellContentClient();
  ~ShellContentClient() override;

  void AddPepperPlugins(
      std::vector<content::PepperPluginInfo>* plugins) override;
  void AddAdditionalSchemes(Schemes* schemes) override;
  base::string16 GetLocalizedString(int message_id) override;
  base::StringPiece GetDataResource(int resource_id,
                                    ui::ScaleFactor scale_factor) override;
  base::RefCountedMemory* GetDataResourceBytes(int resource_id) override;
  gfx::Image& GetNativeImageNamed(int resource_id) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ShellContentClient);
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_COMMON_SHELL_CONTENT_CLIENT_H_
