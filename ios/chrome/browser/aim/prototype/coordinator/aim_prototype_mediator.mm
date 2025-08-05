// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/aim/prototype/coordinator/aim_prototype_mediator.h"

#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "components/search_engines/template_url_service.h"
#import "components/search_engines/util.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ui/base/page_transition_types.h"
#import "url/gurl.h"

@implementation AIMPrototypeMediator {
  NSMutableArray<UIImage*>* _images;
  raw_ptr<UrlLoadingBrowserAgent> _urlLoadingBrowserAgent;
  raw_ptr<TemplateURLService> _templateURLService;
}

- (instancetype)initWithUrlLoadingBrowserAgent:
                    (UrlLoadingBrowserAgent*)urlLoadingBrowserAgent
                            templateURLService:
                                (TemplateURLService*)templateURLService {
  self = [super init];
  if (self) {
    _images = [NSMutableArray array];
    _urlLoadingBrowserAgent = urlLoadingBrowserAgent;
    _templateURLService = templateURLService;
  }
  return self;
}

- (void)processImage:(UIImage*)image {
  // TODO(crbug.com/40280872): Implement image processing.
  [_images addObject:image];
  [self.consumer setImages:_images];
}

- (void)disconnect {
  _urlLoadingBrowserAgent = nullptr;
  _templateURLService = nullptr;
}

#pragma mark - AIMPrototypeMutator

- (void)sendText:(NSString*)text {
  // TODO(crbug.com/40280872): Add multimodal capabilities.
  // TODO(crbug.com/40280872): Replace hardcoded entrypoint.
  GURL url = GetUrlForAim(_templateURLService, "62", base::Time::Now(),
                          base::SysNSStringToUTF16(text));
  UrlLoadParams params = UrlLoadParams::InCurrentTab(url);
  params.web_params.transition_type = ui::PAGE_TRANSITION_GENERATED;
  _urlLoadingBrowserAgent->Load(params);
  [self.delegate dismissAimPrototype];
}

@end
