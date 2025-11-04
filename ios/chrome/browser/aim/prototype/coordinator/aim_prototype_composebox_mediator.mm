// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/aim/prototype/coordinator/aim_prototype_composebox_mediator.h"

#import <PDFKit/PDFKit.h>

#import <memory>
#import <queue>
#import <set>
#import <unordered_map>
#import <utility>

#import "base/files/file_path.h"
#import "base/functional/bind.h"
#import "base/ios/block_types.h"
#import "base/memory/ptr_util.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/ref_counted_memory.h"
#import "base/sequence_checker.h"
#import "base/strings/string_number_conversions.h"
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
#import "ios/chrome/browser/intelligence/persist_tab_context/model/persist_tab_context_browser_agent.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper.h"
#import "ios/chrome/browser/shared/model/url/url_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/common/NSString+Chromium.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_delegate_bridge.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "net/base/apple/url_conversions.h"
#import "net/base/url_util.h"
#import "ui/base/page_transition_types.h"
#import "ui/gfx/favicon_size.h"
#import "url/gurl.h"

// Utilitary to delay execution until the web state is loaded.
@interface WebStateDefferedExecutor : NSObject <CRWWebStateObserver>

// Executes the given `completion` once the web state is loaded.
- (void)webState:(web::WebState*)webState
    executeOnceLoaded:(ProceduralBlock)completion;

@end

@implementation WebStateDefferedExecutor {
  // Observer for the web state loading.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserverBridge;
  // Stores the callbacks to be used once the web state is loaded.
  std::unordered_map<web::WebStateID, ProceduralBlock> _callbacks;
  // Temporarily stores the active observations.
  std::unordered_map<web::WebStateID, base::WeakPtr<web::WebState>>
      _activeObservations;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _webStateObserverBridge =
        std::make_unique<web::WebStateObserverBridge>(self);
  }

  return self;
}

- (void)webState:(web::WebState*)webState
    executeOnceLoaded:(ProceduralBlock)completion {
  _callbacks[webState->GetUniqueIdentifier()] = completion;
  BOOL realized = webState->IsRealized();
  BOOL loading = webState->IsLoading();

  if (!realized) {
    [self observeWebState:webState];
    [self forceRealizeWebState:webState];
    return;
  }

  if (loading) {
    [self observeWebState:webState];
    return;
  }

  [self callCompletionForID:webState->GetUniqueIdentifier()];
}

#pragma mark - Private

- (void)observeWebState:(web::WebState*)webState {
  webState->AddObserver(_webStateObserverBridge.get());
  _activeObservations[webState->GetUniqueIdentifier()] = webState->GetWeakPtr();
}

- (void)removeObserverForWebState:(web::WebState*)webState {
  webState->RemoveObserver(_webStateObserverBridge.get());
  _activeObservations.erase(webState->GetUniqueIdentifier());
}

- (void)forceRealizeWebState:(web::WebState*)webState {
  web::IgnoreOverRealizationCheck();
  webState->ForceRealized();
}

- (void)callCompletionForID:(web::WebStateID)webStateID {
  if (auto block = _callbacks[webStateID]) {
    block();
    _callbacks.erase(webStateID);
  }
}

- (void)removeRemainingWebStateObservations {
  std::vector<base::WeakPtr<web::WebState>> remainingObservedWebStates;
  remainingObservedWebStates.reserve(_activeObservations.size());

  for (auto kv : _activeObservations) {
    remainingObservedWebStates.push_back(kv.second);
  }

  for (base::WeakPtr<web::WebState> weakWebState : remainingObservedWebStates) {
    web::WebState* webState = weakWebState.get();
    if (webState) {
      [self removeObserverForWebState:webState];
    }
  }
}

- (void)dealloc {
  [self removeRemainingWebStateObservations];
}

#pragma mark - CRWWebStateObserver

- (void)webState:(web::WebState*)webState didLoadPageWithSuccess:(BOOL)success {
  [self removeObserverForWebState:webState];
  [self callCompletionForID:webState->GetUniqueIdentifier()];
}

- (void)webStateDestroyed:(web::WebState*)webState {
  [self removeObserverForWebState:webState];
  [self callCompletionForID:webState->GetUniqueIdentifier()];
}

@end

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
  // A browser agent for retrieving APC from the cache.
  raw_ptr<PersistTabContextBrowserAgent> _persistTabContextAgent;

  // Stores the page context wrappers for the duration of the APC retrieval.
  std::unordered_map<web::WebStateID, PageContextWrapper*> _pageContextWrappers;
  std::unordered_map<base::UnguessableToken,
                     web::WebStateID,
                     base::UnguessableTokenHash>
      _latestTabSelectionMapping;

  // Utilitary to delay execution until the web state is loaded.
  WebStateDefferedExecutor* _webStateDefferedExecutor;

  // Check that the different methods are called from the correct sequence, as
  // this class defers work via PostTask APIs.
  SEQUENCE_CHECKER(_sequenceChecker);
}

