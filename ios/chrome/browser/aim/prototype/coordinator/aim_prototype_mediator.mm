// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/aim/prototype/coordinator/aim_prototype_mediator.h"

#import <PDFKit/PDFKit.h>

#import <memory>

#import "base/files/file_path.h"
#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/ref_counted_memory.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/bind_post_task.h"
#import "base/task/thread_pool.h"
#import "base/time/time.h"
#import "base/unguessable_token.h"
#import "components/lens/contextual_input.h"
#import "components/omnibox/composebox/ios/composebox_file_upload_observer_bridge.h"
#import "components/omnibox/composebox/ios/composebox_query_controller_ios.h"
#import "components/search_engines/template_url_service.h"
#import "components/search_engines/util.h"
#import "ios/chrome/browser/aim/prototype/public/features.h"
#import "ios/chrome/browser/aim/prototype/ui/aim_input_item.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "net/base/apple/url_conversions.h"
#import "net/base/url_util.h"
#import "ui/base/page_transition_types.h"
#import "url/gurl.h"

namespace {

// Reads data from a file URL. Runs on a background thread.
NSData* ReadDataFromURL(GURL url) {
  NSURL* ns_url = net::NSURLWithGURL(url);
  BOOL accessing = [ns_url startAccessingSecurityScopedResource];

  NSData* data = nil;
  @try {
    // Always attempt to read the data. This will work for non-scoped URLs,
    // and for scoped URLs if `accessing` is true. It will fail if `accessing`
    // is false for a scoped URL, which is the correct behavior.
    data = [NSData dataWithContentsOfURL:ns_url];
  } @finally {
    // Only stop accessing if we were successfully granted access.
    if (accessing) {
      [ns_url stopAccessingSecurityScopedResource];
    }
  }
  return data;
}

// Generates a UIImage preview for the given PDF data.
UIImage* GeneratePDFPreview(NSData* pdf_data) {
  if (!pdf_data) {
    return nil;
  }
  PDFDocument* doc = [[PDFDocument alloc] initWithData:pdf_data];
  if (!doc) {
    return nil;
  }
  PDFPage* page = [doc pageAtIndex:0];
  if (!page) {
    return nil;
  }
  // TODO(crbug.com/40280872): Determine the correct size for the thumbnail.
  return [page thumbnailOfSize:CGSizeMake(200, 200)
                        forBox:kPDFDisplayBoxCropBox];
}

}  // namespace

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
  const base::UnguessableToken& token = item.fileToken;

  __weak __typeof(self) weakSelf = self;
  // Load the preview image.
  [itemProvider
      loadPreviewImageWithOptions:nil
                completionHandler:^(UIImage* previewImage, NSError* error) {
                  dispatch_async(dispatch_get_main_queue(), ^{
                    [weakSelf didLoadPreviewImage:previewImage
                                 forItemWithToken:token];
                  });
                }];

  // Concurrently load the full image.
  [itemProvider loadObjectOfClass:[UIImage class]
                completionHandler:^(__kindof id<NSItemProviderReading> object,
                                    NSError* error) {
                  dispatch_async(dispatch_get_main_queue(), ^{
                    [weakSelf didLoadFullImage:(UIImage*)object
                              forItemWithToken:token];
                  });
                }];
}

- (void)processPDFFileURL:(GURL)PDFFileURL {
  AIMInputItem* item = [[AIMInputItem alloc] init];
  [_items addObject:item];
  [self.consumer setItems:_items];
  const base::UnguessableToken& token = item.fileToken;

  // Read the data in the background then call `onDataReadForItem`.
  __weak __typeof(self) weakSelf = self;
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&ReadDataFromURL, PDFFileURL),
      base::BindPostTaskToCurrentDefault(base::BindOnce(^(NSData* data) {
        AIMPrototypeMediator* strongSelf = weakSelf;
        if (!strongSelf) {
          return;
        }
        [strongSelf onDataReadForItemWithToken:token
                                       fromURL:PDFFileURL
                                      withData:data];
      })));
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

