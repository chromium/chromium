// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/coordinator/composebox_input_plate_mediator.h"

#import <PDFKit/PDFKit.h>

#import <memory>
#import <queue>
#import <set>
#import <unordered_map>
#import <utility>

#import "base/apple/foundation_util.h"
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
#import "components/contextual_search/contextual_search_context_controller.h"
#import "components/contextual_search/contextual_search_session_handle.h"
#import "components/lens/contextual_input.h"
#import "components/lens/lens_bitmap_processing.h"
#import "components/omnibox/browser/aim_eligibility_service.h"
#import "components/omnibox/browser/lens_suggest_inputs_utils.h"
#import "components/omnibox/common/omnibox_features.h"
#import "components/omnibox/composebox/ios/composebox_file_upload_observer_bridge.h"
#import "components/omnibox/composebox/ios/composebox_query_controller_ios.h"
#import "components/search/search.h"
#import "components/search_engines/template_url_service.h"
#import "components/search_engines/util.h"
#import "ios/chrome/browser/composebox/coordinator/composebox_constants.h"
#import "ios/chrome/browser/composebox/coordinator/composebox_url_loader.h"
#import "ios/chrome/browser/composebox/coordinator/web_state_deferred_executor.h"
#import "ios/chrome/browser/composebox/public/composebox_input_plate_controls.h"
#import "ios/chrome/browser/composebox/public/features.h"
#import "ios/chrome/browser/composebox/ui/composebox_input_item.h"
#import "ios/chrome/browser/composebox/ui/composebox_metrics_recorder.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/intelligence/persist_tab_context/model/persist_tab_context_browser_agent.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper.h"
#import "ios/chrome/browser/search_engines/model/search_engine_observer_bridge.h"
#import "ios/chrome/browser/shared/model/url/url_util.h"
#import "ios/chrome/browser/shared/model/utils/mime_type_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/browser/url_loading/model/url_loading_util.h"
#import "ios/chrome/common/NSString+Chromium.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_delegate_bridge.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "mojo/public/cpp/base/big_buffer.h"
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

@interface ComposeboxInputPlateMediator () <SearchEngineObserving>
@end

@implementation ComposeboxInputPlateMediator {
  // The ordered list of items for display.
  NSMutableArray<ComposeboxInputItem*>* _items;
  // The C++ session handle for this feature.
  std::unique_ptr<contextual_search::ContextualSearchSessionHandle>
      _contextualSearchSession;
  // The observer bridge for file upload status.
  std::unique_ptr<ComposeboxFileUploadObserverBridge> _composeboxObserverBridge;
  // The different modes for the composebox.
  ComposeboxModeHolder* _modeHolder;
  // The web state list.
  raw_ptr<WebStateList> _webStateList;
  // The favicon loader.
  raw_ptr<FaviconLoader> _faviconLoader;
  // A browser agent for retrieving APC from the cache.
  raw_ptr<PersistTabContextBrowserAgent> _persistTabContextAgent;
  // A template URL service.
  raw_ptr<TemplateURLService> _templateURLService;
  // Observer for the TemplateURLService.
  std::unique_ptr<SearchEngineObserverBridge> _searchEngineObserver;
  // Service to check for AI mode eligibility.
  raw_ptr<AimEligibilityService> _aimEligibilityService;
  // Subscription for AIM eligibility changes.
  base::CallbackListSubscription _aimEligibilitySubscription;

  // Stores the page context wrappers for the duration of the APC retrieval.
  std::unordered_map<web::WebStateID, PageContextWrapper*> _pageContextWrappers;
  std::unordered_map<base::UnguessableToken,
                     web::WebStateID,
                     base::UnguessableTokenHash>
      _latestTabSelectionMapping;

  // Utilitary to delay execution until the web state is loaded.
  WebStateDeferredExecutor* _webStateDeferredExecutor;

  // Check that the different methods are called from the correct sequence, as
  // this class defers work via PostTask APIs.
  SEQUENCE_CHECKER(_sequenceChecker);

  // Whether the textfield is multiline or not.
  BOOL _isMultiline;
  // Whether the browser is in incognito mode.
  BOOL _isIncognito;
  // Whether the mediator is currently updating the compact mode.
  BOOL _isUpdatingCompactMode;
  // Whether the omnibox has text inputted.
  BOOL _hasText;
}

- (instancetype)
    initWithContextualSearchSession:
        (std::unique_ptr<contextual_search::ContextualSearchSessionHandle>)
            contextualSearchSession
                       webStateList:(WebStateList*)webStateList
                      faviconLoader:(FaviconLoader*)faviconLoader
             persistTabContextAgent:
                 (PersistTabContextBrowserAgent*)persistTabContextAgent
                        isIncognito:(BOOL)isIncognito
                         modeHolder:(ComposeboxModeHolder*)modeHolder
                 templateURLService:(TemplateURLService*)templateURLService
              aimEligibilityService:
                  (AimEligibilityService*)aimEligibilityService {
  self = [super init];
  if (self) {
    _items = [NSMutableArray array];
    _contextualSearchSession = std::move(contextualSearchSession);
    _contextualSearchSession->NotifySessionStarted();
    CHECK(_contextualSearchSession->GetController());
    _composeboxObserverBridge =
        std::make_unique<ComposeboxFileUploadObserverBridge>(
            self, _contextualSearchSession->GetController());
    _webStateList = webStateList;
    _faviconLoader = faviconLoader;
    _webStateDeferredExecutor = [[WebStateDeferredExecutor alloc] init];
    _persistTabContextAgent = persistTabContextAgent;
    _isIncognito = isIncognito;
    _modeHolder = modeHolder;
    [_modeHolder addObserver:self];
    _templateURLService = templateURLService;
    _searchEngineObserver =
        std::make_unique<SearchEngineObserverBridge>(self, _templateURLService);
    _aimEligibilityService = aimEligibilityService;
    if (_aimEligibilityService) {
      __weak __typeof(self) weakSelf = self;
      _aimEligibilitySubscription =
          _aimEligibilityService->RegisterEligibilityChangedCallback(
              base::BindRepeating(^{
                [weakSelf updateButtonsVisibility];
              }));
    }
  }
  return self;
}

