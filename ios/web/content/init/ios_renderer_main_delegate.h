// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_CONTENT_INIT_IOS_RENDERER_MAIN_DELEGATE_H_
#define IOS_WEB_CONTENT_INIT_IOS_RENDERER_MAIN_DELEGATE_H_

#import "build/blink_buildflags.h"
#import "content/public/app/content_main_delegate.h"
#import "content/public/common/content_client.h"
#import "content/public/renderer/content_renderer_client.h"

#if !BUILDFLAG(USE_BLINK)
#error File can only be included when USE_BLINK is true
#endif

namespace web {

class IOSRendererMainDelegate : public content::ContentMainDelegate {
 public:
  IOSRendererMainDelegate();
  IOSRendererMainDelegate(const IOSRendererMainDelegate&) = delete;
  IOSRendererMainDelegate& operator=(const IOSRendererMainDelegate&) = delete;
  ~IOSRendererMainDelegate() override;

  // ContentMainDelegate implementation:
  void PreSandboxStartup() override;
  content::ContentClient* CreateContentClient() override;
  content::ContentRendererClient* CreateContentRendererClient() override;

 private:
  std::unique_ptr<content::ContentClient> content_client_;
  std::unique_ptr<content::ContentRendererClient> renderer_client_;
};

}  // namespace web

#endif  // IOS_WEB_CONTENT_INIT_IOS_RENDERER_MAIN_DELEGATE_H_