- (void)attachCurrentTabContent {
  // TODO(crbug.com/442564280): Attach the current tab content to the user's
  // query.
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

// Handles the loaded preview `image` for the item with the given `token`.
- (void)didLoadPreviewImage:(UIImage*)previewImage
           forItemWithToken:(const base::UnguessableToken&)token {
  AIMInputItem* item = [self itemForToken:token];
  if (!item) {
    return;
  }
  // Only set the preview if a preview doesn't already exist. This prevents
  // overwriting the full-res image if it arrives first.
  if (previewImage && !item.previewImage) {
    item.previewImage = previewImage;
    [self.consumer updateState:item.state forItemWithToken:item.fileToken];
  }
}

// Handles the loaded full `image` for the item with the given `token`.
- (void)didLoadFullImage:(UIImage*)image
        forItemWithToken:(const base::UnguessableToken&)token {
  AIMInputItem* item = [self itemForToken:token];
  if (!item) {
    return;
  }

  if (!image) {
    item.state = AIMInputItemState::kError;
    [self.consumer updateState:item.state forItemWithToken:item.fileToken];
    return;
  }

  __weak __typeof(self) weakSelf = self;
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, base::BindOnce(^{
        [weakSelf didFinishSimulatedLoadForImage:image itemToken:token];
      }),
      GetImageLoadDelay());
}

// Called after the simulated image load delay for the item with the given
// `token`. This simulates a network delay for development purposes.
- (void)didFinishSimulatedLoadForImage:(UIImage*)image
                             itemToken:(const base::UnguessableToken&)token {
  AIMInputItem* item = [self itemForToken:token];
  if (!item) {
    return;
  }

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
      [weakSelf onFileUploadStatusChanged:token
                                 mimeType:lens::MimeType::kImage
                         fileUploadStatus:FileUploadStatus::kUploadFailed
                                errorType:std::nullopt];
    });
  } else {
    task = base::BindOnce(^{
      [weakSelf uploadImage:image itemToken:token];
    });
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, std::move(task), GetUploadDelay());
}

// Uploads the `image` for the item with the given `token`.
- (void)uploadImage:(UIImage*)image
          itemToken:(const base::UnguessableToken&)token {
  AIMInputItem* item = [self itemForToken:token];
  if (!item) {
    return;
  }
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
  composebox::ImageEncodingOptions image_options;
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

// Handles the read `data` from the given `url` for the item with the given
// `token`. This is the callback for the asynchronous file read.
- (void)onDataReadForItemWithToken:(const base::UnguessableToken&)token
                           fromURL:(GURL)url
                          withData:(NSData*)data {
  AIMInputItem* item = [self itemForToken:token];
  if (!item) {
    return;
  }

  if (!data) {
    item.state = AIMInputItemState::kError;
    [self.consumer updateState:item.state forItemWithToken:item.fileToken];
    return;
  }

  // Start the file upload immediately.
  item.state = AIMInputItemState::kUploading;
  [self.consumer updateState:item.state forItemWithToken:item.fileToken];

  std::unique_ptr<lens::ContextualInputData> inputData =
      std::make_unique<lens::ContextualInputData>();
  inputData->context_input = std::vector<lens::ContextualInput>();
  inputData->primary_content_type = lens::MimeType::kPdf;
  inputData->page_url = url;
  inputData->page_title = url.ExtractFileName();

  std::vector<uint8_t> vectorData([data length]);
  [data getBytes:vectorData.data() length:[data length]];
  inputData->context_input->push_back(
      lens::ContextualInput(std::move(vectorData), lens::MimeType::kPdf));
  _composeboxQueryController->StartFileUploadFlow(
      item.fileToken, std::move(inputData), std::nullopt);

  // Concurrently, generate a preview for the UI.
  __weak __typeof(self) weakSelf = self;
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&GeneratePDFPreview, data),
      base::BindPostTaskToCurrentDefault(base::BindOnce(^(UIImage* preview) {
        AIMPrototypeMediator* strongSelf = weakSelf;
        if (!strongSelf) {
          return;
        }
        [strongSelf didLoadPreviewImage:preview forItemWithToken:token];
      })));
}

@end
