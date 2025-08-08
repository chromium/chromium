// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/aim/prototype/coordinator/aim_prototype_mediator.h"

#import <memory>

#import "base/files/file_path.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/ref_counted_memory.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "base/unguessable_token.h"
#import "components/omnibox/composebox/ios/composebox_file_upload_observer_bridge.h"
#import "components/omnibox/composebox/ios/composebox_query_controller_ios.h"
#import "components/search_engines/template_url_service.h"
#import "components/search_engines/util.h"
#import "ios/chrome/browser/aim/prototype/ui/aim_input_item.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ui/base/page_transition_types.h"
#import "url/gurl.h"

@implementation AIMPrototypeMediator {
  // The ordered list of items for display.
  NSMutableArray<AIMInputItem*>* _items;
  // The URL loading browser agent.
  raw_ptr<UrlLoadingBrowserAgent> _urlLoadingBrowserAgent;
  // The C++ controller for this feature.
  std::unique_ptr<ComposeboxQueryControllerIOS> _composeboxQueryController;
  // The observer bridge for file upload status.
  std::unique_ptr<ComposeboxFileUploadObserverBridge> _observerBridge;
}

- (instancetype)initWithUrlLoadingBrowserAgent:
                    (UrlLoadingBrowserAgent*)urlLoadingBrowserAgent
                     composeboxQueryController:
                         (std::unique_ptr<ComposeboxQueryControllerIOS>)
                             composeboxQueryController {
  self = [super init];
  if (self) {
    _items = [NSMutableArray array];
    _urlLoadingBrowserAgent = urlLoadingBrowserAgent;
    _composeboxQueryController = std::move(composeboxQueryController);
    _observerBridge = std::make_unique<ComposeboxFileUploadObserverBridge>(
        self, _composeboxQueryController.get());
    _composeboxQueryController->NotifySessionStarted();
  }
  return self;
}

- (void)disconnect {
  _composeboxQueryController->NotifySessionAbandoned();
  _urlLoadingBrowserAgent = nullptr;
  _composeboxQueryController.reset();
  _observerBridge.reset();
}

- (void)processImageItemProvider:(NSItemProvider*)itemProvider {
  if (![itemProvider canLoadObjectOfClass:[UIImage class]]) {
    return;
  }

  AIMInputItem* item = [[AIMInputItem alloc] init];
  [_items addObject:item];
  [self.consumer setItems:_items];

  __weak __typeof(self) weakSelf = self;
  // Load the preview image.
  [itemProvider
      loadPreviewImageWithOptions:nil
                completionHandler:^(UIImage* previewImage, NSError* error) {
                  dispatch_async(dispatch_get_main_queue(), ^{
                    [weakSelf didLoadPreviewImage:previewImage forItem:item];
                  });
                }];

  // Concurrently load the full image.
  [itemProvider loadObjectOfClass:[UIImage class]
                completionHandler:^(__kindof id<NSItemProviderReading> object,
                                    NSError* error) {
                  dispatch_async(dispatch_get_main_queue(), ^{
                    [weakSelf didLoadFullImage:(UIImage*)object forItem:item];
                  });
                }];
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

#pragma mark - ComposeboxFileUploadObserver

- (void)onFileUploadStatusChanged:(const base::UnguessableToken&)fileToken
                         mimeType:(lens::MimeType)mimeType
                 fileUploadStatus:(FileUploadStatus)fileUploadStatus
                        errorType:(const std::optional<FileUploadErrorType>&)
                                      errorType {
  AIMInputItem* item = [self itemForToken:fileToken];
  if (!item) {
    return;
  }

  switch (fileUploadStatus) {
    case FileUploadStatus::kUploadSuccessful:
      item.state = AIMInputItemState::kLoaded;
      break;
    case FileUploadStatus::kUploadFailed:
    case FileUploadStatus::kValidationFailed:
    case FileUploadStatus::kUploadExpired:
      // TODO(crbug.com/40280872): Handle error case in consumer.
      item.state = AIMInputItemState::kError;
      break;
    case FileUploadStatus::kNotUploaded:
    case FileUploadStatus::kProcessing:
    case FileUploadStatus::kUploadStarted:
      // No-op, as the state is already `Uploading`.
      return;
  }

  [self.consumer updateState:item.state forItemWithToken:item.fileToken];
}

#pragma mark - Private

// Handles the loaded preview `image` for the given `item`.
- (void)didLoadPreviewImage:(UIImage*)previewImage forItem:(AIMInputItem*)item {
  // Only set the preview if a preview doesn't already exist. This prevents
  // overwriting the full-res image if it arrives first.
  if (previewImage && !item.previewImage) {
    item.previewImage = previewImage;
    [self.consumer updateState:item.state forItemWithToken:item.fileToken];
  }
}

// Handles the loaded full `image` for the given `item`.
- (void)didLoadFullImage:(UIImage*)image forItem:(AIMInputItem*)item {
  if (!image) {
    // TODO(crbug.com/40280872): Handle error case.
    return;
  }

  item.state = AIMInputItemState::kUploading;
  [self.consumer updateState:item.state forItemWithToken:item.fileToken];

  if (!item.previewImage) {
    item.previewImage = image;
    [self.consumer setItems:_items];
  }

  auto file_info = std::make_unique<ComposeboxQueryController::FileInfo>();
  file_info->file_token_ = item.fileToken;
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

// Returns the item with the given `token` or nil if not found.
- (AIMInputItem*)itemForToken:(const base::UnguessableToken&)token {
  for (AIMInputItem* item in _items) {
    if (item.fileToken == token) {
      return item;
    }
  }
  return nil;
}

@end