- (instancetype)
    initWithComposeboxQueryController:
        (std::unique_ptr<ComposeboxQueryControllerIOS>)composeboxQueryController
                         webStateList:(WebStateList*)webStateList
                        faviconLoader:(FaviconLoader*)faviconLoader
               persistTabContextAgent:
                   (PersistTabContextBrowserAgent*)persistTabContextAgent {
  self = [super init];
  if (self) {
    _items = [NSMutableArray array];
    _composeboxQueryController = std::move(composeboxQueryController);
    _composeboxObserverBridge =
        std::make_unique<ComposeboxFileUploadObserverBridge>(
            self, _composeboxQueryController.get());
    _composeboxQueryController->InitializeIfNeeded();
    _webStateList = webStateList;
    _faviconLoader = faviconLoader;
    _webStateDefferedExecutor = [[WebStateDefferedExecutor alloc] init];
    _persistTabContextAgent = persistTabContextAgent;
  }
  return self;
}

- (void)disconnect {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
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

#pragma mark - AimPrototypeTabPickerSelectionDelegate

- (std::set<web::WebStateID>)webStateIDsForAttachedTabs {
  std::set<web::WebStateID> webStateIDs;
  for (AIMInputItem* item in _items) {
    web::WebStateID webStateID = _latestTabSelectionMapping[item.token];
    if (webStateID.valid()) {
      webStateIDs.insert(webStateID);
    }
  }

  return webStateIDs;
}

- (void)attachSelectedTabsWithWebStateIDs:
    (std::set<web::WebStateID>)selectedWebStateIDs {
  _pageContextWrappers.clear();

  std::set<web::WebStateID> alreadyProcessedIDs =
      [self webStateIDsForAttachedTabs];

  std::set<web::WebStateID> deselectedIDs;
  set_difference(alreadyProcessedIDs.begin(), alreadyProcessedIDs.end(),
                 selectedWebStateIDs.begin(), selectedWebStateIDs.end(),
                 inserter(deselectedIDs, deselectedIDs.begin()));
  [self removeDeselectedIDs:deselectedIDs];

  std::set<web::WebStateID> newlyAddedIDs;
  set_difference(selectedWebStateIDs.begin(), selectedWebStateIDs.end(),
                 alreadyProcessedIDs.begin(), alreadyProcessedIDs.end(),
                 inserter(newlyAddedIDs, newlyAddedIDs.begin()));

  __weak __typeof(self) weakSelf = self;
  for (int i = 0; i < _webStateList->count(); ++i) {
    web::WebState* webState = _webStateList->GetWebStateAt(i);
    web::WebStateID candidateID = webState->GetUniqueIdentifier();
    if (!newlyAddedIDs.contains(candidateID)) {
      continue;
    }

    const base::UnguessableToken token =
        [self createInputItemForWebState:webState];
    _latestTabSelectionMapping[token] = webState->GetUniqueIdentifier();

    if (IsAimPrototypeTabPickerCachedAPCEnabled()) {
      [self attachWebStateContent:webState includeSnapshot:NO token:token];
      continue;
    }

    [_webStateDefferedExecutor webState:webState
                      executeOnceLoaded:^{
                        [weakSelf attachWebStateContent:webState
                                        includeSnapshot:NO
                                                  token:token];
                      }];
  }
}

- (void)removeDeselectedIDs:(std::set<web::WebStateID>)deselectedIDs {
  NSArray<AIMInputItem*>* items = [_items copy];
  for (AIMInputItem* item in items) {
    web::WebStateID webStateID = _latestTabSelectionMapping[item.token];
    if (webStateID.valid() && deselectedIDs.contains(webStateID)) {
      [self removeItem:item];
      _latestTabSelectionMapping.erase(item.token);
    }
  }
}

- (const base::UnguessableToken)createInputItemForWebState:
    (web::WebState*)webState {
  AIMInputItem* item = [[AIMInputItem alloc]
      initWithAimInputItemType:AIMInputItemType::kAIMInputItemTypeTab];
  item.title = base::SysUTF16ToNSString(webState->GetTitle());
  [_items addObject:item];
  [self updateConsumerItems];

  __weak __typeof(self) weakSelf = self;
  const base::UnguessableToken token = item.token;

  /// Based on the favicon loader API, this callback could be called twice.
  auto faviconLoadedBlock = ^(FaviconAttributes* attributes, bool cached) {
    if (attributes.faviconImage) {
      [weakSelf didLoadFaviconIcon:attributes.faviconImage
                  forItemWithToken:token];
    }
  };

  _faviconLoader->FaviconForPageUrl(
      webState->GetVisibleURL(), gfx::kFaviconSize, gfx::kFaviconSize,
      /*fallback_to_google_server=*/true, faviconLoadedBlock);

  return token;
}

// Retrieves and attaches web state content (specifically the APC) to an item.
// The content can be fetched from the cache or computed on the fly. An optional
// snapshot of the page can be included.
- (void)attachWebStateContent:(web::WebState*)webState
              includeSnapshot:(BOOL)includeSnapshot
                        token:(const base::UnguessableToken)token {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  __weak __typeof(self) weakSelf = self;
  base::WeakPtr<web::WebState> weakWebState = webState->GetWeakPtr();

  if (IsAimPrototypeTabPickerCachedAPCEnabled() && _persistTabContextAgent) {
    _persistTabContextAgent->GetSingleContextAsync(
        base::NumberToString(weakWebState->GetUniqueIdentifier().identifier()),
        base::BindOnce(^(std::optional<std::unique_ptr<
                             optimization_guide::proto::PageContext>> context) {
          if (context.has_value()) {
            [weakSelf handlePageContextResponse:std::move(context.value())
                                       webState:weakWebState.get()
                                includeSnapshot:NO
                                          token:token];
          }
        }));
    return;
  }

  PageContextWrapper* pageContextWrapper = [[PageContextWrapper alloc]
        initWithWebState:webState
      completionCallback:base::BindOnce(^(
                             PageContextWrapperCallbackResponse response) {
        if (response.has_value()) {
          [weakSelf handlePageContextResponse:std::move(response.value())
                                     webState:weakWebState.get()
                              includeSnapshot:includeSnapshot
                                        token:token];
        }
      })];

  pageContextWrapper.shouldGetAnnotatedPageContent = YES;
  pageContextWrapper.shouldGetSnapshot = includeSnapshot;
  [pageContextWrapper populatePageContextFieldsAsync];

  _pageContextWrappers[webState->GetUniqueIdentifier()] = pageContextWrapper;
}

// Transforms the page context into input data and uploads the data after a page
// snapshot is generated.
- (void)handlePageContextResponse:
            (std::unique_ptr<optimization_guide::proto::PageContext>)
                page_context
                         webState:(web::WebState*)webState
                  includeSnapshot:(BOOL)includeSnapshot
                            token:(const base::UnguessableToken)token {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);

  _pageContextWrappers.erase(webState->GetUniqueIdentifier());

  if (!webState || !page_context) {
    return;
  }

  __block std::unique_ptr<lens::ContextualInputData> input_data =
      CreateInputDataFromAnnotatedPageContent(
          base::WrapUnique(page_context->release_annotated_page_content()),
          webState);

  if (includeSnapshot) {
    __weak __typeof(self) weakSelf = self;
    SnapshotTabHelper::FromWebState(webState)->RetrieveColorSnapshot(
        ^(UIImage* image) {
          [weakSelf didRetrieveColorSnapshot:image
                                   inputData:std::move(input_data)
                                       token:token];
        });

    return;
  }

  [self startFileUploadFlowWithToken:token inputData:std::move(input_data)];
}