- (void)disconnect {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  [_modeHolder removeObserver:self];
  _modeHolder = nil;
  _faviconLoader = nullptr;
  _webStateDeferredExecutor = nil;
  _persistTabContextAgent = nullptr;
  _searchEngineObserver.reset();
  _templateURLService = nullptr;
  _aimEligibilitySubscription = {};
  _aimEligibilityService = nullptr;
  _composeboxObserverBridge.reset();
  if (_contextualSearchSession) {
    _contextualSearchSession->NotifySessionAbandoned();
    _contextualSearchSession.reset();
  }
  _webStateList = nil;
  _items = nil;
  _URLLoader = nil;
  _consumer = nil;
}

- (void)processImageItemProvider:(NSItemProvider*)itemProvider
                         assetID:(NSString*)assetID {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);

  BOOL unableToLoadUIImage =
      ![itemProvider canLoadObjectOfClass:[UIImage class]];
  BOOL assetAlreadyLoaded = [self assetAlreadyLoaded:assetID];
  if (unableToLoadUIImage || assetAlreadyLoaded) {
    return;
  }

  ComposeboxInputItem* item = [[ComposeboxInputItem alloc]
      initWithComposeboxInputItemType:ComposeboxInputItemType::
                                          kComposeboxInputItemTypeImage
                              assetID:assetID];
  [_items addObject:item];
  [self updateConsumerItems];
  __block base::UnguessableToken identifier = item.identifier;

  __weak __typeof(self) weakSelf = self;
  // Load the preview image.
  [itemProvider
      loadPreviewImageWithOptions:nil
                completionHandler:^(UIImage* previewImage, NSError* error) {
                  dispatch_async(dispatch_get_main_queue(), ^{
                    [weakSelf didLoadPreviewImage:previewImage
                            forItemWithIdentifier:identifier];
                  });
                }];

  // Concurrently load the full image.
  [itemProvider loadObjectOfClass:[UIImage class]
                completionHandler:^(__kindof id<NSItemProviderReading> object,
                                    NSError* error) {
                  dispatch_async(dispatch_get_main_queue(), ^{
                    [weakSelf didLoadFullImage:(UIImage*)object
                         forItemWithIdentifier:identifier];
                  });
                }];
}

- (void)setConsumer:(id<ComposeboxInputPlateConsumer>)consumer {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  _consumer = consumer;

  if (!_webStateList || !consumer) {
    return;
  }

  BOOL canAttachCurrentTab = [self updateOptionToAttachCurrentTab];

  if (canAttachCurrentTab) {
    [self extractFaviconForCurrentTab];
  }

  if (base::FeatureList::IsEnabled(kComposeboxAutoattachTab) &&
      canAttachCurrentTab) {
    [self attachCurrentTabContent];
  }

  [self updateCompactMode];
  [self updateConsumerItems];
  [self updateConsumerActionsState];
  [self updateButtonsVisibility];
}

- (void)processPDFFileURL:(GURL)PDFFileURL {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  NSString* assetID = base::SysUTF8ToNSString(PDFFileURL.spec());
  if ([self assetAlreadyLoaded:assetID]) {
    return;
  }

  // Check file size.
  NSURL* nsURL = net::NSURLWithGURL(PDFFileURL);
  NSError* error = nil;
  NSNumber* fileSize =
      [[nsURL resourceValuesForKeys:@[ NSURLFileSizeKey ]
                              error:&error] objectForKey:NSURLFileSizeKey];
  if (fileSize && [fileSize unsignedLongLongValue] > kMaxPDFFileSize) {
    [self.delegate showSnackbarForItemUploadDidFail];
    return;
  }

  ComposeboxInputItem* item = [[ComposeboxInputItem alloc]
      initWithComposeboxInputItemType:ComposeboxInputItemType::
                                          kComposeboxInputItemTypeFile
                              assetID:assetID];
  item.title = base::SysUTF8ToNSString(PDFFileURL.ExtractFileName());
  [_items addObject:item];
  [self updateConsumerItems];
  base::UnguessableToken identifier = item.identifier;

  // Read the data in the background then call `onDataReadForItem`.
  __weak __typeof(self) weakSelf = self;
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&ReadDataFromURL, PDFFileURL),
      base::BindOnce(^(NSData* data) {
        [weakSelf onDataReadForItemWithIdentifier:identifier
                                          fromURL:PDFFileURL
                                         withData:data];
      }));
}

