// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/aim/prototype/coordinator/aim_prototype_composebox_mediator.h"

#import <PDFKit/PDFKit.h>

#import <memory>

#import "base/files/file_path.h"
#import "base/functional/bind.h"
#import "base/memory/ptr_util.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/ref_counted_memory.h"
#import "base/sequence_checker.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/task/bind_post_task.h"
#import "base/task/thread_pool.h"
#import "base/time/time.h"
#import "base/unguessable_token.h"
#import "components/lens/contextual_input.h"
#import "components/lens/lens_bitmap_processing.h"
#import "components/omnibox/composebox/ios/composebox_file_upload_observer_bridge.h"
#import "components/omnibox/composebox/ios/composebox_query_controller_ios.h"
#import "components/search_engines/template_url_service.h"
#import "components/search_engines/util.h"
#import "ios/chrome/browser/aim/prototype/coordinator/aim_prototype_url_loader.h"
#import "ios/chrome/browser/aim/prototype/public/features.h"
#import "ios/chrome/browser/aim/prototype/ui/aim_input_item.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper.h"
#import "ios/chrome/browser/shared/model/url/url_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/common/NSString+Chromium.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/web/public/web_state.h"
#import "net/base/apple/url_conversions.h"
#import "net/base/url_util.h"
#import "ui/base/page_transition_types.h"
#import "ui/gfx/favicon_size.h"
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

// Creates an initial ContextualInputData object using the information from the
// passed in `annotated_page_content` and `web_state`.
std::unique_ptr<lens::ContextualInputData>
CreateInputDataFromAnnotatedPageContent(
    std::unique_ptr<optimization_guide::proto::AnnotatedPageContent>
        annotated_page_content,
    web::WebState* web_state) {
  if (!annotated_page_content || !web_state) {
    return nullptr;
  }

  std::string serialized_context;
  annotated_page_content->SerializeToString(&serialized_context);

  std::vector<uint8_t> vector_data(serialized_context.begin(),
                                   serialized_context.end());

  auto input_data = std::make_unique<lens::ContextualInputData>();
  input_data->primary_content_type = lens::MimeType::kAnnotatedPageContent;
  input_data->context_input = std::vector<lens::ContextualInput>();
  input_data->context_input->push_back(lens::ContextualInput(
      std::move(vector_data), lens::MimeType::kAnnotatedPageContent));

  input_data->page_url = web_state->GetVisibleURL();
  input_data->page_title = base::UTF16ToUTF8(web_state->GetTitle());
  return input_data;
}

}  // namespace

@implementation AIMPrototypeComposeboxMediator {
  // The ordered list of items for display.
  NSMutableArray<AIMInputItem*>* _items;
  // The C++ controller for this feature.
  std::unique_ptr<ComposeboxQueryControllerIOS> _composeboxQueryController;
  // The observer bridge for file upload status.
  std::unique_ptr<ComposeboxFileUploadObserverBridge> _composeboxObserverBridge;
  // Whether AI mode is enabled.
  BOOL _AIModeEnabled;
  // The web state list.
  raw_ptr<WebStateList> _webStateList;
  // A page context wrapper used to extract annotated page content (APC).
  PageContextWrapper* _pageContextWrapper;
  // The favicon loader.
  raw_ptr<FaviconLoader> _faviconLoader;
  // Check that the different methods are called from the correct sequence, as
  // this class defers work via PostTask APIs.
  SEQUENCE_CHECKER(_sequenceChecker);
}

- (instancetype)
    initWithComposeboxQueryController:
        (std::unique_ptr<ComposeboxQueryControllerIOS>)composeboxQueryController
                         webStateList:(WebStateList*)webStateList
                        faviconLoader:(FaviconLoader*)faviconLoader {
  self = [super init];
  if (self) {
    _items = [NSMutableArray array];
    _composeboxQueryController = std::move(composeboxQueryController);
    _composeboxObserverBridge =
        std::make_unique<ComposeboxFileUploadObserverBridge>(
            self, _composeboxQueryController.get());
    _composeboxQueryController->NotifySessionStarted();
    _webStateList = webStateList;
    _faviconLoader = faviconLoader;
  }
  return self;
}

