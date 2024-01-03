// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_COMMON_SHELL_CONTENT_CLIENT_H_
#define EXTENSIONS_SHELL_COMMON_SHELL_CONTENT_CLIENT_H_

#include <string_view>

#include "content/public/common/content_client.h"

namespace extensions {

class ShellContentClient : public content::ContentClient {
 public:
  ShellContentClient();

  ShellContentClient(const ShellContentClient&) = delete;
  ShellContentClient& operator=(const ShellContentClient&) = delete;

  ~ShellContentClient() override;

  void AddPlugins(std::vector<content::ContentPluginInfo>* plugins) override;
  void AddAdditionalSchemes(Schemes* schemes) override;
  std::u16string GetLocalizedString(int message_id) override;
  std::string_view GetDataResource(
      int resource_id,
      ui::ResourceScaleFactor scale_factor) override;
  base::RefCountedMemory* GetDataResourceBytes(int resource_id) override;
  gfx::Image& GetNativeImageNamed(int resource_id) override;
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_COMMON_SHELL_CONTENT_CLIENT_H_