- (BOOL)canAddMoreAttachments {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  return _items.count < kAttachmentLimit;
}

- (NSUInteger)maxNumberOfGalleryItemsAllowed {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);

  NSUInteger availableSlots = kAttachmentLimit - _items.count;
  switch (_modeHolder.mode) {
    case ComposeboxMode::kRegularSearch:
    case ComposeboxMode::kAIM: {
      // For RegularSearch and AIM, allow up to kAttachmentLimit items.
      return availableSlots;
    }
    case ComposeboxMode::kImageGeneration: {
      // For ImageGeneration, allow 1 image if no images are present, otherwise
      // 0.
      BOOL hasImage = NO;
      for (ComposeboxInputItem* item in _items) {
        if (item.type ==
            ComposeboxInputItemType::kComposeboxInputItemTypeImage) {
          hasImage = YES;
          break;
        }
      }
      return hasImage ? 0 : MIN(availableSlots, 1);
    }
  }
}

#pragma mark - ComposeboxInputPlateMutator

- (void)removeItem:(ComposeboxInputItem*)item {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  [_items removeObject:item];

  if (_contextualSearchSession) {
    _contextualSearchSession->DeleteFile(item.serverToken);
    [self reloadSuggestions];
  }

  if (base::FeatureList::IsEnabled(kComposeboxAutoattachTab) &&
      _items.count == 0) {
    _modeHolder.mode = ComposeboxMode::kRegularSearch;
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
  if (_modeHolder.mode == ComposeboxMode::kImageGeneration) {
    search_url_request_info->additional_params["imgn"] = "1";
  }
  GURL URL = _contextualSearchSession->CreateSearchUrl(
      std::move(search_url_request_info));
  // TODO(crbug.com/40280872): Handle AIM enabled in the query controller.
  if ([_modeHolder isRegularSearch]) {
    URL = net::AppendOrReplaceQueryParameter(URL, "udm", "24");
  }

  UrlLoadParams params = CreateOmniboxUrlLoadParams(
      URL, /*post_content=*/nullptr, WindowOpenDisposition::CURRENT_TAB,
      ui::PAGE_TRANSITION_GENERATED,
      /*destination_url_entered_without_scheme=*/false, _isIncognito);

  [self.URLLoader loadURLParams:params];
}

#pragma mark - ComposeboxModeObserver

- (void)composeboxModeDidChange:(ComposeboxMode)mode {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (base::FeatureList::IsEnabled(
          omnibox::kComposeboxUsesChromeComposeClient)) {
    [self reloadSuggestions];
  }

  [self.consumer setAIModeEnabled:mode == ComposeboxMode::kAIM];
  [self.consumer
      setImageGenerationEnabled:mode == ComposeboxMode::kImageGeneration];

  switch (mode) {
    case ComposeboxMode::kRegularSearch:
      if (_contextualSearchSession) {
        _contextualSearchSession->ClearFiles();
      }
      [_items removeAllObjects];
      [self updateConsumerItems];
      break;
    case ComposeboxMode::kAIM:
      break;
    case ComposeboxMode::kImageGeneration:
      [self cleanAttachmentsForImageGeneration];
      break;
  }

  [self updateCompactMode];
  [self updateButtonsVisibility];
  [self updateConsumerActionsState];
}

#pragma mark - ComposeboxTabPickerSelectionDelegate

- (std::set<web::WebStateID>)webStateIDsForAttachedTabs {
  std::set<web::WebStateID> webStateIDs;
  for (ComposeboxInputItem* item in _items) {
    web::WebStateID webStateID = _latestTabSelectionMapping[item.identifier];
    if (webStateID.valid()) {
      webStateIDs.insert(webStateID);
    }
  }

  return webStateIDs;
}

- (NSUInteger)nonTabAttachmentCount {
  NSUInteger result = 0;
  for (ComposeboxInputItem* item in _items) {
    if (item.type != ComposeboxInputItemType::kComposeboxInputItemTypeTab) {
      result++;
    }
  }
  return result;
}

- (void)attachSelectedTabsWithWebStateIDs:
            (std::set<web::WebStateID>)selectedWebStateIDs
                        cachedWebStateIDs:
                            (std::set<web::WebStateID>)cachedWebStateIDs {
  [self.metricsRecorder recordTabPickerTabsAttached:selectedWebStateIDs.size()];

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

    base::UnguessableToken identifier =
        [self createInputItemForWebState:webState];

    // When attaching a tab, we must also send its snapshot. The
    // snapshotTabHelper is only created if the webstate is realized. If the
    // tab's APC is not cached, we must also load the webstate so its APC can be
    // extracted on the fly.
    if (cachedWebStateIDs.contains(candidateID)) {
      [_webStateDeferredExecutor webState:webState
                      executeOnceRealized:^{
                        [weakSelf attachWebStateContent:webState
                                             identifier:identifier
                                           hasCachedAPC:YES];
                      }];
    } else {
      [_webStateDeferredExecutor webState:webState
                        executeOnceLoaded:^(BOOL success) {
                          if (!success) {
                            [weakSelf handleFailedAttachment:identifier];
                            return;
                          }
                          [weakSelf attachWebStateContent:webState
                                               identifier:identifier
                                             hasCachedAPC:NO];
                        }];
    }
  }
}

