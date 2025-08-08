// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/aim/prototype/coordinator/aim_prototype_mediator.h"

#import "base/files/file_path.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/ref_counted_memory.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "base/unguessable_token.h"
#import "components/omnibox/composebox/ios/composebox_query_controller_ios.h"
#import "components/search_engines/template_url_service.h"
#import "components/search_engines/util.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ui/base/page_transition_types.h"
#import "url/gurl.h"

@implementation AIMPrototypeMediator {
  NSMutableArray<UIImage*>* _images;
  raw_ptr<UrlLoadingBrowserAgent> _urlLoadingBrowserAgent;
  std::unique_ptr<ComposeboxQueryControllerIOS> _composeboxQueryController;
}

- (instancetype)initWithUrlLoadingBrowserAgent:
                    (UrlLoadingBrowserAgent*)urlLoadingBrowserAgent
                     composeboxQueryController:
                         (std::unique_ptr<ComposeboxQueryControllerIOS>)
                             composeboxQueryController {
  self = [super init];
  if (self) {
    _images = [NSMutableArray array];
    _urlLoadingBrowserAgent = urlLoadingBrowserAgent;
    _composeboxQueryController = std::move(composeboxQueryController);
    _composeboxQueryController->NotifySessionStarted();
  }
  return self;
}

- (void)processImage:(UIImage*)image {
  [_images addObject:image];
  [self.consumer setImages:_images];

  auto file_info = std::make_unique<ComposeboxQueryController::FileInfo>();
  file_info->file_token_ = base::UnguessableToken::Create();
  file_info->file_name = "image.png";
  file_info->mime_type_ = lens::MimeType::kImage;

  NSData* data = UIImagePNGRepresentation(image);
  std::vector<uint8_t> vector_data([data length]);
  [data getBytes:vector_data.data() length:[data length]];
  scoped_refptr<base::RefCountedBytes> bytes =
      base::MakeRefCounted<base::RefCountedBytes>(std::move(vector_data));

  // TODO(crbug.com/40280872): Plumb encoding options from a central config.
  composebox::ImageEncodingOptions image_options;
  image_options.max_width = 1024;
  image_options.max_height = 1024;
  image_options.compression_quality = 80;

  _composeboxQueryController->StartFileUploadFlow(std::move(file_info), bytes,
                                                  image_options);
}

- (void)disconnect {
  _composeboxQueryController->NotifySessionAbandoned();
  _urlLoadingBrowserAgent = nullptr;
  _composeboxQueryController.reset();
}

#pragma mark - AIMPrototypeMutator

- (void)sendText:(NSString*)text {
  GURL url = _composeboxQueryController->CreateAimUrl(
      base::SysNSStringToUTF8(text), base::Time::Now());
  UrlLoadParams params = UrlLoadParams::InCurrentTab(url);
  params.web_params.transition_type = ui::PAGE_TRANSITION_GENERATED;
  _urlLoadingBrowserAgent->Load(params);
  [self.delegate dismissAimPrototype];
}

@end