- (void)disconnect {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  _composeboxQueryController->NotifySessionAbandoned();
  _faviconLoader = nullptr;
  _composeboxObserverBridge.reset();
  _composeboxQueryController.reset();
}

- (void)processImageItemProvider:(NSItemProvider*)itemProvider {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (![itemProvider canLoadObjectOfClass:[UIImage class]]) {
    return;
  }

  AIMInputItem* item = [[AIMInputItem alloc]
      initWithAimInputItemType:AIMInputItemType::kAIMInputItemTypeImage];
  [_items addObject:item];
  [self updateConsumerItems];
  const base::UnguessableToken token = item.token;

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

- (void)setConsumer:(id<AIMPrototypeComposeboxConsumer>)consumer {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  _consumer = consumer;

  if (!_webStateList) {
    return;
  }

  web::WebState* webState = _webStateList->GetActiveWebState();
  BOOL canAttachTab = webState && !IsUrlNtp(webState->GetVisibleURL());
  [_consumer setCanAttachTabAction:canAttachTab];
  if (base::FeatureList::IsEnabled(kAIMPrototypeAutoattachTab) &&
      canAttachTab) {
    [self attachCurrentTabContent];
  }
}

- (void)processPDFFileURL:(GURL)PDFFileURL {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  AIMInputItem* item = [[AIMInputItem alloc]
      initWithAimInputItemType:AIMInputItemType::kAIMInputItemTypeFile];
  item.title = base::SysUTF8ToNSString(PDFFileURL.ExtractFileName());
  [_items addObject:item];
  [self updateConsumerItems];
  const base::UnguessableToken token = item.token;

  // Read the data in the background then call `onDataReadForItem`.
  __weak __typeof(self) weakSelf = self;
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&ReadDataFromURL, PDFFileURL),
      base::BindOnce(^(NSData* data) {
        [weakSelf onDataReadForItemWithToken:token
                                     fromURL:PDFFileURL
                                    withData:data];
      }));
}

#pragma mark - AIMPrototypeComposeboxMutator

- (void)removeItem:(AIMInputItem*)item {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  [_items removeObject:item];

  if (_composeboxQueryController) {
    _composeboxQueryController->DeleteFile(item.token);
  }

  if (base::FeatureList::IsEnabled(kAIMPrototypeAutoattachTab) &&
      _items.count == 0) {
    [self.consumer setAIModeEnabled:NO];
  }

  [self updateConsumerItems];
}

- (void)sendText:(NSString*)text {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  std::unique_ptr<ComposeboxQueryController::CreateSearchUrlRequestInfo>
      search_url_request_info = std::make_unique<
          ComposeboxQueryController::CreateSearchUrlRequestInfo>();
  search_url_request_info->query_text = base::SysNSStringToUTF8(text);
  search_url_request_info->query_start_time = base::Time::Now();
  GURL URL = _composeboxQueryController->CreateSearchUrl(
      std::move(search_url_request_info));
  // TODO(crbug.com/40280872): Handle AIM enabled in the query controller.
  if (!_AIModeEnabled) {
    URL = net::AppendOrReplaceQueryParameter(URL, "udm", "24");
  }
  [self.URLLoader loadURL:URL];
}

- (void)setAIModeEnabled:(BOOL)enabled {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  _AIModeEnabled = enabled;
  if (!_AIModeEnabled) {
    if (_composeboxQueryController) {
      _composeboxQueryController->ClearFiles();
    }
    [_items removeAllObjects];
    [self.consumer setItems:_items];
  }
}

- (void)attachCurrentTabContent {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  web::WebState* webState = _webStateList->GetActiveWebState();
  if (!webState) {
    return;
  }

  __weak __typeof(self) weakSelf = self;
  _pageContextWrapper = [[PageContextWrapper alloc]
        initWithWebState:webState
      completionCallback:base::BindOnce(^(
                             PageContextWrapperCallbackResponse response) {
        if (response.has_value()) {
          [weakSelf didGetPageContext:std::move(response)];
        }
      })];
  _pageContextWrapper.shouldGetAnnotatedPageContent = YES;
  _pageContextWrapper.shouldGetSnapshot = YES;
  [_pageContextWrapper populatePageContextFieldsAsync];
}