- (void)removeDeselectedIDs:(std::set<web::WebStateID>)deselectedIDs {
  NSArray<ComposeboxInputItem*>* items = [_items copy];
  for (ComposeboxInputItem* item in items) {
    web::WebStateID webStateID = _latestTabSelectionMapping[item.identifier];
    if (webStateID.valid() && deselectedIDs.contains(webStateID)) {
      [self removeItem:item];
      _latestTabSelectionMapping.erase(item.identifier);
    }
  }
}

// Creates an input item for the given `webState` and adds it to the items list.
// Returns the identifier of the created item.
- (base::UnguessableToken)createInputItemForWebState:(web::WebState*)webState {
  ComposeboxInputItem* item = [[ComposeboxInputItem alloc]
      initWithComposeboxInputItemType:ComposeboxInputItemType::
                                          kComposeboxInputItemTypeTab];
  item.title = base::SysUTF16ToNSString(webState->GetTitle());
  [_items addObject:item];
  base::UnguessableToken identifier = item.identifier;
  _latestTabSelectionMapping[identifier] = webState->GetUniqueIdentifier();

  [self updateConsumerItems];
  [self updateConsumerActionsState];

  if (_faviconLoader) {
    __weak __typeof(self) weakSelf = self;

    /// Based on the favicon loader API, this callback could be called twice.
    auto faviconLoadedBlock = ^(FaviconAttributes* attributes, bool cached) {
      if (attributes.faviconImage) {
        [weakSelf didLoadFaviconIcon:attributes.faviconImage
               forItemWithIdentifier:identifier];
      }
    };

    _faviconLoader->FaviconForPageUrl(
        webState->GetVisibleURL(), gfx::kFaviconSize, gfx::kFaviconSize,
        /*fallback_to_google_server=*/true, faviconLoadedBlock);
  }

  return identifier;
}

// Retrieves and attaches web state content (specifically the APC) to an item.
// The content can be fetched from the cache or computed on the fly. An optional
// snapshot of the page can be included.
- (void)attachWebStateContent:(web::WebState*)webState
                   identifier:(base::UnguessableToken)identifier
                 hasCachedAPC:(BOOL)hasCachedAPC {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  __weak __typeof(self) weakSelf = self;
  base::WeakPtr<web::WebState> weakWebState = webState->GetWeakPtr();

  if (hasCachedAPC && _persistTabContextAgent) {
    _persistTabContextAgent->GetSingleContextAsync(
        base::NumberToString(weakWebState->GetUniqueIdentifier().identifier()),
        base::BindOnce(^(std::optional<std::unique_ptr<
                             optimization_guide::proto::PageContext>> context) {
          if (context.has_value()) {
            [weakSelf handlePageContextResponse:std::move(context.value())
                                       webState:weakWebState.get()
                                     identifier:identifier];
          } else {
            [weakSelf handleFailedAttachment:identifier];
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
                                   identifier:identifier];
        } else {
          [weakSelf handleFailedAttachment:identifier];
        }
      })];

  pageContextWrapper.shouldGetAnnotatedPageContent = YES;
  pageContextWrapper.shouldGetSnapshot = YES;
  [pageContextWrapper populatePageContextFieldsAsync];

  _pageContextWrappers[webState->GetUniqueIdentifier()] = pageContextWrapper;
}

// Transforms the page context into input data and uploads the data after a page
// snapshot is generated.
- (void)handlePageContextResponse:
            (std::unique_ptr<optimization_guide::proto::PageContext>)
                page_context
                         webState:(web::WebState*)webState
                       identifier:(base::UnguessableToken)identifier {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);

  if (!webState || !page_context) {
    return;
  }

  _pageContextWrappers.erase(webState->GetUniqueIdentifier());

  __block std::unique_ptr<lens::ContextualInputData> input_data =
      CreateInputDataFromAnnotatedPageContent(
          base::WrapUnique(page_context->release_annotated_page_content()),
          webState);

  __weak __typeof(self) weakSelf = self;
  SnapshotTabHelper::FromWebState(webState)->RetrieveColorSnapshot(
      ^(UIImage* image) {
        [weakSelf didRetrieveColorSnapshot:image
                                 inputData:std::move(input_data)
                                identifier:identifier];
      });
}

// Uploads the tab context for the given identifier.
- (void)uploadTabForIdentifier:(base::UnguessableToken)identifier
                     inputData:
                         (std::unique_ptr<lens::ContextualInputData>)inputData {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (!_contextualSearchSession) {
    return;
  }

  web::WebStateID webStateID = _latestTabSelectionMapping[identifier];
  if (!webStateID.valid()) {
    return;
  }

  __weak __typeof(self) weakSelf = self;
  auto callback = base::BindOnce(
      ^(std::unique_ptr<lens::ContextualInputData> data,
        const base::UnguessableToken& serverToken) {
        [weakSelf onTabContextAdded:serverToken
                      forIdentifier:identifier
                          inputData:std::move(data)];
      },
      std::move(inputData));

  _contextualSearchSession->AddTabContext(webStateID.identifier(),
                                          std::move(callback));
}