- (void)startFileUploadFlowWithToken:(const base::UnguessableToken)token
                           inputData:
                               (std::unique_ptr<lens::ContextualInputData>)
                                   input_data {
  // TODO(crbug.com/40280872): Plumb encoding options from a central config.
  lens::ImageEncodingOptions image_options;
  image_options.max_width = 1024;
  image_options.max_height = 1024;
  image_options.compression_quality = 80;
  _composeboxQueryController->StartFileUploadFlow(token, std::move(input_data),
                                                  image_options);
}

- (void)attachCurrentTabContent {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  web::WebState* webState = _webStateList->GetActiveWebState();
  if (!webState) {
    return;
  }
  const base::UnguessableToken token =
      [self createInputItemForWebState:webState];
  [self attachWebStateContent:webState includeSnapshot:YES token:token];
}

#pragma mark - ComposeboxFileUploadObserver

- (void)onFileUploadStatusChanged:(const base::UnguessableToken&)fileToken
                         mimeType:(lens::MimeType)mimeType
                 fileUploadStatus:
                     (contextual_search::FileUploadStatus)fileUploadStatus
                        errorType:(const std::optional<
                                      contextual_search::FileUploadErrorType>&)
                                      errorType {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  AIMInputItem* item = [self itemForToken:fileToken];
  if (!item) {
    return;
  }

  switch (fileUploadStatus) {
    case contextual_search::FileUploadStatus::kUploadSuccessful:
      item.state = AIMInputItemState::kLoaded;
      break;
    case contextual_search::FileUploadStatus::kUploadFailed:
    case contextual_search::FileUploadStatus::kValidationFailed:
    case contextual_search::FileUploadStatus::kUploadExpired:
      item.state = AIMInputItemState::kError;
      break;
    case contextual_search::FileUploadStatus::kNotUploaded:
    case contextual_search::FileUploadStatus::kProcessing:
    case contextual_search::FileUploadStatus::kProcessingSuggestSignalsReady:
    case contextual_search::FileUploadStatus::kUploadStarted:
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
                         fileUploadStatus:contextual_search::FileUploadStatus::
                                              kUploadFailed
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

  [self startFileUploadFlowWithToken:token inputData:std::move(input_data)];
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
  // Update send, lens and mic button visibility.
  [self.consumer hideLensAndMicButton:text.length()];
  [self.consumer hideSendButton:!text.length()];
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
