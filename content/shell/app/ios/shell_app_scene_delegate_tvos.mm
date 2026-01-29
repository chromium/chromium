// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "content/shell/app/ios/shell_app_scene_delegate_tvos.h"

#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/shell/browser/shell.h"

@implementation ShellAppSceneDelegateTVOS

- (void)sceneWillEnterForeground:(UIScene*)scene {
  [super sceneWillEnterForeground:scene];

  CHECK_EQ(1u, content::Shell::windows().size());
  content::WebContents* web_contents =
      content::Shell::windows()[0]->web_contents();
  // A visible page is automatically unfrozen and it should not call
  // `SetPageFrozen`.
  if (web_contents->GetVisibility() != content::Visibility::VISIBLE) {
    // Unfreeze the page when entering foreground, triggering
    // `document.onresume` event in JavaScript.
    web_contents->SetPageFrozen(false);
  }
  // Explicitly update visibility when the scene becomes visible, even though
  // WebContents is initially created with `Visibility::VISIBLE`.
  // See `WebContentsImpl::UpdateWebContentsVisibility`.
  web_contents->UpdateWebContentsVisibility(content::Visibility::VISIBLE);
}

- (void)sceneDidBecomeActive:(UIScene*)scene {
  CHECK_EQ(1u, content::Shell::windows().size());
  content::WebContents* web_contents =
      content::Shell::windows()[0]->web_contents();
  web_contents->GetRenderViewHost()->GetWidget()->Focus();
}

- (void)sceneWillResignActive:(UIScene*)scene {
  CHECK_EQ(1u, content::Shell::windows().size());
  content::WebContents* web_contents =
      content::Shell::windows()[0]->web_contents();
  web_contents->GetRenderViewHost()->GetWidget()->Blur();
}

- (void)sceneDidEnterBackground:(UIScene*)scene {
  CHECK_EQ(1u, content::Shell::windows().size());
  content::WebContents* web_contents =
      content::Shell::windows()[0]->web_contents();
  web_contents->UpdateWebContentsVisibility(content::Visibility::HIDDEN);
  // Freeze the page when entering background, triggering `document.onfreeze`
  // event in JavaScript.
  web_contents->SetPageFrozen(true);
}

@end