// Invoked when a tab has been successfully added to the
// session. Uploads the tab context to the server.
- (void)onTabContextAdded:(base::UnguessableToken)serverToken
            forIdentifier:(base::UnguessableToken)identifier
                inputData:
                    (std::unique_ptr<lens::ContextualInputData>)inputData {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  ComposeboxInputItem* item = [self itemForIdentifier:identifier];
  if (item) {
    item.serverToken = serverToken;
  }

  // TODO(crbug.com/40280872): Plumb encoding options from a central config.
  lens::ImageEncodingOptions image_options;
  image_options.max_width = 1024;
  image_options.max_height = 1024;
  image_options.compression_quality = 80;

  if (_contextualSearchSession) {
    _contextualSearchSession->StartTabContextUploadFlow(
        serverToken, std::move(inputData), image_options);
  }
}

// Invoked when a file context has been successfully uploaded to the server and
// added to the session.
- (void)onFileContextAdded:(base::UnguessableToken)serverToken
             forIdentifier:(base::UnguessableToken)identifier {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  ComposeboxInputItem* item = [self itemForIdentifier:identifier];
  if (item) {
    item.serverToken = serverToken;
  }
}

- (void)extractFaviconForCurrentTab {
  if (!_faviconLoader) {
    return;
  }
  web::WebState* webState = _webStateList->GetActiveWebState();
  if (!webState) {
    return;
  }

  __weak __typeof(self) weakSelf = self;

  auto faviconLoadedBlock = ^(FaviconAttributes* attributes, bool cached) {
    if (attributes.faviconImage) {
      [weakSelf.consumer setCurrentTabFavicon:attributes.faviconImage];
    }
  };

  _faviconLoader->FaviconForPageUrl(
      webState->GetVisibleURL(), gfx::kFaviconSize, gfx::kFaviconSize,
      /*fallback_to_google_server=*/true, faviconLoadedBlock);
}

- (void)attachCurrentTabContent {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (![self canAddMoreAttachments]) {
    [self.delegate showAttachmentLimitError];
    return;
  }
  web::WebState* webState = _webStateList->GetActiveWebState();
  if (!webState) {
    return;
  }

  [self.metricsRecorder
      recordAttachmentButtonUsed:FuseboxAttachmentButtonType::kCurrentTab];

  std::set<web::WebStateID> webStateIDs = [self webStateIDsForAttachedTabs];
  webStateIDs.insert(webState->GetUniqueIdentifier());
  [self attachSelectedTabsWithWebStateIDs:webStateIDs cachedWebStateIDs:{}];
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
  ComposeboxInputItem* item = [self itemForServerToken:fileToken];
  if (!item) {
    return;
  }

  switch (fileUploadStatus) {
    case contextual_search::FileUploadStatus::kUploadSuccessful:
      item.state = ComposeboxInputItemState::kLoaded;
      break;
    case contextual_search::FileUploadStatus::kUploadFailed:
    case contextual_search::FileUploadStatus::kValidationFailed:
    case contextual_search::FileUploadStatus::kUploadExpired:
      [self handleFailedAttachment:item.identifier];
      break;
    case contextual_search::FileUploadStatus::kProcessingSuggestSignalsReady:
      [self reloadSuggestions];
      break;
    case contextual_search::FileUploadStatus::kNotUploaded:
    case contextual_search::FileUploadStatus::kProcessing:
    case contextual_search::FileUploadStatus::kUploadStarted:
      // No-op, as the state is already `Uploading`.
      return;
  }

  [self.consumer updateState:item.state forItemWithIdentifier:item.identifier];
}

#pragma mark - LoadQueryCommands

- (void)loadQuery:(NSString*)query immediately:(BOOL)immediately {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  [self sendText:query];
}

#pragma mark - Private

// Reloads the displayed suggestions based on the attachments/modeHolder.
- (void)reloadSuggestions {
  BOOL shouldRestartAutocomplete = (_items.count == 0);

  if (_items.count == 1) {
    shouldRestartAutocomplete = YES;
    if (_items[0].type ==
        ComposeboxInputItemType::kComposeboxInputItemTypeImage) {
      shouldRestartAutocomplete =
          IsComposeboxFetchContextualSuggestionsForImageEnabled();
    }
  } else if (_items.count > 1) {
    shouldRestartAutocomplete =
        IsComposeboxFetchContextualSuggestionsForMultiAttachmentsEnabled();
  }
  [self.delegate
      reloadAutocompleteSuggestionsRestarting:shouldRestartAutocomplete];
}

// Cleans attachments when switching to image generation mode.
// This method ensures that only one image attachment is kept, and all other
// attachments (including other images, tabs, and files) are removed.
- (void)cleanAttachmentsForImageGeneration {
  NSMutableArray<ComposeboxInputItem*>* itemsToKeep = [NSMutableArray array];
  ComposeboxInputItem* imageToKeep = nil;

  // Find one image to keep.
  for (ComposeboxInputItem* item in _items) {
    if (item.type == ComposeboxInputItemType::kComposeboxInputItemTypeImage &&
        !imageToKeep) {
      imageToKeep = item;
      [itemsToKeep addObject:item];
      break;
    }
  }

  if (itemsToKeep.count == _items.count) {
    // No items were removed.
    return;
  }

  // Find items to remove from the backend.
  for (ComposeboxInputItem* item in _items) {
    if (![itemsToKeep containsObject:item]) {
      if (_contextualSearchSession) {
        _contextualSearchSession->DeleteFile(item.serverToken);
      }
    }
  }

  _items = itemsToKeep;

  [self updateConsumerItems];
}