#pragma mark - ComposeboxFileUploadObserver

- (void)onFileUploadStatusChanged:(const base::UnguessableToken&)fileToken
                         mimeType:(lens::MimeType)mimeType
                 fileUploadStatus:(FileUploadStatus)fileUploadStatus
                        errorType:(const std::optional<FileUploadErrorType>&)
                                      errorType {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
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
    case FileUploadStatus::kProcessingSuggestSignalsReady:
    case FileUploadStatus::kUploadStarted:
      // No-op, as the state is already `Uploading`.
      return;
  }

  [self.consumer updateState:item.state forItemWithToken:item.token];
}

#pragma mark - LoadQueryCommands

- (void)loadQuery:(NSString*)query immediately:(BOOL)immediately {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  [self sendText:query];
}

#pragma mark - Private

// Handles the loaded preview `image` for the item with the given `token`.
- (void)didLoadPreviewImage:(UIImage*)previewImage
           forItemWithToken:(base::UnguessableToken)token {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  AIMInputItem* item = [self itemForToken:token];
  if (!item) {
    return;
  }
  // Only set the preview if a preview doesn't already exist. This prevents
  // overwriting the full-res image if it arrives first.
  if (previewImage && !item.previewImage) {
    item.previewImage = previewImage;
    [self.consumer updateState:item.state forItemWithToken:item.token];
  }
}

// Handles the loaded favicon `image` for the item with the given `token`.
- (void)didLoadFaviconIcon:(UIImage*)faviconImage
          forItemWithToken:(base::UnguessableToken)token {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  AIMInputItem* item = [self itemForToken:token];
  if (!item) {
    return;
  }

  // Update the item's leading icon with the latest fetched favicon.
  if (faviconImage && faviconImage != item.leadingIconImage) {
    item.leadingIconImage = faviconImage;
    [self.consumer updateState:item.state forItemWithToken:item.token];
  }
}

