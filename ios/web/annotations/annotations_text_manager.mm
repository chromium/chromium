// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/annotations/annotations_text_manager.h"
#import "ios/web/annotations/annotations_java_script_feature.h"
#import "ios/web/annotations/annotations_text_manager_impl.h"

namespace web {

void AnnotationsTextManager::CreateForWebState(WebState* web_state) {
  DCHECK(web_state);
  if (!FromWebState(web_state)) {
    web_state->SetUserData(
        UserDataKey(), std::make_unique<AnnotationsTextManagerImpl>(web_state));
  }
}

ContentWorld AnnotationsTextManager::GetFeatureContentWorld() {
  return AnnotationsJavaScriptFeature::GetInstance()
      ->GetSupportedContentWorld();
}

}  // namespace web