// Handles the loaded preview `image` for the item with the given `identifier`.
- (void)didLoadPreviewImage:(UIImage*)previewImage
      forItemWithIdentifier:(base::UnguessableToken)identifier {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  ComposeboxInputItem* item = [self itemForIdentifier:identifier];
  if (!item) {
    return;
  }
  // Only set the preview if a preview doesn't already exist. This prevents
  // overwriting the full-res image if it arrives first.
  if (previewImage && !item.previewImage) {
    item.previewImage = previewImage;
    [self.consumer updateState:item.state
         forItemWithIdentifier:item.identifier];
  }
}

// Handles the loaded favicon `image` for the item with the given `identifier`.
- (void)didLoadFaviconIcon:(UIImage*)faviconImage
     forItemWithIdentifier:(base::UnguessableToken)identifier {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  ComposeboxInputItem* item = [self itemForIdentifier:identifier];
  if (!item) {
    return;
  }

  // Update the item's leading icon with the latest fetched favicon.
  if (faviconImage && faviconImage != item.leadingIconImage) {
    item.leadingIconImage = faviconImage;
    [self.consumer updateState:item.state
         forItemWithIdentifier:item.identifier];
  }
}

// Handles the loaded full `image` for the item with the given `identifier`.
- (void)didLoadFullImage:(UIImage*)image
    forItemWithIdentifier:(base::UnguessableToken)identifier {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  ComposeboxInputItem* item = [self itemForIdentifier:identifier];
  if (!item) {
    return;
  }

  if (!image) {
    item.state = ComposeboxInputItemState::kError;
    [self.consumer updateState:item.state
         forItemWithIdentifier:item.identifier];
    return;
  }

  __weak __typeof(self) weakSelf = self;
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, base::BindOnce(^{
        [weakSelf didFinishSimulatedLoadForImage:image
                                  itemIdentifier:identifier];
      }),
      GetImageLoadDelay());
}

// Called after the simulated image load delay for the item with the given
// `identifier`. This simulates a network delay for development purposes.
- (void)didFinishSimulatedLoadForImage:(UIImage*)image
                        itemIdentifier:(base::UnguessableToken)identifier {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  ComposeboxInputItem* item = [self itemForIdentifier:identifier];
  if (!item) {
    return;
  }

  item.state = ComposeboxInputItemState::kUploading;
  [self.consumer updateState:item.state forItemWithIdentifier:item.identifier];

  if (!item.previewImage) {
    item.previewImage = image;
    [self updateConsumerItems];
  }

  base::OnceClosure task;
  __weak __typeof(self) weakSelf = self;
  if (ShouldForceUploadFailure()) {
    // Determine a server token to use for the simulated failure.
    // Using the identifier as the server token for simulation.
    item.serverToken = identifier;
    task = base::BindOnce(^{
      [weakSelf onFileUploadStatusChanged:identifier
                                 mimeType:lens::MimeType::kImage
                         fileUploadStatus:contextual_search::FileUploadStatus::
                                              kUploadFailed
                                errorType:std::nullopt];
    });
  } else {
    task = base::BindOnce(^{
      [weakSelf uploadImage:image itemIdentifier:identifier];
    });
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, std::move(task), GetUploadDelay());
}

// Uploads the `image` for the item with the given `identifier`.
- (void)uploadImage:(UIImage*)image
     itemIdentifier:(base::UnguessableToken)identifier {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  ComposeboxInputItem* item = [self itemForIdentifier:identifier];
  if (!item || !_contextualSearchSession) {
    return;
  }

  // TODO(crbug.com/40280872): Plumb encoding options from a central config.
  lens::ImageEncodingOptions image_options;
  image_options.max_width = 1024;
  image_options.max_height = 1024;
  image_options.compression_quality = 80;

  NSData* data = UIImagePNGRepresentation(image);
  mojo_base::BigBuffer buffer(base::apple::NSDataToSpan(data));
  __weak __typeof(self) weakSelf = self;
  auto callback = base::BindOnce(^(const base::UnguessableToken& serverToken) {
    [weakSelf onFileContextAdded:serverToken forIdentifier:identifier];
  });

  _contextualSearchSession->AddFileContext(kPortableNetworkGraphicMimeType,
                                           std::move(buffer), image_options,
                                           std::move(callback));
}

// Returns the item with the given `identifier` or nil if not found.
- (ComposeboxInputItem*)itemForIdentifier:(base::UnguessableToken)identifier {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  for (ComposeboxInputItem* item in _items) {
    if (item.identifier == identifier) {
      return item;
    }
  }
  return nil;
}

// Returns the item with the given `serverToken` or nil if not found.
- (ComposeboxInputItem*)itemForServerToken:(base::UnguessableToken)serverToken {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  for (ComposeboxInputItem* item in _items) {
    if (item.serverToken == serverToken) {
      return item;
    }
  }
  return nil;
}