// Handles the loaded full `image` for the item with the given `token`.
- (void)didLoadFullImage:(UIImage*)image
        forItemWithToken:(base::UnguessableToken)token {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  AIMInputItem* item = [self itemForToken:token];
  if (!item) {
    return;
  }

  if (!image) {
    item.state = AIMInputItemState::kError;
    [self.consumer updateState:item.state forItemWithToken:item.token];
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
                             itemToken:(base::UnguessableToken)token {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  AIMInputItem* item = [self itemForToken:token];
  if (!item) {
    return;
  }

  item.state = AIMInputItemState::kUploading;
  [self.consumer updateState:item.state forItemWithToken:item.token];

  if (!item.previewImage) {
    item.previewImage = image;
    [self updateConsumerItems];
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
- (void)uploadImage:(UIImage*)image itemToken:(base::UnguessableToken)token {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
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
  lens::ImageEncodingOptions image_options;
  image_options.max_width = 1024;
  image_options.max_height = 1024;
  image_options.compression_quality = 80;

  _composeboxQueryController->StartFileUploadFlow(
      item.token, std::move(input_data), image_options);
}

// Returns the item with the given `token` or nil if not found.
- (AIMInputItem*)itemForToken:(base::UnguessableToken)token {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  for (AIMInputItem* item in _items) {
    if (item.token == token) {
      return item;
    }
  }
  return nil;
}

// Transforms the page context into input data and uploads the data after a page
// snapshot is generated.
- (void)didGetPageContext:
    (PageContextWrapperCallbackResponse)pageContextResponse {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  web::WebState* webState = _webStateList->GetActiveWebState();

  if (!pageContextResponse.has_value() || !webState) {
    return;
  }

  AIMInputItem* item = [[AIMInputItem alloc]
      initWithAimInputItemType:AIMInputItemType::kAIMInputItemTypeTab];
  item.title = base::SysUTF16ToNSString(webState->GetTitle());
  [_items addObject:item];
  [self updateConsumerItems];
  const base::UnguessableToken token = item.token;

  std::unique_ptr<optimization_guide::proto::PageContext> page_context =
      std::move(pageContextResponse.value());

  __block std::unique_ptr<lens::ContextualInputData> input_data =
      CreateInputDataFromAnnotatedPageContent(
          base::WrapUnique(page_context->release_annotated_page_content()),
          webState);

  __weak __typeof(self) weakSelf = self;

  /// Based on the favicon loader API, this callback could be called twice.
  auto faviconLoadedBlock = ^(FaviconAttributes* attributes) {
    if (attributes.faviconImage) {
      [weakSelf didLoadFaviconIcon:attributes.faviconImage
                  forItemWithToken:token];
    }
  };

  _faviconLoader->FaviconForPageUrl(
      webState->GetVisibleURL(), gfx::kFaviconSize, gfx::kFaviconSize,
      /*fallback_to_google_server=*/true, faviconLoadedBlock);

  SnapshotTabHelper::FromWebState(webState)->RetrieveColorSnapshot(
      ^(UIImage* image) {
        [weakSelf didRetrieveColorSnapshot:image
                                 inputData:std::move(input_data)
                                     token:token];
      });
}

// Handles uploading the context after the snapshot is generated.
- (void)didRetrieveColorSnapshot:(UIImage*)image
                       inputData:(std::unique_ptr<lens::ContextualInputData>)
                                     input_data
                           token:(base::UnguessableToken)token {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (image) {
    NSData* data = UIImagePNGRepresentation(image);
    std::vector<uint8_t> image_vector_data([data length]);
    [data getBytes:image_vector_data.data() length:[data length]];
    input_data->viewport_screenshot_bytes = std::move(image_vector_data);
  }
  [self didLoadPreviewImage:image forItemWithToken:token];

  // TODO(crbug.com/40280872): Plumb encoding options from a central config.
  lens::ImageEncodingOptions image_options;
  image_options.max_width = 1024;
  image_options.max_height = 1024;
  image_options.compression_quality = 80;
  _composeboxQueryController->StartFileUploadFlow(token, std::move(input_data),
                                                  image_options);
}

// Handles the read `data` from the given `url` for the item with the given
// `token`. This is the callback for the asynchronous file read.
- (void)onDataReadForItemWithToken:(base::UnguessableToken)token
                           fromURL:(GURL)url
                          withData:(NSData*)data {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  AIMInputItem* item = [self itemForToken:token];
  if (!item) {
    return;
  }

  if (!data) {
    item.state = AIMInputItemState::kError;
    [self.consumer updateState:item.state forItemWithToken:item.token];
    return;
  }

  // Start the file upload immediately.
  item.state = AIMInputItemState::kUploading;
  [self.consumer updateState:item.state forItemWithToken:item.token];

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
      item.token, std::move(inputData), std::nullopt);

  // Concurrently, generate a preview for the UI.
  __weak __typeof(self) weakSelf = self;
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&GeneratePDFPreview, data),
      base::BindOnce(^(UIImage* preview) {
        [weakSelf didLoadPreviewImage:preview forItemWithToken:token];
      }));
}

#pragma mark - AIMOmniboxClientDelegate

- (void)omniboxDidAcceptText:(const std::u16string&)text
              destinationURL:(const GURL&)destinationURL
                isSearchType:(BOOL)isSearchType {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (isSearchType) {
    if (IsAimURL(destinationURL)) {
      [self.consumer setAIModeEnabled:YES];
    }
    [self sendText:[NSString cr_fromString16:text]];
  } else {
    [self.URLLoader loadURL:destinationURL];
  }
}

- (void)omniboxDidChangeText:(const std::u16string&)text
               isSearchQuery:(BOOL)isSearchQuery
         userInputInProgress:(BOOL)userInputInProgress {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  // Update lens and mic button visibility.
  [self.consumer hideLensAndMicButton:text.length()];
}

#pragma mark - Private helpers

/// Updates the consumer items and maybe trigger AIM.
- (void)updateConsumerItems {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  [self.consumer setItems:_items];
  if (_items.count > 0) {
    [self.consumer setAIModeEnabled:YES];
  }
}

@end
