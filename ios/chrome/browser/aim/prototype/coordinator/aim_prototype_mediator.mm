// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/aim/prototype/coordinator/aim_prototype_mediator.h"

#import <memory>

#import "base/files/file_path.h"
#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/ref_counted_memory.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "base/time/time.h"
#import "base/unguessable_token.h"
#import "components/lens/contextual_input.h"
#import "components/lens/lens_bitmap_processing.h"
#import "components/omnibox/composebox/ios/composebox_file_upload_observer_bridge.h"
#import "components/omnibox/composebox/ios/composebox_query_controller_ios.h"
#import "components/search_engines/template_url_service.h"
#import "components/search_engines/util.h"
#import "ios/chrome/browser/aim/prototype/public/features.h"
#import "ios/chrome/browser/aim/prototype/ui/aim_input_item.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "net/base/url_util.h"
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
  // Whether AI mode is enabled.
  BOOL _AIModeEnabled;
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
  GURL URL = _composeboxQueryController->CreateAimUrl(
      base::SysNSStringToUTF8(text), base::Time::Now());
  // TODO(crbug.com/40280872): Handle AIM enabled in the query controller.
  if (!_AIModeEnabled) {
    URL = net::AppendOrReplaceQueryParameter(URL, "udm", "24");
  }
  UrlLoadParams params = UrlLoadParams::InCurrentTab(URL);
  params.web_params.transition_type = ui::PAGE_TRANSITION_GENERATED;
  _urlLoadingBrowserAgent->Load(params);
  [self.delegate dismissAimPrototype];
}

- (void)setAIModeEnabled:(BOOL)enabled {
  _AIModeEnabled = enabled;
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

#pragma mark - LoadQueryCommands

- (void)loadQuery:(NSString*)query immediately:(BOOL)immediately {
  [self sendText:query];
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
    item.state = AIMInputItemState::kError;
    [self.consumer updateState:item.state forItemWithToken:item.fileToken];
    return;
  }

  __weak __typeof(self) weakSelf = self;
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, base::BindOnce(^{
        [weakSelf didFinishSimulatedLoadForImage:image item:item];
      }),
      GetImageLoadDelay());
}

// Called after the simulated image load delay.
- (void)didFinishSimulatedLoadForImage:(UIImage*)image
                                  item:(AIMInputItem*)item {
  item.state = AIMInputItemState::kUploading;
  [self.consumer updateState:item.state forItemWithToken:item.fileToken];

  if (!item.previewImage) {
    item.previewImage = image;
    [self.consumer setItems:_items];
  }

  base::OnceClosure task;
  __weak __typeof(self) weakSelf = self;
  if (ShouldForceUploadFailure()) {
    task = base::BindOnce(^{
      [weakSelf onFileUploadStatusChanged:item.fileToken
                                 mimeType:lens::MimeType::kImage
                         fileUploadStatus:FileUploadStatus::kUploadFailed
                                errorType:std::nullopt];
    });
  } else {
    task = base::BindOnce(^{
      [weakSelf uploadImage:image forItem:item];
    });
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, std::move(task), GetUploadDelay());
}

// Uploads the `image` for the given `item`.
- (void)uploadImage:(UIImage*)image forItem:(AIMInputItem*)item {
  std::unique_ptr<lens::ContextualInputData> input_data =
      std::make_unique<lens::ContextualInputData>();
  input_data->context_input = std::vector<lens::ContextualInput>();
  input_data->primary_content_type = lens::MimeType::kImage;

  NSData* data = UIImagePNGRepresentation(image);
  std::vector<uint8_t> vector_data([data length]);
  [data getBytes:vector_data.data() length:[data length]];

  input_data->context_input->push_back(
      lens::ContextualInput(std::move(vector_data), lens::MimeType::kImage));

  // TODO(crbug.com/40280872): Plumb encoding options from a central config.
  lens::ImageEncodingOptions image_options;
  image_options.max_width = 1024;
  image_options.max_height = 1024;
  image_options.compression_quality = 80;

  _composeboxQueryController->StartFileUploadFlow(
      item.fileToken, std::move(input_data), image_options);
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