// Handles uploading the context after the snapshot is generated.
- (void)didRetrieveColorSnapshot:(UIImage*)image
                       inputData:(std::unique_ptr<lens::ContextualInputData>)
                                     input_data
                      identifier:(base::UnguessableToken)identifier {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (image) {
    NSData* data = UIImagePNGRepresentation(image);
    std::vector<uint8_t> image_vector_data([data length]);
    [data getBytes:image_vector_data.data() length:[data length]];
    input_data->viewport_screenshot_bytes = std::move(image_vector_data);
  }
  [self didLoadPreviewImage:image forItemWithIdentifier:identifier];

  [self uploadTabForIdentifier:identifier inputData:std::move(input_data)];
}

// Handles the read `data` from the given `url` for the item with the given
// `identifier`. This is the callback for the asynchronous file read.
- (void)onDataReadForItemWithIdentifier:(base::UnguessableToken)identifier
                                fromURL:(GURL)url
                               withData:(NSData*)data {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  ComposeboxInputItem* item = [self itemForIdentifier:identifier];
  if (!item) {
    return;
  }

  if (!data) {
    item.state = ComposeboxInputItemState::kError;
    [self.consumer updateState:item.state
         forItemWithIdentifier:item.identifier];
    return;
  }

  // Start the file upload immediately.
  item.state = ComposeboxInputItemState::kUploading;
  [self.consumer updateState:item.state forItemWithIdentifier:item.identifier];

  if (_contextualSearchSession) {
    mojo_base::BigBuffer buffer(base::apple::NSDataToSpan(data));
    __weak __typeof(self) weakSelf = self;
    auto callback =
        base::BindOnce(^(const base::UnguessableToken& serverToken) {
          [weakSelf onFileContextAdded:serverToken forIdentifier:identifier];
        });
    _contextualSearchSession->AddFileContext(
        kAdobePortableDocumentFormatMimeType, std::move(buffer), std::nullopt,
        std::move(callback));
  }

  // Concurrently, generate a preview for the UI.
  __weak __typeof(self) weakSelf = self;
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&GeneratePDFPreview, data),
      base::BindOnce(^(UIImage* preview) {
        [weakSelf didLoadPreviewImage:preview forItemWithIdentifier:identifier];
      }));
}

- (BOOL)assetAlreadyLoaded:(NSString*)assetID {
  if (!assetID) {
    return NO;
  }
  for (ComposeboxInputItem* item in _items) {
    if ([item.assetID isEqualToString:assetID]) {
      return YES;
    }
  }

  return NO;
}

- (BOOL)isEligibleToAIM {
  if (!_aimEligibilityService) {
    return NO;
  }
  return _aimEligibilityService->IsAimEligible();
}

- (BOOL)isDSEGoogle {
  if (!_templateURLService) {
    return NO;
  }
  return search::DefaultSearchProviderIsGoogle(_templateURLService);
}

- (BOOL)compactModeRequired {
  if (!IsComposeboxCompactModeEnabled()) {
    return NO;
  }
  BOOL requiresExpansion = _isMultiline ||
                           _modeHolder.mode == ComposeboxMode::kAIM ||
                           _modeHolder.mode == ComposeboxMode::kImageGeneration;
  return !requiresExpansion;
}

#pragma mark - ComposeboxOmniboxClientDelegate

- (std::optional<lens::proto::LensOverlaySuggestInputs>)suggestInputs {
  if (!_contextualSearchSession) {
    return std::nullopt;
  }
  return _contextualSearchSession->GetSuggestInputs();
}

- (void)omniboxDidAcceptText:(const std::u16string&)text
              destinationURL:(const GURL&)destinationURL
               URLLoadParams:(const UrlLoadParams&)URLLoadParams
                isSearchType:(BOOL)isSearchType {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  // If omnibox proposes an AIM suggestion, switch to AI mode.
  if (IsAimURL(destinationURL) && [_modeHolder isRegularSearch]) {
    _modeHolder.mode = ComposeboxMode::kAIM;
  }
  switch (_modeHolder.mode) {
    case ComposeboxMode::kRegularSearch:
      [self.URLLoader loadURLParams:URLLoadParams];
      [self.metricsRecorder recordAutocompleteRequestTypeAtNavigation:
                                AutocompleteRequestType::kSearch];
      break;
    case ComposeboxMode::kAIM:
      [self.metricsRecorder recordAutocompleteRequestTypeAtNavigation:
                                AutocompleteRequestType::kAIMode];
      [self sendText:[NSString cr_fromString16:text]];
      break;
    case ComposeboxMode::kImageGeneration:
      [self.metricsRecorder recordAutocompleteRequestTypeAtNavigation:
                                AutocompleteRequestType::kImageGeneration];
      [self sendText:[NSString cr_fromString16:text]];
      break;
  }
}

- (void)omniboxDidChangeText:(const std::u16string&)text
               isSearchQuery:(BOOL)isSearchQuery
         userInputInProgress:(BOOL)userInputInProgress {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  _hasText = text.length() > 0;
  [self updateButtonsVisibility];
}

- (ComposeboxMode)composeboxMode {
  return _modeHolder.mode;
}

#pragma mark - Private helpers

- (void)handleFailedAttachment:(base::UnguessableToken)identifier {
  [self.delegate showSnackbarForItemUploadDidFail];
  [self removeItem:[self itemForIdentifier:identifier]];
}

