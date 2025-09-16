// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/aim/prototype/coordinator/aim_prototype_mediator.h"

#import <PDFKit/PDFKit.h>

#import <memory>

#import "base/files/file_path.h"
#import "base/functional/bind.h"
#import "base/memory/ptr_util.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/ref_counted_memory.h"
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
#import "ios/chrome/browser/aim/prototype/public/features.h"
#import "ios/chrome/browser/aim/prototype/ui/aim_input_item.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/common/NSString+Chromium.h"
#import "ios/web/public/web_state.h"
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

@implementation AIMPrototypeMediator {
  // The ordered list of items for display.
  NSMutableArray<AIMInputItem*>* _items;
  // The URL loading browser agent.
  raw_ptr<UrlLoadingBrowserAgent> _urlLoadingBrowserAgent;
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
}

- (instancetype)initWithUrlLoadingBrowserAgent:
                    (UrlLoadingBrowserAgent*)urlLoadingBrowserAgent
                     composeboxQueryController:
                         (std::unique_ptr<ComposeboxQueryControllerIOS>)
                             composeboxQueryController
                                  webStateList:(WebStateList*)webStateList {
  self = [super init];
  if (self) {
    _items = [NSMutableArray array];
    _urlLoadingBrowserAgent = urlLoadingBrowserAgent;
    _composeboxQueryController = std::move(composeboxQueryController);
    _composeboxObserverBridge =
        std::make_unique<ComposeboxFileUploadObserverBridge>(
            self, _composeboxQueryController.get());
    _composeboxQueryController->NotifySessionStarted();
    _webStateList = webStateList;
  }
  return self;
}

- (void)disconnect {
  _composeboxQueryController->NotifySessionAbandoned();
  _urlLoadingBrowserAgent = nullptr;
  _composeboxQueryController.reset();
  _composeboxObserverBridge.reset();
}

- (void)processImageItemProvider:(NSItemProvider*)itemProvider {
  if (![itemProvider canLoadObjectOfClass:[UIImage class]]) {
    return;
  }

  AIMInputItem* item = [[AIMInputItem alloc]
      initWithAimInputItemType:AIMInputItemType::kAIMInputItemTypeImage];
  [_items addObject:item];
  [self.consumer setItems:_items];
  const base::UnguessableToken& token = item.token;

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
  AIMInputItem* item = [[AIMInputItem alloc]
      initWithAimInputItemType:AIMInputItemType::kAIMInputItemTypeFile];
  item.title = base::SysUTF8ToNSString(PDFFileURL.ExtractFileName());
  item.subtitle = @"PDF";
  [_items addObject:item];
  [self.consumer setItems:_items];
  const base::UnguessableToken& token = item.token;

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

  // TODO(crbug.com/442371203): Dismissing the view directly here will
  // lead to a crash because some calls made after pressing the return
  // key are still being performed. This hack postpones the dismiss action.
  __weak AIMPrototypeMediator* weakSelf = self;
  base::OnceClosure completion = base::BindOnce(^{
    [weakSelf dismissAimPrototype];
  });
  constexpr base::TimeDelta kDelay = base::Seconds(0);
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, std::move(completion), kDelay);
}

- (void)setAIModeEnabled:(BOOL)enabled {
  _AIModeEnabled = enabled;
}

- (void)attachCurrentTabContent {
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
  [_pageContextWrapper populatePageContextFieldsAsync];
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

  [self.consumer updateState:item.state forItemWithToken:item.token];
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
    [self.consumer updateState:item.state forItemWithToken:item.token];
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
                             itemToken:(const base::UnguessableToken&)token {
  AIMInputItem* item = [self itemForToken:token];
  if (!item) {
    return;
  }

  item.state = AIMInputItemState::kUploading;
  [self.consumer updateState:item.state forItemWithToken:item.token];

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
  lens::ImageEncodingOptions image_options;
  image_options.max_width = 1024;
  image_options.max_height = 1024;
  image_options.compression_quality = 80;

  _composeboxQueryController->StartFileUploadFlow(
      item.token, std::move(input_data), image_options);
}

// Returns the item with the given `token` or nil if not found.
- (AIMInputItem*)itemForToken:(const base::UnguessableToken&)token {
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
  web::WebState* webState = _webStateList->GetActiveWebState();

  if (!pageContextResponse.has_value() || !webState) {
    return;
  }

  AIMInputItem* item = [[AIMInputItem alloc]
      initWithAimInputItemType:AIMInputItemType::kAIMInputItemTypeTab];
  item.title = base::SysUTF16ToNSString(webState->GetTitle());
  item.subtitle = base::SysUTF8ToNSString(webState->GetVisibleURL().host());
  [_items addObject:item];
  [self.consumer setItems:_items];
  __block const base::UnguessableToken& token = item.token;

  std::unique_ptr<optimization_guide::proto::PageContext> page_context =
      std::move(pageContextResponse.value());

  __block std::unique_ptr<lens::ContextualInputData> input_data =
      CreateInputDataFromAnnotatedPageContent(
          base::WrapUnique(page_context->release_annotated_page_content()),
          webState);

  __weak __typeof(self) weakSelf = self;
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
                           token:(const base::UnguessableToken&)token {
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
- (void)onDataReadForItemWithToken:(const base::UnguessableToken&)token
                           fromURL:(GURL)url
                          withData:(NSData*)data {
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
      base::BindPostTaskToCurrentDefault(base::BindOnce(^(UIImage* preview) {
        AIMPrototypeMediator* strongSelf = weakSelf;
        if (!strongSelf) {
          return;
        }
        [strongSelf didLoadPreviewImage:preview forItemWithToken:token];
      })));
}

#pragma mark - AIMOmniboxClientDelegate

- (void)omniboxDidAcceptText:(const std::u16string&)text
              destinationURL:(const GURL&)destinationURL
                isSearchType:(BOOL)isSearchType {
  if (isSearchType) {
    [self sendText:[NSString cr_fromString16:text]];
  } else {
    UrlLoadParams params = UrlLoadParams::InCurrentTab(destinationURL);
    params.web_params.transition_type = ui::PAGE_TRANSITION_GENERATED;
    _urlLoadingBrowserAgent->Load(params);

    // TODO(crbug.com/442371203): Dismissing the view directly here will
    // lead to a crash because some calls made after pressing the return
    // key are still being performed. This hack postpones the dismiss action.
    __weak AIMPrototypeMediator* weakSelf = self;
    base::OnceClosure completion = base::BindOnce(^{
      [weakSelf dismissAimPrototype];
    });
    constexpr base::TimeDelta kDelay = base::Seconds(0);
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, std::move(completion), kDelay);
  }
}

#pragma mark - Private helpers

- (void)dismissAimPrototype {
  [self.delegate dismissAimPrototype];
}

@end