- (void)updateButtonsVisibility {
  BOOL compactMode = [self compactModeRequired];
  BOOL hasAttachments = _items.count > 0;
  BOOL hasContent = hasAttachments || _hasText;
  BOOL dseGoogle = [self isDSEGoogle];
  BOOL eligibleToAIM = [self isEligibleToAIM];
  BOOL allowsMultimodalActions = dseGoogle && eligibleToAIM;
  BOOL canSend = hasContent && !compactMode;
  BOOL showShortcuts = !hasContent && !canSend;
  BOOL showLeadingImage = !compactMode || !allowsMultimodalActions;

  ComposeboxInputPlateControls leadingAction =
      allowsMultimodalActions ? ComposeboxInputPlateControls::kPlus
                              : ComposeboxInputPlateControls::kNone;

  ComposeboxInputPlateControls leadingImage =
      showLeadingImage ? ComposeboxInputPlateControls::kLeadingImage
                       : ComposeboxInputPlateControls::kNone;

  ComposeboxInputPlateControls modeSwitchButton;
  switch (_modeHolder.mode) {
    case ComposeboxMode::kAIM:
      modeSwitchButton = ComposeboxInputPlateControls::kAIM;
      break;
    case ComposeboxMode::kImageGeneration:
      modeSwitchButton = ComposeboxInputPlateControls::kCreateImage;
      break;
    case ComposeboxMode::kRegularSearch:
      modeSwitchButton = ComposeboxInputPlateControls::kNone;
      break;
  }

  ComposeboxInputPlateControls trailingAction =
      ComposeboxInputPlateControls::kNone;
  if (canSend) {
    trailingAction = ComposeboxInputPlateControls::kSend;
  } else if (showShortcuts && dseGoogle) {
    trailingAction = ComposeboxInputPlateControls::kVoice |
                     ComposeboxInputPlateControls::kLens;
  } else if (showShortcuts && !dseGoogle) {
    trailingAction = ComposeboxInputPlateControls::kVoice;
  }

  ComposeboxInputPlateControls visibleControls =
      (leadingImage | leadingAction | modeSwitchButton | trailingAction);

  [self.consumer updateVisibleControls:visibleControls];
}

- (BOOL)updateOptionToAttachCurrentTab {
  web::WebState* webState = _webStateList->GetActiveWebState();
  if (!webState) {
    [_consumer hideAttachCurrentTabAction:YES];
    return NO;
  }

  std::set<web::WebStateID> alreadyProcessedIDs =
      [self webStateIDsForAttachedTabs];
  BOOL isNTP = IsUrlNtp(webState->GetVisibleURL());
  BOOL alreadyProcessed =
      alreadyProcessedIDs.contains(webState->GetUniqueIdentifier());

  BOOL canAttachTab = !isNTP && !alreadyProcessed;
  [_consumer hideAttachCurrentTabAction:!canAttachTab];
  return canAttachTab;
}

/// Updates the consumer actions enabled/disable state.
- (void)updateConsumerActionsState {
  BOOL hasTabOrFile = NO;
  for (ComposeboxInputItem* item in _items) {
    if (item.type == ComposeboxInputItemType::kComposeboxInputItemTypeTab ||
        item.type == ComposeboxInputItemType::kComposeboxInputItemTypeFile) {
      hasTabOrFile = YES;
      break;
    }
  }
  [self.consumer disableCreateImageActions:hasTabOrFile];

  BOOL isImageCreation = _modeHolder.mode == ComposeboxMode::kImageGeneration;
  [self.consumer disableAttachTabActions:isImageCreation];
  [self.consumer disableAttachFileActions:isImageCreation];

  BOOL canAddMoreImages = [self maxNumberOfGalleryItemsAllowed] > 0;
  [self.consumer disableGalleryActions:!canAddMoreImages];
  [self.consumer disableCameraActions:!canAddMoreImages];
}
/// Updates the consumer items and maybe trigger AIM.
- (void)updateConsumerItems {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  [self.consumer setItems:_items];
  [self updateOptionToAttachCurrentTab];
  [self updateConsumerActionsState];
  [self updateButtonsVisibility];

  if (_items.count > 0 && [_modeHolder isRegularSearch]) {
    // AI mode is implicitly enabled by items attachment.
    [self.metricsRecorder
        recordAiModeActivationSource:AiModeActivationSource::kImplicit];
    _modeHolder.mode = ComposeboxMode::kAIM;
  }
}

/// Updates the consumer whether to show in compact mode.
- (void)updateCompactMode {
  BOOL compact = [self compactModeRequired];
  [self.consumer setCompact:compact];
}

#pragma mark - SearchEngineObserving

- (void)searchEngineChanged {
  [self updateButtonsVisibility];
}

- (void)templateURLServiceShuttingDown:(TemplateURLService*)urlService {
  CHECK_EQ(urlService, _templateURLService);
  _templateURLService = nullptr;
}

#pragma mark - TextFieldViewContainingHeightDelegate

- (void)textFieldViewContaining:(UIView<TextFieldViewContaining>*)sender
                didChangeHeight:(CGFloat)height {
  // Prevent re-entrant calls if `_isMultiline` changes when updating compact
  // mode.
  if (_isUpdatingCompactMode) {
    return;
  }
  _isMultiline = sender.numberOfLines > 1;
  _isUpdatingCompactMode = YES;
  [self updateCompactMode];
  [self updateButtonsVisibility];
  _isUpdatingCompactMode = NO;
}

@end
